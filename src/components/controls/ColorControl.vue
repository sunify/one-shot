<script setup>
import DropdownPanel from './DropdownPanel.vue';

defineProps({
  breathingEnabled: {
    type: Boolean,
    default: true,
  },
  modelValue: {
    type: String,
    required: true,
  },
})

defineEmits(['update:breathingEnabled', 'update:modelValue'])
</script>

<template>
  <div class="color-layout">
    <DropdownPanel>
      <template #trigger="{ toggle, triggerRef, isOpen }">
        <button class="color-button" :class="{ 'color-button-active': isOpen }" :ref="triggerRef" @click="toggle">
          <div class="color-input-overlay" style="background: var(--top-color);" />
          <div class="color-input-overlay" style="background: var(--top-shade-color); opacity: 0.1;" />
          <span>Подсветка</span>
        </button>
      </template>
      <template #default>
        <label class="color-input">
          <input :value="modelValue" type="color" @input="$emit('update:modelValue', $event.target.value)" />
          <div class="color-input-overlay" style="background: var(--top-color);" />
          <div class="color-input-overlay" style="background: var(--top-shade-color); opacity: 0.1;" />
        </label>
        <label class="toggle-field">
          <input
            :checked="breathingEnabled"
            type="checkbox"
            @change="$emit('update:breathingEnabled', $event.target.checked)"
          />
          <span>Дыхание цвета</span>
        </label>
      </template>
    </DropdownPanel>
  </div>
</template>

<style scoped>
.color-layout {
  display: flex;
  gap: 18px;
  flex-direction: column;
  align-items: center;
  position: relative;
}

.color-layout::before {
  content: '';
  left: 50%;
  bottom: 100%;
  margin-bottom: -15px;
  height: 60px;
  width: 1.5px;
  background-color: var(--color-border);
  position: absolute;
}

.color-input {
  position: relative;
  width: 150px;
  height: 50px;
  border-radius: 8px;
  overflow: hidden;
  border: 1.3px solid var(--color-border);
}

.color-input input[type="color"] {
  appearance: none;
  -webkit-appearance: none;
  padding: 0;
  width: 100%;
  height: 100%;
  border: 0;
  background: transparent;
  cursor: pointer;
  display: block;
  opacity: 0;
  border-radius: 50%;
}

.color-input input[type="color"]::-webkit-color-swatch-wrapper {
  padding: 0;
}

.color-input input[type="color"]::-webkit-color-swatch {
  border: 0;
  border-radius: 50%;
}

.color-input input[type="color"]::-moz-color-swatch {
  border: 0;
  border-radius: 50%;
}

.color-input-overlay {
  position: absolute;
  left: 0;
  top: 0;
  bottom: 0;
  right: 0;
  pointer-events: none;
  border-radius: 4;
  transition: all 0.15s ease-in-out;
}

.toggle-field {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  color: var(--color-text);
}

.toggle-field input {
  width: auto;
  min-height: auto;
}

.color-button {
  background-color: var(--color-bg);
  color: var(--color-text);
  border: 1.5px solid var(--color-border);
  padding: 7px 15px 10px;
  font-size: 1.2rem;
  position: relative;
  font-weight: 500;
  position: absolute;
  left: 50%;
  top: 100%;
  transform: translate(-50%, 0) scale(0.7);
  margin-top: 15px;
  height: 48px;
  width: 48px;
  border-radius: 50%;
  overflow: hidden;
  transition: all 0.15s ease-in-out;
  cursor: pointer;
}

.color-button:hover,
.color-button:active,
.color-button-active {
  height: 48px;
  width: 150px;
  border-radius: 8px;
  transform: translate(-50%, 0) scale(1);
}

.color-button:hover {
  border: 5px solid var(--color-border);
  border-top-width: 2px;
  border-left-width: 2px;
}

.color-button:active,
.color-button-active {
  border: 3px solid var(--color-border) !important;
}

.color-button span {
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  opacity: 0;
  transition: all 0.15s ease-in-out;
}

.color-button:hover span,
.color-button:active span,
.color-button-active span {
  opacity: 1;
}

.color-button:hover .color-input-overlay,
.color-button:active .color-input-overlay,
.color-button-active .color-input-overlay {
  opacity: 0 !important;
}
</style>
