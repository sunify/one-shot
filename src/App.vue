<script setup>
import { computed, onBeforeUnmount, watch } from 'vue'
import throttle from 'lodash-es/throttle'
import DevicePreview from './components/DevicePreview.vue'
import ColorControl from './components/controls/ColorControl.vue'
import GestureField from './components/controls/GestureField.vue'
import PanelSection from './components/layout/PanelSection.vue'
import { useConfiguratorState } from './composables/useConfiguratorState'
import { useDeviceConnection } from './composables/useDeviceConnection'
import { useLightingPreview } from './composables/useLightingPreview'
import { HOTKEY_SELECT_VALUE, MEDIA_KEY_OPTIONS, MODIFIER_OPTIONS, getDeviceName } from './protocol'

const {
  applyConfig,
  deviceType,
  form,
  gestureFields,
  isDevicePressed,
  selectedColor,
  supportsLighting,
  suppressAutoSave,
  updateGesture,
} = useConfiguratorState()

const { colorPreviewStyle } = useLightingPreview(form, deviceType)

const {
  connect,
  disconnect,
  isBusy,
  isConnected,
  isConnecting,
  resetConfig,
  saveConfig,
  statusText,
} = useDeviceConnection({
  applyConfig,
  deviceType,
  form,
  isDevicePressed,
})

const appTitle = computed(() =>
  isConnected.value ? `Конфигуратор<br />${getDeviceName(deviceType.value)}` : 'Конфигуратор',
)

const gestureOptions = [
  {
    label: 'Медиа',
    options: MEDIA_KEY_OPTIONS.map((option) => ({
      label: option.label,
      value: `consumer:${option.value}`,
    })),
  },
]

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
</script>

<template>
  <main class="shell" :class="{ 'connected': isConnected }" :style="colorPreviewStyle">
    <section class="hero">
      <h1 v-html="appTitle" />
      <button v-if="!isConnected" class="primary" :disabled="isBusy || isConnecting" @click="connect">
        {{ isConnecting ? 'Подключение...' : 'Подключить устройство' }}
      </button>
    </section>

    <PanelSection :panel-class="`color-panel ${supportsLighting ? '' : 'no-lighting'}`">
      <DevicePreview :is-pressed="isDevicePressed" :width="250" />
      <ColorControl
        v-if="isConnected && supportsLighting"
        v-model="selectedColor"
        :breathing-enabled="form.breathingEnabled"
        @update:breathing-enabled="form.breathingEnabled = $event"
      />
    </PanelSection>

    <PanelSection v-if="isConnected">
      <div class="grid">
        <GestureField
          v-for="gestureField in gestureFields"
          :key="gestureField.key"
          :gesture="form[gestureField.key]"
          :gesture-options="gestureOptions"
          :hotkey-select-value="HOTKEY_SELECT_VALUE"
          :is-mac-like="isMacLike"
          :label="gestureField.label"
          :modifier-options="modifierOptions"
          @invalid-hotkey-char="handleInvalidHotkeyChar"
          @update:gesture="updateGesture(gestureField.key, $event)"
        />
      </div>
    </PanelSection>

    <div class="footer">
      lun<em>ё</em>v, 2026
    </div>
  </main>
</template>
