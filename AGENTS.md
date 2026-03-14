# Project Notes

## Scope
- `firmware/firmware.ino` is the source of truth for the device protocol and persistent config format.
- The web configurator must stay compatible with the firmware protocol in `src/protocol.js`.

## Protocol
- Serial frames use: `0x4F 0x53 | version:u8 | command:u8 | payload_len:u8 | payload | crc8:u8`.
- CRC uses polynomial `0x07` over `version`, `command`, `payload_len`, and payload bytes.
- `CMD_GET_CONFIG (0x01)` requests config.
- `CMD_SET_CONFIG (0x02)` writes config payload.
- `CMD_RESET_CONFIG (0x03)` restores defaults and returns config.
- `CMD_CONFIG (0x81)` returns packed config payload.
- Config payload is 11 bytes total: `version:u8`, `single:u16`, `double:u16`, `triple:u16`, `r:u8`, `g:u8`, `b:u8`, `crc:u8`.

## Persistence
- On Arduino Pro Micro, persistent config is stored in EEPROM via `EEPROM.get/put`.
- If config layout changes, bump `CONFIG_VERSION` and update both firmware and configurator together.

## Build
- Reproducible firmware builds should use `arduino-cli` with the repo-local `arduino-cli.yaml`.
- Default firmware target is `arduino:avr:leonardo`.
- Firmware build settings live in the repo-root `.env`.
- Upload port is configured via `ARDUINO_PORT` in the repo-root `.env`.
- USB name is overridden at build time via `build.usb_product` and `build.usb_manufacturer` rather than by editing Arduino IDE core files.
