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
    <img class="preview-base" :src="previewSvg" alt="rrrraw">
    <svg class="explode-masks" viewBox="0 0 1396 1118" aria-hidden="true">
      <rect x="150" y="300" width="280" height="205" />
      <path
        d="M320 38C445 38 533 96 534 190L535 292C514 361 414 407 300 411C190 410 90 360 81 296L81 190C83 110 184 38 320 38Z"
        transform="translate(800 0)"
      />
    </svg>

    <svg
      v-for="number in 3"
      :key="number"
      class="installed-key"
      :class="[`key-${number}`, { pressed: pressedControls[`button${number}`] }]"
      viewBox="160 320 250 180"
      aria-hidden="true"
    >
      <path
        d="M270 332L401 397L402 445L282 489L164 430L164 387Z"
        fill="#fff"
        stroke="none"
      />
      <image :href="previewSvg" width="1396" height="1118" />
    </svg>

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
      <svg class="encoder-base" viewBox="0 0 632 476">
        <defs>
          <clipPath id="rrrraw-encoder-clip">
            <path d="M320 38C445 38 533 96 534 190L535 292C514 361 414 407 300 411C190 410 90 360 81 296L81 190C83 110 184 38 320 38Z" />
          </clipPath>
        </defs>
        <path
          d="M320 38C445 38 533 96 534 190L535 292C514 361 414 407 300 411C190 410 90 360 81 296L81 190C83 110 184 38 320 38Z"
          fill="#fff"
          stroke="none"
        />
        <g clip-path="url(#rrrraw-encoder-clip)">
          <image :href="previewSvg" x="-800" width="1396" height="1118" />
        </g>
      </svg>
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

.explode-masks {
  position: absolute;
  inset: 0;
  z-index: 1;
  display: block;
  width: 100%;
  height: 100%;
  fill: #fff;
  stroke: none;
  pointer-events: none;
}

.installed-key,
.installed-encoder {
  position: absolute;
  z-index: 2;
  display: block;
  pointer-events: none;
  transition: transform 100ms ease-out;
}

.installed-key {
  width: 10.745%;
  height: 9.66%;
  overflow: visible;
}

.installed-key.pressed,
.installed-encoder.pressed {
  transform: translateY(4px);
}

.key-1 {
  top: 60.73%;
  left: 14.18%;
}

.key-2 {
  top: 67.44%;
  left: 24.21%;
}

.key-3 {
  top: 74.15%;
  left: 34.24%;
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
  fill: #fff;
  stroke: none;
}

.dimple {
  fill: #fff;
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
  z-index: 3;
  display: block;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.button-hit {
  width: 11%;
  height: 9%;
  transform: rotate(29deg);
}

.button-hit.button-1 {
  top: 61%;
  left: 14%;
}

.button-hit.button-2 {
  top: 68%;
  left: 24%;
}

.button-hit.button-3 {
  top: 75%;
  left: 34%;
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
