<script setup>
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
    <label class="field">
      <input :value="modelValue" type="color" @input="$emit('update:modelValue', $event.target.value)" />
    </label>
    <label class="toggle-field">
      <input
        :checked="breathingEnabled"
        type="checkbox"
        @change="$emit('update:breathingEnabled', $event.target.checked)"
      />
      <span>Дыхание цвета</span>
    </label>
  </div>
</template>
