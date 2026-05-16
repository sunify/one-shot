import { computed } from 'vue'
import { ANIMATION_MODES, DEVICE_TYPES } from '../protocol'

export function useLightingPreview(form, deviceType, caseColors, supportsLighting) {
  const hsl = computed(() => {
    const r = form.red / 255
    const g = form.green / 255
    const b = form.blue / 255

    const max = Math.max(r, g, b)
    const min = Math.min(r, g, b)
    const delta = max - min

    let hue = 0
    const lightness = (max + min) / 2

    if (delta !== 0) {
      if (max === r) {
        hue = ((g - b) / delta) % 6
      } else if (max === g) {
        hue = (b - r) / delta + 2
      } else {
        hue = (r - g) / delta + 4
      }
    }

    hue = Math.round(hue * 60)
    if (hue < 0) {
      hue += 360
    }

    const saturation = delta === 0 ? 0 : delta / (1 - Math.abs(2 * lightness - 1))

    return {
      h: hue,
      s: Math.round(saturation * 100),
      l: Math.round(lightness * 100),
    }
  })

  const colorPreviewStyle = computed(() => {
    const base = {
      '--button-color': caseColors.keycap,
      '--top-color': caseColors.topCase,
      '--top-shade-color': caseColors.topCaseShade,
      '--bottom-color': caseColors.bottomCase,
      '--selection-color': caseColors.keycap,
    }

    if (deviceType.value === DEVICE_TYPES.magicButton || !supportsLighting.value) {
      return base
    }

    if (form.animationMode === ANIMATION_MODES.rainbow) {
      return {
        ...base,
        '--selection-color': 'hsl(280,60%,65%)',
        '--top-color': 'hsl(280,60%,65%)',
        '--top-shade-color': 'transparent',
        '--button-color': '#FFF',
      }
    }

    const dynamicTop = `hsla(${hsl.value.h % 360}, ${hsl.value.s + 10}%, ${Math.max(40, hsl.value.l + 15)}%, ${hsl.value.s / 100})`
    return {
      ...base,
      '--selection-color': dynamicTop,
      '--top-color': dynamicTop,
    }
  })

  const isRainbow = computed(() => supportsLighting.value && form.animationMode === ANIMATION_MODES.rainbow)

  return {
    colorPreviewStyle,
    isRainbow,
    hsl,
  }
}
