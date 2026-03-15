<script setup>
import { ACTION_TYPES, isEditableCharHotkey, toHexCode } from '../../protocol'
import HotkeyEditor from './HotkeyEditor.vue'

const props = defineProps({
  gesture: {
    type: Object,
    required: true,
  },
  gestureOptions: {
    type: Array,
    required: true,
  },
  hotkeySelectValue: {
    type: String,
    required: true,
  },
  isMacLike: {
    type: Boolean,
    default: false,
  },
  label: {
    type: String,
    required: true,
  },
  modifierOptions: {
    type: Array,
    required: true,
  },
})

const emit = defineEmits(['update:gesture', 'invalid-hotkey-char'])

function gestureSelectValue() {
  if (props.gesture.type === ACTION_TYPES.hotkey) {
    if (isEditableCharHotkey(props.gesture)) {
      return props.hotkeySelectValue
    }

    return `hotkey:${props.gesture.code}:${props.gesture.modifiers}`
  }

  return `consumer:${props.gesture.code}`
}

function updateGesture(value) {
  if (value === props.hotkeySelectValue) {
    emit('update:gesture', {
      type: ACTION_TYPES.hotkey,
      code: props.gesture.type === ACTION_TYPES.hotkey ? props.gesture.code : 0x04,
      modifiers: props.gesture.type === ACTION_TYPES.hotkey ? props.gesture.modifiers : 0,
    })
    return
  }

  if (value.startsWith('hotkey:')) {
    const [, code, modifiers] = value.split(':')
    emit('update:gesture', {
      type: ACTION_TYPES.hotkey,
      code: Number(code),
      modifiers: Number(modifiers),
    })
    return
  }

  emit('update:gesture', {
    type: ACTION_TYPES.consumer,
    code: Number(value.replace('consumer:', '')),
    modifiers: 0,
  })
}
</script>

<template>
  <label class="field">
    <span>{{ label }}</span>
    <select :value="gestureSelectValue()" @change="updateGesture($event.target.value)">
      <option :value="hotkeySelectValue">Горячая клавиша</option>
      <option
        v-if="gesture.type === ACTION_TYPES.consumer"
        :value="`consumer:${gesture.code}`"
      >
        Неподдерживаемое действие · {{ toHexCode(gesture.code) }}
      </option>
      <optgroup v-for="group in gestureOptions" :key="group.label" :label="group.label">
        <option v-for="option in group.options" :key="option.value" :value="option.value">
          {{ option.label }}
        </option>
      </optgroup>
    </select>
    <HotkeyEditor
      v-if="isEditableCharHotkey(gesture)"
      :gesture="gesture"
      :is-mac-like="isMacLike"
      :modifier-options="modifierOptions"
      @invalid-char="$emit('invalid-hotkey-char')"
      @update:gesture="$emit('update:gesture', $event)"
    />
  </label>
</template>
