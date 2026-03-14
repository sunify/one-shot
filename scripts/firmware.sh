#!/bin/sh

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ENV_FILE="$ROOT_DIR/.env"

if [ -f "$ENV_FILE" ]; then
  set -a
  . "$ENV_FILE"
  set +a
fi

MODE="${1:-compile}"
EXPORT_FLAG=""

if [ "$MODE" = "export" ]; then
  EXPORT_FLAG="--export-binaries"
fi

if [ "$MODE" = "upload" ]; then
  if [ -z "${ARDUINO_PORT:-}" ]; then
    echo "ARDUINO_PORT is not set in .env" >&2
    exit 1
  fi

  arduino-cli compile \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${ARDUINO_FQBN}" \
    --build-path "$ROOT_DIR/${ARDUINO_BUILD_PATH}" \
    --build-property "build.usb_product=\"${USB_PRODUCT}\"" \
    --build-property "build.usb_manufacturer=\"${USB_MANUFACTURER}\"" \
    "$ROOT_DIR/${ARDUINO_SKETCH_PATH}"

  arduino-cli upload \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${ARDUINO_FQBN}" \
    --build-path "$ROOT_DIR/${ARDUINO_BUILD_PATH}" \
    -p "${ARDUINO_PORT}" \
    "$ROOT_DIR/${ARDUINO_SKETCH_PATH}"

  exit 0
fi

arduino-cli compile \
  --config-file "$ROOT_DIR/arduino-cli.yaml" \
  -b "${ARDUINO_FQBN}" \
  --build-path "$ROOT_DIR/${ARDUINO_BUILD_PATH}" \
  ${EXPORT_FLAG} \
  --build-property "build.usb_product=\"${USB_PRODUCT}\"" \
  --build-property "build.usb_manufacturer=\"${USB_MANUFACTURER}\"" \
  "$ROOT_DIR/${ARDUINO_SKETCH_PATH}"
