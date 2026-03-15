<script setup>
import { computed } from 'vue'
import { hotkeyCharFromCode, hotkeyFromChar } from '../../protocol'

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
  if (option.label === 'Meta') {
    return 'command'
  }

  if (option.label === 'Alt') {
    return 'option'
  }

  if (option.label === 'Ctrl') {
    return 'control'
  }

  if (option.label === 'Shift') {
    return 'shift'
  }

  return option.label.toLowerCase()
}

function modifierSymbol(option) {
  if (option.label === 'Meta') {
    return '⌘'
  }

  if (option.label === 'Alt') {
    return '⌥'
  }

  if (option.label === 'Ctrl') {
    return '⌃'
  }

  if (option.label === 'Shift') {
    return '⇧'
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
  let nextValue = value.split('').filter((char) => hotkeyFromChar(char, props.gesture.modifiers)).slice(-1)

  if (!nextValue) {
    return
  }

  const hotkey = hotkeyFromChar(nextValue, props.gesture.modifiers)

  if (!hotkey) {
    emit('invalid-char')
    return
  }

  emit('update:gesture', {
    ...props.gesture,
    code: hotkey?.code ?? 0,
    modifiers: hotkey?.modifiers ?? (props.gesture.modifiers & ~0x02),
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
          <span class="modifier-key" :data-key="option.label.toLowerCase()">
            <span class="modifier-symbol">{{ modifierSymbol(option) }}</span>
            <span class="modifier-label">{{ modifierLabel(option) }}</span>
          </span>
        </label>
      </div>
      <input
        :value="hotkeyCharFromCode(gesture.code, gesture.modifiers)"
        class="hotkey-char-input"
        maxlength="2"
        type="text"
        @input="updateKey($event.target.value)"
      />
    </div>
  </div>
</template>
