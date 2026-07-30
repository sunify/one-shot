#include <Arduino.h>
#include <USBHIDKeyboard.h>
#include <device_protocol.h>

extern "C" {
#include <ch32x035_flash.h>
}

#ifndef CH32_DEVICE_TYPE
#define CH32_DEVICE_TYPE DEVICE_TYPE_ONE_SHOT
#endif
#ifndef CH32_PRODUCT_NAME
#define CH32_PRODUCT_NAME "Magic Button"
#endif
#ifndef NUM_LEDS
#define NUM_LEDS 0
#endif
#ifndef KEYCAP_R
#define KEYCAP_R 0x5A
#endif
#ifndef KEYCAP_G
#define KEYCAP_G 0xB9
#endif
#ifndef KEYCAP_B
#define KEYCAP_B 0xCF
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
#define TOP_CASE_SHADE_R 0xFF
#endif
#ifndef TOP_CASE_SHADE_G
#define TOP_CASE_SHADE_G 0xFF
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
#ifndef TOP_CASE_SHADE_ENABLED
#define TOP_CASE_SHADE_ENABLED 0
#endif
#ifndef THIRD_ACTION_TRIGGER
#define THIRD_ACTION_TRIGGER 1
#endif

#if !defined(CH32_BUTTON_PIN)
#if defined(PB11)
#define CH32_BUTTON_PIN PB11
#elif defined(PIN_PB11)
#define CH32_BUTTON_PIN PIN_PB11
#else
#error "This Arduino core does not define PB11/PIN_PB11."
#endif
#endif

constexpr uint8_t BUTTON_PIN = CH32_BUTTON_PIN;

constexpr uint16_t DEBOUNCE_MS = 10;
constexpr uint16_t REARM_MS = 80;
constexpr uint16_t QUICK_TAP_MAX_PRESS_MS = 100;
constexpr uint16_t DOUBLE_TAP_MS = 200;
constexpr uint16_t LONG_PRESS_MS = 600;
constexpr uint8_t CONFIG_VERSION = 6;
constexpr uint32_t CONFIG_FLASH_ADDR = 0x0800F700;
constexpr uint32_t CONFIG_FLASH_SIZE = 256;
constexpr uint32_t CONFIG_FLASH_MAGIC = 0x4D423332;
constexpr uint8_t RAW_HID_KEY_OFFSET = 136;
constexpr uint8_t FEATURE_REPORT_ID = 3;
constexpr uint8_t FEATURE_REPORT_SIZE = 64;

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction singleTap;
  GestureAction doubleTap;
  GestureAction longPress;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t animationMode;
  uint8_t sleepTimeout;
  uint8_t crc;
};

struct __attribute__((packed)) StoredConfig {
  uint32_t magic;
  DeviceConfig config;
};

int stableState = HIGH;
int lastRawState = HIGH;
uint32_t lastRawChangeAt = 0;
uint32_t lastActionAt = 0;
uint32_t pressedAt = 0;
uint32_t singleTapDueAt = 0;
bool waitForSecondTap = false;
bool longPressSent = false;
DeviceConfig config;

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTap = {ACTION_TYPE_HOTKEY, 0x16, MODIFIER_ALT};
  cfg.doubleTap = {ACTION_TYPE_HOTKEY, 0x29, 0};
  cfg.longPress = {ACTION_TYPE_HOTKEY, 0x28, MODIFIER_CTRL | MODIFIER_GUI};
  cfg.red = 250;
  cfg.green = 255;
  cfg.blue = 210;
  cfg.animationMode = 1;
  cfg.sleepTimeout = 0;
  cfg.crc = 0;
  cfg.crc = computeConfigCrc(cfg);
  return cfg;
}

void applyConfig(const DeviceConfig &cfg) {
  config = cfg;
}

bool persistConfig(DeviceConfig cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);

  uint32_t page[CONFIG_FLASH_SIZE / sizeof(uint32_t)];
  for (uint16_t i = 0; i < CONFIG_FLASH_SIZE / sizeof(uint32_t); i++) {
    page[i] = 0xFFFFFFFF;
  }

  StoredConfig stored = {CONFIG_FLASH_MAGIC, cfg};
  memcpy(page, &stored, sizeof(stored));

  noInterrupts();
  FLASH_Unlock_Fast();
  FLASH_ErasePage_Fast(CONFIG_FLASH_ADDR);
  FLASH_BufReset();
  for (uint16_t i = 0; i < CONFIG_FLASH_SIZE / sizeof(uint32_t); i++) {
    FLASH_BufLoad(CONFIG_FLASH_ADDR + i * sizeof(uint32_t), page[i]);
  }
  FLASH_ProgramPage_Fast(CONFIG_FLASH_ADDR);
  FLASH_Lock_Fast();
  interrupts();

  const StoredConfig *written = reinterpret_cast<const StoredConfig *>(CONFIG_FLASH_ADDR);
  if (written->magic == CONFIG_FLASH_MAGIC && isConfigValid(written->config, CONFIG_VERSION)) {
    applyConfig(cfg);
    return true;
  }

  return false;
}

void loadConfig() {
  const StoredConfig *stored = reinterpret_cast<const StoredConfig *>(CONFIG_FLASH_ADDR);
  if (stored->magic == CONFIG_FLASH_MAGIC && isConfigValid(stored->config, CONFIG_VERSION)) {
    applyConfig(stored->config);
    return;
  }

  const DeviceConfig cfg = defaultConfig();
  if (!persistConfig(cfg)) {
    applyConfig(cfg);
  }
}

uint8_t hidModifiers(uint8_t modifiers) {
  uint8_t result = 0;
  if (modifiers & MODIFIER_CTRL) result |= 0x01;
  if (modifiers & MODIFIER_SHIFT) result |= 0x02;
  if (modifiers & MODIFIER_ALT) result |= 0x04;
  if (modifiers & MODIFIER_GUI) result |= 0x08;
  return result;
}

void sendGestureAction(const GestureAction &action) {
  if (action.type == ACTION_TYPE_HOTKEY) {
    Keyboard.releaseAll();
    const uint8_t modifiers = hidModifiers(action.modifiers);
    if (modifiers & 0x01) Keyboard.press(KEY_LEFT_CTRL);
    if (modifiers & 0x02) Keyboard.press(KEY_LEFT_SHIFT);
    if (modifiers & 0x04) Keyboard.press(KEY_LEFT_ALT);
    if (modifiers & 0x08) Keyboard.press(KEY_LEFT_GUI);
    Keyboard.press(RAW_HID_KEY_OFFSET + static_cast<uint8_t>(action.code));
    delay(12);
    Keyboard.releaseAll();
    return;
  }

  if (action.type == ACTION_TYPE_CONSUMER) {
    Keyboard.consumerPress(action.code);
    delay(12);
    Keyboard.consumerRelease();
  }
}

void setFeatureResponse(uint8_t command, const uint8_t *payload, uint8_t payloadLen) {
  uint8_t frame[FEATURE_REPORT_SIZE] = {0};
  uint8_t crc = 0;
  crc = crc8Update(crc, PROTOCOL_VERSION);
  crc = crc8Update(crc, command);
  crc = crc8Update(crc, payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    crc = crc8Update(crc, payload[i]);
  }

  frame[0] = FEATURE_REPORT_ID;
  frame[1] = FRAME_MAGIC_1;
  frame[2] = FRAME_MAGIC_2;
  frame[3] = PROTOCOL_VERSION;
  frame[4] = command;
  frame[5] = payloadLen;
  if (payloadLen > 0) {
    memcpy(frame + 6, payload, payloadLen);
  }
  frame[6 + payloadLen] = crc;
  USB_setFeatureReportResponse(frame, sizeof(frame));
}

void setStatusResponse(uint8_t command, uint8_t status) {
  const uint8_t payload[] = {status};
  setFeatureResponse(command, payload, sizeof(payload));
}

void sendButtonEventReport(uint8_t state) {
  constexpr uint8_t payloadLen = 1;
  uint8_t frame[2 + 3 + payloadLen + 1] = {0};
  frame[0] = FRAME_MAGIC_1;
  frame[1] = FRAME_MAGIC_2;
  frame[2] = PROTOCOL_VERSION;
  frame[3] = CMD_BUTTON_EVENT;
  frame[4] = payloadLen;
  frame[5] = state;

  uint8_t crc = 0;
  for (uint8_t i = 2; i < sizeof(frame) - 1; i++) {
    crc = crc8Update(crc, frame[i]);
  }
  frame[sizeof(frame) - 1] = crc;
  USB_writeVendorInputReport(frame, sizeof(frame));
}

void setConfigResponse() {
  setFeatureResponse(CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(config));
}

void setPongResponse() {
  uint8_t nameLen = strlen(CH32_PRODUCT_NAME);
  if (nameLen > FEATURE_REPORT_SIZE - 8) {
    nameLen = FEATURE_REPORT_SIZE - 8;
  }
  uint8_t payload[FEATURE_REPORT_SIZE] = {0};
  payload[0] = STATUS_OK;
  payload[1] = CH32_DEVICE_TYPE;
  memcpy(payload + 2, CH32_PRODUCT_NAME, nameLen);
  setFeatureResponse(CMD_PONG, payload, 2 + nameLen);
}

void setDeviceInfoResponse() {
  const uint8_t payload[] = {
    NUM_LEDS,
    KEYCAP_R,
    KEYCAP_G,
    KEYCAP_B,
    TOP_CASE_R,
    TOP_CASE_G,
    TOP_CASE_B,
    TOP_CASE_SHADE_R,
    TOP_CASE_SHADE_G,
    TOP_CASE_SHADE_B,
    BOTTOM_CASE_R,
    BOTTOM_CASE_G,
    BOTTOM_CASE_B,
    TOP_CASE_SHADE_ENABLED,
    THIRD_ACTION_TRIGGER,
  };
  setFeatureResponse(CMD_DEVICE_INFO, payload, sizeof(payload));
}

bool readFeatureFrame(const uint8_t *report, uint8_t reportLen, uint8_t &command, const uint8_t *&payload, uint8_t &payloadLen) {
  if (reportLen > 0 && report[0] == FEATURE_REPORT_ID) {
    report++;
    reportLen--;
  }

  if (reportLen < 6 || report[0] != FRAME_MAGIC_1 || report[1] != FRAME_MAGIC_2) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return false;
  }

  const uint8_t version = report[2];
  command = report[3];
  payloadLen = report[4];
  const uint8_t frameLen = 2 + 3 + payloadLen + 1;
  if (version != PROTOCOL_VERSION || frameLen > reportLen) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_COMMAND);
    return false;
  }

  uint8_t crc = 0;
  for (uint8_t i = 2; i < frameLen - 1; i++) {
    crc = crc8Update(crc, report[i]);
  }
  if (crc != report[frameLen - 1]) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_CRC);
    return false;
  }

  payload = report + 5;
  return true;
}

void handleSetConfig(const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(nextConfig));
  if (!isConfigValid(nextConfig, CONFIG_VERSION) ||
      !isActionValid(nextConfig.singleTap) ||
      !isActionValid(nextConfig.doubleTap) ||
      !isActionValid(nextConfig.longPress)) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  if (!persistConfig(nextConfig)) {
    setStatusResponse(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  setStatusResponse(CMD_ACK, STATUS_OK);
}

void handleFeatureReports() {
  uint8_t report[FEATURE_REPORT_SIZE] = {0};
  const uint8_t len = USB_readFeatureReport(report, sizeof(report));
  if (len == 0) {
    return;
  }

  uint8_t command = 0;
  uint8_t payloadLen = 0;
  const uint8_t *payload = nullptr;
  if (!readFeatureFrame(report, len, command, payload, payloadLen)) {
    return;
  }

  if (command == CMD_GET_CONFIG) {
    setConfigResponse();
  } else if (command == CMD_SET_CONFIG) {
    handleSetConfig(payload, payloadLen);
  } else if (command == CMD_RESET_CONFIG) {
    if (!persistConfig(defaultConfig())) {
      setStatusResponse(CMD_ERROR, STATUS_BAD_PAYLOAD);
      return;
    }
    setConfigResponse();
  } else if (command == CMD_PING) {
    setPongResponse();
  } else if (command == CMD_GET_DEVICE_INFO) {
    setDeviceInfoResponse();
  } else {
    setStatusResponse(CMD_ERROR, STATUS_BAD_COMMAND);
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  loadConfig();
  stableState = digitalRead(BUTTON_PIN);
  lastRawState = stableState;
  lastRawChangeAt = millis();

  Keyboard.begin();
  Keyboard.releaseAll();
}

void loop() {
  handleFeatureReports();

  const int rawState = digitalRead(BUTTON_PIN);
  const uint32_t now = millis();

  if (waitForSecondTap && stableState == HIGH && rawState == HIGH &&
      static_cast<int32_t>(now - singleTapDueAt) >= 0) {
    waitForSecondTap = false;
    sendGestureAction(config.singleTap);
    lastActionAt = now;
  }

  if (stableState == LOW && !longPressSent && now - pressedAt >= LONG_PRESS_MS) {
    waitForSecondTap = false;
    longPressSent = true;
    sendGestureAction(config.longPress);
    lastActionAt = now;
  }

  if (rawState != lastRawState) {
    lastRawState = rawState;
    lastRawChangeAt = now;
    return;
  }

  if (rawState == stableState || now - lastRawChangeAt < DEBOUNCE_MS) {
    return;
  }

  stableState = rawState;
  sendButtonEventReport(stableState == LOW ? BUTTON_PRESSED : BUTTON_RELEASED);

  if (stableState == LOW) {
    if (now - lastActionAt < REARM_MS) {
      return;
    }
    pressedAt = now;
    longPressSent = false;
    return;
  }

  if (longPressSent) {
    return;
  }

  const uint32_t pressDuration = now - pressedAt;
  if (pressDuration >= LONG_PRESS_MS) {
    return;
  }

  if (waitForSecondTap) {
    waitForSecondTap = false;
    sendGestureAction(config.doubleTap);
    lastActionAt = now;
    return;
  }

  if (pressDuration > QUICK_TAP_MAX_PRESS_MS) {
    sendGestureAction(config.singleTap);
    lastActionAt = now;
    return;
  }

  waitForSecondTap = true;
  singleTapDueAt = pressedAt + DOUBLE_TAP_MS;
}
