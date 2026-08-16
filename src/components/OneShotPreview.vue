<script setup>
import { computed, onBeforeUnmount, ref } from 'vue'
import baseAsset from '../assets/one-shot-base.svg'
import magicCapAsset from '../assets/magic-cap.svg'
import magicLogoAsset from '../assets/magic-logo.svg'
import oneShotButtonAsset from '../assets/one-shot-button.svg'

const props = defineProps({
  capVariant: {
    type: String,
    default: 'one-shot',
  },
  activeAnimation: {
    type: Object,
    default: null,
  },
  isPressed: {
    type: Boolean,
    default: false,
  },
  isRainbow: {
    type: Boolean,
    default: false,
  },
  pressedControls: {
    type: Object,
    default: () => ({}),
  },
  width: {
    type: [Number, String],
    default: 200,
  },
})

const emit = defineEmits(['press'])

const buttonOffset = ref(0)
let pressTime = 0
let mouseUpTimeout = null

const visualButtonOffset = computed(() => (
  props.pressedControls.main || props.isPressed ? 28 : buttonOffset.value
))
const isMagicCap = computed(() => props.capVariant === 'magic')
const capBaseY = computed(() => (isMagicCap.value ? 14 : 26))

function handleMouseDown() {
  buttonOffset.value = 28
  pressTime = Date.now()
  emit('press')
}

function handleWindowMouseUp() {
  if (mouseUpTimeout) {
    clearTimeout(mouseUpTimeout)
  }

  const timeSincePress = Date.now() - pressTime
  mouseUpTimeout = window.setTimeout(() => {
    buttonOffset.value = 0
    mouseUpTimeout = null
  }, pressTime === 0 ? 0 : Math.max(0, 100 - timeSincePress))
  pressTime = 0
}

window.addEventListener('mouseup', handleWindowMouseUp)

onBeforeUnmount(() => {
  window.removeEventListener('mouseup', handleWindowMouseUp)
  if (mouseUpTimeout) {
    clearTimeout(mouseUpTimeout)
  }
})
</script>

<template>
  <svg
    xmlns="http://www.w3.org/2000/svg"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xml:space="preserve"
    :width="width"
    style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5"
    viewBox="-4 -4 785 681"
  >
    <defs>
      <linearGradient id="rainbow-grad" x1="0" y1="0" x2="1" y2="0">
        <stop offset="0%" stop-color="hsl(0,90%,65%)" />
        <stop offset="17%" stop-color="hsl(60,90%,65%)" />
        <stop offset="33%" stop-color="hsl(120,90%,65%)" />
        <stop offset="50%" stop-color="hsl(180,90%,65%)" />
        <stop offset="67%" stop-color="hsl(240,90%,65%)" />
        <stop offset="83%" stop-color="hsl(300,90%,65%)" />
        <stop offset="100%" stop-color="hsl(360,90%,65%)" />
      </linearGradient>
      <clipPath id="one-shot-base-clip">
        <polygon points="369,0 407,0 429,3 446,7 469,15 481,21 737,169 756,183 767,196 773,208 776,224 776,392 770,456 764,472 750,489 735,500 479,648 463,657 439,666 415,671 407,672 369,672 349,669 322,661 297,648 41,500 20,483 9,466 6,456 0,392 0,224 3,208 12,192 27,177 39,169 295,21 317,11 342,4" />
      </clipPath>
      <clipPath id="one-shot-button-clip">
        <polygon points="245,0 266,0 283,4 300,12 473,112 498,127 507,137 511,150 511,215 508,226 500,236 492,242 315,344 293,356 276,361 270,362 241,362 218,356 203,348 18,241 8,233 1,221 0,215 0,150 4,137 12,128 19,123 211,12 228,4" />
      </clipPath>
      <clipPath id="magic-cap-clip">
        <polygon points="300,1 321,1 342,5 361,13 593,147 605,157 617,178 620,189 620,287 616,298 605,310 598,315 373,445 356,454 340,459 321,462 300,462 281,459 265,454 248,445 23,315 7,301 2,291 1,287 1,189 4,178 13,161 28,147 246,21 270,8 287,3" />
      </clipPath>
      <clipPath id="one-shot-foreground-clip">
        <path d="M0 190 167 273 330 369q58 40 116 0l164-96 167-83v483H0Z" />
      </clipPath>
    </defs>
    <g clip-path="url(#one-shot-base-clip)">
      <rect width="777" height="673" fill="#fff" />
      <rect
        width="777"
        height="673"
        :style="`fill:${isRainbow ? 'url(#rainbow-grad)' : 'var(--top-color)'}`"
      />
      <rect
        width="777"
        height="673"
        style="fill:var(--top-shade-color);fill-opacity:.1"
      />
      <path
        d="M0 418c10 18 23 29 41 32l253 146q94 50 188 0l250-146c19-7 32-18 45-32v255H0Z"
        style="fill:var(--bottom-color, #fff)"
      />
    </g>
    <image :href="baseAsset" width="777" height="673" />

    <g
      style="user-select:none;cursor:pointer"
      :transform="`translate(132, ${capBaseY + visualButtonOffset})`"
      @mousedown.prevent="handleMouseDown"
    >
      <g v-if="isMagicCap" transform="scale(0.8231511254)">
        <rect
          width="622"
          height="464"
          clip-path="url(#magic-cap-clip)"
          style="fill:var(--button-color, #fff)"
        />
        <image :href="magicLogoAsset" width="622" height="464" />
        <image :href="magicCapAsset" width="622" height="464" />
      </g>
      <template v-else>
        <rect
          width="512"
          height="363"
          clip-path="url(#one-shot-button-clip)"
          style="fill:var(--button-color, #fff)"
        />
        <image :href="oneShotButtonAsset" width="512" height="363" />
      </template>
    </g>

    <g
      clip-path="url(#one-shot-foreground-clip)"
      pointer-events="none"
      aria-hidden="true"
    >
      <g clip-path="url(#one-shot-base-clip)">
        <rect width="777" height="673" fill="#fff" />
        <rect
          width="777"
          height="673"
          :style="`fill:${isRainbow ? 'url(#rainbow-grad)' : 'var(--top-color)'}`"
        />
        <rect
          width="777"
          height="673"
          style="fill:var(--top-shade-color);fill-opacity:.1"
        />
        <path
          d="M0 418c10 18 23 29 41 32l253 146q94 50 188 0l250-146c19-7 32-18 45-32v255H0Z"
          style="fill:var(--bottom-color, #fff)"
        />
      </g>
      <image :href="baseAsset" width="777" height="673" />
    </g>
  </svg>
</template>
