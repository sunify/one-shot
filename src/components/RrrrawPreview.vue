<script setup>
defineProps({
  pressedControls: {
    type: Object,
    default: () => ({}),
  },
  width: {
    type: Number,
    default: 250,
  },
})

defineEmits(['press'])
</script>

<template>
  <div class="rrrraw-preview" :style="{ width: `${width}px` }">
    <div class="case">
      <button
        v-for="number in 3"
        :key="number"
        class="key"
        :class="{ pressed: pressedControls[`button${number}`] }"
        type="button"
        :aria-label="`Кнопка ${number}`"
        @click="$emit('press', `button${number}`)"
      >
        {{ number }}
      </button>
      <button
        class="encoder"
        :class="{ pressed: pressedControls.encoder }"
        type="button"
        aria-label="Кнопка энкодера"
        @click="$emit('press', 'encoder')"
      >
        <span>↺</span><strong>ENC</strong><span>↻</span>
      </button>
    </div>
  </div>
</template>

<style scoped>
.rrrraw-preview {
  aspect-ratio: 1.35;
  display: grid;
  place-items: center;
}

.case {
  align-items: center;
  background: #dedede;
  border: 2px solid #242424;
  border-radius: 18px;
  box-shadow: 0 8px 0 #999;
  display: grid;
  gap: 12px;
  grid-template-columns: repeat(3, 1fr);
  padding: 20px;
  transform: perspective(500px) rotateX(7deg);
  width: 100%;
}

button {
  color: #242424;
  cursor: pointer;
  font: inherit;
}

.key {
  aspect-ratio: 1;
  background: #fafafa;
  border: 2px solid #242424;
  border-radius: 12px;
  box-shadow: 0 5px 0 #888;
  font-size: 1.1rem;
  font-weight: 700;
}

.encoder {
  align-items: center;
  aspect-ratio: 1;
  background: #aaa;
  border: 3px double #242424;
  border-radius: 50%;
  box-shadow: 0 5px 0 #777;
  display: flex;
  flex-direction: column;
  grid-column: 2;
  justify-content: center;
  line-height: 1;
}

.encoder span {
  font-size: 0.75rem;
  opacity: 0.65;
}

.encoder strong {
  font-size: 0.65rem;
}

.pressed {
  box-shadow: 0 1px 0 #777;
  transform: translateY(4px);
}
</style>
