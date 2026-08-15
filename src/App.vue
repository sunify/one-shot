<script setup>
import { computed, onBeforeUnmount, watch, ref } from 'vue'
import throttle from 'lodash-es/throttle'
import ColorControl from './components/controls/ColorControl.vue'
import GestureField from './components/controls/GestureField.vue'
import TurboModeSwitch from './components/controls/TurboModeSwitch.vue'
import PanelSection from './components/layout/PanelSection.vue'
import { useConfiguratorState } from './composables/useConfiguratorState'
import { useDeviceConnection } from './composables/useDeviceConnection'
import { useLightingPreview } from './composables/useLightingPreview'
import { HOTKEY_SELECT_VALUE, MEDIA_KEY_OPTIONS, MODIFIER_OPTIONS } from './protocol'

const {
  applyButtonEvent,
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
  applyButtonEvent,
  applyConfig,
  applyDeviceInfo,
  applyDeviceOptions,
  clearButtonEvents,
  deviceDefinition,
  deviceType,
  form,
  supportsTurboMode,
})

const appTitle = computed(() => {
  if (!isConnected.value) return 'Конфигуратор'
  const name = productName.value || deviceDefinition.value.name
  return `Конфигуратор<br />${name}`
})

const gestureOptions = MEDIA_KEY_OPTIONS.map((option) => ({
  label: option.label,
  value: `consumer:${option.value}`,
}))

const isMacLike = /Mac|iPhone|iPad|iPod/.test(navigator.platform)
const modifierOptions = MODIFIER_OPTIONS

const scheduleAutoSave = throttle(async () => {
  if (!isConnected.value || isConnecting.value) {
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

onBeforeUnmount(() => {
  scheduleAutoSave.cancel()
  clearInterval(animationInterval)
  disconnect()
})

watch(
  form,
  () => {
    if (suppressAutoSave.value) {
      suppressAutoSave.value = false
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
const currentPreviewAnimation = computed(() => currentPreviewBinding.value
  ? {
      ...currentPreviewBinding.value.animation,
      controlId: currentPreviewBinding.value.controlId,
    }
  : null)

function handleGestureFieldOpen(binding) {
  currentPreviewBinding.value = binding.animation ? binding : null;
}

function handleGestureFieldClose() {
  currentPreviewBinding.value = null;
}

let previewPressTimeout
function handlePreviewPress(previewId = 'main') {
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
  <main class="shell" :class="{ 'connected': isConnected }" :style="colorPreviewStyle">
    <section class="hero">
      <h1 v-html="appTitle" />
      <div v-if="!isConnected" class="connection-actions">
        <button class="button primary" :disabled="isBusy || isConnecting" @click="connectBluetooth">
          {{ isConnecting ? 'Подключение...' : 'Подключить по Bluetooth' }}
        </button>
        <button class="button secondary" :disabled="isBusy || isConnecting" @click="connect">
          Подключить по USB
        </button>
      </div>
      <p v-if="!isConnected" class="connection-hint">
        Тестовый rrrraw доступен для Bluetooth-настройки постоянно.
      </p>
    </section>

    <PanelSection :panel-class="`color-panel ${supportsLighting ? '' : 'no-lighting'}`">
      <component
        :is="deviceDefinition.preview"
        :active-animation="currentPreviewAnimation"
        :is-pressed="isDevicePressed"
        :is-rainbow="isRainbow"
        :pressed-controls="controlPressStates"
        :width="deviceDefinition.previewWidth ?? 250"
        @press="handlePreviewPress"
        @press-start="handlePreviewPressStart"
        @press-end="handlePreviewPressEnd"
      />
      <ColorControl
        v-if="isConnected && supportsLighting"
        v-model="selectedColor"
        :animation-mode="form.animationMode"
        :sleep-timeout="form.sleepTimeout"
        @update:animation-mode="form.animationMode = $event"
        @update:sleep-timeout="form.sleepTimeout = $event"
      />
    </PanelSection>

    <PanelSection v-if="isConnected">
      <TurboModeSwitch
        v-if="supportsTurboMode"
        v-model="form.turboMode"
      />
      <div class="grid">
        <section v-for="control in controls" :key="control.id" class="control-group">
          <h2 v-if="controls.length > 1" class="control-label">{{ control.label }}</h2>
          <div class="control-bindings">
            <GestureField
              v-for="binding in control.bindings"
              :key="binding.key"
              :gesture="form[binding.key]"
              :gesture-options="gestureOptions"
              :hotkey-select-value="HOTKEY_SELECT_VALUE"
              :is-mac-like="isMacLike"
              :label="binding.label"
              :modifier-options="modifierOptions"
              :allow-modifier-only="binding.capabilities?.includes('modifierHold')"
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
  max-width: 1100px;
  width: 100%;
  margin: 0;
  padding: 10vh 100px 64px 100px;
  text-align: center;
}

.hero {
  border: 0;
  background: transparent;
  border-radius: 0;
  box-shadow: none;
  margin-bottom: 4rem;
  text-align: center;
  padding: 0;
}

.connected .hero {
  margin-bottom: 5.8rem;
}

h1 {
  font-size: 3.4rem;
  line-height: 1;
  font-weight: 900;
  margin-top: 1rem;
  color: var(--color-title);
}

.connection-actions {
  align-items: center;
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  margin-top: 2rem;
}

.connection-hint {
  color: var(--color-title);
  font-size: 0.8rem;
  line-height: 1.35;
  margin: 1rem auto 0;
  max-width: 25rem;
  opacity: 0.5;
}

h2 {
  font-size: 1.4rem;
  font-weight: 500;
  line-height: 1;
  letter-spacing: -0.03em;
}

.primary {
  padding: 14px 16px;
  font-size: 1rem;
  line-height: 1;
  text-align: center;
  font-weight: 500;
}

.secondary {
  background: transparent;
  color: var(--color-title);
  font-size: 0.9rem;
  padding: 8px 12px;
}

.primary:disabled {
  opacity: 0.45;
  cursor: not-allowed;
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
}
</style>
