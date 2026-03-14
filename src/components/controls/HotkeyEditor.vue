<script setup>
import { computed } from 'vue'
import { formatHotkey, hotkeyCharFromCode, hotkeyCodeFromChar } from '../../protocol'

const props = defineProps({
  gesture: {
    type: Object,
    required: true,
  },
  isMacLike: {
    type: Boolean,
    default: false,
  },
  modifierOptions: {
    type: Array,
    required: true,
  },
})

const emit = defineEmits(['update:gesture', 'invalid-char'])

const selectedModifiers = computed(() =>
  props.modifierOptions
    .filter((option) => (props.gesture.modifiers & Number(option.value)) !== 0)
    .map((option) => option.value),
)

function modifierLabel(option) {
  if (option.label === 'Meta' && props.isMacLike) {
    return '⌘'
  }

  return option.label
}

function updateModifiers(optionValue, checked) {
  const nextValues = checked
    ? [...selectedModifiers.value, optionValue]
    : selectedModifiers.value.filter((value) => value !== optionValue)

  const modifiers = nextValues.reduce((mask, value) => mask | Number(value), 0)

  emit('update:gesture', {
    ...props.gesture,
    modifiers,
  })
}

function updateKey(value) {
  const nextValue = value.slice(0, 1)
  const code = hotkeyCodeFromChar(nextValue)

  if (nextValue && code === 0) {
    emit('invalid-char')
    return
  }

  emit('update:gesture', {
    ...props.gesture,
    code,
  })
}
</script>

<template>
  <div class="hotkey-editor">
    <div class="hotkey-row">
      <div class="modifier-picker" role="group" aria-label="Hotkey modifiers">
        <label v-for="option in modifierOptions" :key="option.value" class="modifier-option">
          <input
            :checked="selectedModifiers.includes(option.value)"
            type="checkbox"
            @change="updateModifiers(option.value, $event.target.checked)"
          />
          <span class="modifier-key">{{ modifierLabel(option) }}</span>
        </label>
      </div>
      <input
        :value="hotkeyCharFromCode(gesture.code)"
        class="hotkey-char-input"
        maxlength="1"
        type="text"
        @input="updateKey($event.target.value)"
      />
    </div>
    <p class="hotkey-preview">{{ formatHotkey(gesture) }}</p>
  </div>
</template>
