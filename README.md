# One Shot

Репозиторий с двумя родственными устройствами и общим web-конфигуратором по Serial:

- `firmware/one-shot/one-shot.ino` — Leonardo / Pro Micro, три тапа и подсветка
- `firmware/magic-button/magic-button.ino` — ESP32-S3, одиночное / двойное / долгое нажатие, без подсветки

## Web

```bash
npm install
npm run dev
```

## Firmware

Для воспроизводимой локальной сборки используется `arduino-cli`.

Параметры сборки лежат в `.env`:

```bash
ARDUINO_FQBN_ONE_SHOT=arduino:avr:leonardo
ARDUINO_SKETCH_PATH_ONE_SHOT=firmware/one-shot
ARDUINO_BUILD_PATH_ONE_SHOT=.arduino/build
ARDUINO_PORT_ONE_SHOT=/dev/cu.usbmodemHIDJB1
USB_PRODUCT_ONE_SHOT="One Shot"
USB_MANUFACTURER_ONE_SHOT=lunyov

ARDUINO_FQBN_MAGIC_BUTTON=esp32:esp32:esp32s3
ARDUINO_SKETCH_PATH_MAGIC_BUTTON=firmware/magic-button
ARDUINO_BUILD_PATH_MAGIC_BUTTON=.arduino/build-magic-button
ARDUINO_PORT_MAGIC_BUTTON=
USB_PRODUCT_MAGIC_BUTTON="Magic Button"
USB_MANUFACTURER_MAGIC_BUTTON=Huntflow
```

Сборка `One Shot`:

```bash
npm run firmware:compile
```

Экспорт бинарников `One Shot`:

```bash
npm run firmware:export
```

Сборка и загрузка `One Shot`:

```bash
npm run firmware:upload
```

Сборка `Magic Button`:

```bash
npm run firmware:compile:magic
```

Для `Magic Button` USB CDC поднимается явно в самом скетче, чтобы порт появлялся в Web Serial уже с кастомным USB-именем.

Экспорт бинарников `Magic Button`:

```bash
npm run firmware:export:magic
```

Сборка и загрузка `Magic Button`:

```bash
npm run firmware:upload:magic
```

### UICPal MINI nRF52840

Для BLE/USB-версии на UICPal MINI nRF52840 в форм-факторе XIAO используется отдельный target. Кнопка подключается между `D1` (`P0.03`) и `GND`.

```bash
arduino-cli core update-index --config-file arduino-cli.yaml
arduino-cli core install Seeeduino:nrf52 --config-file arduino-cli.yaml
npm run firmware:compile:magic:xiao
npm run firmware:upload:magic:xiao
```

Если serial-порт не появляется, дважды быстро нажмите Reset для входа в UF2 bootloader.

Прошивка также публикует BLE-сервис конфигурации на базе Nordic UART Service.
В Chromium-конфигураторе выберите «Подключить по Bluetooth» и затем
`Super Magic Button`. USB и BLE используют одинаковые кадры протокола, поэтому
настройки и их формат остаются общими для обоих транспортов.

Скрипт `scripts/firmware.sh` читает значения из `.env` и передает их в `arduino-cli`. Для `Magic Button` по умолчанию используются `USB_PRODUCT="Magic Button"` и `USB_MANUFACTURER="Huntflow"`, а имя для ESP32 зафиксировано прямо в скетче, чтобы не упираться в экранирование build flags.
