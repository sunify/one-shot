<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref } from 'vue'
import ColorControl from './components/controls/ColorControl.vue'
import GestureField from './components/controls/GestureField.vue'
import AppShell from './components/layout/AppShell.vue'
import PanelSection from './components/layout/PanelSection.vue'
import {
  ACTION_TYPES,
  COMMANDS,
  FUNCTION_KEY_OPTIONS,
  HOTKEY_SELECT_VALUE,
  MEDIA_KEY_OPTIONS,
  MODIFIER_OPTIONS,
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
const hasRememberedPort = ref(false)
const hasAvailablePort = ref(false)
const statusText = ref('Устройство не подключено')
const receiveBuffer = ref(new Uint8Array())
const pendingResolver = ref(null)

const form = reactive({
  singleTap: { type: ACTION_TYPES.consumer, code: 0x00cd, modifiers: 0 },
  doubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
  tripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
  red: 250,
  green: 255,
  blue: 210,
  breathingEnabled: true,
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

const gestureOptions = [
  {
    label: 'Media',
    options: MEDIA_KEY_OPTIONS.map((option) => ({
      label: `${option.label} · ${toHexCode(option.value)}`,
      value: `consumer:${option.value}`,
    })),
  },
  {
    label: 'Function Keys',
    options: FUNCTION_KEY_OPTIONS.map((option) => ({
      label: option.label,
      value: `hotkey:${option.code}:0`,
    })),
  },
]

const isMacLike = /Mac|iPhone|iPad|iPod/.test(navigator.platform)
const modifierOptions = MODIFIER_OPTIONS

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
  form.breathingEnabled = config.breathingEnabled
}

function completePending(frame) {
  if (!pendingResolver.value) {
    return
  }

  const resolve = pendingResolver.value
  pendingResolver.value = null
  resolve(frame)
}

function getPortSignature(serialPort) {
  if (!serialPort?.getInfo) {
    return ''
  }

  const info = serialPort.getInfo()
  return [info.usbVendorId ?? 'na', info.usbProductId ?? 'na'].join(':')
}

function isSamePort(a, b) {
  if (!a || !b) {
    return false
  }

  return a === b || getPortSignature(a) === getPortSignature(b)
}

async function refreshKnownPorts() {
  if (!('serial' in navigator)) {
    hasAvailablePort.value = false
    hasRememberedPort.value = false
    return
  }

  const ports = await navigator.serial.getPorts()
  hasAvailablePort.value = ports.length > 0

  if (port.value) {
    hasRememberedPort.value = ports.some((knownPort) => isSamePort(knownPort, port.value))
  } else {
    hasRememberedPort.value = hasAvailablePort.value
  }

  if (!isConnected.value) {
    if (hasRememberedPort.value) {
      statusText.value = 'Устройство доступно для подключения'
    } else {
      statusText.value = 'Устройство не подключено'
    }
  }
}

async function handleSerialConnect() {
  await refreshKnownPorts()
}

async function handleSerialDisconnect(event) {
  const disconnectedPort = event.target

  if (port.value && isSamePort(disconnectedPort, port.value)) {
    statusText.value = 'Устройство отключено'
    await disconnect({ preserveStatus: true })
  }

  await refreshKnownPorts()
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
    hasRememberedPort.value = true
    hasAvailablePort.value = true
    statusText.value = 'Устройство подключено'
    void readLoop()
    await refreshConfig()
  } catch (error) {
    statusText.value = error.message ?? 'Не удалось подключить устройство'
  }
}

async function disconnect(options = {}) {
  const { preserveStatus = false } = options
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
    await refreshKnownPorts()

    if (!preserveStatus && !hasRememberedPort.value) {
      statusText.value = 'Устройство не подключено'
    }
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

function updateGesture(field, gesture) {
  form[field] = gesture
}

function handleInvalidHotkeyChar() {
  statusText.value = 'Пока поддерживаются только латинские буквы и цифры'
}

onBeforeUnmount(() => {
  if ('serial' in navigator) {
    navigator.serial.removeEventListener('connect', handleSerialConnect)
    navigator.serial.removeEventListener('disconnect', handleSerialDisconnect)
  }

  disconnect()
})

onMounted(async () => {
  if (!('serial' in navigator)) {
    statusText.value = 'Web Serial поддерживается только в Chromium-браузерах'
    return
  }

  navigator.serial.addEventListener('connect', handleSerialConnect)
  navigator.serial.addEventListener('disconnect', handleSerialDisconnect)
  await refreshKnownPorts()
})
</script>

<template>
  <AppShell
    :is-busy="isBusy"
    :is-connected="isConnected"
    :status-text="statusText"
    title="Конфигуратор<br />для OneShot"
    @connect="connect"
  >
    <template v-if="isConnected">
      <PanelSection>
        <div class="grid">
          <GestureField
            v-for="gestureField in gestureFields"
            :key="gestureField.key"
            :gesture="form[gestureField.key]"
            :gesture-options="gestureOptions"
            :hotkey-select-value="HOTKEY_SELECT_VALUE"
            :is-mac-like="isMacLike"
            :label="gestureField.label"
            :modifier-options="modifierOptions"
            @invalid-hotkey-char="handleInvalidHotkeyChar"
            @update:gesture="updateGesture(gestureField.key, $event)"
          />
        </div>
      </PanelSection>

      <PanelSection panel-class="accent-panel" title="Подсветка" :accent-style="colorPreviewStyle">
        <ColorControl
          v-model="selectedColor"
          :breathing-enabled="form.breathingEnabled"
          @update:breathing-enabled="form.breathingEnabled = $event"
        />
      </PanelSection>

      <section class="footer-actions">
        <button class="ghost" :disabled="isBusy" @click="resetConfig">
          Сбросить
        </button>
        <button class="primary" :disabled="isBusy" @click="saveConfig">
          Сохранить
        </button>
      </section>
    </template>
  </AppShell>
</template>
