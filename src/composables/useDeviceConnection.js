import { onBeforeUnmount, onMounted, ref } from 'vue'
import {
  BUTTON_EVENT_STATE,
  COMMANDS,
  DEFAULT_DEVICE_INFO,
  DEVICE_TYPES,
  SERIAL_BAUD,
  STATUS,
  buildFrame,
  decodeConfig,
  decodeDeviceInfo,
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

function dataViewToUint8Array(view) {
  return new Uint8Array(view.buffer, view.byteOffset, view.byteLength)
}

function toHex(bytes) {
  return [...bytes].map((byte) => byte.toString(16).padStart(2, '0')).join(' ')
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

export function useDeviceConnection({ applyConfig, applyDeviceInfo, deviceType, form, isDevicePressed }) {
  const port = ref(null)
  const reader = ref(null)
  const writer = ref(null)
  const hidDevice = ref(null)
  const transport = ref(null)
  const isConnected = ref(false)
  const isConnecting = ref(false)
  const isBusy = ref(false)
  const hasRememberedPort = ref(false)
  const hasAvailablePort = ref(false)
  const statusText = ref('Устройство не подключено')
  const productName = ref('')
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
    const hasSerial = 'serial' in navigator
    const hasHid = 'hid' in navigator

    if (!hasSerial && !hasHid) {
      hasAvailablePort.value = false
      hasRememberedPort.value = false
      return
    }

    const ports = hasSerial ? await navigator.serial.getPorts() : []
    const hidDevices = hasHid ? await navigator.hid.getDevices() : []
    hasAvailablePort.value = ports.length > 0
    hasAvailablePort.value = ports.length > 0 || hidDevices.length > 0

    if (hidDevice.value) {
      hasRememberedPort.value = hidDevices.some((device) => device === hidDevice.value)
    } else if (port.value) {
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

  async function waitForFrame(expectedCommands, timeoutMs = 1500) {
    return new Promise((resolve, reject) => {
      const timeoutId = window.setTimeout(() => {
        pendingResolver.value = null
        reject(new Error('Таймаут ожидания ответа от устройства'))
      }, timeoutMs)

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
      await hidDevice.value?.close()
    } catch (error) {
      statusText.value = error.message
    } finally {
      reader.value = null
      writer.value = null
      port.value = null
      hidDevice.value = null
      transport.value = null
      isConnected.value = false
      await refreshKnownPorts()

      if (!preserveStatus && !hasRememberedPort.value) {
        statusText.value = 'Устройство не подключено'
      }
    }
  }

  async function sendCommand(command, payload = new Uint8Array(), expected = [COMMANDS.ack], timeoutMs = 1500) {
    if (transport.value === 'hid') {
      if (!hidDevice.value?.opened) {
        throw new Error('Сначала подключите устройство')
      }

      const report = new Uint8Array(63)
      const requestFrame = buildFrame(command, payload)
      report.set(requestFrame)
      console.debug('[webhid] send', {
        command: `0x${command.toString(16)}`,
        expected: expected.map((item) => `0x${item.toString(16)}`),
        frame: toHex(requestFrame),
        report: toHex(report),
      })

      try {
        await hidDevice.value.sendFeatureReport(3, report)
        console.debug('[webhid] sendFeatureReport ok')
      } catch (error) {
        console.error('[webhid] sendFeatureReport failed', error)
        throw error
      }

      await delay(60)

      let response
      try {
        response = dataViewToUint8Array(await hidDevice.value.receiveFeatureReport(3))
        console.debug('[webhid] receiveFeatureReport ok', {
          length: response.length,
          response: toHex(response),
        })
      } catch (error) {
        console.error('[webhid] receiveFeatureReport failed', error)
        throw error
      }

      const parsed = parseFrames(response)
      console.debug('[webhid] parsed response', {
        frames: parsed.frames.map((candidate) => ({
          command: `0x${candidate.command.toString(16)}`,
          payload: toHex(candidate.payload),
        })),
        rest: toHex(parsed.rest),
      })
      const frame = parsed.frames.find((candidate) => expected.includes(candidate.command))

      if (!frame) {
        console.error('[webhid] no expected frame', {
          expected,
          response: toHex(response),
          parsed,
        })
        throw new Error('Неожиданный или пустой HID-ответ от устройства')
      }

      return frame
    }

    if (!writer.value) {
      throw new Error('Сначала подключите устройство')
    }

    const responsePromise = waitForFrame(expected, timeoutMs)
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

    if (frame.payload.length > 2) {
      productName.value = new TextDecoder().decode(frame.payload.slice(2))
    } else {
      productName.value = ''
    }

    statusText.value = 'Подключено'
  }

  async function fetchDeviceInfo() {
    try {
      const frame = await sendCommand(
        COMMANDS.getDeviceInfo,
        new Uint8Array(),
        [COMMANDS.deviceInfo, COMMANDS.error],
        500,
      )

      if (frame.command === COMMANDS.deviceInfo) {
        applyDeviceInfo(decodeDeviceInfo(frame.payload))
        return
      }
    } catch (error) {
      // Старая прошивка не знает CMD_GET_DEVICE_INFO либо не отвечает —
      // фолбэчимся на дефолты по deviceType.
    }

    const fallback = DEFAULT_DEVICE_INFO[deviceType.value] ?? DEFAULT_DEVICE_INFO[DEVICE_TYPES.oneShot]
    applyDeviceInfo(fallback)
  }

  async function connect() {
    if (!('serial' in navigator) && !('hid' in navigator)) {
      statusText.value = 'Конфигуратор работает только в Chromium-браузерах'
      return
    }

    try {
      isConnecting.value = true

      if (port.value || isConnected.value) {
        await disconnect()
      }

      if ('hid' in navigator) {
        console.debug('[webhid] requesting device')
        const devices = await navigator.hid.requestDevice({
          filters: [{ vendorId: 0x16c0, productId: 0x27e5, usagePage: 0xff00, usage: 0x01 }],
        })
        console.debug('[webhid] selected devices', devices.map((device) => ({
          productName: device.productName,
          vendorId: `0x${device.vendorId.toString(16)}`,
          productId: `0x${device.productId.toString(16)}`,
          opened: device.opened,
          collections: device.collections,
        })))

        if (devices.length > 0) {
          hidDevice.value = devices[0]
          try {
            await hidDevice.value.open()
            console.debug('[webhid] open ok', {
              productName: hidDevice.value.productName,
              opened: hidDevice.value.opened,
              collections: hidDevice.value.collections,
            })
          } catch (error) {
            console.error('[webhid] open failed', error)
            throw error
          }
          transport.value = 'hid'
          hasRememberedPort.value = true
          hasAvailablePort.value = true
          statusText.value = ''
          await verifyDevice()
          await fetchDeviceInfo()
          isConnected.value = true
          await refreshConfig()
          return
        }
      }

      port.value = await navigator.serial.requestPort({
        filters: [
          { usbVendorId: 0x2341 }, // Arduino
          { usbVendorId: 0x1B4F }, // SparkFun
          { usbVendorId: 0x239A }, // Adafruit nRF52
          { usbVendorId: 0x1A86 }, // CH340
          { usbVendorId: 0x10C4 }, // CP2102
          { usbVendorId: 0x303A }, // Espressif
        ],
      })
      await openPortWithRetry(port.value)
      reader.value = port.value.readable.getReader()
      writer.value = port.value.writable.getWriter()
      transport.value = 'serial'
      receiveBuffer.value = new Uint8Array()
      hasRememberedPort.value = true
      hasAvailablePort.value = true
      statusText.value = ''
      void readLoop()
      await verifyDevice()
      await fetchDeviceInfo()
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
    if (!('serial' in navigator) && !('hid' in navigator)) {
      statusText.value = 'Конфигуратор работает только в Хроме'
      return
    }

    if ('serial' in navigator) {
      navigator.serial.addEventListener('connect', handleSerialConnect)
      navigator.serial.addEventListener('disconnect', handleSerialDisconnect)
    }
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
    productName,
    resetConfig,
    saveConfig,
    statusText,
  }
}
