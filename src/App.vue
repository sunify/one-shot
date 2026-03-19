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
      <button v-if="!isConnected" class="button primary" :disabled="isBusy || isConnecting" @click="connect">
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

<style scoped>
.shell {
  min-height: 100vh;
  position: relative;
  width: 700px;
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
}

h1 + .primary {
  margin-top: 2rem;
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

.primary:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.grid {
  gap: 20px;
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
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
</style>
