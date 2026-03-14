export const PROTOCOL_VERSION = 1
export const SERIAL_BAUD = 115200
export const FRAME_MAGIC = [0x4f, 0x53]

export const COMMANDS = {
  getConfig: 0x01,
  setConfig: 0x02,
  resetConfig: 0x03,
  ping: 0x04,
  config: 0x81,
  ack: 0x82,
  pong: 0x84,
  error: 0xff,
}

export const STATUS = {
  ok: 0x00,
  badPayload: 0x01,
  badCrc: 0x02,
  badCommand: 0x03,
}

export const MEDIA_KEY_OPTIONS = [
  { label: 'Play / Pause', value: 0x00cd },
  { label: 'Next Track', value: 0x00b5 },
  { label: 'Previous Track', value: 0x00b6 },
  { label: 'Stop', value: 0x00b7 },
  { label: 'Mute', value: 0x00e2 },
  { label: 'Volume Up', value: 0x00e9 },
  { label: 'Volume Down', value: 0x00ea },
  { label: 'Browser Home', value: 0x0223 },
  { label: 'Calculator', value: 0x0192 },
  { label: 'Search', value: 0x0221 },
]

export function crc8Update(crc, data) {
  let next = crc ^ data
  for (let index = 0; index < 8; index += 1) {
    next = (next & 0x80) !== 0 ? ((next << 1) ^ 0x07) & 0xff : (next << 1) & 0xff
  }
  return next
}

export function computeCrc(bytes) {
  return bytes.reduce((crc, byte) => crc8Update(crc, byte), 0)
}

export function buildFrame(command, payload = new Uint8Array()) {
  const header = new Uint8Array([PROTOCOL_VERSION, command, payload.length])
  const crc = computeCrc([...header, ...payload])
  return new Uint8Array([...FRAME_MAGIC, ...header, ...payload, crc])
}

export function encodeConfig(config) {
  const payload = new Uint8Array(10)
  const view = new DataView(payload.buffer)

  payload[0] = PROTOCOL_VERSION
  view.setUint16(1, config.singleTapCode, true)
  view.setUint16(3, config.doubleTapCode, true)
  view.setUint16(5, config.tripleTapCode, true)
  payload[7] = config.red
  payload[8] = config.green
  payload[9] = config.blue

  const crc = computeCrc(payload)
  return new Uint8Array([...payload, crc])
}

export function decodeConfig(payload) {
  if (payload.length !== 11) {
    throw new Error(`Unexpected config payload size: ${payload.length}`)
  }

  const raw = payload.slice(0, 10)
  const storedCrc = payload[10]
  const computed = computeCrc(raw)

  if (storedCrc !== computed) {
    throw new Error('Config CRC mismatch')
  }

  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength)
  return {
    version: raw[0],
    singleTapCode: view.getUint16(1, true),
    doubleTapCode: view.getUint16(3, true),
    tripleTapCode: view.getUint16(5, true),
    red: raw[7],
    green: raw[8],
    blue: raw[9],
  }
}

export function parseFrames(buffer) {
  const frames = []
  let offset = 0

  while (offset <= buffer.length - 6) {
    if (buffer[offset] !== FRAME_MAGIC[0] || buffer[offset + 1] !== FRAME_MAGIC[1]) {
      offset += 1
      continue
    }

    const version = buffer[offset + 2]
    const command = buffer[offset + 3]
    const payloadLength = buffer[offset + 4]
    const frameLength = 2 + 3 + payloadLength + 1

    if (offset + frameLength > buffer.length) {
      break
    }

    const frame = buffer.slice(offset, offset + frameLength)
    const payload = frame.slice(5, 5 + payloadLength)
    const expectedCrc = frame[frame.length - 1]
    const actualCrc = computeCrc(frame.slice(2, frame.length - 1))

    if (expectedCrc === actualCrc) {
      frames.push({ version, command, payload })
      offset += frameLength
      continue
    }

    offset += 1
  }

  return {
    frames,
    rest: buffer.slice(offset),
  }
}

export function toHexCode(value) {
  return `0x${value.toString(16).toUpperCase().padStart(4, '0')}`
}
