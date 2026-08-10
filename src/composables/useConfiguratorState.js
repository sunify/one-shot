import { computed, reactive, ref } from 'vue'
import {
  DEVICE_CAPABILITIES,
  DEVICE_TYPES,
  THIRD_ACTION_TRIGGERS,
} from '../protocol'
import { getDeviceDefinition } from '../devices/deviceDefinitions'

function cloneGesture(gesture) {
  return {
    type: gesture.type,
    code: gesture.code,
    modifiers: gesture.modifiers,
  }
}

export function useConfiguratorState() {
  const deviceType = ref(DEVICE_TYPES.oneShot)
  const deviceDefinition = computed(() => getDeviceDefinition(deviceType.value))
  const isDevicePressed = ref(false)
  const suppressAutoSave = ref(false)
  const numLeds = ref(1)
  const deviceCapabilities = ref(0)
  const thirdActionTrigger = ref(THIRD_ACTION_TRIGGERS.tripleTap)
  const caseColors = reactive({
    keycap: '#ffffff',
    topCase: '#ffffff',
    topCaseShade: '#cf00ff',
    bottomCase: '#ffffff',
  })

  const form = reactive({
    deviceType: DEVICE_TYPES.oneShot,
    ...deviceDefinition.value.defaults,
  })

  const selectedColor = computed({
    get() {
      return `#${[form.red, form.green, form.blue].map((value) => value.toString(16).padStart(2, '0')).join('')}`
    },
    set(value) {
      form.red = Number.parseInt(value.slice(1, 3), 16)
      form.green = Number.parseInt(value.slice(3, 5), 16)
      form.blue = Number.parseInt(value.slice(5, 7), 16)
    },
  })

  const supportsLighting = computed(() => numLeds.value > 0)
  const supportsTurboMode = computed(
    () => (deviceCapabilities.value & DEVICE_CAPABILITIES.turboMode) !== 0,
  )

  const controls = computed(() => {
    const context = {
      form,
      thirdActionTrigger: thirdActionTrigger.value,
    }
    const resolve = (value) => typeof value === 'function' ? value(context) : value

    return deviceDefinition.value.controls
      .filter((control) => control.when?.(context) ?? true)
      .map((control) => ({
        ...control,
        bindings: control.bindings
          .filter((binding) => binding.when?.(context) ?? true)
          .map((binding) => ({
            ...binding,
            label: resolve(binding.label),
            animation: resolve(binding.animation),
          })),
      }))
  })

  function applyConfig(config) {
    suppressAutoSave.value = true
    deviceType.value = config.deviceType ?? DEVICE_TYPES.oneShot
    form.deviceType = deviceType.value

    const bindingKeys = new Set(
      deviceDefinition.value.controls.flatMap((control) => control.bindings.map((binding) => binding.key)),
    )
    for (const key of bindingKeys) {
      if (config[key]) {
        form[key] = cloneGesture(config[key])
      } else {
        delete form[key]
      }
    }

    for (const field of deviceDefinition.value.configLayouts.flatMap((layout) => layout.fields)) {
      if (field.type !== 'gesture' && config[field.key] != null) {
        form[field.key] = config[field.key]
      }
    }
  }

  function updateBinding(field, gesture) {
    form[field] = gesture
  }

  function applyDeviceInfo(info) {
    numLeds.value = info.numLeds
    deviceCapabilities.value = info.capabilities ?? 0
    thirdActionTrigger.value = info.thirdActionTrigger ?? deviceDefinition.value.defaultInfo.thirdActionTrigger
    caseColors.keycap = info.keycap
    caseColors.topCase = info.topCase
    caseColors.topCaseShade = info.topCaseShade
    caseColors.bottomCase = info.bottomCase
  }

  function applyDeviceOptions(options) {
    suppressAutoSave.value = true
    form.turboMode = options.turboMode === true
  }

  return {
    applyConfig,
    applyDeviceInfo,
    applyDeviceOptions,
    caseColors,
    controls,
    deviceDefinition,
    deviceType,
    form,
    isDevicePressed,
    numLeds,
    selectedColor,
    supportsLighting,
    supportsTurboMode,
    suppressAutoSave,
    thirdActionTrigger,
    updateBinding,
  }
}
