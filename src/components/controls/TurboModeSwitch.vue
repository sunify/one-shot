<script setup>
defineProps({
  modelValue: {
    type: Boolean,
    default: false,
  },
})

const emit = defineEmits(['update:modelValue'])
</script>

<template>
  <label class="turbo-mode">
    <span class="switch">
      <input
        type="checkbox"
        :checked="modelValue"
        @change="emit('update:modelValue', $event.target.checked)"
      >
      <span class="switch-track">
        <span class="switch-thumb" />
      </span>
    </span>

    <span class="turbo-copy">
      <strong>Турбо-режим</strong>
    </span>
  </label>
</template>

<style scoped>
.turbo-mode {
  display: flex;
  flex-direction: column;
  text-align: center;
  align-items: center;
  justify-content: center;
  gap: 4px;
  margin: 0 auto 28px;
  cursor: pointer;
  user-select: none;
  width: fit-content;
}

.turbo-copy {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  gap: 2px;
}

.turbo-copy strong {
  font-size: 1rem;
  font-weight: 650;
}

.turbo-copy > span {
  font-size: 0.78rem;
  opacity: 0.62;
}

.switch {
  position: relative;
  flex: 0 0 auto;
  width: 51px;
  height: 31px;
}

.switch input {
  position: absolute;
  z-index: 2;
  inset: 0;
  width: 100%;
  height: 100%;
  margin: 0;
  opacity: 0;
  cursor: pointer;
}

.switch-track {
  position: absolute;
  inset: 0;
  border-radius: 999px;
  background: #b7b7bb;
  box-shadow: inset 0 0 0 1px rgb(0 0 0 / 8%);
  transition: background-color 180ms ease;
}

.switch-thumb {
  position: absolute;
  top: 2px;
  left: 2px;
  width: 27px;
  height: 27px;
  border-radius: 50%;
  background: #fff;
  box-shadow: 0 2px 5px rgb(0 0 0 / 28%), 0 0 1px rgb(0 0 0 / 20%);
  transition: transform 180ms cubic-bezier(.2, .8, .2, 1);
}

.switch input:checked + .switch-track {
  background: #34c759;
}

.switch input:checked + .switch-track .switch-thumb {
  transform: translateX(20px);
}

.switch input:focus-visible + .switch-track {
  outline: 3px solid rgb(52 199 89 / 30%);
  outline-offset: 2px;
}
</style>
