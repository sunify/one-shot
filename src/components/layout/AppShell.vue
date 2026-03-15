<script setup>
import { ref } from 'vue';

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

const buttonOffset = ref(0);
let pressTime = 0;
function handleMouseDown() {
  buttonOffset.value = 12;
  pressTime = Date.now();
}

let timeout;
window.addEventListener('mouseup', () => {
  if (timeout) {
    clearTimeout(timeout);
  }
  const timeSincePress = Date.now() - pressTime;
  timeout = setTimeout(() => {
    buttonOffset.value = 0;
    timeout = null;
  }, pressTime === 0 ? 0 : Math.max(0, 100 - timeSincePress));
  pressTime = 0;
});

defineEmits(['connect'])
</script>

<template>
  <main class="shell">
    <section class="hero">
      <svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5" viewBox="0 0 300 210" width="200">
        <g style="user-select: none; cursor: pointer;" @mousedown="handleMouseDown" @mouseup="" :transform="`translate(0, ${buttonOffset})`">
          <path d="M51.7 59.5s-.2-26.8 0-34.2c0-1.2 5.8-7 6.6-7.2a3361 3361 0 0 1 80-16.6s7.4-.5 12.2-.5c4 0 10.3.5 10.3.5l11.2 2 67 14.6s2.7.7 3.7 1.5c1.5 1.2 5.6 4.6 5.6 5.7v34.2L167.4 76l-8.7.5-5.6.6h-8.2l-14.4-1z" style="fill:#fff;stroke:#000;stroke-width:1.67px"/>
          <path d="M56.8 19.1s.3 2 1 2.6c1 .7 4.6 1.5 4.6 1.5l71.2 15s10 1 14.9 1 14.8-1 14.8-1l73.7-15s3.2-.9 4.1-1.5a7 7 0 0 0 1.6-2" style="fill:none;stroke:#000;stroke-width:1.25px"/>
          <path d="M52.2 25.8s1.7 1.2 2.6 1.6c1.2.5 3 1.4 4.6 1.8C72 32 131 43.9 131 43.9s12.4 1.6 18.4 1.6c5.6 0 17.4-1.6 17.4-1.6L240 29.2s4.4-1.2 5.6-1.8c.6-.3 2.1-2 2.1-2" style="fill:none;stroke:#000;stroke-width:1.25px"/>
          <path d="m62.4 23.2-3.5 6V60m75.2-21.7-3 5.6-.6 31.6m33.3-37.2 3.1 5.1v32.1m69.6-52.3 3.6 6V60" style="fill:none;stroke:#000;stroke-width:1.25px"/>
        </g>
        <path d="M1 148v26.8c0 2 4.5 6 7.7 7.8a69 69 0 0 0 12.8 4.1l87.5 18.7 17 2c7.8.8 16.5 1.6 22.5 1.6 11.6 0 39.4-3.6 39.4-3.6l90.1-18.6s8.3-1.8 10.8-2.6q2.3-1 4-2.6c1.8-1.6 6.2-5.3 6.2-6.7v-26.4s-4.9 3.1-7.5 4.2c-3 1.3-11 3.3-11 3.3l-89 16.7c0 1.9-29.3 4.7-43.6 4.8-14 0-42-4.2-42-4.2-26.7-5-87.5-18.6-87.5-18.6S8.2 153.3 1 148" style="fill:#fff;stroke:#000;stroke-width:1.67px"/>
        <path d="M51.7 40.8S24.8 46 17.9 47.6a33 33 0 0 0-12.8 6.7A17 17 0 0 0 1 59.5v88.4s3.8 3.1 6.1 4.2a92 92 0 0 0 11.3 3l87.6 18.2s22.7 4.6 45 4.1c13.4-.2 40-3.6 40-3.6l89.6-18s9.7-2.3 12.8-3.7c1.6-.8 5.6-4.7 5.6-4.7V60.5s-.9-1.8-1.5-2.6c-.9-1-3.5-4.2-5.7-5.7a59 59 0 0 0-7.1-3.6l-36.4-7.8v18.7c-27.2 5.9-81.4 16-81.4 16s-11.2 1.6-17 1.6c-6 0-19.4-1.6-19.4-1.6l-79-16z" style="fill:var(--color-preview);stroke:#000;stroke-width:1.67px"/>
        <path d="M51.7 40.8S24.8 46 17.9 47.6a33 33 0 0 0-12.8 6.7A17 17 0 0 0 1 59.5v88.4s3.8 3.1 6.1 4.2a92 92 0 0 0 11.3 3l87.6 18.2s22.7 4.6 45 4.1c13.4-.2 40-3.6 40-3.6l89.6-18s9.7-2.3 12.8-3.7c1.6-.8 5.6-4.7 5.6-4.7V60.5s-.9-1.8-1.5-2.6c-.9-1-3.5-4.2-5.7-5.7a59 59 0 0 0-7.1-3.6l-36.4-7.8v18.7c-27.2 5.9-81.4 16-81.4 16s-11.2 1.6-17 1.6c-6 0-19.4-1.6-19.4-1.6l-79-16z" style="fill:#cf00ff;fill-opacity:.2;stroke:#000;stroke-width:1.67px"/>
        <path d="M51.6 50.7s-7 1.6-7 4.1c-.1 3 6.6 4.7 6.6 4.7m197.1-8.3s6.6 1.5 6.7 3.9c0 2.4-6.2 4.4-6.2 4.4" style="fill:none;stroke:#000;stroke-width:1.25px"/>
        <path d="M6.1 53.8s.8 3.3 2 4.1a52 52 0 0 0 13.4 5.2 1943 1943 0 0 0 88 18.6 246 246 0 0 0 40.5 3.1c12.7 0 37.9-3 37.9-3l89-18.2s9-2 11.9-3.6c4.7-2.8 5-5.7 5-5.7" style="fill:none;stroke:#000;stroke-width:1.25px"/>
        <path d="M1 61.5s3.2 2.9 5.1 3.7c3 1.2 8.1 2.7 12.3 3.6C35.1 72.5 106 87.4 106 87.4s27.6 3.6 41.4 3.6c14.3 0 44-3.6 44-3.6l89.2-18.1s7.6-1.8 10.7-3.1c2.8-1.2 7.7-4.7 7.7-4.7M4 178s9.2 3 13.9 4c17 3.9 88 18.7 88 18.7s28 3.6 42 3.6c14.3 0 43.6-3.6 43.6-3.6l89-18s8.3-1.6 11.3-2.7c2.4-.8 6.7-3.6 6.7-3.6" style="fill:none;stroke:#000;stroke-width:1.25px"/>
        <path d="m22 63.6-3.6 5.2V182l3 4.7m87.6 19.2-3-5.2V87.4l3.5-5.7m78.4 0 3.6 5.7v113.3l-3.6 4.7m90.6-18.6 2-4.7V69.3l-3.5-5.7" style="fill:none;stroke:#000;stroke-width:1.25px"/>
      </svg>


      <h1 v-html="title" />

      <div v-if="!isConnected" class="actions">
        <button class="primary" :disabled="isBusy" @click="$emit('connect')">
          Подключить устройство
        </button>
      </div>

      <p v-if="!isConnected" class="status">{{ statusText }}</p>
    </section>

    <slot />

    <slot name="footer" />
    <p v-if="isConnected" class="status">{{ statusText }}</p>
  </main>
</template>
