import { onBeforeUnmount, onMounted, ref } from 'vue'
import {
  BUTTON_EVENT_STATE,
  COMMANDS,
  DEVICE_TYPES,
  SERIAL_BAUD,
  STATUS,
  buildFrame,
  decodeConfig,
  encodeConfig,
  parseFrames,
} from '../protocol'

function mergeBuffers(current, chunk) {
  const merged = new Uint8Array(current.length + chunk.length)
  merged.set(current)
  merged.set(chunk, current.length)
  return merged
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

function delay(ms) {
  return new Promise((resolve) => {
    window.setTimeout(resolve, ms)
  })
}

function isRetryableOpenError(error) {
  const message = error?.message ?? ''

  return (
    message.includes("Failed to execute 'open' on 'SerialPort': Failed to open serial port.") ||
    message.includes('The port is already open.')
  )
}

async function openPortWithRetry(serialPort) {
  const delays = [0, 200, 500]
  let lastError = null

  for (const waitMs of delays) {
    if (waitMs > 0) {
      await delay(waitMs)
    }

    try {
      await serialPort.open({ baudRate: SERIAL_BAUD })
      return
    } catch (error) {
      lastError = error

      if (!isRetryableOpenError(error)) {
        throw error
      }
    }
  }

  throw lastError
}

function normalizeSerialError(error) {
  const message = error?.message ?? ''

  if (message.includes("Failed to execute 'open' on 'SerialPort': Failed to open serial port.")) {
    return 'Не удалось открыть порт. Похоже, устройство уже занято другой вкладкой или приложением.'
  }

  if (message.includes('The port is already open.')) {
    return 'Порт уже открыт. Закройте другое подключение к устройству и попробуйте снова.'
  }

  return message || 'Не удалось подключить устройство'
}

export function useDeviceConnection({ applyConfig, deviceType, form, isDevicePressed }) {
  const port = ref(null)
  const reader = ref(null)
  const writer = ref(null)
  const isConnected = ref(false)
  const isConnecting = ref(false)
  const isBusy = ref(false)
  const hasRememberedPort = ref(false)
  const hasAvailablePort = ref(false)
  const statusText = ref('Устройство не подключено')
  const receiveBuffer = ref(new Uint8Array())
  const pendingResolver = ref(null)

  function completePending(frame) {
    if (!pendingResolver.value) {
      return
    }

    const resolve = pendingResolver.value
    pendingResolver.value = null
    resolve(frame)
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
      statusText.value = hasRememberedPort.value ? '' : 'Устройство не подключено'
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
          } else if (frame.command === COMMANDS.buttonEvent) {
            isDevicePressed.value = frame.payload[0] === BUTTON_EVENT_STATE.pressed
          } else if (frame.command === COMMANDS.error) {
            statusText.value = `Ошибка устройства: ${frame.payload[0]}`
          }

          if (frame.command !== COMMANDS.buttonEvent) {
            completePending(frame)
          }
        }
      }
    } catch (error) {
      statusText.value = error.message
    } finally {
      isConnected.value = false
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

  async function verifyDevice() {
    let frame

    try {
      frame = await sendCommand(COMMANDS.ping, new Uint8Array(), [COMMANDS.pong, COMMANDS.error])
    } catch (error) {
      if (error instanceof Error && error.message === 'Таймаут ожидания ответа от устройства') {
        throw new Error('Подключено неподдерживаемое устройство или устройство не отвечает')
      }

      throw error
    }

    if (frame.command === COMMANDS.error || frame.payload[0] !== STATUS.ok) {
      throw new Error('Подключено неподдерживаемое устройство')
    }

    deviceType.value = frame.payload[1] ?? DEVICE_TYPES.oneShot
    form.deviceType = deviceType.value
    statusText.value = 'Подключено'
  }

  async function connect() {
    if (!('serial' in navigator)) {
      statusText.value = 'Конфигуратор работает только в Chromium-браузерах'
      return
    }

    try {
      isConnecting.value = true

      if (port.value || isConnected.value) {
        await disconnect()
      }

      port.value = await navigator.serial.requestPort()
      await openPortWithRetry(port.value)
      reader.value = port.value.readable.getReader()
      writer.value = port.value.writable.getWriter()
      receiveBuffer.value = new Uint8Array()
      hasRememberedPort.value = true
      hasAvailablePort.value = true
      statusText.value = ''
      void readLoop()
      await verifyDevice()
      isConnected.value = true
      await refreshConfig()
    } catch (error) {
      if (port.value) {
        await disconnect({ preserveStatus: true })
      }
      statusText.value = normalizeSerialError(error)
    } finally {
      isConnecting.value = false
    }
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
    if (!window.confirm('Сбросить конфигурацию к настройкам по умолчанию?')) {
      return
    }

    await withBusyState(async () => {
      const frame = await sendCommand(COMMANDS.resetConfig, new Uint8Array(), [COMMANDS.config, COMMANDS.error])
      if (frame.command === COMMANDS.error) {
        throw new Error(`Не удалось сбросить конфигурацию: ${frame.payload[0]}`)
      }

      applyConfig(decodeConfig(frame.payload))
    })
  }

  onMounted(async () => {
    if (!('serial' in navigator)) {
      statusText.value = 'Конфгирутор работает только в Хроме'
      return
    }

    navigator.serial.addEventListener('connect', handleSerialConnect)
    navigator.serial.addEventListener('disconnect', handleSerialDisconnect)
    await refreshKnownPorts()
  })

  onBeforeUnmount(() => {
    if ('serial' in navigator) {
      navigator.serial.removeEventListener('connect', handleSerialConnect)
      navigator.serial.removeEventListener('disconnect', handleSerialDisconnect)
    }

    disconnect()
  })

  return {
    connect,
    disconnect,
    hasAvailablePort,
    hasRememberedPort,
    isBusy,
    isConnected,
    isConnecting,
    refreshConfig,
    resetConfig,
    saveConfig,
    statusText,
  }
}
