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

<style scoped>
.panel {
  border: 0;
  background: transparent;
  border-radius: 0;
  box-shadow: none;
  margin-bottom: 4rem;
  text-align: center;
  padding: 0;
}

.color-panel {
  display: flex;
  align-items: center;
  flex-direction: column;
  padding-bottom: 40px;
}

.no-lighting {
  padding-bottom: 0;
}
</style>
