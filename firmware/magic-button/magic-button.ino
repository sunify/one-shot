#ifndef USB_PRODUCT
#define USB_PRODUCT "Magic Button"
#endif

#ifndef USB_MANUFACTURER
#define USB_MANUFACTURER "Huntflow"
#endif

#include "USB.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDKeyboard.h"
#include <Preferences.h>
#include "device_protocol.h"

#if !ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial;
#endif

#if ARDUINO_USB_CDC_ON_BOOT
#define SERIAL_PORT Serial
#else
#define SERIAL_PORT USBSerial
#endif

USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;
Preferences preferences;

#define BUTTON_PIN 7

const uint16_t LONG_PRESS_TIME = 1000;
const uint16_t DOUBLE_TAP_INIT_TIME = 100;
const uint16_t DOUBLE_TAP_TIME = 300;
const uint16_t DEBOUNCE_TIME = 50;

enum GestureCode : uint8_t {
  GESTURE_SINGLE_TAP = 1,
  GESTURE_DOUBLE_TAP = 2,
  GESTURE_LONG_PRESS = 3
};

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction singleTap;
  GestureAction doubleTap;
  GestureAction longPress;
  uint8_t crc;
};

const uint8_t CONFIG_VERSION = 1;
const char *PREFERENCES_NAMESPACE = "magic-btn";
const char *PREFERENCES_KEY = "config";

DeviceConfig config;

int state = HIGH;
uint32_t pressTime = 0;
bool sent = false;
bool waitForDoubleTap = false;

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTap = {ACTION_TYPE_HOTKEY, 0x16, MODIFIER_ALT};
  cfg.doubleTap = {ACTION_TYPE_HOTKEY, 0x29, 0};
  cfg.longPress = {ACTION_TYPE_HOTKEY, 0x28, MODIFIER_CTRL | MODIFIER_GUI};
  cfg.crc = 0;
  return cfg;
}

void applyConfig(const DeviceConfig &cfg) {
  config = cfg;
}

void persistConfig(DeviceConfig &cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);
  preferences.putBytes(PREFERENCES_KEY, &cfg, sizeof(DeviceConfig));
  applyConfig(cfg);
}

void loadConfig() {
  DeviceConfig stored;
  size_t readBytes = preferences.getBytes(PREFERENCES_KEY, &stored, sizeof(DeviceConfig));

  if (readBytes != sizeof(DeviceConfig) || !isConfigValid(stored, CONFIG_VERSION)) {
    stored = defaultConfig();
    persistConfig(stored);
    return;
  }

  applyConfig(stored);
}

void resetConfig() {
  DeviceConfig cfg = defaultConfig();
  persistConfig(cfg);
}

void sendConfigFrame() {
  sendFrame(SERIAL_PORT, CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

void pressModifiers(uint8_t modifiers) {
  if (modifiers & MODIFIER_CTRL) Keyboard.press(KEY_LEFT_CTRL);
  if (modifiers & MODIFIER_SHIFT) Keyboard.press(KEY_LEFT_SHIFT);
  if (modifiers & MODIFIER_ALT) Keyboard.press(KEY_LEFT_ALT);
  if (modifiers & MODIFIER_GUI) Keyboard.press(KEY_LEFT_GUI);
}

void sendGestureAction(const GestureAction &action) {
  if (action.type == ACTION_TYPE_HOTKEY) {
    pressModifiers(action.modifiers);
    Keyboard.pressRaw(static_cast<uint8_t>(action.code));
    delay(12);
    Keyboard.releaseAll();
    return;
  }

  ConsumerControl.press(action.code);
  delay(12);
  ConsumerControl.release();
}

void sendAction(uint8_t gestureCode) {
  GestureAction action = config.longPress;

  if (gestureCode == GESTURE_SINGLE_TAP) {
    action = config.singleTap;
  } else if (gestureCode == GESTURE_DOUBLE_TAP) {
    action = config.doubleTap;
  }

  sendGestureAction(action);
}

void handleSetConfig(const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(DeviceConfig));

  if (!isConfigValid(nextConfig, CONFIG_VERSION)) {
    sendError(SERIAL_PORT, STATUS_BAD_CRC);
    return;
  }

  if (!isActionValid(nextConfig.singleTap) ||
      !isActionValid(nextConfig.doubleTap) ||
      !isActionValid(nextConfig.longPress)) {
    sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  sendStatusFrame(SERIAL_PORT, CMD_ACK, STATUS_OK);
}

void handleSerial() {
  while (SERIAL_PORT.available() >= 2) {
    if (SERIAL_PORT.read() != FRAME_MAGIC_1) {
      continue;
    }

    if (SERIAL_PORT.read() != FRAME_MAGIC_2) {
      continue;
    }

    uint8_t header[3];
    if (!readExact(SERIAL_PORT, header, sizeof(header))) {
      sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t version = header[0];
    uint8_t cmd = header[1];
    uint8_t payloadLen = header[2];
    uint8_t payload[sizeof(DeviceConfig)] = {0};

    if (payloadLen > sizeof(payload)) {
      sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
      return;
    }

    if (!readExact(SERIAL_PORT, payload, payloadLen)) {
      sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t receivedCrc = 0;
    if (!readExact(SERIAL_PORT, &receivedCrc, 1)) {
      sendError(SERIAL_PORT, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t computedCrc = 0;
    computedCrc = crc8Update(computedCrc, version);
    computedCrc = crc8Update(computedCrc, cmd);
    computedCrc = crc8Update(computedCrc, payloadLen);
    for (uint8_t i = 0; i < payloadLen; i++) {
      computedCrc = crc8Update(computedCrc, payload[i]);
    }

    if (version != PROTOCOL_VERSION) {
      sendError(SERIAL_PORT, STATUS_BAD_COMMAND);
      continue;
    }

    if (receivedCrc != computedCrc) {
      sendError(SERIAL_PORT, STATUS_BAD_CRC);
      continue;
    }

    if (cmd == CMD_GET_CONFIG) {
      sendConfigFrame();
    } else if (cmd == CMD_SET_CONFIG) {
      handleSetConfig(payload, payloadLen);
    } else if (cmd == CMD_RESET_CONFIG) {
      resetConfig();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
      sendPingFrame(SERIAL_PORT, DEVICE_TYPE_MAGIC_BUTTON);
    } else {
      sendError(SERIAL_PORT, STATUS_BAD_COMMAND);
    }
  }
}

void updateButton() {
  int newState = digitalRead(BUTTON_PIN);
  uint32_t timeSinceLastPress = millis() - pressTime;

  if (state != newState) {
    if (newState == LOW) {
      sendButtonEvent(SERIAL_PORT, BUTTON_PRESSED);
      if (!waitForDoubleTap) {
        pressTime = millis();
      }
    } else {
      sendButtonEvent(SERIAL_PORT, BUTTON_RELEASED);
      if (timeSinceLastPress <= DOUBLE_TAP_INIT_TIME) {
        waitForDoubleTap = true;
      } else if (timeSinceLastPress < LONG_PRESS_TIME) {
        if (!sent) {
          if (waitForDoubleTap) {
            sendAction(GESTURE_DOUBLE_TAP);
            waitForDoubleTap = false;
          } else if (timeSinceLastPress > DEBOUNCE_TIME) {
            sendAction(GESTURE_SINGLE_TAP);
          }
          sent = true;
        }
      }

      if (sent) {
        sent = false;
        waitForDoubleTap = false;
        delay(300);
      }
    }
  } else {
    if (state == LOW && timeSinceLastPress >= LONG_PRESS_TIME && !sent) {
      sendAction(GESTURE_LONG_PRESS);
      sent = true;
    }
  }

  if (waitForDoubleTap && timeSinceLastPress >= DOUBLE_TAP_TIME && state == HIGH) {
    sendAction(GESTURE_SINGLE_TAP);
    waitForDoubleTap = false;
  }

  state = newState;
  delay(50);
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  preferences.begin(PREFERENCES_NAMESPACE, false);
  loadConfig();

  USB.productName(USB_PRODUCT);
  USB.manufacturerName(USB_MANUFACTURER);
  SERIAL_PORT.begin(SERIAL_BAUD);
  Keyboard.begin();
  ConsumerControl.begin();
  USB.begin();
}

void loop() {
  handleSerial();
  updateButton();
}
