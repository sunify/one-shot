import OneShotPreview from '../components/OneShotPreview.vue'
import BebopPreview from '../components/BebopPreview.vue'
import RrrrawPreview from '../components/RrrrawPreview.vue'
import {
  ACTION_TYPES,
  DEVICE_TYPES,
  THIRD_ACTION_TRIGGERS,
} from '../protocol'

const defaultGestures = {
  singleTap: { type: ACTION_TYPES.consumer, code: 0x00cd, modifiers: 0 },
  doubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
  tripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
}

const buttonBindings = [
  {
    key: 'singleTap',
    label: ({ form }) => form.turboMode ? 'Нажатие' : 'Одиночное нажатие',
    animation: { type: 'tap', count: 1 },
  },
  {
    key: 'doubleTap',
    label: 'Двойное нажатие',
    animation: { type: 'tap', count: 2 },
    when: ({ form }) => !form.turboMode,
  },
  {
    key: 'tripleTap',
    label: ({ thirdActionTrigger }) => thirdActionTrigger === THIRD_ACTION_TRIGGERS.longPress
      ? 'Долгое нажатие'
      : 'Тройное нажатие',
    animation: ({ thirdActionTrigger }) => thirdActionTrigger === THIRD_ACTION_TRIGGERS.longPress
      ? { type: 'press' }
      : { type: 'tap', count: 3 },
    when: ({ form }) => !form.turboMode,
  },
]

function prefixedButtonBindings(prefix) {
  return buttonBindings.map((binding) => ({
    ...binding,
    key: `${prefix}${binding.key[0].toUpperCase()}${binding.key.slice(1)}`,
  }))
}

function pressBindings(prefix) {
  return [
    {
      key: `${prefix}Single`,
      label: 'Нажатие',
      animation: { type: 'tap', count: 1 },
      capabilities: ['modifierHold'],
    },
    {
      key: `${prefix}Long`,
      label: 'Долгое нажатие',
      animation: { type: 'press' },
      capabilities: ['modifierHold'],
    },
  ]
}

const gesture = (key, offset) => ({ key, offset, type: 'gesture' })
const byte = (key, offset, defaultValue = 0) => ({ key, offset, type: 'u8', defaultValue })

const oneShotBaseFields = [
  gesture('singleTap', 1),
  gesture('doubleTap', 5),
  gesture('tripleTap', 9),
  byte('red', 13),
  byte('green', 14),
  byte('blue', 15),
  byte('animationMode', 16),
  byte('sleepTimeout', 17),
]

const commonDefaultInfo = {
  capabilities: 0,
}

export const DEVICE_DEFINITIONS = {
  [DEVICE_TYPES.oneShot]: {
    type: DEVICE_TYPES.oneShot,
    name: 'One Shot',
    preview: OneShotPreview,
    controls: [
      {
        id: 'main',
        type: 'button',
        label: 'Кнопка',
        bindings: buttonBindings,
      },
      {
        id: 'encoder',
        type: 'encoder',
        label: 'Энкодер',
        when: ({ form }) => form.encoderCW != null,
        bindings: [
          { key: 'encoderCW', label: 'По часовой стрелке', capabilities: ['mouse'] },
          { key: 'encoderCCW', label: 'Против часовой стрелки', capabilities: ['mouse'] },
        ],
      },
    ],
    configLayouts: [
      {
        payloadLength: 19,
        version: 6,
        fields: oneShotBaseFields,
        when: (config) => config.encoderCW == null,
      },
      {
        payloadLength: 28,
        version: 6,
        fields: [
          ...oneShotBaseFields,
          gesture('encoderCW', 18),
          gesture('encoderCCW', 22),
          byte('encoderSensitivity', 26),
        ],
        when: (config) => config.encoderCW != null,
      },
    ],
    defaults: {
      ...defaultGestures,
      red: 250,
      green: 255,
      blue: 210,
      animationMode: 1,
      sleepTimeout: 0,
      turboMode: false,
    },
    defaultInfo: {
      ...commonDefaultInfo,
      numLeds: 1,
      keycap: '#ffffff',
      topCase: '#ffffff',
      topCaseShade: '#cf00ff',
      bottomCase: '#ffffff',
      thirdActionTrigger: THIRD_ACTION_TRIGGERS.tripleTap,
    },
  },
  [DEVICE_TYPES.magicButton]: {
    type: DEVICE_TYPES.magicButton,
    name: 'Волшебная кнопка',
    preview: OneShotPreview,
    controls: [
      {
        id: 'main',
        type: 'button',
        label: 'Кнопка',
        bindings: buttonBindings,
      },
    ],
    configLayouts: [
      {
        payloadLength: 14,
        version: 1,
        fields: [
          gesture('singleTap', 1),
          gesture('doubleTap', 5),
          gesture('tripleTap', 9),
        ],
      },
    ],
    defaults: {
      ...defaultGestures,
      turboMode: false,
    },
    defaultInfo: {
      ...commonDefaultInfo,
      numLeds: 0,
      keycap: '#5ab9cf',
      topCase: '#ffffff',
      topCaseShade: '#ffffff',
      bottomCase: '#ffffff',
      thirdActionTrigger: THIRD_ACTION_TRIGGERS.longPress,
    },
  },
  [DEVICE_TYPES.bebop]: {
    type: DEVICE_TYPES.bebop,
    name: 'Bebop',
    preview: BebopPreview,
    controls: [
      {
        id: 'right',
        protocolId: 1,
        previewId: 'left',
        type: 'button',
        label: 'Peppa',
        bindings: prefixedButtonBindings('right'),
      },
      {
        id: 'left',
        protocolId: 0,
        previewId: 'right',
        type: 'button',
        label: 'Bebop',
        bindings: prefixedButtonBindings('left'),
      },
    ],
    configLayouts: [
      {
        payloadLength: 31,
        version: 1,
        fields: [
          gesture('leftSingleTap', 1),
          gesture('leftDoubleTap', 5),
          gesture('leftTripleTap', 9),
          gesture('rightSingleTap', 13),
          gesture('rightDoubleTap', 17),
          gesture('rightTripleTap', 21),
          byte('red', 25),
          byte('green', 26),
          byte('blue', 27),
          byte('animationMode', 28),
          byte('sleepTimeout', 29),
        ],
      },
    ],
    defaults: {
      leftSingleTap: { type: ACTION_TYPES.hotkey, code: 0x05, modifiers: 0 },
      leftDoubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
      leftTripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
      rightSingleTap: { type: ACTION_TYPES.hotkey, code: 0x13, modifiers: 0 },
      rightDoubleTap: { type: ACTION_TYPES.consumer, code: 0x00b5, modifiers: 0 },
      rightTripleTap: { type: ACTION_TYPES.consumer, code: 0x00b6, modifiers: 0 },
      red: 250,
      green: 255,
      blue: 210,
      animationMode: 1,
      sleepTimeout: 0,
      turboMode: false,
    },
    defaultInfo: {
      ...commonDefaultInfo,
      numLeds: 0,
      keycap: '#ffffff',
      topCase: '#ffffff',
      topCaseShade: '#cf00ff',
      bottomCase: '#ffffff',
      thirdActionTrigger: THIRD_ACTION_TRIGGERS.tripleTap,
    },
  },
  [DEVICE_TYPES.rrrraw]: {
    type: DEVICE_TYPES.rrrraw,
    name: 'rrrraw',
    configuratorTitle: 'Конфигуратор<br />Krutilki',
    preview: RrrrawPreview,
    previewWidth: 562.5,
    controls: [
      {
        id: 'button1',
        protocolId: 0,
        type: 'button',
        label: 'Кнопка 1',
        bindings: pressBindings('button1'),
      },
      {
        id: 'button2',
        protocolId: 1,
        type: 'button',
        label: 'Кнопка 2',
        bindings: pressBindings('button2'),
      },
      {
        id: 'button3',
        protocolId: 2,
        type: 'button',
        label: 'Кнопка 3',
        bindings: pressBindings('button3'),
      },
      {
        id: 'encoder',
        protocolId: 3,
        type: 'encoder',
        label: 'Энкодер',
        bindings: [
          ...pressBindings('encoderPress'),
          {
            key: 'encoderCCW',
            label: 'По часовой стрелке',
            animation: { type: 'rotate', direction: 'cw' },
            capabilities: ['mouse'],
          },
          {
            key: 'encoderCW',
            label: 'Против часовой стрелки',
            animation: { type: 'rotate', direction: 'ccw' },
            capabilities: ['mouse'],
          },
        ],
      },
    ],
    configLayouts: [
      {
        payloadLength: 42,
        version: 2,
        fields: [
          gesture('button1Single', 1),
          gesture('button1Long', 5),
          gesture('button2Single', 9),
          gesture('button2Long', 13),
          gesture('button3Single', 17),
          gesture('button3Long', 21),
          gesture('encoderPressSingle', 25),
          gesture('encoderPressLong', 29),
          gesture('encoderCW', 33),
          gesture('encoderCCW', 37),
        ],
      },
    ],
    defaults: {
      button1Single: { type: ACTION_TYPES.hotkey, code: 0x1e, modifiers: 0 },
      button1Long: { type: ACTION_TYPES.hotkey, code: 0x1f, modifiers: 0 },
      button2Single: { type: ACTION_TYPES.hotkey, code: 0x20, modifiers: 0 },
      button2Long: { type: ACTION_TYPES.hotkey, code: 0x21, modifiers: 0 },
      button3Single: { type: ACTION_TYPES.hotkey, code: 0x22, modifiers: 0 },
      button3Long: { type: ACTION_TYPES.hotkey, code: 0x23, modifiers: 0 },
      encoderPressSingle: { type: ACTION_TYPES.hotkey, code: 0x24, modifiers: 0 },
      encoderPressLong: { type: ACTION_TYPES.hotkey, code: 0x25, modifiers: 0 },
      encoderCW: { type: ACTION_TYPES.hotkey, code: 0x26, modifiers: 0 },
      encoderCCW: { type: ACTION_TYPES.hotkey, code: 0x27, modifiers: 0 },
      turboMode: false,
    },
    defaultInfo: {
      ...commonDefaultInfo,
      numLeds: 0,
      keycap: '#ffffff',
      topCase: '#dedede',
      topCaseShade: '#999999',
      bottomCase: '#777777',
      thirdActionTrigger: THIRD_ACTION_TRIGGERS.longPress,
    },
  },
}

export const DEFAULT_DEVICE_DEFINITION = DEVICE_DEFINITIONS[DEVICE_TYPES.oneShot]

export function getDeviceDefinition(deviceType) {
  return DEVICE_DEFINITIONS[deviceType] ?? DEFAULT_DEVICE_DEFINITION
}

export function hasDeviceDefinition(deviceType) {
  return DEVICE_DEFINITIONS[deviceType] != null
}
