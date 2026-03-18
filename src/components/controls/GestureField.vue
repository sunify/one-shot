<script setup>
import {
  autoUpdate,
  flip,
  offset,
  shift,
  useFloating,
} from '@floating-ui/vue'
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { ACTION_TYPES, formatConsumerPreview, formatHotkey, isEditableCharHotkey, toHexCode } from '../../protocol'
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
const isOpen = ref(false)
const root = ref(null)
const reference = ref(null)
const floating = ref(null)

const { floatingStyles } = useFloating(reference, floating, {
  placement: 'bottom-center',
  middleware: [offset(8), flip(), shift({ padding: 12 })],
  whileElementsMounted: autoUpdate,
})

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

  return formatConsumerPreview(props.gesture.code)
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

  isOpen.value = false
  emit('update:gesture', {
    type: ACTION_TYPES.consumer,
    code: Number(value.replace('consumer:', '')),
    modifiers: 0,
  })
}

function closeDropdown() {
  isOpen.value = false
}

function handleDocumentPointerDown(event) {
  if (!root.value?.contains(event.target)) {
    closeDropdown()
  }
}

function handleDocumentKeydown(event) {
  if (event.key === 'Escape') {
    closeDropdown()
  }
}

watch(isOpen, (open) => {
  if (open) {
    document.addEventListener('pointerdown', handleDocumentPointerDown)
    document.addEventListener('keydown', handleDocumentKeydown)
    return
  }

  document.removeEventListener('pointerdown', handleDocumentPointerDown)
  document.removeEventListener('keydown', handleDocumentKeydown)
})

onBeforeUnmount(() => {
  document.removeEventListener('pointerdown', handleDocumentPointerDown)
  document.removeEventListener('keydown', handleDocumentKeydown)
})
</script>

<template>
  <div ref="root" class="gesture-card">
    <span class="gesture-trigger-label">
      {{ label }}
    </span>
    <button
      ref="reference"
      class="gesture-trigger"
      type="button"
      @click="isOpen = !isOpen"
    >
      <span class="gesture-trigger-value">{{ currentActionLabel }}</span>
    </button>

    <div
      v-if="isOpen"
      ref="floating"
      class="gesture-dropdown"
      :style="floatingStyles"
    >
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
    </div>
  </div>
</template>
