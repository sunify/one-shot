<script setup>
defineProps({
  title: {
    type: String,
    default: '',
  },
  panelClass: {
    type: String,
    default: '',
  },
})
</script>

<template>
  <section :class="['panel', panelClass]">
    <slot />
  </section>
</template>
