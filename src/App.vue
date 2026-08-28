<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, watch, ref } from 'vue'
import throttle from 'lodash-es/throttle'
import ColorControl from './components/controls/ColorControl.vue'
import GestureField from './components/controls/GestureField.vue'
import TurboModeSwitch from './components/controls/TurboModeSwitch.vue'
import PanelSection from './components/layout/PanelSection.vue'
import { useConfiguratorState } from './composables/useConfiguratorState'
import { useDeviceConnection } from './composables/useDeviceConnection'
import { useLightingPreview } from './composables/useLightingPreview'
import { getDeviceDefinition, hasDeviceDefinition } from './devices/deviceDefinitions'
import { DEVICE_TYPES, HOTKEY_SELECT_VALUE, MEDIA_KEY_OPTIONS, MODIFIER_OPTIONS } from './protocol'

const previewDeviceParam = new URLSearchParams(window.location.search).get('preview')
const previewDeviceAliases = {
  'one-shot': DEVICE_TYPES.oneShot,
  magic: DEVICE_TYPES.magicButton,
  'magic-button': DEVICE_TYPES.magicButton,
  bebop: DEVICE_TYPES.bebop,
  rrrraw: DEVICE_TYPES.rrrraw,
}
const previewDeviceType = previewDeviceAliases[previewDeviceParam]
  ?? Number.parseInt(previewDeviceParam ?? '', 10)
const isPreviewConnection = ref(hasDeviceDefinition(previewDeviceType))

const {
  applyButtonEvent: applyConfiguratorButtonEvent,
  applyConfig,
  applyDeviceInfo,
  applyDeviceOptions,
  caseColors,
  clearButtonEvents,
  controlPressStates,
  controls,
  deviceDefinition,
  deviceType,
  form,
  isDevicePressed,
  selectedColor,
  supportsLighting,
  supportsTurboMode,
  suppressAutoSave,
  setPreviewControlPressed,
  setPreviewSurfacePressed,
  updateBinding,
} = useConfiguratorState()

const { colorPreviewStyle, isRainbow } = useLightingPreview(form, caseColors, supportsLighting)
const physicalPreviewAnimation = ref(null)
const selectedControlId = ref(null)
const shellElement = ref(null)
const previewComponent = ref(null)
const gestureEditor = ref(null)
const connectorGeometry = ref(null)
let physicalEncoderAnimationTimeout
let physicalEncoderEventSequence = 0
let connectorAnimationFrame = null
let connectorResizeObserver = null

const selectedControl = computed(() => (
  controls.value.find((control) => control.id === selectedControlId.value)
  ?? controls.value[0]
  ?? null
))

function selectControl(controlId) {
  const control = controls.value.find((candidate) => candidate.id === controlId)
  if (!control) return

  selectedControlId.value = control.id
}

function selectPreviewControl(previewId) {
  const control = controls.value.find(
    (candidate) => (candidate.previewId ?? candidate.id) === previewId,
  )
  selectControl(control?.id ?? previewId)
}

function handlePhysicalButtonEvent(protocolId, isPressed) {
  applyConfiguratorButtonEvent(protocolId, isPressed)
  if (!isPressed) return

  const control = controls.value.find(
    (candidate, index) => (candidate.protocolId ?? index) === protocolId,
  )
  selectControl(control?.id)
}

watch(
  controls,
  (availableControls) => {
    const control = availableControls.find((candidate) => candidate.id === selectedControlId.value)
      ?? availableControls[0]
    selectedControlId.value = control?.id ?? null
  },
  { immediate: true },
)

function getComponentElement(component) {
  return component?.$el ?? component
}

function updateConnectorGeometry() {
  connectorAnimationFrame = null

  const shell = shellElement.value
  const preview = getComponentElement(previewComponent.value)
  const editor = gestureEditor.value
  const previewControlId = selectedControl.value?.previewId ?? selectedControl.value?.id
  const controlAnchor = preview && [...preview.querySelectorAll('[data-control-anchor]')]
    .find((element) => element.dataset.controlAnchor === previewControlId)
  if (!isConnected.value || controls.value.length < 2 || !controlAnchor || !shell || !editor) {
    connectorGeometry.value = null
    return
  }

  const shellRect = shell.getBoundingClientRect()
  const anchorRect = controlAnchor.getBoundingClientRect()
  const editorRect = editor.getBoundingClientRect()
  const startX = anchorRect.left - shellRect.left + anchorRect.width / 2
  const startY = anchorRect.top - shellRect.top + anchorRect.height / 2 + 10
  const targetX = editorRect.left - shellRect.left + editorRect.width / 2
  const targetY = editorRect.top - shellRect.top + 24
  const elbowY = targetY - 50

  connectorGeometry.value = {
    height: shell.scrollHeight,
    path: `M ${startX} ${startY} L ${startX} ${elbowY} L ${targetX} ${targetY}`,
    startX,
    startY,
    targetX,
    targetY,
    width: shell.clientWidth,
  }
}

async function scheduleConnectorUpdate() {
  await nextTick()
  if (connectorAnimationFrame != null) {
    window.cancelAnimationFrame(connectorAnimationFrame)
  }
  connectorAnimationFrame = window.requestAnimationFrame(updateConnectorGeometry)
}

function observeConnectorElements() {
  connectorResizeObserver?.disconnect()
  connectorResizeObserver = new ResizeObserver(scheduleConnectorUpdate)
  const preview = getComponentElement(previewComponent.value)
  if (preview) connectorResizeObserver.observe(preview)
  if (gestureEditor.value) connectorResizeObserver.observe(gestureEditor.value)
}

const {
  connect,
  connectBluetooth,
  disconnect,
  isBusy,
  isConnected,
  isConnecting,
  productName,
  resetConfig,
  saveConfig,
  statusText,
} = useDeviceConnection({
  applyButtonEvent: handlePhysicalButtonEvent,
  applyConfig,
  applyDeviceInfo,
  applyDeviceOptions,
  applyEncoderEvent: handlePhysicalEncoderEvent,
  clearButtonEvents,
  deviceDefinition,
  deviceType,
  form,
  supportsTurboMode,
})

watch([selectedControl, isConnected, supportsTurboMode], async () => {
  await nextTick()
  observeConnectorElements()
  scheduleConnectorUpdate()
}, { immediate: true })

onMounted(() => {
  window.addEventListener('resize', scheduleConnectorUpdate)
})

const connectingMethod = ref(null)
const CUSTOM_PRESETS_STORAGE_KEY = 'one-shot:custom-presets:v1'
const SAVE_PRESET_VALUE = '__save_preset__'

function loadCustomPresets() {
  try {
    const stored = JSON.parse(window.localStorage.getItem(CUSTOM_PRESETS_STORAGE_KEY) ?? '[]')
    if (!Array.isArray(stored)) return []
    return stored.filter((preset) => (
      typeof preset?.id === 'string'
      && typeof preset?.label === 'string'
      && Number.isInteger(preset?.deviceType)
      && preset?.bindings
      && typeof preset.bindings === 'object'
    ))
  } catch {
    return []
  }
}

const customPresets = ref(loadCustomPresets())
const deviceCustomPresets = computed(() => customPresets.value.filter(
  (preset) => preset.deviceType === deviceType.value,
))
const availablePresets = computed(() => [
  ...(deviceDefinition.value.presets ?? []),
  ...deviceCustomPresets.value,
])

function persistCustomPresets() {
  window.localStorage.setItem(CUSTOM_PRESETS_STORAGE_KEY, JSON.stringify(customPresets.value))
}

async function handleConnect(method) {
  connectingMethod.value = method

  try {
    if (method === 'bluetooth') {
      await connectBluetooth()
    } else {
      await connect()
    }
  } finally {
    if (!isConnected.value && connectingMethod.value === method) {
      connectingMethod.value = null
    }
  }
}

if (isPreviewConnection.value) {
  const previewDefinition = getDeviceDefinition(previewDeviceType)
  applyConfig({
    deviceType: previewDeviceType,
    ...previewDefinition.defaults,
  })
  applyDeviceInfo(previewDefinition.defaultInfo)
  productName.value = previewDefinition.name
  isConnected.value = true
  statusText.value = 'Подключено (preview)'
}

const appTitle = computed(() => {
  if (!isConnected.value) return 'Конфигуратор'
  if (deviceDefinition.value.configuratorTitle) return deviceDefinition.value.configuratorTitle
  const name = productName.value || deviceDefinition.value.name
  return `${name}`
})

const gestureOptions = MEDIA_KEY_OPTIONS.map((option) => ({
  label: option.label,
  value: `consumer:${option.value}`,
}))

const isMacLike = /Mac|iPhone|iPad|iPod/.test(navigator.platform)
const modifierOptions = MODIFIER_OPTIONS

const scheduleAutoSave = throttle(async () => {
  if (isPreviewConnection.value || !isConnected.value || isConnecting.value) {
    return
  }

  if (isBusy.value) {
    scheduleAutoSave()
    return
  }

  try {
    await saveConfig()
  } catch (error) {
    statusText.value = error?.message ?? 'Не удалось сохранить конфигурацию'
  }
}, 50)

function handleInvalidHotkeyChar() {
  statusText.value = 'Пока поддерживаются латинские буквы, цифры и основные знаки'
}

function applyPreset(presetId, selectElement) {
  const preset = availablePresets.value.find((candidate) => candidate.id === presetId)
  if (!preset) return

  for (const [key, gesture] of Object.entries(preset.bindings)) {
    updateBinding(key, { ...gesture })
  }
  handleGestureFieldClose()
  statusText.value = `Пресет «${preset.label}» применён`
  selectElement.value = ''
}

function currentGestureBindings() {
  const keys = new Set(
    deviceDefinition.value.configLayouts
      .flatMap((layout) => layout.fields)
      .filter((field) => field.type === 'gesture')
      .map((field) => field.key),
  )
  return Object.fromEntries(
    [...keys]
      .filter((key) => form[key] != null)
      .map((key) => [key, { ...form[key] }]),
  )
}

function saveCustomPreset(selectElement) {
  const label = window.prompt('Название пресета')?.trim()
  if (!label) {
    selectElement.value = ''
    return
  }

  const existing = customPresets.value.find(
    (preset) => preset.deviceType === deviceType.value && preset.label === label,
  )
  const preset = {
    id: existing?.id ?? `custom:${deviceType.value}:${Date.now()}`,
    label,
    deviceType: deviceType.value,
    bindings: currentGestureBindings(),
  }

  if (existing) {
    customPresets.value = customPresets.value.map(
      (candidate) => candidate.id === existing.id ? preset : candidate,
    )
  } else {
    customPresets.value = [...customPresets.value, preset]
  }

  try {
    persistCustomPresets()
    statusText.value = `Пресет «${label}» сохранён`
  } catch (error) {
    statusText.value = error?.message ?? 'Не удалось сохранить пресет'
  }
  selectElement.value = ''
}

function handlePresetSelect(event) {
  const selectElement = event.target
  if (selectElement.value === SAVE_PRESET_VALUE) {
    saveCustomPreset(selectElement)
    return
  }
  applyPreset(selectElement.value, selectElement)
}

onBeforeUnmount(() => {
  scheduleAutoSave.cancel()
  clearInterval(animationInterval)
  clearTimeout(physicalEncoderAnimationTimeout)
  connectorResizeObserver?.disconnect()
  window.removeEventListener('resize', scheduleConnectorUpdate)
  if (connectorAnimationFrame != null) {
    window.cancelAnimationFrame(connectorAnimationFrame)
  }
  disconnect()
})

watch(
  form,
  () => {
    if (suppressAutoSave.value) {
      return
    }

    if (isConnected.value) {
      scheduleAutoSave()
    }
  },
  { deep: true },
)

watch(
  () => form.turboMode,
  () => handleGestureFieldClose(),
)

const currentPreviewBinding = ref(null);
const currentPreviewAnimation = computed(() => physicalPreviewAnimation.value ?? (
  currentPreviewBinding.value
    ? {
        ...currentPreviewBinding.value.animation,
        controlId: currentPreviewBinding.value.controlId,
      }
    : null
))

function handlePhysicalEncoderEvent(controlId, direction) {
  selectControl(controlId)
  physicalEncoderEventSequence += 1
  physicalPreviewAnimation.value = {
    type: 'rotate-step',
    direction: direction === 'cw' ? 'ccw' : 'cw',
    controlId,
    sequence: physicalEncoderEventSequence,
  }
  clearTimeout(physicalEncoderAnimationTimeout)
  physicalEncoderAnimationTimeout = window.setTimeout(() => {
    physicalPreviewAnimation.value = null
    physicalEncoderAnimationTimeout = null
  }, 180)
}

function handleGestureFieldOpen(binding) {
  currentPreviewBinding.value = binding.animation ? binding : null;
}

function handleGestureFieldClose() {
  currentPreviewBinding.value = null;
}

let previewPressTimeout
function handlePreviewPress(previewId = 'main') {
  selectPreviewControl(previewId)
  if (previewPressTimeout) {
    clearTimeout(previewPressTimeout)
  }
  setPreviewSurfacePressed(previewId, true)
  previewPressTimeout = window.setTimeout(() => {
    setPreviewSurfacePressed(previewId, false)
    previewPressTimeout = null
  }, 120)
}

function handlePreviewPressStart(previewId) {
  selectPreviewControl(previewId)
  setPreviewSurfacePressed(previewId, true)
}

function handlePreviewPressEnd(previewId) {
  setPreviewSurfacePressed(previewId, false)
}

function delay(ms) {
  return new Promise((resolve) => {
    window.setTimeout(resolve, ms)
  })
}
async function runAnimation(binding) {
  const { animation, controlId } = binding
  if (animation.type === 'tap') {
    for (let i = 0; i < animation.count; i += 1) {
      setPreviewControlPressed(controlId, true);
      await delay(100);
      setPreviewControlPressed(controlId, false);
      await delay(200);
    }
  } else if (animation.type === 'press') {
    setPreviewControlPressed(controlId, true);
    await delay(700);
    setPreviewControlPressed(controlId, false);
  }
}

let animationInterval;
watch(currentPreviewBinding, (binding, previousBinding) => {
  clearInterval(animationInterval);
  if (previousBinding) {
    setPreviewControlPressed(previousBinding.controlId, false)
  }
  if (binding !== null) {
    if (binding.animation.type === 'rotate') {
      return
    }
    const runFrame = () => {
      runAnimation(binding);
    };
    runFrame();
    animationInterval = setInterval(runFrame, 2000);
  } else {
    for (const control of controls.value) {
      setPreviewControlPressed(control.id, false)
    }
  }
});
</script>

<template>
  <main ref="shellElement" class="shell" :class="{ 'connected': isConnected }" :style="colorPreviewStyle">
    <svg
      v-if="connectorGeometry"
      class="control-connector"
      :height="connectorGeometry.height"
      :viewBox="`0 0 ${connectorGeometry.width} ${connectorGeometry.height}`"
      aria-hidden="true"
      preserveAspectRatio="none"
    >
      <path :d="connectorGeometry.path" />
      <circle :cx="connectorGeometry.startX" :cy="connectorGeometry.startY" r="2.5" />
      <circle :cx="connectorGeometry.targetX" :cy="connectorGeometry.targetY" r="2.5" />
    </svg>
    <select
      v-if="isConnected && (deviceDefinition.presets?.length || deviceCustomPresets.length)"
      class="input select preset-select"
      aria-label="Пресет"
      value=""
      @change="handlePresetSelect"
    >
      <option value="" disabled>Пресет</option>
      <optgroup v-if="deviceDefinition.presets?.length" label="Встроенные">
        <option v-for="preset in deviceDefinition.presets" :key="preset.id" :value="preset.id">
          {{ preset.label }}
        </option>
      </optgroup>
      <optgroup v-if="deviceCustomPresets.length" label="Мои пресеты">
        <option v-for="preset in deviceCustomPresets" :key="preset.id" :value="preset.id">
          {{ preset.label }}
        </option>
      </optgroup>
      <optgroup label="Действия">
        <option :value="SAVE_PRESET_VALUE">Сохранить текущий…</option>
      </optgroup>
    </select>
    <section class="hero">
      <h1 v-if="isConnected" v-html="appTitle" />
      <h1 v-else class="connection-title">Настройщик</h1>
      <div v-if="!isConnected" class="connection-actions">
        <button
          class="button connection-button"
          :disabled="connectingMethod === 'bluetooth'"
          @click="handleConnect('bluetooth')"
        >
          {{ connectingMethod === 'bluetooth' ? 'Подключение...' : 'Подключить по Bluetooth' }}
        </button>
        <button
          class="button connection-button"
          :disabled="connectingMethod === 'usb'"
          @click="handleConnect('usb')"
        >
          {{ connectingMethod === 'usb' ? 'Подключение...' : 'Подключить по USB' }}
        </button>
      </div>
    </section>

    <PanelSection v-if="isConnected" :panel-class="`preview-panel color-panel ${supportsLighting ? '' : 'no-lighting'}`">
      <component
        ref="previewComponent"
        :is="deviceDefinition.preview"
        v-bind="deviceDefinition.previewProps"
        :active-animation="currentPreviewAnimation"
        :is-pressed="isDevicePressed"
        :is-rainbow="isRainbow"
        :pressed-controls="controlPressStates"
        :width="deviceDefinition.previewWidth ?? 250"
        @press="handlePreviewPress"
        @press-start="handlePreviewPressStart"
        @press-end="handlePreviewPressEnd"
        @rotate-step="handlePhysicalEncoderEvent"
      />
      <ColorControl
        v-if="isConnected && supportsLighting"
        class="preview-settings"
        v-model="selectedColor"
        :animation-mode="form.animationMode"
        :sleep-timeout="form.sleepTimeout"
        @update:animation-mode="form.animationMode = $event"
        @update:sleep-timeout="form.sleepTimeout = $event"
      />
    </PanelSection>

    <PanelSection v-if="isConnected" panel-class="bindings-panel">
      <TurboModeSwitch
        v-if="supportsTurboMode"
        v-model="form.turboMode"
      />
      <div class="grid">
        <section v-if="selectedControl" ref="gestureEditor" class="control-group">
          <h2 v-if="controls.length > 1" class="control-label">{{ selectedControl.label }}</h2>
          <div class="control-bindings">
            <GestureField
              v-for="binding in selectedControl.bindings"
              :key="binding.key"
              :gesture="form[binding.key]"
              :gesture-options="gestureOptions"
              :hotkey-select-value="HOTKEY_SELECT_VALUE"
              :is-mac-like="isMacLike"
              :label="binding.label"
              :modifier-options="modifierOptions"
              :allow-modifier-only="binding.capabilities?.includes('modifierHold')"
              :disabled="binding.disabled"
              :show-mouse-options="binding.capabilities?.includes('mouse')"
              @open="handleGestureFieldOpen(binding)"
              @close="handleGestureFieldClose"
              @invalid-hotkey-char="handleInvalidHotkeyChar"
              @update:gesture="updateBinding(binding.key, $event)"
            />
          </div>
        </section>
      </div>
    </PanelSection>

    <div class="footer">
      lun<em>ё</em>v, 2026
    </div>
  </main>
</template>

<style scoped>
.shell {
  min-height: 100vh;
  position: relative;
  min-width: 700px;
  max-width: 1300px;
  width: 100%;
  margin: 0;
  padding: 4vh 100px 20px 100px;
  text-align: center;
}

.shell:not(.connected) {
  align-items: center;
  display: grid;
  justify-items: center;
  padding: 64px 50px 300px;
}

.preset-select {
  position: absolute;
  right: 100px;
  top: 20px;
  width: auto;
  min-width: 150px;
  z-index: 7;
}

.control-connector {
  left: 0;
  overflow: visible;
  pointer-events: none;
  position: absolute;
  top: 0;
  width: 100%;
  z-index: 5;
}

.control-connector path {
  fill: none;
  stroke: #000;
  stroke-linecap: square;
  stroke-linejoin: miter;
  stroke-width: 2.5px;
  vector-effect: non-scaling-stroke;
}

.control-connector circle {
  fill: #000;
}

.hero {
  border: 0;
  background: transparent;
  border-radius: 0;
  box-shadow: none;
  text-align: center;
  padding: 0;
}

.connected .hero {
  margin-bottom: -7.5rem;
}

h1 {
  font-size: 18rem;
  line-height: 1;
  font-weight: 900;
  margin-top: 1rem;
  color: var(--color-title);
  display: flex;
  white-space: nowrap;
  justify-content: center;
}

.connection-actions {
  align-items: center;
  display: flex;
  flex-direction: row;
  gap: 16px;
  justify-content: center;
}

.connection-title {
  font-size: 9rem;
  margin-bottom: 3rem;
}

h2 {
  font-size: 1.4rem;
  font-weight: 500;
  line-height: 1;
  letter-spacing: -0.03em;
}

.connection-button {
  align-items: center;
  display: inline-flex;
  font-size: 1rem;
  font-weight: 500;
  height: 64px;
  justify-content: center;
  padding: 14px 20px;
  width: 280px;
}

.connection-button:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}

.grid {
  gap: 28px;
  display: flex;
  align-items: center;
  justify-content: center;
}

.control-bindings {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
  justify-content: center;
}

.control-label {
  margin: 0 0 12px;
}

.footer {
  position: absolute;
  bottom: 20px;
  font-size: 1.1rem;
  font-weight: 500;
  text-align: center;
  left: 0;
  right: 0;
  opacity: 0.3;
}

.control-group {
  max-width: 500px;
  padding-top: 40px;
  position: relative;
  z-index: 6;
}

@media print {
  .shell {
    min-height: 0;
    min-width: 0;
    max-width: none;
    padding-bottom: 0;
  }

  .control-connector,
  .connection-actions,
  .bindings-panel,
  .preset-select,
  .preview-settings {
    display: none !important;
  }

  .preview-panel {
    margin-bottom: 0 !important;
    padding-bottom: 0 !important;
  }

  .footer {
    bottom: 64px;
    position: fixed;
  }
}
</style>
