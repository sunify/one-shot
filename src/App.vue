<script setup>
import { computed, onBeforeUnmount, reactive, ref } from 'vue'
import {
  COMMANDS,
  MEDIA_KEY_OPTIONS,
  SERIAL_BAUD,
  STATUS,
  buildFrame,
  decodeConfig,
  encodeConfig,
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
  singleTapCode: 0x00cd,
  doubleTapCode: 0x00b5,
  tripleTapCode: 0x00b6,
  red: 250,
  green: 255,
  blue: 210,
})

const selectedColor = computed({
  get() {
    return `#${[form.red, form.green, form.blue]
      .map((value) => value.toString(16).padStart(2, '0'))
      .join('')}`
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

function mergeBuffers(current, chunk) {
  const merged = new Uint8Array(current.length + chunk.length)
  merged.set(current)
  merged.set(chunk, current.length)
  return merged
}

function applyConfig(config) {
  form.singleTapCode = config.singleTapCode
  form.doubleTapCode = config.doubleTapCode
  form.tripleTapCode = config.tripleTapCode
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
          statusText.value = 'Конфигурация считана'
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
    statusText.value = error.message
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
    statusText.value = 'Конфигурация обновлена из устройства'
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
    statusText.value = 'Конфигурация сброшена к значениям по умолчанию'
  })
}

function updateCode(field, value) {
  form[field] = Number(value)
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
          <label class="field">
            <span>Нажатие</span>
            <select :value="form.singleTapCode" @change="updateCode('singleTapCode', $event.target.value)">
              <option v-for="option in MEDIA_KEY_OPTIONS" :key="option.value" :value="option.value">
                {{ option.label }} · {{ toHexCode(option.value) }}
              </option>
            </select>
          </label>

          <label class="field">
            <span>Двойное нажатие</span>
            <select :value="form.doubleTapCode" @change="updateCode('doubleTapCode', $event.target.value)">
              <option v-for="option in MEDIA_KEY_OPTIONS" :key="option.value" :value="option.value">
                {{ option.label }} · {{ toHexCode(option.value) }}
              </option>
            </select>
          </label>

          <label class="field">
            <span>Тройное нажатие</span>
            <select :value="form.tripleTapCode" @change="updateCode('tripleTapCode', $event.target.value)">
              <option v-for="option in MEDIA_KEY_OPTIONS" :key="option.value" :value="option.value">
                {{ option.label }} · {{ toHexCode(option.value) }}
              </option>
            </select>
          </label>
        </div>
      </section>

      <section class="panel accent-panel" :style="colorPreviewStyle">
        <div class="panel-head">
          <div>
            <p class="eyebrow">Подсветка</p>
            <h2>RGB цвет базы</h2>
          </div>
          <div class="swatch"></div>
        </div>

        <div class="color-layout">
          <label class="field">
            <span>Цвет</span>
            <input v-model="selectedColor" type="color" />
          </label>
        </div>
      </section>

      <section class="footer-actions">
        <button class="primary" :disabled="isBusy" @click="saveConfig">
          Сохранить в устройство
        </button>
        <button class="ghost" :disabled="isBusy" @click="resetConfig">
          Сбросить по умолчанию
        </button>
      </section>
    </template>
  </main>
</template>
