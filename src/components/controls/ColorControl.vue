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
