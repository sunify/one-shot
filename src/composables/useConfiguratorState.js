import { computed, reactive, ref } from 'vue'
import { ACTION_TYPES, DEVICE_TYPES, MOUSE_AXES } from '../protocol'

function cloneGesture(gesture) {
  return {
    type: gesture.type,
    code: gesture.code,
    modifiers: gesture.modifiers,
  }
}

export function useConfiguratorState() {
  const deviceType = ref(DEVICE_TYPES.oneShot)
  const isDevicePressed = ref(false)
  const suppressAutoSave = ref(false)

  const form = reactive({
    deviceType: DEVICE_TYPES.oneShot,
    singleTap: { type: ACTION_TYPES.consumer, code: 0x00cd, modifiers: 0 },
    doubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
    tripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
    red: 250,
    green: 255,
    blue: 210,
    animationMode: 1,
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

  const supportsLighting = computed(() => deviceType.value === DEVICE_TYPES.oneShot)
  const hasEncoder = computed(() => form.encoderCW != null)

  const gestureFields = computed(() => {
    const fields = [
      { key: 'singleTap', label: 'Одиночное нажатие', animation: { type: 'tap', count: 1 } },
      { key: 'doubleTap', label: 'Двойное нажатие', animation: { type: 'tap', count: 2 } },
      {
        key: 'tripleTap',
        label: deviceType.value === DEVICE_TYPES.magicButton ? 'Долгое нажатие' : 'Тройное нажатие',
        animation: deviceType.value === DEVICE_TYPES.magicButton ? { type: 'press' } : { type: 'tap', count: 3 }
      },
    ]

    if (hasEncoder.value) {
      fields.push(
        { key: 'encoderCW', label: 'Энкодер →', animation: null },
        { key: 'encoderCCW', label: 'Энкодер ←', animation: null },
      )
    }

    return fields
  })

  function applyConfig(config) {
    suppressAutoSave.value = true
    deviceType.value = config.deviceType ?? DEVICE_TYPES.oneShot
    form.deviceType = deviceType.value
    form.singleTap = cloneGesture(config.singleTap)
    form.doubleTap = cloneGesture(config.doubleTap)
    form.tripleTap = cloneGesture(config.tripleTap)

    if (deviceType.value === DEVICE_TYPES.oneShot) {
      form.red = config.red
      form.green = config.green
      form.blue = config.blue
      form.animationMode = config.animationMode
    }

    if (config.encoderCW) {
      form.encoderCW = cloneGesture(config.encoderCW)
      form.encoderCCW = cloneGesture(config.encoderCCW)
      form.encoderSensitivity = config.encoderSensitivity
    } else {
      delete form.encoderCW
      delete form.encoderCCW
      delete form.encoderSensitivity
    }
  }

  function updateGesture(field, gesture) {
    form[field] = gesture
  }

  return {
    applyConfig,
    deviceType,
    form,
    gestureFields,
    hasEncoder,
    isDevicePressed,
    selectedColor,
    supportsLighting,
    suppressAutoSave,
    updateGesture,
  }
}
