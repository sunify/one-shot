<script setup>
import { computed } from 'vue'
import previewSvg from '../../rrrraw.svg'
import encoderPhaseSvg from '../assets/rrrraw-encoder-phase.svg'

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
    default: 500,
  },
})

defineEmits(['press'])

const rotationDirection = computed(() => (
  props.activeAnimation?.type === 'rotate' ? props.activeAnimation.direction : null
))
</script>

<template>
  <div class="rrrraw-preview" :style="{ width: `${width}px` }">
    <img class="preview-base" :src="previewSvg" alt="rrrraw">

    <div
      v-if="rotationDirection"
      class="encoder-animation"
      :class="rotationDirection"
      aria-hidden="true"
    >
      <img class="encoder-phase" :src="encoderPhaseSvg" alt="">
      <svg class="encoder-dimple" viewBox="0 0 1396 1118">
        <ellipse class="dimple-eraser" cx="1252" cy="185" rx="49" ry="31" />
        <ellipse class="dimple" cx="1252" cy="185" rx="45" ry="27" />
      </svg>
    </div>

    <button
      v-for="number in 3"
      :key="number"
      class="hit-area button-hit"
      :class="[`button-${number}`, { pressed: pressedControls[`button${number}`] }]"
      type="button"
      :aria-label="`Нажать кнопку ${number}`"
      @pointerdown.prevent="$emit('press', `button${number}`)"
    />
    <button
      class="hit-area encoder-hit"
      :class="{ pressed: pressedControls.encoder }"
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
.encoder-animation,
.encoder-dimple {
  position: absolute;
  inset: 0;
  display: block;
  width: 100%;
  height: 100%;
  pointer-events: none;
  user-select: none;
}

.encoder-phase {
  position: absolute;
  top: -1.6995%;
  left: 57.3066%;
  width: 45.2722%;
  height: 42.576%;
  animation: facet-phase 240ms steps(1, end) infinite;
}

.dimple-eraser {
  fill: #fff;
  stroke: none;
}

.dimple {
  fill: #fff;
  stroke: #000;
  stroke-width: 2.5;
  transform-box: fill-box;
  transform-origin: center;
  animation: dimple-orbit 1200ms linear infinite;
}

.encoder-animation.ccw .dimple {
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

.hit-area.pressed {
  background: rgb(255 214 0 / 18%);
}

.button-hit {
  width: 15%;
  height: 12%;
  transform: rotate(31deg);
}

.button-1 {
  top: 59%;
  left: 10%;
}

.button-2 {
  top: 65%;
  left: 21%;
}

.button-3 {
  top: 70%;
  left: 32%;
}

.encoder-hit {
  top: 4%;
  left: 63%;
  width: 33%;
  height: 32%;
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
