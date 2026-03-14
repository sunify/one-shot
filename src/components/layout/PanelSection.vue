<script setup>
defineProps({
  eyebrow: {
    type: String,
    default: '',
  },
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
      <div>
        <p v-if="eyebrow" class="eyebrow">{{ eyebrow }}</p>
        <h2 v-if="title">{{ title }}</h2>
      </div>
      <slot name="aside" />
    </div>

    <slot />
  </section>
</template>
