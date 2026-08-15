<script setup>
import { computed } from 'vue'
import { ACTION_TYPES, HOTKEY_KEY_OPTIONS, MOUSE_OPTIONS } from '../../protocol'

const props = defineProps({
  allowModifierOnly: {
    type: Boolean,
    default: false,
  },
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
  showMouseOptions: {
    type: Boolean,
    default: false,
  },
})

const emit = defineEmits(['update:gesture'])

const selectedModifiers = computed(() =>
  props.modifierOptions
    .filter((option) => (props.gesture.modifiers & Number(option.value)) !== 0)
    .map((option) => option.value),
)

const keyOptions = computed(() => props.allowModifierOnly
  ? HOTKEY_KEY_OPTIONS
  : HOTKEY_KEY_OPTIONS.filter((option) => option.code !== 0))

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

const isMouse = computed(() => props.gesture.type === ACTION_TYPES.mouse)
const mouseAxis = computed(() => props.gesture.code & 0xff)
const mouseAmount = computed(() => (props.gesture.code >> 8) & 0xff || 2)

const selectValue = computed(() => {
  if (isMouse.value) {
    return `mouse:${mouseAxis.value}`
  }
  return String(props.gesture.code)
})

function updateKey(value) {
  if (value.startsWith('mouse:')) {
    const axis = Number(value.replace('mouse:', ''))
    emit('update:gesture', {
      type: ACTION_TYPES.mouse,
      code: axis | (mouseAmount.value << 8),
      modifiers: props.gesture.modifiers,
    })
    return
  }

  emit('update:gesture', {
    type: ACTION_TYPES.hotkey,
    code: Number(value),
    modifiers: props.gesture.modifiers,
  })
}

function updateAmount(value) {
  const clamped = Math.max(1, Math.min(20, Number(value)))
  emit('update:gesture', {
    ...props.gesture,
    code: mouseAxis.value | (clamped << 8),
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
        :value="selectValue"
        class="input select hotkey-char-input"
        @change="updateKey($event.target.value)"
      >
        <option v-for="option in keyOptions" :key="option.code" :value="String(option.code)">
          {{ option.label }}
        </option>
        <optgroup v-if="showMouseOptions" label="Мышь">
          <option v-for="opt in MOUSE_OPTIONS" :key="opt.value" :value="`mouse:${opt.value}`">
            {{ opt.label }}
          </option>
        </optgroup>
      </select>
    </div>
    <div v-if="isMouse" class="hotkey-row">
      <label class="amount-label">Величина</label>
      <input
        type="number"
        class="input amount-input"
        min="1"
        max="20"
        :value="mouseAmount"
        @input="updateAmount($event.target.value)"
      />
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
  background: var(--color-bg);
}

.modifier-option {
  position: relative;
  display: flex;
  flex-shrink: 0;
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
  background: var(--color-bg);
  color: var(--color-text);
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
  background-color: var(--color-active-check);
}

.modifier-option input:focus-visible + .modifier-key {
  outline: 2px solid var(--color-border);
  outline-offset: -2px;
}

.hotkey-char-input {
  width: auto;
}

.amount-label {
  font-size: 0.9rem;
  font-weight: 500;
  display: flex;
  align-items: center;
}

.amount-input {
  width: 64px;
}
</style>
