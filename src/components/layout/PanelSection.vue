<script setup>
defineProps({
  title: {
    type: String,
    default: '',
  },
  accentStyle: {
    type: Object,
    default: null,
  },
  panelClass: {
    type: String,
    default: '',
  },
})
</script>

<template>
  <section :class="['panel', panelClass]" :style="accentStyle">
    <div class="panel-head">
      <h2 v-if="title">{{ title }}</h2>
      <slot name="aside" />
    </div>

    <slot />
  </section>
</template>
