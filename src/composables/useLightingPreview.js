import { computed } from 'vue'
import { DEVICE_TYPES } from '../protocol'

export function useLightingPreview(form, deviceType) {
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
    if (deviceType.value === DEVICE_TYPES.magicButton) {
      return {
        '--selection-color': '#5AB9CF',
        '--top-color': '#FFF',
        '--top-shade-color': '#FFF',
        '--button-color': '#5AB9CF',
      }
    }

    return {
      '--selection-color': `hsla(${hsl.value.h % 360}, ${hsl.value.s + 10}%, ${Math.max(40, hsl.value.l + 15)}%, ${hsl.value.s / 100})`,
      '--top-color': `hsla(${hsl.value.h % 360}, ${hsl.value.s + 10}%, ${Math.max(40, hsl.value.l + 15)}%, ${hsl.value.s / 100})`,
      '--top-shade-color': '#cf00ff',
      '--button-color': '#FFF',
    }
  })

  return {
    colorPreviewStyle,
    hsl,
  }
}
