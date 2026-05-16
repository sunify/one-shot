# Device Info Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Расширить протокол устройства командой `CMD_GET_DEVICE_INFO`, чтобы конфигуратор знал `num_leds` и цвета корпуса/кейкапа из JSON-профиля; скрыть настройки подсветки при `num_leds == 0`. Старая прошивка и старый конфигуратор продолжают работать без изменений.

**Architecture:** Опциональная команда `0x05` → ответ `0x85` с 13-байтовым payload (`num_leds` + 4×RGB). Прошивка собирает payload из compile-time `#define`-ов; build-скрипт `firmware.sh` парсит блок `colors` из профиля и пробрасывает `-D` флаги. Конфигуратор после `PING` шлёт новую команду с коротким таймаутом и фолбэчится на дефолты по `deviceType` при ошибке/таймауте.

**Tech Stack:** Arduino (`firmware/one-shot`), C++ Arduino library `device_protocol`, Vue 3 + Vite (конфигуратор), `jq` (build-скрипт), Web Serial API.

**Note:** В проекте нет автоматических тестов. Вместо TDD верификация = (1) `npm run firmware:compile` для прошивки, (2) `npm run dev` + ручная проверка в Chrome для UI. Для функций конфигуратора, у которых есть детерминированный ввод/вывод (декодер payload), пишем минимальный inline-чек через `console.assert` в dev-консоли.

---

## File Structure

**Modify:**
- `libraries/device_protocol/src/device_protocol.h` — добавить command-коды `CMD_GET_DEVICE_INFO`, `CMD_DEVICE_INFO`.
- `firmware/one-shot/one-shot.ino` — обработка новой команды + дефолты дефайнов цветов.
- `scripts/firmware.sh` — парсинг `colors` из профиля, проброс `-D` флагов.
- `profiles/0-led.json` — добавить `colors` под нужды 0-led сборки.
- `src/protocol.js` — константы, `decodeDeviceInfo`, `DEFAULT_DEVICE_INFO`.
- `src/composables/useConfiguratorState.js` — поля `numLeds`, `caseColors`, новый `supportsLighting`, `applyDeviceInfo`.
- `src/composables/useDeviceConnection.js` — `fetchDeviceInfo` после `verifyDevice` с таймаутом и фолбэком; параметр `timeoutMs` в `waitForFrame`.
- `src/composables/useLightingPreview.js` — чтение цветов из state вместо хардкодов.
- `src/components/DevicePreview.vue` — заменить `fill:#fff` нижней части на `var(--bottom-color)`.
- `package.json` — версия `2.1.0`.

**Не трогаем:**
- `firmware/magic-button/magic-button.ino` — фолбэк через `STATUS_BAD_COMMAND` уже работает.
- `profiles/3-led.json`, `profiles/default.json`, `profiles/rotary.json` — дефолты в `.ino` совпадают с текущим визуалом one-shot, добавлять `colors` опционально (можно потом).

---

## Task 1: Расширить заголовок протокола новыми командами

**Files:**
- Modify: `libraries/device_protocol/src/device_protocol.h:22-32`

- [ ] **Step 1: Добавить command-коды**

В enum `Command` добавить две новые записи (между `CMD_PING = 0x04` и `CMD_CONFIG = 0x81`):

```cpp
enum Command : uint8_t {
  CMD_GET_CONFIG = 0x01,
  CMD_SET_CONFIG = 0x02,
  CMD_RESET_CONFIG = 0x03,
  CMD_PING = 0x04,
  CMD_GET_DEVICE_INFO = 0x05,
  CMD_CONFIG = 0x81,
  CMD_ACK = 0x82,
  CMD_PONG = 0x84,
  CMD_DEVICE_INFO = 0x85,
  CMD_BUTTON_EVENT = 0x90,
  CMD_ERROR = 0xFF
};
```

- [ ] **Step 2: Закоммитить**

```bash
git add libraries/device_protocol/src/device_protocol.h
git commit -m "Add CMD_GET_DEVICE_INFO/CMD_DEVICE_INFO to device protocol"
```

---

## Task 2: Прошивка one-shot — дефолты цветов и хендлер новой команды

**Files:**
- Modify: `firmware/one-shot/one-shot.ino`

- [ ] **Step 1: Добавить дефайны цветов с дефолтами**

После блока `#ifndef NUM_LEDS … #endif` (around `firmware/one-shot/one-shot.ino:16-18`) добавить новый блок:

```cpp
#ifndef KEYCAP_R
#define KEYCAP_R 0xFF
#endif
#ifndef KEYCAP_G
#define KEYCAP_G 0xFF
#endif
#ifndef KEYCAP_B
#define KEYCAP_B 0xFF
#endif

#ifndef TOP_CASE_R
#define TOP_CASE_R 0xFF
#endif
#ifndef TOP_CASE_G
#define TOP_CASE_G 0xFF
#endif
#ifndef TOP_CASE_B
#define TOP_CASE_B 0xFF
#endif

#ifndef TOP_CASE_SHADE_R
#define TOP_CASE_SHADE_R 0xCF
#endif
#ifndef TOP_CASE_SHADE_G
#define TOP_CASE_SHADE_G 0x00
#endif
#ifndef TOP_CASE_SHADE_B
#define TOP_CASE_SHADE_B 0xFF
#endif

#ifndef BOTTOM_CASE_R
#define BOTTOM_CASE_R 0xFF
#endif
#ifndef BOTTOM_CASE_G
#define BOTTOM_CASE_G 0xFF
#endif
#ifndef BOTTOM_CASE_B
#define BOTTOM_CASE_B 0xFF
#endif
```

- [ ] **Step 2: Добавить хелпер отправки device info**

Перед функцией `void sendConfigFrame()` (around `firmware/one-shot/one-shot.ino:139`) добавить:

```cpp
void sendDeviceInfoFrame() {
  uint8_t payload[13];
  payload[0]  = NUM_LEDS;
  payload[1]  = KEYCAP_R;
  payload[2]  = KEYCAP_G;
  payload[3]  = KEYCAP_B;
  payload[4]  = TOP_CASE_R;
  payload[5]  = TOP_CASE_G;
  payload[6]  = TOP_CASE_B;
  payload[7]  = TOP_CASE_SHADE_R;
  payload[8]  = TOP_CASE_SHADE_G;
  payload[9]  = TOP_CASE_SHADE_B;
  payload[10] = BOTTOM_CASE_R;
  payload[11] = BOTTOM_CASE_G;
  payload[12] = BOTTOM_CASE_B;
  sendFrame(Serial, CMD_DEVICE_INFO, payload, sizeof(payload));
}
```

- [ ] **Step 3: Подключить команду в диспетчер**

В `handleSerial()` (around `firmware/one-shot/one-shot.ino:486-499`), добавить ветку перед `else { sendError(...) }`:

```cpp
    if (cmd == CMD_GET_CONFIG) {
      sendConfigFrame();
    } else if (cmd == CMD_SET_CONFIG) {
      handleSetConfig(payload, payloadLen);
    } else if (cmd == CMD_RESET_CONFIG) {
      resetConfig();
      updateBaseColor();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
      sendPingFrame(Serial, DEVICE_TYPE_ONE_SHOT, USB_PRODUCT);
    } else if (cmd == CMD_GET_DEVICE_INFO) {
      sendDeviceInfoFrame();
    } else {
      sendError(Serial, STATUS_BAD_COMMAND);
    }
```

- [ ] **Step 4: Скомпилировать прошивку с дефолтным профилем**

Run: `npm run firmware:compile`
Expected: компиляция успешна, без warning-ов в нашем коде. Output содержит `Sketch uses` и не содержит `error:`.

- [ ] **Step 5: Скомпилировать прошивку с 0-led профилем**

Run: `sh ./scripts/firmware.sh compile one-shot 0-led`
Expected: компиляция успешна. Это проверяет, что `NUM_LEDS=0` не ломает FastLED-код (и что хендлер всё равно собирается).

- [ ] **Step 6: Закоммитить**

```bash
git add firmware/one-shot/one-shot.ino
git commit -m "Handle CMD_GET_DEVICE_INFO with compile-time color defaults"
```

---

## Task 3: Build-скрипт — парсить блок colors из профиля

**Files:**
- Modify: `scripts/firmware.sh:36-46`

- [ ] **Step 1: Добавить парсинг hex-цветов и сборку дефайнов**

После строки `EXTRA_FLAGS="-DBTN_GROUND_PIN=${BTN_GROUND_PIN} ..."` (around line 46), перед блоком `ROTARY_ENABLED`, добавить:

```sh
    append_color_flags() {
      key="$1"
      prefix="$2"
      hex=$(jq -r ".colors.${key} // empty" "$PROFILE_FILE")
      if [ -z "$hex" ]; then
        return
      fi

      hex_no_hash="${hex#\#}"
      if [ "${#hex_no_hash}" -ne 6 ]; then
        echo "Invalid color in profile: ${key}=${hex}" >&2
        exit 1
      fi

      r="0x${hex_no_hash%????}"
      g_b="${hex_no_hash#??}"
      g="0x${g_b%??}"
      b="0x${hex_no_hash#????}"

      EXTRA_FLAGS="${EXTRA_FLAGS} -D${prefix}_R=${r} -D${prefix}_G=${g} -D${prefix}_B=${b}"
    }

    append_color_flags "keycap" "KEYCAP"
    append_color_flags "top_case" "TOP_CASE"
    append_color_flags "top_case_shade" "TOP_CASE_SHADE"
    append_color_flags "bottom_case" "BOTTOM_CASE"
```

- [ ] **Step 2: Проверить парсинг в shell**

Run:
```bash
bash -c 'hex_no_hash="FFEEDD"; r="0x${hex_no_hash%????}"; g_b="${hex_no_hash#??}"; g="0x${g_b%??}"; b="0x${hex_no_hash#????}"; echo "$r $g $b"'
```
Expected: `0xFF 0xEE 0xDD`

- [ ] **Step 3: Скомпилировать с профилем без `colors`**

Run: `npm run firmware:compile`
Expected: компиляция успешна, дефайны цветов не пробрасываются (берутся дефолты из `.ino`).

- [ ] **Step 4: Закоммитить**

```bash
git add scripts/firmware.sh
git commit -m "Parse colors block from profile and emit -D flags"
```

---

## Task 4: Профиль 0-led — добавить блок colors

**Files:**
- Modify: `profiles/0-led.json`

- [ ] **Step 1: Дополнить профиль (на твой вкус, минимум — чтобы блок был)**

Финальный JSON:

```json
{
  "button_ground_pin": "9",
  "button_input_pin": "10",
  "data_pin": "A3",
  "num_leds": 0,
  "rotary_enabled": false,
  "usb_product": "One Shot",
  "usb_manufacturer": "lunyov",
  "colors": {
    "keycap": "#FFFFFF",
    "top_case": "#FFFFFF",
    "top_case_shade": "#CF00FF",
    "bottom_case": "#FFFFFF"
  }
}
```

(Эти значения дублируют дефолты — оставлены как пример. Поменяй любое из четырёх под реальный корпус 0-led.)

- [ ] **Step 2: Скомпилировать с этим профилем**

Run: `sh ./scripts/firmware.sh compile one-shot 0-led`
Expected: компиляция успешна. В логе видны добавленные `-DKEYCAP_R=0xFF`-флаги (можно подсмотреть `--verbose`, опционально).

- [ ] **Step 3: Закоммитить**

```bash
git add profiles/0-led.json
git commit -m "Add colors block to 0-led profile"
```

---

## Task 5: Конфигуратор — константы и декодер device info

**Files:**
- Modify: `src/protocol.js:5-15` (COMMANDS), а также добавить новые экспорты в конец файла.

- [ ] **Step 1: Расширить COMMANDS**

Заменить текущий объект `COMMANDS` на:

```js
export const COMMANDS = {
  getConfig: 0x01,
  setConfig: 0x02,
  resetConfig: 0x03,
  ping: 0x04,
  getDeviceInfo: 0x05,
  config: 0x81,
  ack: 0x82,
  pong: 0x84,
  deviceInfo: 0x85,
  buttonEvent: 0x90,
  error: 0xff,
}
```

- [ ] **Step 2: Добавить хелпер форматирования цвета**

В конец файла (после `getDeviceName`) добавить:

```js
function rgbToHex(r, g, b) {
  return `#${[r, g, b].map((v) => v.toString(16).padStart(2, '0')).join('')}`
}

export function decodeDeviceInfo(payload) {
  if (payload.length < 13) {
    throw new Error(`Unexpected device info payload size: ${payload.length}`)
  }

  return {
    numLeds: payload[0],
    keycap: rgbToHex(payload[1], payload[2], payload[3]),
    topCase: rgbToHex(payload[4], payload[5], payload[6]),
    topCaseShade: rgbToHex(payload[7], payload[8], payload[9]),
    bottomCase: rgbToHex(payload[10], payload[11], payload[12]),
  }
}
```

- [ ] **Step 3: Добавить дефолты по deviceType**

Сразу после `decodeDeviceInfo` добавить:

```js
export const DEFAULT_DEVICE_INFO = {
  [DEVICE_TYPES.oneShot]: {
    numLeds: 1,
    keycap: '#ffffff',
    topCase: '#ffffff',
    topCaseShade: '#cf00ff',
    bottomCase: '#ffffff',
  },
  [DEVICE_TYPES.magicButton]: {
    numLeds: 0,
    keycap: '#5ab9cf',
    topCase: '#ffffff',
    topCaseShade: '#ffffff',
    bottomCase: '#ffffff',
  },
}
```

(Для one-shot ставим `numLeds: 1` как «есть подсветка» — это фолбэк только когда команда не поддержана; реальное число придёт от прошивки.)

- [ ] **Step 4: Проверить декодер вручную**

Run: `npm run dev`, открой DevTools → Console и выполни:

```js
const m = await import('/src/protocol.js')
const buf = new Uint8Array([3, 0xff, 0xee, 0xdd, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0xaa, 0xbb, 0xcc])
console.log(m.decodeDeviceInfo(buf))
```

Expected:
```
{ numLeds: 3, keycap: '#ffeedd', topCase: '#102030', topCaseShade: '#405060', bottomCase: '#aabbcc' }
```

- [ ] **Step 5: Закоммитить**

```bash
git add src/protocol.js
git commit -m "Add CMD_GET_DEVICE_INFO codec and per-deviceType defaults"
```

---

## Task 6: useConfiguratorState — поля numLeds, caseColors и новый supportsLighting

**Files:**
- Modify: `src/composables/useConfiguratorState.js`

- [ ] **Step 1: Добавить новые ref-ы и обновить supportsLighting**

В функции `useConfiguratorState`, рядом с `const deviceType = ref(...)` (around line 13):

```js
  const deviceType = ref(DEVICE_TYPES.oneShot)
  const isDevicePressed = ref(false)
  const suppressAutoSave = ref(false)
  const numLeds = ref(1)
  const caseColors = reactive({
    keycap: '#ffffff',
    topCase: '#ffffff',
    topCaseShade: '#cf00ff',
    bottomCase: '#ffffff',
  })
```

Заменить определение `supportsLighting` (around line 40):

```js
  const supportsLighting = computed(() => numLeds.value > 0)
```

- [ ] **Step 2: Добавить applyDeviceInfo**

После функции `applyConfig` (around line 89) добавить:

```js
  function applyDeviceInfo(info) {
    numLeds.value = info.numLeds
    caseColors.keycap = info.keycap
    caseColors.topCase = info.topCase
    caseColors.topCaseShade = info.topCaseShade
    caseColors.bottomCase = info.bottomCase
  }
```

- [ ] **Step 3: Экспортировать новые значения**

В возвращаемом объекте (around line 95-106):

```js
  return {
    applyConfig,
    applyDeviceInfo,
    caseColors,
    deviceType,
    form,
    gestureFields,
    hasEncoder,
    isDevicePressed,
    numLeds,
    selectedColor,
    supportsLighting,
    suppressAutoSave,
    updateGesture,
  }
```

- [ ] **Step 4: Импорт `reactive`**

Убедиться, что `reactive` уже в импорте `from 'vue'` (он там уже есть в строке 1).

- [ ] **Step 5: Закоммитить**

```bash
git add src/composables/useConfiguratorState.js
git commit -m "Track numLeds and caseColors in configurator state"
```

---

## Task 7: useDeviceConnection — fetchDeviceInfo с таймаутом и фолбэком

**Files:**
- Modify: `src/composables/useDeviceConnection.js`

- [ ] **Step 1: Параметризовать таймаут в waitForFrame**

Заменить функцию `waitForFrame` (around line 151-169) на:

```js
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
```

- [ ] **Step 2: Добавить timeoutMs в sendCommand**

Заменить функцию `sendCommand` (around line 233-241):

```js
  async function sendCommand(command, payload = new Uint8Array(), expected = [COMMANDS.ack], timeoutMs = 1500) {
    if (!writer.value) {
      throw new Error('Сначала подключите устройство')
    }

    const responsePromise = waitForFrame(expected, timeoutMs)
    await writer.value.write(buildFrame(command, payload))
    return responsePromise
  }
```

- [ ] **Step 3: Импортировать новые символы**

Заменить блок импорта в начале файла (lines 2-12):

```js
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
```

- [ ] **Step 4: Принимать applyDeviceInfo в опциях**

Изменить сигнатуру `useDeviceConnection` (line 91):

```js
export function useDeviceConnection({ applyConfig, applyDeviceInfo, deviceType, form, isDevicePressed }) {
```

- [ ] **Step 5: Добавить fetchDeviceInfo**

После функции `verifyDevice` (around line 290), перед `connect`, добавить:

```js
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
      // Старая прошивка / таймаут — фолбэк ниже.
    }

    const fallback = DEFAULT_DEVICE_INFO[deviceType.value] ?? DEFAULT_DEVICE_INFO[DEVICE_TYPES.oneShot]
    applyDeviceInfo(fallback)
  }
```

- [ ] **Step 6: Вызвать fetchDeviceInfo между verifyDevice и refreshConfig**

В функции `connect()` (around line 320-324), заменить:

```js
      void readLoop()
      await verifyDevice()
      isConnected.value = true
      await refreshConfig()
```

на:

```js
      void readLoop()
      await verifyDevice()
      await fetchDeviceInfo()
      isConnected.value = true
      await refreshConfig()
```

- [ ] **Step 7: Обновить вызов в App.vue**

В `src/App.vue` (around line 13-23, использование `useConfiguratorState`):

```js
const {
  applyConfig,
  applyDeviceInfo,
  deviceType,
  form,
  gestureFields,
  isDevicePressed,
  selectedColor,
  supportsLighting,
  suppressAutoSave,
  updateGesture,
} = useConfiguratorState()
```

И в вызове `useDeviceConnection` (around line 27-42) добавить `applyDeviceInfo`:

```js
const {
  connect,
  disconnect,
  isBusy,
  isConnected,
  isConnecting,
  productName,
  resetConfig,
  saveConfig,
  statusText,
} = useDeviceConnection({
  applyConfig,
  applyDeviceInfo,
  deviceType,
  form,
  isDevicePressed,
})
```

- [ ] **Step 8: Запустить dev-сервер и проверить ошибок нет**

Run: `npm run dev`
Expected: Vite запускается, браузер открывает страницу без console errors. Подключение устройства не обязательно — этот шаг проверяет, что синтаксис корректный и приложение монтируется.

- [ ] **Step 9: Закоммитить**

```bash
git add src/composables/useDeviceConnection.js src/App.vue
git commit -m "Fetch device info after PING with timeout fallback"
```

---

## Task 8: useLightingPreview — читать цвета из state

**Files:**
- Modify: `src/composables/useLightingPreview.js`

- [ ] **Step 1: Изменить сигнатуру и убрать хардкоды**

Полностью заменить файл на:

```js
import { computed } from 'vue'
import { ANIMATION_MODES, DEVICE_TYPES } from '../protocol'

export function useLightingPreview(form, deviceType, caseColors) {
  const hsl = computed(() => {
    const r = form.red / 255
    const g = form.green / 255
    const b = form.blue / 255

    const max = Math.max(r, g, b)
    const min = Math.min(r, g, b)
    const delta = max - min

    let hue = 0
    const lightness = (max + min) / 2

    if (delta !== 0) {
      if (max === r) {
        hue = ((g - b) / delta) % 6
      } else if (max === g) {
        hue = (b - r) / delta + 2
      } else {
        hue = (r - g) / delta + 4
      }
    }

    hue = Math.round(hue * 60)
    if (hue < 0) {
      hue += 360
    }

    const saturation = delta === 0 ? 0 : delta / (1 - Math.abs(2 * lightness - 1))

    return {
      h: hue,
      s: Math.round(saturation * 100),
      l: Math.round(lightness * 100),
    }
  })

  const colorPreviewStyle = computed(() => {
    const base = {
      '--button-color': caseColors.keycap,
      '--top-color': caseColors.topCase,
      '--top-shade-color': caseColors.topCaseShade,
      '--bottom-color': caseColors.bottomCase,
      '--selection-color': caseColors.keycap,
    }

    if (deviceType.value === DEVICE_TYPES.magicButton) {
      return {
        ...base,
        '--selection-color': caseColors.keycap,
      }
    }

    if (form.animationMode === ANIMATION_MODES.rainbow) {
      return {
        ...base,
        '--selection-color': 'hsl(280,60%,65%)',
        '--top-color': 'hsl(280,60%,65%)',
        '--top-shade-color': 'transparent',
        '--button-color': '#FFF',
      }
    }

    const dynamicTop = `hsla(${hsl.value.h % 360}, ${hsl.value.s + 10}%, ${Math.max(40, hsl.value.l + 15)}%, ${hsl.value.s / 100})`
    return {
      ...base,
      '--selection-color': dynamicTop,
      '--top-color': dynamicTop,
    }
  })

  const isRainbow = computed(() => form.animationMode === ANIMATION_MODES.rainbow)

  return {
    colorPreviewStyle,
    isRainbow,
    hsl,
  }
}
```

- [ ] **Step 2: Передать caseColors в App.vue**

В `src/App.vue` (around line 25):

```js
const { colorPreviewStyle, isRainbow } = useLightingPreview(form, deviceType, caseColors)
```

И добавить `caseColors` в деструктуризацию `useConfiguratorState()`:

```js
const {
  applyConfig,
  applyDeviceInfo,
  caseColors,
  deviceType,
  form,
  gestureFields,
  isDevicePressed,
  selectedColor,
  supportsLighting,
  suppressAutoSave,
  updateGesture,
} = useConfiguratorState()
```

- [ ] **Step 3: Запустить dev-сервер и проверить превью**

Run: `npm run dev`
Expected: страница рендерится. Без подключения устройства превью должно выглядеть как раньше для one-shot (белый кейкап, белый верх с сиреневым shade-overlay, белый низ).

- [ ] **Step 4: Закоммитить**

```bash
git add src/composables/useLightingPreview.js src/App.vue
git commit -m "Read case colors from state instead of deviceType hardcoding"
```

---

## Task 9: DevicePreview — нижняя часть корпуса через CSS-переменную

**Files:**
- Modify: `src/components/DevicePreview.vue:86`

- [ ] **Step 1: Заменить `fill:#fff` на `var(--bottom-color)`**

Найти строку (line 86):

```html
<path d="M1 148v26.8c0 2 4.5 6 7.7 7.8a69 ..." style="fill:#fff;stroke:#000;stroke-width:1.67px"/>
```

Заменить на:

```html
<path d="M1 148v26.8c0 2 4.5 6 7.7 7.8a69 ..." style="fill:var(--bottom-color, #fff);stroke:#000;stroke-width:1.67px"/>
```

(Сохрани полный `d=...` атрибут как есть, меняется только `fill` в `style`.)

- [ ] **Step 2: Запустить dev-сервер и проверить визуал**

Run: `npm run dev`
Expected: нижняя часть корпуса по-прежнему белая (дефолт `#ffffff` из state). При смене значения `caseColors.bottomCase` через DevTools (`$0.style.setProperty('--bottom-color', '#ffaa00')` на `.shell`) нижняя часть должна перекрашиваться.

- [ ] **Step 3: Закоммитить**

```bash
git add src/components/DevicePreview.vue
git commit -m "Bind device preview bottom case to --bottom-color"
```

---

## Task 10: Bump version

**Files:**
- Modify: `package.json:3`

- [ ] **Step 1: Обновить версию**

Заменить `"version": "2.0.0"` на `"version": "2.1.0"`.

- [ ] **Step 2: Закоммитить**

```bash
git add package.json
git commit -m "Bump version to 2.1.0"
```

---

## Task 11: End-to-end ручная проверка

- [ ] **Step 1: Прошить устройство (default профиль)**

Run: `npm run firmware:upload`
Expected: прошивка загружена, устройство мигает.

- [ ] **Step 2: Проверить что подсветка работает в конфигураторе**

Run: `npm run dev`, подключить устройство.
Expected:
- ColorControl видно (numLeds > 0).
- Цвета корпуса соответствуют дефолтам one-shot (белый кейкап, сиреневый shade).
- Изменение цвета в ColorControl динамически перекрашивает верх корпуса (как раньше).

- [ ] **Step 3: Прошить устройство 0-led профилем**

Run: `sh ./scripts/firmware.sh upload one-shot 0-led`
Expected: успешно прошито.

- [ ] **Step 4: Проверить, что подсветки нет в UI**

Подключить устройство в конфигуратор.
Expected:
- ColorControl **отсутствует** (numLeds == 0).
- Цвета корпуса — дефолтные либо те, что заданы в `0-led.json`.
- Жесты по-прежнему настраиваются.

- [ ] **Step 5: Проверить совместимость со старой прошивкой**

Если есть устройство со старой прошивкой (без поддержки `CMD_GET_DEVICE_INFO`) — подключить.
Expected:
- В консоли может быть warning от `waitForFrame` (внутри `try/catch` подавлен).
- ColorControl показывается, цвета корпуса — фолбэк по `deviceType` (как раньше).
- Никакого UI-зависания на старте.

(Если старого устройства под рукой нет — пропустить, отметить как «не проверено».)

---

## Self-Review

**Spec coverage:**
- ✅ `CMD_GET_DEVICE_INFO=0x05` / `CMD_DEVICE_INFO=0x85` — Task 1, 2, 5.
- ✅ Payload 13 байт `num_leds + 4×rgb` — Task 2, 5.
- ✅ Дефолты one-shot в `.ino` — Task 2.
- ✅ `firmware.sh` парсит `colors` — Task 3.
- ✅ Опциональный блок `colors` в JSON — Task 4 (как пример), остальные профили без блока работают.
- ✅ Конфигуратор: `decodeDeviceInfo`, `DEFAULT_DEVICE_INFO` — Task 5.
- ✅ State `numLeds`, `caseColors`, `supportsLighting = numLeds > 0` — Task 6.
- ✅ `fetchDeviceInfo` с таймаутом 500мс и фолбэком — Task 7.
- ✅ `useLightingPreview` через state, не хардкоды — Task 8.
- ✅ `--bottom-color` в `DevicePreview.vue` — Task 9.
- ✅ Бамп версии до 2.1.0 — Task 10.
- ✅ Совместимость старая прошивка / новый конфигуратор — Task 11 step 5.

**Placeholder scan:** все шаги содержат конкретный код или конкретные команды; «TBD» / «add appropriate handling» отсутствуют.

**Type consistency:** `caseColors` объект с ключами `keycap | topCase | topCaseShade | bottomCase` (camelCase) — единообразно во всех Tasks 5-9. `DeviceInfo` payload-поля используют те же имена. `applyDeviceInfo` принимает объект с теми же ключами. ✅
