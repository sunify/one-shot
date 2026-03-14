<script setup>
defineProps({
  isBusy: {
    type: Boolean,
    default: false,
  },
  isConnected: {
    type: Boolean,
    default: false,
  },
  statusText: {
    type: String,
    default: '',
  },
  title: {
    type: String,
    default: '',
  },
})

defineEmits(['connect'])
</script>

<template>
  <main class="shell">
    <section class="hero">
      <h1 v-html="title" />

      <div v-if="!isConnected" class="actions">
        <button class="primary" :disabled="isBusy" @click="$emit('connect')">
          Подключить устройство
        </button>
      </div>

      <p class="status">{{ statusText }}</p>
    </section>

    <slot />
  </main>
</template>
