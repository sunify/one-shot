<script setup>
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import baseSvg from '../assets/rrrraw-base.svg'
import encoderSvg from '../assets/rrrraw-encoder.svg'
import encoderPhaseSvg from '../assets/rrrraw-encoder-phase.svg'
import keySvg from '../assets/rrrraw-key.svg'

const props = defineProps({
  activeAnimation: {
    type: Object,
    default: null,
  },
  pressedControls: {
    type: Object,
    default: () => ({}),
  },
  width: {
    type: Number,
    default: 375,
  },
})

const emit = defineEmits(['press-start', 'press-end'])

function handlePointerDown(event, controlId) {
  event.currentTarget.setPointerCapture?.(event.pointerId)
  emit('press-start', controlId)
}

function handlePointerEnd(event, controlId) {
  if (event.currentTarget.hasPointerCapture?.(event.pointerId)) {
    event.currentTarget.releasePointerCapture(event.pointerId)
  }
  emit('press-end', controlId)
}

const rotationDirection = computed(() => (
  props.activeAnimation?.type === 'rotate' ? props.activeAnimation.direction : null
))

const encoderAngle = ref(0)
const dimpleStyle = computed(() => ({
  transform: `translate(${145 * (Math.cos(encoderAngle.value) - 1)}px, ${88 * Math.sin(encoderAngle.value)}px)`,
}))
const encoderPhaseOpacity = computed(() => (
  Math.floor(encoderAngle.value / (Math.PI / 5)) % 2 === 0 ? 0 : 1
))

let animationFrame = null
let previousFrameTime = null

function animateEncoder(time) {
  if (!rotationDirection.value) {
    animationFrame = null
    previousFrameTime = null
    return
  }

  if (previousFrameTime != null) {
    const elapsed = Math.min(time - previousFrameTime, 50)
    const direction = rotationDirection.value === 'cw' ? 1 : -1
    const nextAngle = encoderAngle.value + direction * elapsed * Math.PI * 2 / 1200
    encoderAngle.value = (nextAngle + Math.PI * 2) % (Math.PI * 2)
  }

  previousFrameTime = time
  animationFrame = window.requestAnimationFrame(animateEncoder)
}

watch(rotationDirection, (direction) => {
  if (direction && animationFrame == null) {
    previousFrameTime = null
    animationFrame = window.requestAnimationFrame(animateEncoder)
  } else if (!direction && animationFrame != null) {
    window.cancelAnimationFrame(animationFrame)
    animationFrame = null
    previousFrameTime = null
  }
})

onBeforeUnmount(() => {
  if (animationFrame != null) {
    window.cancelAnimationFrame(animationFrame)
  }
})
</script>

<template>
  <div class="rrrraw-preview" :style="{ width: `${width}px` }">
    <div class="preview-scene">
      <img class="preview-base" :src="baseSvg" alt="rrrraw">

      <img
        v-for="number in 3"
        :key="`erase-${number}`"
        class="key-layer key-erase"
        :class="`key-${number}`"
        :src="keySvg"
        alt=""
        aria-hidden="true"
      >
      <img class="encoder-erase" :src="encoderSvg" alt="" aria-hidden="true">

      <img
        v-for="number in 3"
        :key="number"
        class="key-layer installed-key"
        :class="[`key-${number}`, { pressed: pressedControls[`button${number}`] }]"
        :src="keySvg"
        alt=""
        aria-hidden="true"
      >

      <div
        class="installed-encoder"
        :class="[
          rotationDirection,
          {
            rotating: rotationDirection,
            pressed: pressedControls.encoder,
          },
        ]"
        aria-hidden="true"
      >
        <img class="encoder-base" :src="encoderSvg" alt="">
        <img
          class="encoder-phase"
          :src="encoderPhaseSvg"
          :style="{ opacity: encoderPhaseOpacity }"
          alt=""
        >
        <svg class="encoder-dimple" viewBox="78 35 460 380">
          <ellipse class="dimple-eraser" cx="452" cy="185" rx="49" ry="31" />
          <ellipse class="dimple" :style="dimpleStyle" cx="452" cy="185" rx="45" ry="27" />
        </svg>
      </div>

      <svg class="foreground-depth" viewBox="0 0 1576 1214" aria-hidden="true">
        <g fill="#fff" stroke="#000" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
          <path d="M155 585L322 674L480 610L490 641L322 704L145 610Z" />
          <path d="M349 697L500 786L657 722L667 753L500 816L339 722Z" />
          <path d="M530 801L682 890L839 826L849 857L682 920L520 826Z" />
          <path d="M555 486C590 575 730 622 890 622C1050 622 1193 573 1228 486L1238 515C1200 610 1055 654 890 654C720 654 580 607 545 515Z" />
        </g>
      </svg>

      <button
        v-for="number in 3"
        :key="`hit-${number}`"
        class="hit-area button-hit"
        :class="`button-${number}`"
        type="button"
        :aria-label="`Нажать кнопку ${number}`"
        @pointerdown.prevent="handlePointerDown($event, `button${number}`)"
        @pointerup.prevent="handlePointerEnd($event, `button${number}`)"
        @pointercancel="handlePointerEnd($event, `button${number}`)"
      />
      <button
        class="hit-area encoder-hit"
        type="button"
        aria-label="Нажать энкодер"
        @pointerdown.prevent="handlePointerDown($event, 'encoder')"
        @pointerup.prevent="handlePointerEnd($event, 'encoder')"
        @pointercancel="handlePointerEnd($event, 'encoder')"
      />
    </div>
  </div>
</template>

<style scoped>
.rrrraw-preview {
  position: relative;
  display: inline-block;
  aspect-ratio: 1576 / 1214;
  overflow: hidden;
  line-height: 0;
}

.preview-scene {
  position: absolute;
  inset: 0;
}

.preview-base,
.encoder-base,
.encoder-dimple {
  position: absolute;
  display: block;
  pointer-events: none;
  user-select: none;
}

.preview-base {
  inset: 0;
  z-index: 0;
  width: 100%;
  height: 100%;
}

.encoder-base,
.encoder-dimple {
  inset: 0;
  width: 100%;
  height: 100%;
}

.key-layer,
.encoder-erase,
.installed-encoder {
  position: absolute;
  display: block;
  pointer-events: none;
}

.installed-key,
.installed-encoder {
  z-index: 2;
  transition: transform 100ms ease-out;
}

.key-erase,
.encoder-erase {
  z-index: 1;
  filter: brightness(0) invert(1);
}

.key-layer {
  width: 23.54%;
  height: 21.17%;
}

.installed-key.pressed,
.installed-encoder.pressed {
  transform: translateY(4px);
}

.key-1 {
  top: 35.17%;
  left: 8%;
}

.key-2 {
  top: 44.4%;
  left: 20.3%;
}

.key-3 {
  top: 52.97%;
  left: 31.79%;
}

.installed-encoder {
  top: 12.36%;
  left: 34.77%;
  width: 43.78%;
  height: 38.96%;
}

.encoder-erase {
  top: 12.36%;
  left: 34.77%;
  width: 43.78%;
  height: 38.96%;
}

.foreground-depth {
  position: absolute;
  inset: 0;
  z-index: 3;
  display: block;
  width: 100%;
  height: 100%;
  pointer-events: none;
}

.encoder-phase {
  position: absolute;
  top: -5%;
  left: 0;
  display: block;
  width: 100%;
  height: 100%;
  opacity: 0;
  pointer-events: none;
  user-select: none;
}

.dimple-eraser {
  fill: #ffbd00;
  stroke: none;
}

.dimple {
  fill: #ffbd00;
  stroke: #000;
  stroke-width: 2.5;
  transform-box: fill-box;
  transform-origin: center;
}

.hit-area {
  position: absolute;
  z-index: 3;
  display: block;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.button-hit {
  width: 21%;
  height: 18%;
  transform: rotate(29deg);
}

.button-hit.button-1 {
  top: 36%;
  left: 9%;
}

.button-hit.button-2 {
  top: 45%;
  left: 21%;
}

.button-hit.button-3 {
  top: 54%;
  left: 33%;
}

.encoder-hit {
  top: 15%;
  left: 36%;
  width: 42%;
  height: 34%;
  border-radius: 50%;
}

@media (prefers-reduced-motion: reduce) {
  .installed-key,
  .installed-encoder {
    transition: none;
  }
}
</style>
