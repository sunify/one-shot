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
TARGET="${2:-one-shot}"
EXPORT_FLAG=""

case "$TARGET" in
  one-shot)
    TARGET_FQBN="${ARDUINO_FQBN_ONE_SHOT:-${ARDUINO_FQBN:-arduino:avr:leonardo}}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_ONE_SHOT:-${ARDUINO_SKETCH_PATH:-firmware/one-shot}}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_ONE_SHOT:-${ARDUINO_BUILD_PATH:-.arduino/build}}"
    TARGET_PORT="${ARDUINO_PORT_ONE_SHOT:-${ARDUINO_PORT:-}}"
    TARGET_USB_PRODUCT="${USB_PRODUCT_ONE_SHOT:-${USB_PRODUCT:-One Shot}}"
    TARGET_USB_MANUFACTURER="${USB_MANUFACTURER_ONE_SHOT:-${USB_MANUFACTURER:-lunyov}}"
    ;;
  magic-button)
    TARGET_FQBN="${ARDUINO_FQBN_MAGIC_BUTTON:-esp32:esp32:esp32s3}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_MAGIC_BUTTON:-firmware/magic-button}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_MAGIC_BUTTON:-.arduino/build-magic-button}"
    TARGET_PORT="${ARDUINO_PORT_MAGIC_BUTTON:-}"
    TARGET_USB_PRODUCT="${USB_PRODUCT_MAGIC_BUTTON:-Magic Button}"
    TARGET_USB_MANUFACTURER="${USB_MANUFACTURER_MAGIC_BUTTON:-Huntflow}"
    ;;
  *)
    echo "Unknown firmware target: $TARGET" >&2
    exit 1
    ;;
esac

if [ "$MODE" = "export" ]; then
  EXPORT_FLAG="--export-binaries"
fi

if [ "$MODE" = "upload" ]; then
  if [ -z "${TARGET_PORT:-}" ]; then
    echo "Port is not set for target: $TARGET" >&2
    exit 1
  fi

  if [ "$TARGET" = "magic-button" ]; then
    arduino-cli compile \
      --config-file "$ROOT_DIR/arduino-cli.yaml" \
      -b "${TARGET_FQBN}" \
      --libraries "$ROOT_DIR/libraries" \
      --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
      "$ROOT_DIR/${TARGET_SKETCH_PATH}"
  else
    arduino-cli compile \
      --config-file "$ROOT_DIR/arduino-cli.yaml" \
      -b "${TARGET_FQBN}" \
      --libraries "$ROOT_DIR/libraries" \
      --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
      --build-property "build.usb_product=\"${TARGET_USB_PRODUCT}\"" \
      --build-property "build.usb_manufacturer=\"${TARGET_USB_MANUFACTURER}\"" \
      "$ROOT_DIR/${TARGET_SKETCH_PATH}"
  fi

  arduino-cli upload \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    -p "${TARGET_PORT}" \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"

  exit 0
fi

if [ "$TARGET" = "magic-button" ]; then
  arduino-cli compile \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --libraries "$ROOT_DIR/libraries" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    ${EXPORT_FLAG} \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"
else
  arduino-cli compile \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --libraries "$ROOT_DIR/libraries" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    ${EXPORT_FLAG} \
    --build-property "build.usb_product=\"${TARGET_USB_PRODUCT}\"" \
    --build-property "build.usb_manufacturer=\"${TARGET_USB_MANUFACTURER}\"" \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"
fi
