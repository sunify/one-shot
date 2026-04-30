# Device Info: расширение протокола и доработка конфигуратора

## Цель

1. Если у устройства 0 светодиодов, конфигуратор не должен показывать настройку подсветки.
2. Цвета кейкапа, верха корпуса (с shade-слоем) и низа корпуса должны задаваться в JSON-профиле прошивки. Текущие захардкоженные значения становятся дефолтами фолбэка.

## Ограничения

- `PROTOCOL_VERSION` не повышаем.
- `CONFIG_VERSION` не повышаем.
- Старая прошивка (без поддержки новой команды) и новый конфигуратор должны корректно сосуществовать: конфигуратор обязан фолбэчиться на дефолты по `deviceType`.
- Старый конфигуратор и новая прошивка тоже должны работать: новая команда — отдельная, не задевает существующие.

## Расширение протокола

Добавляем одну команду-запрос и один ответный кадр. `PROTOCOL_VERSION` остаётся `1`.

| Имя                   | Код    | Направление          | Назначение                                  |
| --------------------- | ------ | -------------------- | ------------------------------------------- |
| `CMD_GET_DEVICE_INFO` | `0x05` | конфигуратор → device | Запросить статичные характеристики прошивки |
| `CMD_DEVICE_INFO`     | `0x85` | device → конфигуратор | Ответ с характеристиками                    |

Если устройство не знает команду, оно отвечает существующим `CMD_ERROR` со `STATUS_BAD_COMMAND`. Конфигуратор также трактует таймаут как «нет поддержки».

### Payload `CMD_DEVICE_INFO`

13 байт, фиксированный layout:

```
offset  size  field
0       1     num_leds
1       3     keycap_rgb       (R, G, B)
4       3     top_case_rgb     (R, G, B)
7       3     top_case_shade   (R, G, B)
10      3     bottom_case_rgb  (R, G, B)
```

Если в будущем понадобятся ещё поля — дописываем в конец. Старый клиент читает первые 13 байт и игнорирует остальное; новый клиент учитывает реальную длину `payload_len` из заголовка кадра.

## Прошивка one-shot

`firmware/one-shot/one-shot.ino`:

- Новый хендлер `CMD_GET_DEVICE_INFO`: собирает 13-байтовый payload из compile-time дефайнов и шлёт `CMD_DEVICE_INFO`.
- Новые дефайны с дефолтами (срабатывают, когда `firmware.sh` не пробросил `-D`):
  - `KEYCAP_R = 0xFF`, `KEYCAP_G = 0xFF`, `KEYCAP_B = 0xFF`
  - `TOP_CASE_R = 0xFF`, `TOP_CASE_G = 0xFF`, `TOP_CASE_B = 0xFF`
  - `TOP_CASE_SHADE_R = 0xCF`, `TOP_CASE_SHADE_G = 0x00`, `TOP_CASE_SHADE_B = 0xFF`
  - `BOTTOM_CASE_R = 0xFF`, `BOTTOM_CASE_G = 0xFF`, `BOTTOM_CASE_B = 0xFF`

Эти дефолты повторяют то, что сейчас захардкожено в [useLightingPreview.js](../../../src/composables/useLightingPreview.js) и [DevicePreview.vue](../../../src/components/DevicePreview.vue) для one-shot. Существующие профили без блока `colors` компилируются один-в-один как сейчас.

`NUM_LEDS` уже пробрасывается из профиля.

## Прошивка magic-button

Без изменений. На `CMD_GET_DEVICE_INFO` команда не обрабатывается — отвечает `CMD_ERROR / STATUS_BAD_COMMAND` через существующую ветку `else` в `handleSerial`. Конфигуратор по фолбэку для `magic-button` берёт:

- `num_leds = 0`
- `keycap = #5AB9CF`
- `top_case = #FFFFFF`
- `top_case_shade = #FFFFFF`
- `bottom_case = #FFFFFF`

Это полностью совпадает с текущим поведением.

## Сборка прошивки (`scripts/firmware.sh`)

Для one-shot из профиля читается опциональный блок `colors`:

```sh
KEYCAP_HEX=$(jq -r '.colors.keycap // empty' "$PROFILE_FILE")
TOP_CASE_HEX=$(jq -r '.colors.top_case // empty' "$PROFILE_FILE")
TOP_CASE_SHADE_HEX=$(jq -r '.colors.top_case_shade // empty' "$PROFILE_FILE")
BOTTOM_CASE_HEX=$(jq -r '.colors.bottom_case // empty' "$PROFILE_FILE")
```

Если значение задано — парсится как `#RRGGBB`, в `EXTRA_FLAGS` добавляются `-DKEYCAP_R=...`, `-DKEYCAP_G=...`, `-DKEYCAP_B=...` и т. д. Если поле отсутствует — флаги не пробрасываются, в `.ino` срабатывает дефолт.

## Профили (`profiles/*.json`)

Новый опциональный блок:

```json
"colors": {
  "keycap": "#FFFFFF",
  "top_case": "#FFFFFF",
  "top_case_shade": "#CF00FF",
  "bottom_case": "#FFFFFF"
}
```

Все поля опциональные; отсутствующее значение = дефолт из `.ino`. Для существующих профилей блок не обязателен.

## Конфигуратор

### `src/protocol.js`

- В `COMMANDS` добавляются `getDeviceInfo: 0x05`, `deviceInfo: 0x85`.
- Новые экспорты:
  - `decodeDeviceInfo(payload)` → `{ numLeds, keycap, topCase, topCaseShade, bottomCase }`, где цвета — строки `#rrggbb`. Требует `payload.length >= 13`.
  - `DEFAULT_DEVICE_INFO` — словарь по `deviceType` с фолбэк-значениями (one-shot и magic-button), повторяющими текущие хардкоды.

### `src/composables/useDeviceConnection.js`

После `verifyDevice` (но до `refreshConfig`) выполняется `fetchDeviceInfo`:

1. Отправить `CMD_GET_DEVICE_INFO` с пустым payload.
2. Ждать любой из: `CMD_DEVICE_INFO` (успех) / `CMD_ERROR` (старая прошивка) / таймаут ~500 мс.
3. На успех — `applyDeviceInfo(decodeDeviceInfo(frame.payload))`.
4. На ошибку или таймаут — `applyDeviceInfo(DEFAULT_DEVICE_INFO[deviceType])`.

`applyDeviceInfo` приходит снаружи (из `useConfiguratorState`).

### `src/composables/useConfiguratorState.js`

- Новые `ref`-ы: `numLeds`, `caseColors` (объект с четырьмя hex-строками).
- При `applyDeviceInfo({ numLeds, keycap, topCase, topCaseShade, bottomCase })` обновляем оба.
- `supportsLighting` теперь `computed(() => numLeds.value > 0)`. Старая привязка к `deviceType === oneShot` уходит.
- Возвращаем `numLeds`, `caseColors`, `applyDeviceInfo` наружу.

### `src/composables/useLightingPreview.js`

- Хардкоженные ветки `if (deviceType === magicButton)` удаляются.
- В возвращаемом `colorPreviewStyle`:
  - `--button-color` = `caseColors.keycap`
  - `--top-color` = `caseColors.topCase` (поверх — динамика от подсветки и rainbow, как сейчас, но дефолт берётся из state)
  - `--top-shade-color` = `caseColors.topCaseShade`
  - `--bottom-color` = `caseColors.bottomCase` (новое)
- Для `rainbow` сохраняется существующее поведение (`--top-color` становится градиентом, `--top-shade-color = transparent`, `--button-color = #FFF`). Для `static`/`breathing` цвет верха продолжает зависеть от выбранного цвета подсветки, но при отсутствии активной подсветки фолбэк — `caseColors.topCase`.

### `src/components/DevicePreview.vue`

В нижнем path-е `fill:#fff` заменяется на `fill:var(--bottom-color, #fff)`. Все остальные цвета уже на CSS-переменных.

### `App.vue`

Без структурных правок: `<ColorControl v-if="isConnected && supportsLighting" />` уже есть, и `supportsLighting` теперь корректно реагирует на `numLeds === 0`.

## Версионирование

`package.json` → `2.1.0` (минор: расширение протокола, обратно совместимое).

## Затрагиваемые файлы

- `libraries/device_protocol/src/device_protocol.h` — новые command-коды.
- `firmware/one-shot/one-shot.ino` — обработка команды, дефайны и дефолты.
- `scripts/firmware.sh` — парсинг `colors` из профилей.
- `profiles/0-led.json`, `profiles/3-led.json`, `profiles/default.json`, `profiles/rotary.json` — опциональный блок `colors` (по необходимости).
- `src/protocol.js` — константы команд, `decodeDeviceInfo`, `DEFAULT_DEVICE_INFO`.
- `src/composables/useDeviceConnection.js` — `fetchDeviceInfo` с таймаутом и фолбэком.
- `src/composables/useConfiguratorState.js` — поля `numLeds`, `caseColors`, новый `supportsLighting`.
- `src/composables/useLightingPreview.js` — чтение цветов из state.
- `src/components/DevicePreview.vue` — `--bottom-color` CSS var.
- `package.json` — версия `2.1.0`.

## Сценарии совместимости

| Прошивка   | Конфигуратор | Поведение                                                                                      |
| ---------- | ------------ | ---------------------------------------------------------------------------------------------- |
| старая     | старый       | Без изменений.                                                                                  |
| старая     | новый        | `CMD_GET_DEVICE_INFO` → `CMD_ERROR` или таймаут → фолбэк по `deviceType` (= текущие хардкоды). |
| новая      | старый       | Старый конфигуратор не шлёт новую команду; ничего не меняется.                                  |
| новая      | новый        | `CMD_DEVICE_INFO` → реальные значения из профиля.                                               |
