# Project Notes

## Scope
- `firmware/one-shot/one-shot.ino` is the source of truth for the current Leonardo / Pro Micro device.
- `firmware/magic-button/magic-button.ino` is the entry point reserved for the ESP32-S3 variant.
- The web configurator must stay compatible with the firmware protocol in `src/protocol.js`.

## Protocol
- Serial frames use: `0x4F 0x53 | version:u8 | command:u8 | payload_len:u8 | payload | crc8:u8`.
- CRC uses polynomial `0x07` over `version`, `command`, `payload_len`, and payload bytes.
- `CMD_GET_CONFIG (0x01)` requests config.
- `CMD_SET_CONFIG (0x02)` writes config payload.
- `CMD_RESET_CONFIG (0x03)` restores defaults and returns config.
- `CMD_CONFIG (0x81)` returns packed config payload.
- `CMD_PONG (0x84)` may include device identification payload: `status:u8`, `device_type:u8`.
- One Shot config payload is 18 bytes total: `version:u8`, then for each gesture `type:u8 code:u16 modifiers:u8`, then `r:u8`, `g:u8`, `b:u8`, `breathing_enabled:u8`, `crc:u8`.
- Magic Button config payload is 14 bytes total: `version:u8`, then for each gesture `type:u8 code:u16 modifiers:u8`, then `crc:u8`.
- Gesture action types: `0x01` for consumer preset, `0x02` for one-key hotkey with modifiers.

## Persistence
- On Arduino Pro Micro, persistent config is stored in EEPROM via `EEPROM.get/put`.
- If config layout changes, bump `CONFIG_VERSION` and update both firmware and configurator together.

## Build
- Reproducible firmware builds should use `arduino-cli` with the repo-local `arduino-cli.yaml`.
- Default firmware target is `arduino:avr:leonardo`.
- Firmware build settings live in the repo-root `.env`.
- Default sketch path in `.env` points to `firmware/one-shot`.
- Shared firmware helpers live in the repo-local Arduino library `libraries/device_protocol`.
- Upload port is configured via `ARDUINO_PORT` in the repo-root `.env`.
- USB name is overridden at build time via `build.usb_product` and `build.usb_manufacturer` rather than by editing Arduino IDE core files.
