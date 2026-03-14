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

export const ACTION_TYPES = {
  consumer: 0x01,
  hotkey: 0x02,
}

export const MODIFIERS = {
  ctrl: 0x01,
  shift: 0x02,
  alt: 0x04,
  meta: 0x08,
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

export const HOTKEY_SELECT_VALUE = 'hotkey'
export const MODIFIER_OPTIONS = [
  { label: 'Meta', value: String(MODIFIERS.meta) },
  { label: 'Ctrl', value: String(MODIFIERS.ctrl) },
  { label: 'Shift', value: String(MODIFIERS.shift) },
  { label: 'Alt', value: String(MODIFIERS.alt) },
]

const KEY_ALIASES = {
  ArrowUp: 'Up',
  ArrowDown: 'Down',
  ArrowLeft: 'Left',
  ArrowRight: 'Right',
  Escape: 'Esc',
  Backspace: 'Backspace',
  Delete: 'Delete',
  Enter: 'Enter',
  Space: 'Space',
  Tab: 'Tab',
}

const CODE_TO_HID = {
  KeyA: 0x04,
  KeyB: 0x05,
  KeyC: 0x06,
  KeyD: 0x07,
  KeyE: 0x08,
  KeyF: 0x09,
  KeyG: 0x0a,
  KeyH: 0x0b,
  KeyI: 0x0c,
  KeyJ: 0x0d,
  KeyK: 0x0e,
  KeyL: 0x0f,
  KeyM: 0x10,
  KeyN: 0x11,
  KeyO: 0x12,
  KeyP: 0x13,
  KeyQ: 0x14,
  KeyR: 0x15,
  KeyS: 0x16,
  KeyT: 0x17,
  KeyU: 0x18,
  KeyV: 0x19,
  KeyW: 0x1a,
  KeyX: 0x1b,
  KeyY: 0x1c,
  KeyZ: 0x1d,
  Digit1: 0x1e,
  Digit2: 0x1f,
  Digit3: 0x20,
  Digit4: 0x21,
  Digit5: 0x22,
  Digit6: 0x23,
  Digit7: 0x24,
  Digit8: 0x25,
  Digit9: 0x26,
  Digit0: 0x27,
  Enter: 0x28,
  Escape: 0x29,
  Backspace: 0x2a,
  Tab: 0x2b,
  Space: 0x2c,
  Minus: 0x2d,
  Equal: 0x2e,
  BracketLeft: 0x2f,
  BracketRight: 0x30,
  Backslash: 0x31,
  Semicolon: 0x33,
  Quote: 0x34,
  Backquote: 0x35,
  Comma: 0x36,
  Period: 0x37,
  Slash: 0x38,
  CapsLock: 0x39,
  F1: 0x3a,
  F2: 0x3b,
  F3: 0x3c,
  F4: 0x3d,
  F5: 0x3e,
  F6: 0x3f,
  F7: 0x40,
  F8: 0x41,
  F9: 0x42,
  F10: 0x43,
  F11: 0x44,
  F12: 0x45,
  PrintScreen: 0x46,
  ScrollLock: 0x47,
  Pause: 0x48,
  Insert: 0x49,
  Home: 0x4a,
  PageUp: 0x4b,
  Delete: 0x4c,
  End: 0x4d,
  PageDown: 0x4e,
  ArrowRight: 0x4f,
  ArrowLeft: 0x50,
  ArrowDown: 0x51,
  ArrowUp: 0x52,
}

const HID_LABELS = Object.fromEntries(
  Object.entries(CODE_TO_HID).map(([code, hid]) => [hid, KEY_ALIASES[code] ?? code.replace(/^Key|^Digit/, '')]),
)

const CHAR_TO_HID = {
  a: 0x04,
  b: 0x05,
  c: 0x06,
  d: 0x07,
  e: 0x08,
  f: 0x09,
  g: 0x0a,
  h: 0x0b,
  i: 0x0c,
  j: 0x0d,
  k: 0x0e,
  l: 0x0f,
  m: 0x10,
  n: 0x11,
  o: 0x12,
  p: 0x13,
  q: 0x14,
  r: 0x15,
  s: 0x16,
  t: 0x17,
  u: 0x18,
  v: 0x19,
  w: 0x1a,
  x: 0x1b,
  y: 0x1c,
  z: 0x1d,
  1: 0x1e,
  2: 0x1f,
  3: 0x20,
  4: 0x21,
  5: 0x22,
  6: 0x23,
  7: 0x24,
  8: 0x25,
  9: 0x26,
  0: 0x27,
}

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

function writeGesture(view, offset, gesture) {
  view.setUint8(offset, gesture.type)
  view.setUint16(offset + 1, gesture.code, true)
  view.setUint8(offset + 3, gesture.modifiers)
}

function readGesture(view, offset) {
  return {
    type: view.getUint8(offset),
    code: view.getUint16(offset + 1, true),
    modifiers: view.getUint8(offset + 3),
  }
}

export function encodeConfig(config) {
  const payload = new Uint8Array(16)
  const view = new DataView(payload.buffer)

  payload[0] = 2
  writeGesture(view, 1, config.singleTap)
  writeGesture(view, 5, config.doubleTap)
  writeGesture(view, 9, config.tripleTap)
  payload[13] = config.red
  payload[14] = config.green
  payload[15] = config.blue

  const crc = computeCrc(payload)
  return new Uint8Array([...payload, crc])
}

export function decodeConfig(payload) {
  if (payload.length !== 17) {
    throw new Error(`Unexpected config payload size: ${payload.length}`)
  }

  const raw = payload.slice(0, 16)
  const storedCrc = payload[16]
  const computed = computeCrc(raw)

  if (storedCrc !== computed) {
    throw new Error('Config CRC mismatch')
  }

  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength)
  return {
    version: raw[0],
    singleTap: readGesture(view, 1),
    doubleTap: readGesture(view, 5),
    tripleTap: readGesture(view, 9),
    red: raw[13],
    green: raw[14],
    blue: raw[15],
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

export function formatHotkey(gesture) {
  if (!gesture?.code) {
    return 'Select a key'
  }

  const parts = []
  if (gesture.modifiers & MODIFIERS.ctrl) parts.push('Ctrl')
  if (gesture.modifiers & MODIFIERS.shift) parts.push('Shift')
  if (gesture.modifiers & MODIFIERS.alt) parts.push('Alt')
  if (gesture.modifiers & MODIFIERS.meta) parts.push('Meta')

  parts.push(KEY_ALIASES[HID_LABELS[gesture.code]] ?? HID_LABELS[gesture.code] ?? toHexCode(gesture.code))
  return parts.join('+')
}

export function hotkeyCharFromCode(code) {
  const label = HID_LABELS[code]
  return label && label.length === 1 ? label : ''
}

export function hotkeyCodeFromChar(value) {
  if (!value) {
    return 0
  }

  return CHAR_TO_HID[value.toLowerCase()] ?? 0
}
