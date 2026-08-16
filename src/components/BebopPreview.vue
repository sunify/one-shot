<script setup>
import { computed, onBeforeUnmount, onMounted } from 'vue'
import baseSvg from '../assets/bebop-base.svg'
import leftSvg from '../assets/bebop-left.svg'
import rightSvg from '../assets/bebop-right.svg'

const props = defineProps({
  activeAnimation: {
    type: Object,
    default: null,
  },
  isPressed: {
    type: Boolean,
    default: false,
  },
  pressedControls: {
    type: Object,
    default: () => ({}),
  },
})

const emit = defineEmits(['press-start', 'press-end'])
const activePointers = new Map()

const leftPressed = computed(() => (
  props.pressedControls.left || (props.isPressed && !props.pressedControls.right)
))
const rightPressed = computed(() => props.pressedControls.right)

function handlePointerDown(event, controlId) {
  activePointers.set(event.pointerId, controlId)
  emit('press-start', controlId)
}

function handlePointerEnd(event) {
  const controlId = activePointers.get(event.pointerId)
  if (!controlId) return

  activePointers.delete(event.pointerId)
  emit('press-end', controlId)
}

onMounted(() => {
  document.addEventListener('pointerup', handlePointerEnd)
  document.addEventListener('pointercancel', handlePointerEnd)
})

onBeforeUnmount(() => {
  document.removeEventListener('pointerup', handlePointerEnd)
  document.removeEventListener('pointercancel', handlePointerEnd)
  for (const controlId of activePointers.values()) {
    emit('press-end', controlId)
  }
  activePointers.clear()
})
</script>

<template>
  <div class="bebop-preview">
    <img class="bebop-layer" :src="baseSvg" alt="">
    <img
      class="bebop-layer bebop-button"
      :class="{ pressed: leftPressed }"
      :src="leftSvg"
      alt="Левая кнопка"
      aria-hidden="true"
    >
    <img
      class="bebop-layer bebop-button"
      :class="{ pressed: rightPressed }"
      :src="rightSvg"
      alt="Правая кнопка"
      aria-hidden="true"
    >
    <button
      class="bebop-hit-area bebop-hit-left"
      type="button"
      aria-label="Нажать левую кнопку"
      @pointerdown.prevent="handlePointerDown($event, 'left')"
    />
    <button
      class="bebop-hit-area bebop-hit-right"
      type="button"
      aria-label="Нажать правую кнопку"
      @pointerdown.prevent="handlePointerDown($event, 'right')"
    />
    <span class="control-anchor anchor-left" data-control-anchor="left" />
    <span class="control-anchor anchor-right" data-control-anchor="right" />
  </div>
</template>

<style scoped>
.bebop-preview {
  position: relative;
  display: inline-block;
  width: 500px;
  aspect-ratio: 6840 / 5181;
  line-height: 0;
}

.bebop-layer {
  position: absolute;
  inset: 0;
  display: block;
  width: 100%;
  height: 100%;
  user-select: none;
}

.bebop-button {
  pointer-events: none;
  transition: transform 100ms ease-out;
}

.bebop-button.pressed {
  transform: translateY(11px);
}

.bebop-hit-area {
  position: absolute;
  z-index: 2;
  display: block;
  padding: 0;
  border: 0;
  background: transparent;
  cursor: pointer;
}

.bebop-hit-left {
  left: 20%;
  top: 15%;
  width: 30%;
  height: 24%;
}

.bebop-hit-right {
  left: 57%;
  top: 34%;
  width: 30%;
  height: 25%;
}

.control-anchor {
  position: absolute;
  width: 0;
  height: 0;
  pointer-events: none;
}

.anchor-left {
  left: 35%;
  top: 27%;
}

.anchor-right {
  left: 60%;
  top: 47%;
}
</style>
