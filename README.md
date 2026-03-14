# One Shot

Однокнопочная клавиатура на Arduino Leonardo / Pro Micro с тремя жестами и web-конфигуратором по Serial.

## Web

```bash
npm install
npm run dev
```

## Firmware

Для воспроизводимой локальной сборки используется `arduino-cli`.

Параметры сборки лежат в `.env`:

```bash
ARDUINO_FQBN=arduino:avr:leonardo
ARDUINO_SKETCH_PATH=firmware
ARDUINO_BUILD_PATH=.arduino/build
ARDUINO_PORT=/dev/cu.usbmodemHIDJB1
USB_PRODUCT="One Shot"
USB_MANUFACTURER=lunyov
```

Сборка прошивки:

```bash
npm run firmware:compile
```

Экспорт бинарников в папку скетча:

```bash
npm run firmware:export
```

Сборка и загрузка в плату:

```bash
npm run firmware:upload
```

Скрипт `scripts/firmware.sh` читает эти значения из `.env` и передает их в `arduino-cli`, включая кастомные USB descriptor properties для имени устройства.
