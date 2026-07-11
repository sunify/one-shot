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
PROFILE="${3:-}"
EXPORT_FLAG=""
EXTRA_FLAGS=""

case "$TARGET" in
  one-shot)
    TARGET_FQBN="${ARDUINO_FQBN_ONE_SHOT:-${ARDUINO_FQBN:-arduino:avr:leonardo}}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_ONE_SHOT:-${ARDUINO_SKETCH_PATH:-firmware/one-shot}}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_ONE_SHOT:-${ARDUINO_BUILD_PATH:-.arduino/build}}"

    if [ -z "$PROFILE" ]; then
      PROFILE="default"
    fi

    PROFILE_FILE="$ROOT_DIR/profiles/${PROFILE}.json"
    if [ ! -f "$PROFILE_FILE" ]; then
      echo "Profile not found: $PROFILE_FILE" >&2
      exit 1
    fi

    echo "Using profile: $PROFILE"

    TARGET_USB_PRODUCT=$(jq -r '.usb_product' "$PROFILE_FILE")
    TARGET_USB_MANUFACTURER=$(jq -r '.usb_manufacturer' "$PROFILE_FILE")

    BTN_GROUND_PIN=$(jq -r '.button_ground_pin' "$PROFILE_FILE")
    BTN_INPUT_PIN=$(jq -r '.button_input_pin' "$PROFILE_FILE")
    DATA_PIN=$(jq -r '.data_pin' "$PROFILE_FILE")
    NUM_LEDS=$(jq -r '.num_leds' "$PROFILE_FILE")

    EXTRA_FLAGS="-DBTN_GROUND_PIN=${BTN_GROUND_PIN} -DBTN_INPUT_PIN=${BTN_INPUT_PIN} -DDATA_PIN=${DATA_PIN} -DNUM_LEDS=${NUM_LEDS}"

    THIRD_ACTION_TRIGGER=$(jq -r '.third_action_trigger // "triple_tap"' "$PROFILE_FILE")
    case "$THIRD_ACTION_TRIGGER" in
      triple_tap)
        EXTRA_FLAGS="${EXTRA_FLAGS} -DTHIRD_ACTION_TRIGGER=0"
        ;;
      long_press)
        EXTRA_FLAGS="${EXTRA_FLAGS} -DTHIRD_ACTION_TRIGGER=1"
        ;;
      *)
        echo "Invalid third_action_trigger in profile: ${THIRD_ACTION_TRIGGER}" >&2
        echo "Expected: triple_tap or long_press" >&2
        exit 1
        ;;
    esac

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

    SHADE_VALUE=$(jq -r '.colors.top_case_shade // empty' "$PROFILE_FILE")
    if [ "$SHADE_VALUE" = "none" ]; then
      EXTRA_FLAGS="${EXTRA_FLAGS} -DTOP_CASE_SHADE_ENABLED=0"
    else
      append_color_flags "top_case_shade" "TOP_CASE_SHADE"
    fi

    append_color_flags "bottom_case" "BOTTOM_CASE"

    ROTARY_ENABLED=$(jq -r '.rotary_enabled // false' "$PROFILE_FILE")
    if [ "$ROTARY_ENABLED" = "true" ]; then
      ROTARY_A_PIN=$(jq -r '.rotary_a_pin' "$PROFILE_FILE")
      ROTARY_B_PIN=$(jq -r '.rotary_b_pin' "$PROFILE_FILE")
      EXTRA_FLAGS="${EXTRA_FLAGS} -DROTARY_ENABLED -DROTARY_A_PIN=${ROTARY_A_PIN} -DROTARY_B_PIN=${ROTARY_B_PIN}"
    fi
    ;;
  magic-button)
    TARGET_FQBN="${ARDUINO_FQBN_MAGIC_BUTTON:-esp32:esp32:esp32s3}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_MAGIC_BUTTON:-firmware/magic-button}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_MAGIC_BUTTON:-.arduino/build-magic-button}"
    TARGET_USB_PRODUCT="${USB_PRODUCT_MAGIC_BUTTON:-Magic Button}"
    TARGET_USB_MANUFACTURER="${USB_MANUFACTURER_MAGIC_BUTTON:-Huntflow}"
    ;;
  magic-button-nrf)
    TARGET_FQBN="${ARDUINO_FQBN_MAGIC_BUTTON_NRF:-adafruit:nrf52:mdbt50qrx}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_MAGIC_BUTTON_NRF:-firmware/magic-button-nrf}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_MAGIC_BUTTON_NRF:-.arduino/build-magic-button-nrf}"
    TARGET_USB_PRODUCT="${USB_PRODUCT_MAGIC_BUTTON_NRF:-Super Magic Button}"
    TARGET_USB_MANUFACTURER="${USB_MANUFACTURER_MAGIC_BUTTON_NRF:-Huntflow}"
    ;;
  magic-button-ch32x035)
    TARGET_FQBN="${ARDUINO_FQBN_MAGIC_BUTTON_CH32X035:-WCH:ch32v:CH32X035_EVT}"
    TARGET_SKETCH_PATH="${ARDUINO_SKETCH_PATH_MAGIC_BUTTON_CH32X035:-firmware/magic-button-ch32x035-hid}"
    TARGET_BUILD_PATH="${ARDUINO_BUILD_PATH_MAGIC_BUTTON_CH32X035:-.arduino/build-magic-button-ch32x035}"

    if [ -z "$PROFILE" ]; then
      PROFILE="magic-ch32x035"
    fi

    PROFILE_FILE="$ROOT_DIR/profiles/${PROFILE}.json"
    if [ ! -f "$PROFILE_FILE" ]; then
      echo "Profile not found: $PROFILE_FILE" >&2
      exit 1
    fi

    echo "Using profile: $PROFILE"

    TARGET_USB_PRODUCT=$(jq -r '.usb_product' "$PROFILE_FILE")
    TARGET_USB_MANUFACTURER=$(jq -r '.usb_manufacturer' "$PROFILE_FILE")
    BTN_INPUT_PIN=$(jq -r '.button_input_pin // "PB11"' "$PROFILE_FILE")
    USB_PRODUCT_C=$(jq -r '"\"" + (.usb_product | gsub(" "; "\\040")) + "\""' "$PROFILE_FILE")
    USB_MANUFACTURER_C=$(jq -r '"\"" + (.usb_manufacturer | gsub(" "; "\\040")) + "\""' "$PROFILE_FILE")

    EXTRA_FLAGS="-DCH32_BUTTON_PIN=${BTN_INPUT_PIN} -DWCH_USBHID_PROD_STR=${USB_PRODUCT_C} -DWCH_USBHID_MANUF_STR=${USB_MANUFACTURER_C}"
    ;;
  *)
    echo "Unknown firmware target: $TARGET" >&2
    exit 1
    ;;
esac

if [ "$MODE" = "export" ]; then
  EXPORT_FLAG="--export-binaries"
fi

compile_one_shot() {
  EXTRA_ARGS="${1:-}"
  arduino-cli compile \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --libraries "$ROOT_DIR/libraries" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    --build-property "build.usb_product=\"${TARGET_USB_PRODUCT}\"" \
    --build-property "build.usb_manufacturer=\"${TARGET_USB_MANUFACTURER}\"" \
    --build-property "compiler.cpp.extra_flags=${EXTRA_FLAGS}" \
    --build-property "compiler.c.extra_flags=${EXTRA_FLAGS}" \
    ${EXTRA_ARGS} \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"
}

compile_magic_button() {
  EXTRA_ARGS="${1:-}"
  arduino-cli compile \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --libraries "$ROOT_DIR/libraries" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    --build-property "build.usb_product=\"${TARGET_USB_PRODUCT}\"" \
    --build-property "build.usb_manufacturer=\"${TARGET_USB_MANUFACTURER}\"" \
    --build-property "compiler.cpp.extra_flags=${EXTRA_FLAGS}" \
    --build-property "compiler.c.extra_flags=${EXTRA_FLAGS}" \
    ${EXTRA_ARGS} \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"
}

if [ "$MODE" = "upload" ]; then
  if [ "$TARGET" = "magic-button-ch32x035" ]; then
    compile_magic_button
    arduino-cli upload \
      --config-file "$ROOT_DIR/arduino-cli.yaml" \
      -b "${TARGET_FQBN}" \
      --input-dir "$ROOT_DIR/${TARGET_BUILD_PATH}" \
      "$ROOT_DIR/${TARGET_SKETCH_PATH}"

    exit 0
  fi

  PORTS_JSON=$(arduino-cli board list --format json 2>/dev/null)

  # Build serial->product name map from ioreg
  USB_NAMES=$(ioreg -p IOUSB -l 2>/dev/null | awk '
    /"USB Product Name"/ { gsub(/.*= "/, ""); gsub(/"/, ""); name=$0 }
    /"USB Serial Number"/ { gsub(/.*= "/, ""); gsub(/"/, ""); if (name) print $0 "|" name; name="" }
  ')

  PORTS=$(echo "$PORTS_JSON" | jq -r '.detected_ports[] | select(.port.protocol_label == "Serial Port (USB)" and (.matching_boards | length > 0)) | "\(.port.address)|\(.matching_boards[0].name // "Unknown")|\(.port.hardware_id // "")"')

  # Enrich ports with USB product names
  ENRICHED_PORTS=""
  echo "$PORTS" | while IFS='|' read -r addr board_name hw_id; do
    usb_name=$(echo "$USB_NAMES" | grep "^${hw_id}|" | head -1 | cut -d'|' -f2)
    if [ -n "$usb_name" ]; then
      printf '%s\n' "$addr|$usb_name|$board_name"
    else
      printf '%s\n' "$addr|$board_name|$hw_id"
    fi
  done > /tmp/oneshot_ports.tmp
  PORTS=$(cat /tmp/oneshot_ports.tmp)
  rm -f /tmp/oneshot_ports.tmp

  # Filter ports by target: one-shot hides ESP boards, magic-button hides Arduino boards
  if [ "$TARGET" = "one-shot" ]; then
    PORTS=$(echo "$PORTS" | grep -iv "esp" || true)
  elif [ "$TARGET" = "magic-button" ] || [ "$TARGET" = "magic-button-nrf" ]; then
    PORTS=$(echo "$PORTS" | grep -iv "arduino\|leonardo\|mega\|uno\|nano" || true)
  fi

  if [ -z "$PORTS" ]; then
    echo "No USB serial ports found." >&2
    exit 1
  fi

  PORT_COUNT=$(echo "$PORTS" | wc -l | tr -d ' ')

  if [ "$PORT_COUNT" -eq 1 ]; then
    TARGET_PORT=$(echo "$PORTS" | cut -d'|' -f1)
    PORT_LABEL=$(echo "$PORTS" | cut -d'|' -f2)
    PORT_EXTRA=$(echo "$PORTS" | cut -d'|' -f3)
    echo "Using port: $TARGET_PORT ($PORT_LABEL, $PORT_EXTRA)"
  else
    echo "Select port:"
    i=1
    echo "$PORTS" | while IFS='|' read -r addr label extra; do
      echo "  $i) $addr ($label, $extra)"
      i=$((i + 1))
    done

    printf "Enter number [1-%s]: " "$PORT_COUNT"
    read -r CHOICE

    if [ -z "$CHOICE" ] || [ "$CHOICE" -lt 1 ] 2>/dev/null || [ "$CHOICE" -gt "$PORT_COUNT" ] 2>/dev/null; then
      echo "Invalid choice." >&2
      exit 1
    fi

    TARGET_PORT=$(echo "$PORTS" | sed -n "${CHOICE}p" | cut -d'|' -f1)
    echo "Using port: $TARGET_PORT"
  fi

  if [ "$TARGET" = "magic-button" ] || [ "$TARGET" = "magic-button-nrf" ]; then
    compile_magic_button
  else
    compile_one_shot
  fi

  arduino-cli upload \
    --config-file "$ROOT_DIR/arduino-cli.yaml" \
    -b "${TARGET_FQBN}" \
    --build-path "$ROOT_DIR/${TARGET_BUILD_PATH}" \
    -p "${TARGET_PORT}" \
    "$ROOT_DIR/${TARGET_SKETCH_PATH}"

  exit 0
fi

if [ "$TARGET" = "magic-button" ] || [ "$TARGET" = "magic-button-nrf" ] || [ "$TARGET" = "magic-button-ch32x035" ]; then
  compile_magic_button "${EXPORT_FLAG}"
else
  compile_one_shot "${EXPORT_FLAG}"
fi
