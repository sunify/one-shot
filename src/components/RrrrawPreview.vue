<script setup>
import { computed } from 'vue'
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

defineEmits(['press'])

const rotationDirection = computed(() => (
  props.activeAnimation?.type === 'rotate' ? props.activeAnimation.direction : null
))
</script>

<template>
  <div class="rrrraw-preview" :style="{ width: `${width}px` }">
    <img class="preview-base" :src="baseSvg" alt="rrrraw">

    <img
      v-for="number in 3"
      :key="number"
      class="installed-key"
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
      <img class="encoder-phase" :src="encoderPhaseSvg" alt="">
      <svg class="encoder-dimple" viewBox="0 0 632 476">
        <ellipse class="dimple-eraser" cx="452" cy="185" rx="49" ry="31" />
        <ellipse class="dimple" cx="452" cy="185" rx="45" ry="27" />
      </svg>
    </div>

    <button
      v-for="number in 3"
      :key="`hit-${number}`"
      class="hit-area button-hit"
      :class="`button-${number}`"
      type="button"
      :aria-label="`Нажать кнопку ${number}`"
      @pointerdown.prevent="$emit('press', `button${number}`)"
    />
    <button
      class="hit-area encoder-hit"
      type="button"
      aria-label="Нажать энкодер"
      @pointerdown.prevent="$emit('press', 'encoder')"
    />
  </div>
</template>

<style scoped>
.rrrraw-preview {
  position: relative;
  display: inline-block;
  aspect-ratio: 1396 / 1118;
  line-height: 0;
}

.preview-base,
.encoder-base,
.encoder-dimple {
  position: absolute;
  inset: 0;
  display: block;
  width: 100%;
  height: 100%;
  pointer-events: none;
  user-select: none;
}

.preview-base {
  z-index: 0;
}

.installed-key,
.installed-encoder {
  position: absolute;
  z-index: 1;
  display: block;
  pointer-events: none;
  transition: transform 100ms ease-out;
}

.installed-key {
  width: 20.057%;
  height: 18.784%;
}

.installed-key.pressed,
.installed-encoder.pressed {
  transform: translateY(4px);
}

.key-1 {
  top: 56.17%;
  left: 9.81%;
}

.key-2 {
  top: 62.88%;
  left: 19.77%;
}

.key-3 {
  top: 69.59%;
  left: 29.8%;
}

.installed-encoder {
  top: 32.74%;
  left: 25.64%;
  width: 45.2722%;
  height: 42.576%;
}

.encoder-phase {
  position: absolute;
  top: -4%;
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

.installed-encoder.rotating .encoder-phase {
  animation: facet-phase 240ms steps(1, end) infinite;
}

.installed-encoder.rotating .dimple {
  animation: dimple-orbit 1200ms linear infinite;
}

.installed-encoder.ccw .dimple {
  animation-direction: reverse;
}

.hit-area {
  position: absolute;
  z-index: 2;
  display: block;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.button-hit {
  width: 17%;
  height: 14%;
  transform: rotate(29deg);
}

.button-hit.button-1 {
  top: 58%;
  left: 11%;
}

.button-hit.button-2 {
  top: 65%;
  left: 21%;
}

.button-hit.button-3 {
  top: 72%;
  left: 31%;
}

.encoder-hit {
  top: 37%;
  left: 31%;
  width: 36%;
  height: 30%;
  border-radius: 50%;
}

@keyframes facet-phase {
  0%, 49% {
    opacity: 0;
  }

  50%, 100% {
    opacity: 1;
  }
}

@keyframes dimple-orbit {
  0%, 100% {
    transform: translate(0, 0);
  }

  12.5% {
    transform: translate(-38px, 55px);
  }

  25% {
    transform: translate(-132px, 99px);
  }

  37.5% {
    transform: translate(-235px, 78px);
  }

  50% {
    transform: translate(-290px, 22px);
  }

  62.5% {
    transform: translate(-252px, -44px);
  }

  75% {
    transform: translate(-158px, -77px);
  }

  87.5% {
    transform: translate(-55px, -55px);
  }
}

@media (prefers-reduced-motion: reduce) {
  .encoder-phase,
  .dimple {
    animation-play-state: paused;
  }
}
</style>
