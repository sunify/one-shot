export const PROTOCOL_VERSION = 1
export const SERIAL_BAUD = 115200
export const FRAME_MAGIC = [0x4f, 0x53]

export const COMMANDS = {
  getConfig: 0x01,
  setConfig: 0x02,
  resetConfig: 0x03,
  ping: 0x04,
  getDeviceInfo: 0x05,
  getDeviceOptions: 0x06,
  setDeviceOptions: 0x07,
  config: 0x81,
  ack: 0x82,
  pong: 0x84,
  deviceInfo: 0x85,
  deviceOptions: 0x86,
  buttonEvent: 0x90,
  error: 0xff,
}

export const BUTTON_EVENT_STATE = {
  released: 0x00,
  pressed: 0x01,
}

export const DEVICE_CAPABILITIES = {
  turboMode: 0x0001,
}

export const DEVICE_OPTION_FLAGS = {
  turboMode: 0x0001,
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
  mouse: 0x03,
}

export const ANIMATION_MODES = {
  static: 0,
  breathing: 1,
  rainbow: 2,
}

export const THIRD_ACTION_TRIGGERS = {
  tripleTap: 0,
  longPress: 1,
}

export const ANIMATION_MODE_OPTIONS = [
  { label: 'Без анимации', value: ANIMATION_MODES.static },
  { label: 'Дыхание', value: ANIMATION_MODES.breathing },
  { label: 'Радуга', value: ANIMATION_MODES.rainbow },
]

export const SLEEP_TIMEOUT_OPTIONS = [
  { label: 'Не выключать', value: 0 },
  { label: 'Через 1 час', value: 1 },
  { label: 'Через 3 часа', value: 3 },
  { label: 'Через 5 часов', value: 5 },
]

export const MOUSE_AXES = {
  scroll: 0x00,
  moveX: 0x01,
  moveY: 0x02,
}

export const MOUSE_OPTIONS = [
  { label: 'Скролл', value: MOUSE_AXES.scroll },
  { label: 'Движение мыши X', value: MOUSE_AXES.moveX },
  { label: 'Движение мыши Y', value: MOUSE_AXES.moveY },
]

export const DEVICE_TYPES = {
  oneShot: 0x01,
  magicButton: 0x02,
  bebop: 0x03,
}

export const MODIFIERS = {
  ctrl: 0x01,
  shift: 0x02,
  alt: 0x04,
  meta: 0x08,
}

export const MEDIA_KEY_OPTIONS = [
  { label: 'Плэй / Пауза', value: 0x00cd },
  { label: 'Следующий трек', value: 0x00b5 },
  { label: 'Предыдущий трек', value: 0x00b6 },
  { label: 'Мьют', value: 0x00e2 },
  { label: 'Громче', value: 0x00e9 },
  { label: 'Тише', value: 0x00ea },
  { label: 'Поиск', value: 0x0221 },
]

export const FUNCTION_KEY_OPTIONS = [
  { label: 'F1', code: 0x3a },
  { label: 'F2', code: 0x3b },
  { label: 'F3', code: 0x3c },
  { label: 'F4', code: 0x3d },
  { label: 'F5', code: 0x3e },
  { label: 'F6', code: 0x3f },
  { label: 'F7', code: 0x40 },
  { label: 'F8', code: 0x41 },
  { label: 'F9', code: 0x42 },
  { label: 'F10', code: 0x43 },
  { label: 'F11', code: 0x44 },
  { label: 'F12', code: 0x45 },
  { label: 'F13', code: 0x68 },
  { label: 'F14', code: 0x69 },
  { label: 'F15', code: 0x6a },
]

export const HOTKEY_KEY_OPTIONS = [
  { label: 'Tab', code: 0x2B },
  { label: 'Esc', code: 0x29 },
  { label: 'Enter', code: 0x28 },
  { label: 'Backspace', code: 0x2a },
  ...'1234567890'.split('').map((label, index) => ({
    label,
    code: 0x1e + index,
  })),
  ...'ABCDEFGHIJKLMNOPQRSTUVWXYZ'.split('').map((label, index) => ({
    label,
    code: 0x04 + index,
  })),
  ...FUNCTION_KEY_OPTIONS,
]

export const HOTKEY_SELECT_VALUE = 'hotkey'
export const MOUSE_SELECT_VALUE = 'mouse'
export const MODIFIER_OPTIONS = [
  { label: 'Meta', value: String(MODIFIERS.meta) },
  { label: 'Ctrl', value: String(MODIFIERS.ctrl) },
  { label: 'Shift', value: String(MODIFIERS.shift) },
  { label: 'Alt', value: String(MODIFIERS.alt) },
]

const KEY_ALIASES = {
  ArrowUp: 'Вверх',
  ArrowDown: 'Вниз',
  ArrowLeft: 'Влево',
  ArrowRight: 'Вправо',
  Escape: 'Esc',
  Backspace: 'Backspace',
  Delete: 'Delete',
  Enter: 'Enter',
  Space: 'Пробел',
  Tab: 'Tab',
  PageUp: 'PgUp',
  PageDown: 'PgDn',
  PrintScreen: 'Print Screen',
  ScrollLock: 'Scroll Lock',
  CapsLock: 'Caps Lock',
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

const CHAR_TO_HOTKEY = {
  a: { code: 0x04, shift: false },
  b: { code: 0x05, shift: false },
  c: { code: 0x06, shift: false },
  d: { code: 0x07, shift: false },
  e: { code: 0x08, shift: false },
  f: { code: 0x09, shift: false },
  g: { code: 0x0a, shift: false },
  h: { code: 0x0b, shift: false },
  i: { code: 0x0c, shift: false },
  j: { code: 0x0d, shift: false },
  k: { code: 0x0e, shift: false },
  l: { code: 0x0f, shift: false },
  m: { code: 0x10, shift: false },
  n: { code: 0x11, shift: false },
  o: { code: 0x12, shift: false },
  p: { code: 0x13, shift: false },
  q: { code: 0x14, shift: false },
  r: { code: 0x15, shift: false },
  s: { code: 0x16, shift: false },
  t: { code: 0x17, shift: false },
  u: { code: 0x18, shift: false },
  v: { code: 0x19, shift: false },
  w: { code: 0x1a, shift: false },
  x: { code: 0x1b, shift: false },
  y: { code: 0x1c, shift: false },
  z: { code: 0x1d, shift: false },
  A: { code: 0x04, shift: true },
  B: { code: 0x05, shift: true },
  C: { code: 0x06, shift: true },
  D: { code: 0x07, shift: true },
  E: { code: 0x08, shift: true },
  F: { code: 0x09, shift: true },
  G: { code: 0x0a, shift: true },
  H: { code: 0x0b, shift: true },
  I: { code: 0x0c, shift: true },
  J: { code: 0x0d, shift: true },
  K: { code: 0x0e, shift: true },
  L: { code: 0x0f, shift: true },
  M: { code: 0x10, shift: true },
  N: { code: 0x11, shift: true },
  O: { code: 0x12, shift: true },
  P: { code: 0x13, shift: true },
  Q: { code: 0x14, shift: true },
  R: { code: 0x15, shift: true },
  S: { code: 0x16, shift: true },
  T: { code: 0x17, shift: true },
  U: { code: 0x18, shift: true },
  V: { code: 0x19, shift: true },
  W: { code: 0x1a, shift: true },
  X: { code: 0x1b, shift: true },
  Y: { code: 0x1c, shift: true },
  Z: { code: 0x1d, shift: true },
  1: { code: 0x1e, shift: false },
  2: { code: 0x1f, shift: false },
  3: { code: 0x20, shift: false },
  4: { code: 0x21, shift: false },
  5: { code: 0x22, shift: false },
  6: { code: 0x23, shift: false },
  7: { code: 0x24, shift: false },
  8: { code: 0x25, shift: false },
  9: { code: 0x26, shift: false },
  0: { code: 0x27, shift: false },
  '[': { code: 0x2f, shift: false },
  ']': { code: 0x30, shift: false },
  '\\': { code: 0x31, shift: false },
  ';': { code: 0x33, shift: false },
  "'": { code: 0x34, shift: false },
  '/': { code: 0x38, shift: false },
  ' ': { code: 0x2c, shift: false },
}

const HOTKEY_DISPLAY_BY_CODE = {
  '44:0': ' ',
  '45:0': '-',
  '45:1': '_',
  '46:0': '=',
  '46:1': '+',
  '47:0': '[',
  '47:1': '{',
  '48:0': ']',
  '48:1': '}',
  '49:0': '\\',
  '49:1': '|',
  '51:0': ';',
  '51:1': ':',
  '52:0': "'",
  '52:1': '"',
  '53:0': '`',
  '53:1': '~',
  '54:0': ',',
  '54:1': '<',
  '55:0': '.',
  '55:1': '>',
  '56:0': '/',
  '56:1': '?',
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

function writeConfigField(view, field, value) {
  if (field.type === 'gesture') {
    writeGesture(view, field.offset, value)
    return
  }

  view.setUint8(field.offset, value ?? field.defaultValue ?? 0)
}

function readConfigField(view, field) {
  if (field.type === 'gesture') {
    return readGesture(view, field.offset)
  }

  return view.getUint8(field.offset)
}

export function encodeConfig(config, deviceDefinition) {
  const layout = deviceDefinition?.configLayouts.find((candidate) => candidate.when?.(config) ?? true)
  if (!layout) {
    throw new Error(`No config layout for device type ${config.deviceType}`)
  }

  const payload = new Uint8Array(layout.payloadLength - 1)
  const view = new DataView(payload.buffer)
  payload[0] = layout.version
  for (const field of layout.fields) {
    writeConfigField(view, field, config[field.key])
  }

  const crc = computeCrc(payload)
  return new Uint8Array([...payload, crc])
}

export function decodeConfig(payload, deviceDefinition) {
  const layout = deviceDefinition?.configLayouts.find((candidate) => candidate.payloadLength === payload.length)
  if (!layout) {
    throw new Error(`Unexpected config payload size: ${payload.length}`)
  }

  const raw = payload.slice(0, -1)
  const storedCrc = payload[payload.length - 1]
  const computed = computeCrc(raw)

  if (storedCrc !== computed) {
    throw new Error('Config CRC mismatch')
  }

  const view = new DataView(raw.buffer, raw.byteOffset, raw.byteLength)
  const config = {
    version: raw[0],
    deviceType: deviceDefinition.type,
  }
  for (const field of layout.fields) {
    config[field.key] = readConfigField(view, field)
  }

  return config
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

export function formatMouseAction(gesture) {
  const axis = gesture.code & 0xff
  const amount = (gesture.code >> 8) & 0xff
  const option = MOUSE_OPTIONS.find((o) => o.value === axis)
  const label = option?.label ?? 'Мышь'

  const parts = []
  if (gesture.modifiers & MODIFIERS.meta) parts.push('⌘')
  if (gesture.modifiers & MODIFIERS.ctrl) parts.push('Ctrl')
  if (gesture.modifiers & MODIFIERS.shift) parts.push('Shift')
  if (gesture.modifiers & MODIFIERS.alt) parts.push('Alt')
  parts.push(`${label} ×${amount}`)
  return parts.join('\u2009+\u2009')
}

export function formatHotkey(gesture) {
  if (!gesture?.code) {
    return 'Выберите клавишу'
  }

  const parts = []
  if (gesture.modifiers & MODIFIERS.meta) parts.push('⌘')
  if (gesture.modifiers & MODIFIERS.ctrl) parts.push('Ctrl')
  if (gesture.modifiers & MODIFIERS.shift) parts.push('Shift')
  if (gesture.modifiers & MODIFIERS.alt) parts.push('Alt')

  parts.push(hotkeyCharFromCode(gesture.code, gesture.modifiers) || KEY_ALIASES[HID_LABELS[gesture.code]] || HID_LABELS[gesture.code] || toHexCode(gesture.code))
  return parts.join('\u2009+\u2009')
}

export function hotkeyCharFromCode(code, modifiers = 0) {
  const shiftEnabled = (modifiers & MODIFIERS.shift) !== 0
  const special = HOTKEY_DISPLAY_BY_CODE[`${code}:${shiftEnabled ? 1 : 0}`]
  if (special) {
    return special
  }

  if (code >= 0x04 && code <= 0x1d) {
    return String.fromCharCode(65 + (code - 0x04))
  }

  const label = HID_LABELS[code]
  return label && label.length === 1 ? label : ''
}

export function hotkeyFromChar(value, modifiers = 0) {
  if (!value) {
    return { code: 0, modifiers }
  }

  const hotkey = CHAR_TO_HOTKEY[value]
  if (!hotkey) {
    return null
  }

  return {
    code: hotkey.code,
    modifiers,
  }
}

export function isEditableCharHotkey(gesture) {
  return gesture?.type === ACTION_TYPES.hotkey
}

export function hotkeySelectLabel(code, modifiers = 0) {
  const specialOption = HOTKEY_KEY_OPTIONS.find((option) => option.code === code)
  if (specialOption) {
    return specialOption.label
  }

  return hotkeyCharFromCode(code, modifiers) || ''
}

function rgbToHex(r, g, b) {
  return `#${[r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('')}`
}

export function decodeDeviceInfo(payload) {
  if (payload.length < 13) {
    throw new Error(`Unexpected device info payload size: ${payload.length}`)
  }

  const shadeEnabled = payload.length >= 14 ? payload[13] !== 0 : true

  return {
    numLeds: payload[0],
    keycap: rgbToHex(payload[1], payload[2], payload[3]),
    topCase: rgbToHex(payload[4], payload[5], payload[6]),
    topCaseShade: shadeEnabled ? rgbToHex(payload[7], payload[8], payload[9]) : 'transparent',
    bottomCase: rgbToHex(payload[10], payload[11], payload[12]),
    thirdActionTrigger: payload[14] ?? THIRD_ACTION_TRIGGERS.tripleTap,
    capabilities: payload.length >= 17 ? payload[15] | (payload[16] << 8) : 0,
  }
}

export function encodeDeviceOptions(options) {
  const flags = options.turboMode ? DEVICE_OPTION_FLAGS.turboMode : 0
  return new Uint8Array([1, flags & 0xff, (flags >> 8) & 0xff])
}

export function decodeDeviceOptions(payload) {
  if (payload.length < 3 || payload[0] !== 1) {
    throw new Error(`Unexpected device options payload: ${[...payload].join(',')}`)
  }

  const flags = payload[1] | (payload[2] << 8)
  return {
    turboMode: (flags & DEVICE_OPTION_FLAGS.turboMode) !== 0,
  }
}
