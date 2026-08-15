<script setup>
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import baseSvg from '../assets/rrrraw-base.svg'
import encoderFillSvg from '../assets/rrrraw-encoder-fill.svg'
import encoderSvg from '../assets/rrrraw-encoder.svg'
import encoderPhaseSvg from '../assets/rrrraw-encoder-phase.svg'
import keySvg from '../assets/rrrraw-key.svg'
import maskSvg from '../assets/rrrraw-mask.svg'

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
const encoderStepSequence = computed(() => (
  props.activeAnimation?.type === 'rotate-step' ? props.activeAnimation.sequence : null
))

const dimpleAngle = ref(0)
const encoderPhaseAngle = ref(0)
const dimpleStyle = computed(() => ({
  transform: `translate(${145 * (Math.cos(dimpleAngle.value) - 1)}px, ${88 * Math.sin(dimpleAngle.value)}px)`,
}))
const encoderPhaseOpacity = computed(() => (
  Math.floor(encoderPhaseAngle.value / (Math.PI / 5)) % 2 === 0 ? 0 : 1
))
const encoderBaseOpacity = computed(() => 1 - encoderPhaseOpacity.value)

function normalizeAngle(angle) {
  return (angle + Math.PI * 2) % (Math.PI * 2)
}

watch(encoderStepSequence, (sequence, previousSequence) => {
  if (sequence == null || sequence === previousSequence) return

  const direction = props.activeAnimation.direction === 'cw' ? 1 : -1
  dimpleAngle.value = normalizeAngle(dimpleAngle.value + direction * Math.PI / 12)
  encoderPhaseAngle.value = normalizeAngle(encoderPhaseAngle.value + direction * Math.PI / 5)
})

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
    const nextDimpleAngle = dimpleAngle.value + direction * elapsed * Math.PI * 2 / 4800
    const nextPhaseAngle = encoderPhaseAngle.value + direction * elapsed * Math.PI * 2 / 1200
    dimpleAngle.value = normalizeAngle(nextDimpleAngle)
    encoderPhaseAngle.value = normalizeAngle(nextPhaseAngle)
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
    <div class="preview-scene" :style="{ zoom: width / 1025 }">
      <img class="preview-base" :src="baseSvg" alt="rrrraw">

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
            stepping: activeAnimation?.type === 'rotate-step',
            pressed: pressedControls.encoder,
          },
        ]"
        aria-hidden="true"
      >
        <img class="encoder-fill" :src="encoderFillSvg" alt="">
        <img
          class="encoder-base"
          :src="encoderSvg"
          :style="{ opacity: encoderBaseOpacity }"
          alt=""
        >
        <img
          class="encoder-phase"
          :src="encoderPhaseSvg"
          :style="{ opacity: encoderPhaseOpacity }"
          alt=""
        >
        <svg class="encoder-dimple" viewBox="78 35 460 380" preserveAspectRatio="none">
          <ellipse class="dimple-eraser" cx="452" cy="185" rx="49" ry="31" />
          <ellipse class="dimple" :style="dimpleStyle" cx="452" cy="185" rx="45" ry="27" />
        </svg>
      </div>

      <img class="preview-mask" :src="maskSvg" alt="" aria-hidden="true">

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
  aspect-ratio: 1025 / 800;
  overflow: hidden;
  line-height: 0;
}

.preview-scene {
  position: absolute;
  top: 0;
  left: 0;
  width: 1025px;
  height: 800px;
  contain: layout paint;
}

.preview-base,
.preview-mask,
.encoder-fill,
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

.preview-mask {
  inset: 0;
  z-index: 3;
  width: 100%;
  height: 100%;
}

.encoder-base,
.encoder-fill,
.encoder-dimple {
  inset: 0;
  width: 100%;
  height: 100%;
}

.key-layer,
.installed-encoder {
  position: absolute;
  display: block;
  pointer-events: none;
}

.installed-key,
.installed-encoder {
  z-index: 2;
  transform: translate3d(0, 0, 0);
  transition: transform 100ms ease-out;
  backface-visibility: hidden;
  will-change: transform;
}

.key-layer {
  width: 255px;
  height: 172px;
}

.installed-key.pressed {
  transform: translate3d(0, 8px, 0);
}

.installed-encoder.pressed {
  transform: translate3d(0, 8px, 0);
}

.key-1 {
  top: 293px;
  left: 74.825px;
}

.key-2 {
  top: 369px;
  left: 205px;
}

.key-3 {
  top: 443px;
  left: 334.663px;
}

.installed-encoder {
  top: 33.6px;
  left: 344.728px;
  width: 460px;
  height: 380px;
}

.encoder-phase {
  position: absolute;
  top: 0;
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

.installed-encoder.stepping .dimple {
  transition: transform 90ms linear;
}

.hit-area {
  position: absolute;
  z-index: 4;
  display: block;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.button-hit {
  width: 255px;
  height: 172px;
  transform: rotate(29deg);
}

.button-hit.button-1 {
  top: 293px;
  left: 74.825px;
}

.button-hit.button-2 {
  top: 369px;
  left: 205px;
}

.button-hit.button-3 {
  top: 443px;
  left: 334.663px;
}

.encoder-hit {
  top: 33.6px;
  left: 353.728px;
  width: 460px;
  height: 380px;
  border-radius: 50%;
}

@media (prefers-reduced-motion: reduce) {
  .installed-key,
  .installed-encoder {
    transition: none;
  }
}
</style>
