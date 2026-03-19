<script setup>
import { computed } from 'vue'
import { HOTKEY_KEY_OPTIONS } from '../../protocol'

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

const emit = defineEmits(['update:gesture'])

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
  emit('update:gesture', {
    ...props.gesture,
    code: Number(value),
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
          <span class="button modifier-key" :class="{ active: selectedModifiers.includes(option.value) }" :data-key="option.label.toLowerCase()">
            <span class="modifier-symbol">{{ modifierSymbol(option) }}</span>
            <span class="modifier-label">{{ modifierLabel(option) }}</span>
          </span>
        </label>
      </div>
      <select
        :value="gesture.code"
        class="hotkey-char-input"
        @change="updateKey($event.target.value)"
      >
        <option v-for="option in HOTKEY_KEY_OPTIONS" :key="option.code" :value="option.code">
          {{ option.label }}
        </option>
      </select>
    </div>
  </div>
</template>

<style scoped>
.hotkey-editor {
  display: grid;
  gap: 8px;
}

.hotkey-row {
  display: flex;
  align-items: stretch;
  gap: 8px;
}

.modifier-picker {
  display: inline-flex;
  gap: 4px;
  align-items: stretch;
  min-width: 0;
  border-radius: 0;
  overflow: hidden;
  background: #ffffff;
}

.modifier-option {
  position: relative;
  display: flex;
  flex: 1 1 0;
}

.modifier-option input {
  position: absolute;
  inset: 0;
  opacity: 0;
  margin: 0;
  cursor: pointer;
}

.modifier-key {
  display: flex;
  flex: 1 1 auto;
  flex-direction: column;
  align-items: flex-end;
  justify-content: center;
  min-width: 68px;
  min-height: 44px;
  padding: 2px 10px 3px;
  white-space: nowrap;
  background: #ffffff;
  color: #000;
  user-select: none;

  &[data-key="shift"] {
    min-width: 82px;
  }
  &[data-key="alt"],
  &[data-key="ctrl"] {
    min-width: 76px;
  }

  &[data-key="meta"] {
    min-width: 86px;
  }
}

.modifier-symbol {
  font-size: 1rem;
  line-height: 1;
}

.modifier-label {
  font-size: 0.7rem;
  line-height: 1;
  letter-spacing: 0.02em;
  text-transform: lowercase;
}

.modifier-key.active {
  background-color: #efefef;
}

.modifier-option input:focus-visible + .modifier-key {
  outline: 2px solid #000;
  outline-offset: -2px;
}

.hotkey-char-input {
  width: 88px !important;
  border: 2px solid #000;
  background: #ffffff;
  color: #000;
  border-radius: 6px;
  padding: 10px 12px;
  min-height: 44px;
  text-align: center;
  font-weight: 500;
  text-transform: uppercase;
  appearance: base-select;
  white-space: nowrap;
}

.hotkey-char-input::picker-icon {
  color: #000;
  translate: -2px 0;
}

.hotkey-char-input:focus-visible {
  outline: 2px solid #000;
  outline-offset: 2px;
}

</style>
