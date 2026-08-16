<script setup>
import { computed, ref } from 'vue'
import { ACTION_TYPES, formatHotkey, formatMouseAction, isEditableCharHotkey, toHexCode } from '../../protocol'
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
  allowModifierOnly: {
    type: Boolean,
    default: false,
  },
  disabled: {
    type: Boolean,
    default: false,
  },
  showMouseOptions: {
    type: Boolean,
    default: false,
  },
})

const emit = defineEmits(['update:gesture', 'invalid-hotkey-char', 'open', 'close'])

const showHotkeyEditor = ref(isEditableCharHotkey(props.gesture) || props.gesture?.type === ACTION_TYPES.mouse);

function consumerLabel(code) {
  const option = props.gestureOptions.find((item) => item.value === `consumer:${code}`)
  if (option) {
    return option.label
  }

  return `Неподдерживаемое действие · ${toHexCode(code)}`
}

const currentActionLabel = computed(() => {
  if (props.gesture.type === ACTION_TYPES.hotkey) {
    return formatHotkey(props.gesture)
  }

  if (props.gesture.type === ACTION_TYPES.mouse) {
    return formatMouseAction(props.gesture)
  }

  return consumerLabel(props.gesture.code)
})

function gestureSelectValue() {
  if (props.gesture.type === ACTION_TYPES.hotkey || props.gesture.type === ACTION_TYPES.mouse) {
    return props.hotkeySelectValue
  }

  return `consumer:${props.gesture.code}`
}

function updateGesture(value, close) {
  if (value === props.hotkeySelectValue) {
    showHotkeyEditor.value = true;
    emit('update:gesture', {
      type: props.gesture.type === ACTION_TYPES.mouse ? ACTION_TYPES.mouse : ACTION_TYPES.hotkey,
      code: (props.gesture.type === ACTION_TYPES.hotkey || props.gesture.type === ACTION_TYPES.mouse)
        ? props.gesture.code : 0x04,
      modifiers: (props.gesture.type === ACTION_TYPES.hotkey || props.gesture.type === ACTION_TYPES.mouse)
        ? props.gesture.modifiers : 0,
    })
    return
  }

  if (value.startsWith('hotkey:')) {
    showHotkeyEditor.value = true;
    const [, code, modifiers] = value.split(':')
    emit('update:gesture', {
      type: ACTION_TYPES.hotkey,
      code: Number(code),
      modifiers: Number(modifiers),
    })
    return
  }
  showHotkeyEditor.value = false;
  emit('update:gesture', {
    type: ACTION_TYPES.consumer,
    code: Number(value.replace('consumer:', '')),
    modifiers: 0,
  })
  close?.()
}

function handleOpen() {
  showHotkeyEditor.value = isEditableCharHotkey(props.gesture) || props.gesture?.type === ACTION_TYPES.mouse
  emit('open')
}

function handleClose() {
  emit('close')
}
</script>

<template>
  <div class="gesture-card" :class="{ disabled }">
    <span class="gesture-trigger-label">
      {{ label }}
    </span>
    <DropdownPanel @open="handleOpen" @close="handleClose">
      <template #trigger="{ toggle, triggerRef, isOpen }">
        <button
          :ref="triggerRef"
          class="button gesture-trigger"
          :class="{ active: isOpen }"
          :disabled="disabled"
          type="button"
          @click="toggle"
        >
          <span class="gesture-trigger-value">{{ currentActionLabel }}</span>
        </button>
      </template>

      <template #default="{ close }">
        <ul v-if="!showHotkeyEditor" class="select-list">
          <li class="select-item" :class="{ selected: gestureSelectValue() === hotkeySelectValue }">
            <button @click="updateGesture(hotkeySelectValue, close)">
              Горячая клавиша
            </button>
          </li>
          <li
            v-for="option in gestureOptions"
            :key="option.value"
            class="select-item"
            :class="{ selected: gestureSelectValue() === option.value }"
          >
            <button @click="updateGesture(option.value, close)">
              {{ option.label }}
            </button>
          </li>
        </ul>
        <template v-else>
          <button class="back-button" @click="showHotkeyEditor = false">
            ← Горячая клавиша
          </button>
          <HotkeyEditor
            :gesture="gesture"
            :allow-modifier-only="allowModifierOnly"
            :is-mac-like="isMacLike"
            :modifier-options="modifierOptions"
            :show-mouse-options="showMouseOptions"
            @invalid-char="$emit('invalid-hotkey-char')"
            @update:gesture="$emit('update:gesture', $event)"
          />
        </template>
      </template>
    </DropdownPanel>
  </div>
</template>

<style scoped>
.gesture-card {
  position: relative;
  text-align: center;
}

.gesture-card.disabled {
  opacity: 0.45;
}

.gesture-card.disabled .gesture-trigger {
  cursor: not-allowed;
}

.gesture-trigger {
  width: 100%;
  padding: 14px 16px;
  font-size: 2rem;
  line-height: 1;
  text-align: left;
}

.gesture-trigger-label {
  font-size: 1.1rem;
  font-weight: 500;
  margin-bottom: 4px;
  display: block;
}

.gesture-trigger-value {
  font-size: 1.2rem;
  font-weight: 500;
  line-height: 1.2;
}

.back-button {
  appearance: none;
  border: 0;
  background: 0;
  text-align: left;
  font-weight: 600;
  cursor: pointer;
  font-size: 1.2rem;
  margin-bottom: 0.5rem;
  margin-top: -0.5rem;
  width: fit-content;
}

.back-button:hover {
  margin-left: -0.5rem;
}
</style>
