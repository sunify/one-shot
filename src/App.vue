<script setup>
import { computed, onBeforeUnmount, reactive, ref } from 'vue'
import {
  ACTION_TYPES,
  COMMANDS,
  HOTKEY_SELECT_VALUE,
  MEDIA_KEY_OPTIONS,
  MODIFIER_OPTIONS,
  SERIAL_BAUD,
  STATUS,
  buildFrame,
  decodeConfig,
  encodeConfig,
  formatHotkey,
  hotkeyCharFromCode,
  hotkeyCodeFromChar,
  parseFrames,
  toHexCode,
} from './protocol'

const port = ref(null)
const reader = ref(null)
const writer = ref(null)
const isConnected = ref(false)
const isBusy = ref(false)
const statusText = ref()
const receiveBuffer = ref(new Uint8Array())
const pendingResolver = ref(null)

const form = reactive({
  singleTap: { type: ACTION_TYPES.consumer, code: 0x00cd, modifiers: 0 },
  doubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
  tripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
  red: 250,
  green: 255,
  blue: 210,
})

const selectedColor = computed({
  get() {
    return `#${[form.red, form.green, form.blue].map((value) => value.toString(16).padStart(2, '0')).join('')}`
  },
  set(value) {
    form.red = Number.parseInt(value.slice(1, 3), 16)
    form.green = Number.parseInt(value.slice(3, 5), 16)
    form.blue = Number.parseInt(value.slice(5, 7), 16)
  },
})

const colorPreviewStyle = computed(() => ({
  '--accent': `rgb(${form.red}, ${form.green}, ${form.blue})`,
}))

const gestureFields = [
  { key: 'singleTap', label: 'Одиночное нажатие' },
  { key: 'doubleTap', label: 'Двойное нажатие' },
  { key: 'tripleTap', label: 'Тройное нажатие' },
]

const gestureOptions = MEDIA_KEY_OPTIONS.map((option) => ({
  label: `${option.label} · ${toHexCode(option.value)}`,
  value: String(option.value),
}))

const modifierOptions = MODIFIER_OPTIONS
const isMacLike = /Mac|iPhone|iPad|iPod/.test(navigator.platform)

function mergeBuffers(current, chunk) {
  const merged = new Uint8Array(current.length + chunk.length)
  merged.set(current)
  merged.set(chunk, current.length)
  return merged
}

function cloneGesture(gesture) {
  return {
    type: gesture.type,
    code: gesture.code,
    modifiers: gesture.modifiers,
  }
}

function applyConfig(config) {
  form.singleTap = cloneGesture(config.singleTap)
  form.doubleTap = cloneGesture(config.doubleTap)
  form.tripleTap = cloneGesture(config.tripleTap)
  form.red = config.red
  form.green = config.green
  form.blue = config.blue
}

function completePending(frame) {
  if (!pendingResolver.value) {
    return
  }

  const resolve = pendingResolver.value
  pendingResolver.value = null
  resolve(frame)
}

async function waitForFrame(expectedCommands) {
  return new Promise((resolve, reject) => {
    const timeoutId = window.setTimeout(() => {
      pendingResolver.value = null
      reject(new Error('Таймаут ожидания ответа от устройства'))
    }, 1500)

    pendingResolver.value = (frame) => {
      window.clearTimeout(timeoutId)

      if (!expectedCommands.includes(frame.command)) {
        reject(new Error(`Неожиданный ответ 0x${frame.command.toString(16)}`))
        return
      }

      resolve(frame)
    }
  })
}

async function readLoop() {
  try {
    while (port.value?.readable) {
      const result = await reader.value.read()
      if (result.done) {
        break
      }

      receiveBuffer.value = mergeBuffers(receiveBuffer.value, result.value)
      const parsed = parseFrames(receiveBuffer.value)
      receiveBuffer.value = parsed.rest

      for (const frame of parsed.frames) {
        if (frame.command === COMMANDS.config) {
          applyConfig(decodeConfig(frame.payload))
        } else if (frame.command === COMMANDS.error) {
          statusText.value = `Ошибка устройства: ${frame.payload[0]}`
        }

        completePending(frame)
      }
    }
  } catch (error) {
    statusText.value = error.message
  } finally {
    isConnected.value = false
  }
}

async function connect() {
  if (!('serial' in navigator)) {
    statusText.value = 'Web Serial поддерживается только в Chromium-браузерах'
    return
  }

  try {
    if (isConnected.value) {
      await disconnect()
    }

    port.value = await navigator.serial.requestPort()
    await port.value.open({ baudRate: SERIAL_BAUD })
    reader.value = port.value.readable.getReader()
    writer.value = port.value.writable.getWriter()
    receiveBuffer.value = new Uint8Array()
    isConnected.value = true
    statusText.value = 'Устройство подключено'
    void readLoop()
    await refreshConfig()
  } catch (error) {
    // statusText.value = error.message
  }
}

async function disconnect() {
  pendingResolver.value = null

  try {
    await reader.value?.cancel()
  } catch (error) {
    statusText.value = error.message
  }

  try {
    reader.value?.releaseLock()
    writer.value?.releaseLock()
    await port.value?.close()
  } catch (error) {
    statusText.value = error.message
  } finally {
    reader.value = null
    writer.value = null
    port.value = null
    isConnected.value = false
  }
}

async function sendCommand(command, payload = new Uint8Array(), expected = [COMMANDS.ack]) {
  if (!writer.value) {
    throw new Error('Сначала подключите устройство')
  }

  const responsePromise = waitForFrame(expected)
  await writer.value.write(buildFrame(command, payload))
  return responsePromise
}

async function withBusyState(work) {
  isBusy.value = true
  try {
    await work()
  } finally {
    isBusy.value = false
  }
}

async function refreshConfig() {
  await withBusyState(async () => {
    const frame = await sendCommand(COMMANDS.getConfig, new Uint8Array(), [COMMANDS.config, COMMANDS.error])
    if (frame.command === COMMANDS.error) {
      throw new Error(`Устройство вернуло ошибку ${frame.payload[0]}`)
    }

    applyConfig(decodeConfig(frame.payload))
  })
}

async function saveConfig() {
  await withBusyState(async () => {
    const payload = encodeConfig(form)
    const frame = await sendCommand(COMMANDS.setConfig, payload, [COMMANDS.ack, COMMANDS.error])

    if (frame.command === COMMANDS.error || frame.payload[0] !== STATUS.ok) {
      throw new Error(`Не удалось сохранить конфигурацию: ${frame.payload[0]}`)
    }

    statusText.value = 'Сохранено'
  })
}

async function resetConfig() {
  await withBusyState(async () => {
    const frame = await sendCommand(COMMANDS.resetConfig, new Uint8Array(), [COMMANDS.config, COMMANDS.error])
    if (frame.command === COMMANDS.error) {
      throw new Error(`Не удалось сбросить конфигурацию: ${frame.payload[0]}`)
    }

    applyConfig(decodeConfig(frame.payload))
  })
}

function gestureSelectValue(gesture) {
  return gesture.type === ACTION_TYPES.hotkey ? HOTKEY_SELECT_VALUE : String(gesture.code)
}

function updateGesture(field, value) {
  if (value === HOTKEY_SELECT_VALUE) {
    const gesture = form[field]
    form[field] = {
      type: ACTION_TYPES.hotkey,
      code: gesture.type === ACTION_TYPES.hotkey ? gesture.code : 0x04,
      modifiers: gesture.type === ACTION_TYPES.hotkey ? gesture.modifiers : 0,
    }
    return
  }

  form[field] = {
    type: ACTION_TYPES.consumer,
    code: Number(value),
    modifiers: 0,
  }
}

function hotkeyLabel(gesture) {
  return formatHotkey(gesture)
}

function selectedModifiers(gesture) {
  return modifierOptions
    .filter((option) => (gesture.modifiers & Number(option.value)) !== 0)
    .map((option) => option.value)
}

function hotkeyChar(gesture) {
  return hotkeyCharFromCode(gesture.code)
}

function modifierLabel(option) {
  if (option.label === 'Meta' && isMacLike) {
    return '⌘'
  }

  return option.label
}

function updateHotkeyModifiers(field, values) {
  const modifiers = values.reduce((mask, value) => mask | Number(value), 0)
  form[field] = {
    ...form[field],
    type: ACTION_TYPES.hotkey,
    modifiers,
  }
}

function updateHotkeyKey(field, value) {
  const nextValue = value.slice(0, 1)
  const code = hotkeyCodeFromChar(nextValue)

  if (nextValue && code === 0) {
    statusText.value = 'Пока поддерживаются только латинские буквы и цифры'
    return
  }

  form[field] = {
    ...form[field],
    type: ACTION_TYPES.hotkey,
    code,
  }
}

onBeforeUnmount(() => {
  disconnect()
})
</script>

<template>
  <main class="shell">
    <section class="hero">
      <h1>Конфигуратор для OneShot</h1>

      <div v-if="!isConnected" class="actions">
        <button class="primary" :disabled="isBusy" @click="connect">
          Подключить устройство
        </button>
      </div>

      <p class="status">{{ statusText }}</p>
    </section>

    <template v-if="isConnected">
      <section class="panel">
        <div class="panel-head">
          <div>
            <p class="eyebrow">Жесты</p>
          </div>
        </div>

        <div class="grid">
          <label v-for="gestureField in gestureFields" :key="gestureField.key" class="field">
            <span>{{ gestureField.label }}</span>
            <select
              :value="gestureSelectValue(form[gestureField.key])"
              @change="updateGesture(gestureField.key, $event.target.value)"
            >
              <option :value="HOTKEY_SELECT_VALUE">Hotkey</option>
              <option v-for="option in gestureOptions" :key="option.value" :value="option.value">
                {{ option.label }}
              </option>
            </select>
            <div v-if="form[gestureField.key].type === ACTION_TYPES.hotkey" class="hotkey-editor">
              <div class="hotkey-row">
                <div class="modifier-picker" role="group" aria-label="Hotkey modifiers">
                  <label v-for="option in modifierOptions" :key="option.value" class="modifier-option">
                    <input
                      :checked="selectedModifiers(form[gestureField.key]).includes(option.value)"
                      type="checkbox"
                      @change="
                        updateHotkeyModifiers(
                          gestureField.key,
                          $event.target.checked
                            ? [...selectedModifiers(form[gestureField.key]), option.value]
                            : selectedModifiers(form[gestureField.key]).filter((value) => value !== option.value),
                        )
                      "
                    />
                    <span class="modifier-key">{{ modifierLabel(option) }}</span>
                  </label>
                </div>
                <input
                  :value="hotkeyChar(form[gestureField.key])"
                  class="hotkey-char-input"
                  maxlength="1"
                  type="text"
                  @input="updateHotkeyKey(gestureField.key, $event.target.value)"
                />
              </div>
            </div>
          </label>
        </div>
      </section>

      <section class="panel accent-panel" :style="colorPreviewStyle">
        <div class="panel-head">
          <div>
            <h2>Подсветка</h2>
          </div>
          <div class="swatch"></div>
        </div>

        <div class="color-layout">
          <label class="field">
            <input v-model="selectedColor" type="color" />
          </label>
        </div>
      </section>

      <section class="footer-actions">
        <button class="ghost" :disabled="isBusy" @click="disconnect">
          Отключить
        </button>
        <button class="ghost" :disabled="isBusy" @click="refreshConfig">
          Считать конфиг
        </button>
        <button class="ghost" :disabled="isBusy" @click="resetConfig">
          Сбросить
        </button>
        <button class="primary" :disabled="isBusy" @click="saveConfig">
          Сохранить
        </button>
      </section>
    </template>
  </main>
</template>
