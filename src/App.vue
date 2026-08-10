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
  applyConfig,
  applyDeviceInfo,
  applyDeviceOptions,
  caseColors,
  controls,
  deviceDefinition,
  deviceType,
  form,
  isDevicePressed,
  selectedColor,
  supportsLighting,
  supportsTurboMode,
  suppressAutoSave,
  updateBinding,
} = useConfiguratorState()

const { colorPreviewStyle, isRainbow } = useLightingPreview(form, caseColors, supportsLighting)

const {
  connect,
  disconnect,
  isBusy,
  isConnected,
  isConnecting,
  productName,
  resetConfig,
  saveConfig,
  statusText,
} = useDeviceConnection({
  applyConfig,
  applyDeviceInfo,
  applyDeviceOptions,
  deviceDefinition,
  deviceType,
  form,
  isDevicePressed,
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

const currentDeviceAnimation = ref(null);
function handleGestureFieldOpen(binding) {
  currentDeviceAnimation.value = binding.animation;
}

function handleGestureFieldClose() {
  currentDeviceAnimation.value = null;
}

function delay(ms) {
  return new Promise((resolve) => {
    window.setTimeout(resolve, ms)
  })
}
async function runAnimation(animation) {
  if (animation.type === 'tap') {
    for (let i = 0; i < animation.count; i += 1) {
      isDevicePressed.value = true;
      await delay(100);
      isDevicePressed.value = false;
      await delay(200);
    }
  } else if (animation.type === 'press') {
    isDevicePressed.value = true;
    await delay(700);
    isDevicePressed.value = false;
  }
}

let animationInterval;
watch(currentDeviceAnimation, () => {
  clearInterval(animationInterval);
  if (currentDeviceAnimation.value !== null) {
    const runFrame = () => {
      runAnimation(currentDeviceAnimation.value);
    };
    runFrame();
    animationInterval = setInterval(runFrame, 2000);
  } else {
    isDevicePressed.value = false;
  }
});
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
      <component
        :is="deviceDefinition.preview"
        :is-pressed="isDevicePressed"
        :is-rainbow="isRainbow"
        :width="250"
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
  color: var(--color-title);
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
  gap: 28px;
  display: flex;
  flex-direction: column;
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
</style>
