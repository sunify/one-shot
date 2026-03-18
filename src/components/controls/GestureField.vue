<script setup>
import { computed } from 'vue'
import { ACTION_TYPES, formatHotkey, isEditableCharHotkey, toHexCode } from '../../protocol'
import DropdownPanel from './DropdownPanel.vue'
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

function hasConsumerOption(code) {
  return props.gestureOptions.some((group) =>
    group.options.some((option) => option.value === `consumer:${code}`),
  )
}

function consumerLabel(code) {
  for (const group of props.gestureOptions) {
    const option = group.options.find((item) => item.value === `consumer:${code}`)
    if (option) {
      return option.label
    }
  }

  return `Неподдерживаемое действие · ${toHexCode(code)}`
}

const currentActionLabel = computed(() => {
  if (props.gesture.type === ACTION_TYPES.hotkey) {
    return formatHotkey(props.gesture)
  }

  return consumerLabel(props.gesture.code)
})

function gestureSelectValue() {
  if (props.gesture.type === ACTION_TYPES.hotkey) {
    return props.hotkeySelectValue
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
  <div class="gesture-card">
    <span class="gesture-trigger-label">
      {{ label }}
    </span>
    <DropdownPanel>
      <template #trigger="{ toggle, triggerRef }">
        <button
          :ref="triggerRef"
          class="gesture-trigger"
          type="button"
          @click="toggle"
        >
          <span class="gesture-trigger-value">{{ currentActionLabel }}</span>
        </button>
      </template>

      <template #default>
        <label class="field">
          <select :value="gestureSelectValue()" @change="updateGesture($event.target.value)">
            <option :value="hotkeySelectValue">Горячая клавиша</option>
            <option
              v-if="gesture.type === ACTION_TYPES.consumer && !hasConsumerOption(gesture.code)"
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
        </label>
        <HotkeyEditor
          v-if="isEditableCharHotkey(gesture)"
          :gesture="gesture"
          :is-mac-like="isMacLike"
          :modifier-options="modifierOptions"
          @invalid-char="$emit('invalid-hotkey-char')"
          @update:gesture="$emit('update:gesture', $event)"
        />
      </template>
    </DropdownPanel>
  </div>
</template>
