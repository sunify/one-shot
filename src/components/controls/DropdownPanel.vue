<script setup>
import { autoUpdate, flip, offset, shift, size, useFloating } from '@floating-ui/vue'
import { computed, onBeforeUnmount, ref, watch } from 'vue'

const props = defineProps({
  placement: {
    type: String,
    default: 'bottom',
  },
})

const root = ref(null)
const reference = ref(null)
const floating = ref(null)
const isOpen = ref(false)

const { x, y, strategy } = useFloating(reference, floating, {
  placement: props.placement,
  middleware: [
    offset(8),
    flip(),
    shift({ padding: 12 }),
    size({
      apply({ rects, elements }) {
        elements.floating.style.minWidth = `${rects.reference.width}px`
      },
    }),
  ],
  whileElementsMounted: autoUpdate,
})

const floatingStyles = computed(() => ({
  position: strategy.value,
  left: `${x.value ?? 0}px`,
  top: `${y.value ?? 0}px`,
}))

function toggle() {
  isOpen.value = !isOpen.value
}

function close() {
  isOpen.value = false
}

function handleDocumentPointerDown(event) {
  if (!root.value?.contains(event.target)) {
    close()
  }
}

function handleDocumentKeydown(event) {
  if (event.key === 'Escape') {
    close()
  }
}

watch(isOpen, (open) => {
  if (open) {
    document.addEventListener('pointerdown', handleDocumentPointerDown)
    document.addEventListener('keydown', handleDocumentKeydown)
    return
  }

  document.removeEventListener('pointerdown', handleDocumentPointerDown)
  document.removeEventListener('keydown', handleDocumentKeydown)
})

onBeforeUnmount(() => {
  document.removeEventListener('pointerdown', handleDocumentPointerDown)
  document.removeEventListener('keydown', handleDocumentKeydown)
})
</script>

<template>
  <div ref="root" class="dropdown-panel-root">
    <slot
      name="trigger"
      :is-open="isOpen"
      :toggle="toggle"
      :trigger-ref="(el) => { reference = el }"
    />

    <div
      v-if="isOpen"
      ref="floating"
      class="dropdown-panel"
      :style="floatingStyles"
    >
      <slot :close="close" />
    </div>
  </div>
</template>

<style scoped>
.dropdown-panel-root {
  position: relative;
}

.dropdown-panel {
  z-index: 5;
  display: grid;
  gap: 12px;
  padding: 20px;
  box-shadow: 10px 10px 0 0 #111;
  border: 2px solid #111;
  border-radius: 8px;
  background: #ffffff;
}
</style>
