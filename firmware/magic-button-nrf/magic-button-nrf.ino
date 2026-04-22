#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "device_protocol.h"

using namespace Adafruit_LittleFS_Namespace;

namespace {

const char *DEVICE_NAME = "Super Magic Button";
const char *MANUFACTURER_NAME = "Huntflow";
const char *MODEL_NAME = "Super Magic Button";
const char *CONFIG_FILE_PATH = "/magic-button.cfg";
const bool ENABLE_SERIAL_DEBUG = false;

#ifndef BUTTON_PIN_PORT
#define BUTTON_PIN_PORT 0
#endif

#ifndef BUTTON_PIN_NUMBER
#define BUTTON_PIN_NUMBER 9
#endif

#if defined(BUTTON_PIN_PORT) && defined(BUTTON_PIN_NUMBER)
const uint32_t BUTTON_GPIO = NRF_GPIO_PIN_MAP(BUTTON_PIN_PORT, BUTTON_PIN_NUMBER);
#define USE_RAW_BUTTON_GPIO 1
#endif

#ifndef BUTTON_GROUND_PIN_PORT
#define BUTTON_GROUND_PIN_PORT 0
#endif

#ifndef BUTTON_GROUND_PIN_NUMBER
#define BUTTON_GROUND_PIN_NUMBER 6
#endif

#if defined(BUTTON_GROUND_PIN_PORT) && defined(BUTTON_GROUND_PIN_NUMBER)
const uint32_t BUTTON_GROUND_GPIO = NRF_GPIO_PIN_MAP(BUTTON_GROUND_PIN_PORT, BUTTON_GROUND_PIN_NUMBER);
#define USE_RAW_BUTTON_GROUND_GPIO 1
#else
#ifndef BUTTON_GROUND_PIN
#define BUTTON_GROUND_PIN -1
#endif
#endif

const uint8_t CONFIG_VERSION = 1;
const uint16_t LONG_PRESS_TIME = 1000;
const uint16_t DOUBLE_TAP_INIT_TIME = 100;
const uint16_t DOUBLE_TAP_TIME = 300;
const uint16_t DEBOUNCE_TIME = 30;
const uint16_t REPORT_DELAY_MS = 12;
const uint16_t STATUS_BLINK_INTERVAL_MS = 300;
const uint32_t BATTERY_UPDATE_INTERVAL_MS = 300000;
const uint32_t SYSTEM_OFF_TIMEOUT_MS = 60000;
const uint16_t ACTIVE_POLL_DELAY_MS = 5;
const uint8_t BATTERY_SAMPLE_COUNT = 3;
const uint8_t BATTERY_PERCENT_HYSTERESIS = 2;
const bool BATTERY_TEST_MODE = false;
const uint8_t BATTERY_TEST_LEVEL = 77;
const uint16_t BATTERY_TEST_MILLIVOLTS = 3850;
const float VDDH_MV_PER_LSB = 3.66210938F;
const int16_t BATTERY_CALIBRATION_OFFSET_MV = 80;
const uint32_t DEBUG_LED_GPIO = NRF_GPIO_PIN_MAP(0, 22);
const uint16_t DEBUG_LED_PULSE_MS = 40;
const uint16_t DEBUG_LED_GAP_MS = 70;
const uint16_t DEBUG_REASON_PULSE_MS = 30;
const uint16_t DEBUG_REASON_GAP_MS = 140;
const uint32_t SLEEP_DEBUG_REPEAT_MS = 2000;

enum UsbHidReportId : uint8_t {
  USB_REPORT_ID_KEYBOARD = 1,
  USB_REPORT_ID_CONSUMER_CONTROL = 2,
};

const uint8_t USB_HID_REPORT_DESCRIPTOR[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(USB_REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(USB_REPORT_ID_CONSUMER_CONTROL)),
};

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

BLEDis deviceInfo;
BLEHidAdafruit hid;
BLEBas batteryService;
Adafruit_USBD_HID usbHid;
DeviceConfig config;

bool storageReady = false;
bool buttonState = HIGH;
bool lastRawButtonState = HIGH;
bool longPressSent = false;
bool waitForDoubleTap = false;
uint32_t lastDebounceAt = 0;
uint32_t pressStartedAt = 0;
uint32_t lastReleaseAt = 0;
uint32_t lastBlinkAt = 0;
uint32_t lastBatteryUpdateAt = 0;
uint32_t lastActivityAt = 0;
uint32_t lastSleepDebugAt = 0;
bool ledState = false;
bool batteryInitialized = false;
bool usbInitialized = false;
bool usbVbusActive = false;
bool lastVbusPresent = false;
uint8_t batteryLevel = 100;
uint16_t batteryMillivolts = 4200;
uint32_t batteryRawAdc = 0;

void setupDebugLed() {
  nrf_gpio_cfg_output(DEBUG_LED_GPIO);
  nrf_gpio_pin_clear(DEBUG_LED_GPIO);
}

void setDebugLed(bool enabled) {
  if (enabled) {
    nrf_gpio_pin_set(DEBUG_LED_GPIO);
  } else {
    nrf_gpio_pin_clear(DEBUG_LED_GPIO);
  }
}

void pulseDebugLed(uint8_t pulses, uint16_t pulseMs = DEBUG_LED_PULSE_MS, uint16_t gapMs = DEBUG_LED_GAP_MS) {
  for (uint8_t i = 0; i < pulses; i++) {
    setDebugLed(true);
    delay(pulseMs);
    setDebugLed(false);

    if (i + 1 < pulses) {
      delay(gapMs);
    }
  }
}

void debugPrintln(const char *message) {
  if (!ENABLE_SERIAL_DEBUG) {
    return;
  }

  Serial.println(message);
}

void markActivity(uint32_t now = millis()) {
  lastActivityAt = now;
}

void setupButtonInput() {
#if defined(USE_RAW_BUTTON_GPIO)
  nrf_gpio_cfg_input(BUTTON_GPIO, NRF_GPIO_PIN_PULLUP);
#else
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#endif
}

int readButtonState() {
#if defined(USE_RAW_BUTTON_GPIO)
  return nrf_gpio_pin_read(BUTTON_GPIO) ? HIGH : LOW;
#else
  return digitalRead(BUTTON_PIN);
#endif
}

void setupButtonGround() {
#if defined(USE_RAW_BUTTON_GROUND_GPIO)
  nrf_gpio_cfg_output(BUTTON_GROUND_GPIO);
  nrf_gpio_pin_clear(BUTTON_GROUND_GPIO);
#elif BUTTON_GROUND_PIN >= 0
  pinMode(BUTTON_GROUND_PIN, OUTPUT);
  digitalWrite(BUTTON_GROUND_PIN, LOW);
#endif
}

uint16_t readBatteryMillivolts() {
#ifdef SAADC_CH_PSELP_PSELP_VDDHDIV5
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  delay(1);

  uint32_t rawSum = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    rawSum += analogReadVDDHDIV5();
  }
  batteryRawAdc = rawSum / BATTERY_SAMPLE_COUNT;

  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  return static_cast<uint16_t>(batteryRawAdc * VDDH_MV_PER_LSB) + BATTERY_CALIBRATION_OFFSET_MV;
#elif defined(SAADC_CH_PSELP_PSELP_VDD)
  analogReference(AR_INTERNAL_3_0);
  analogReadResolution(12);
  delay(1);

  uint32_t rawSum = 0;
  for (uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    rawSum += analogReadVDD();
  }
  batteryRawAdc = rawSum / BATTERY_SAMPLE_COUNT;

  analogReference(AR_DEFAULT);
  analogReadResolution(10);

  return static_cast<uint16_t>(batteryRawAdc * 0.73242188F) + BATTERY_CALIBRATION_OFFSET_MV;
#else
  batteryRawAdc = 0;
  return 4200;
#endif
}

uint8_t batteryPercentFromMillivolts(uint16_t millivolts) {
  struct BatteryPoint {
    uint16_t millivolts;
    uint8_t percent;
  };

  static const BatteryPoint curve[] = {
      {4200, 100},
      {4160, 98},
      {4120, 94},
      {4080, 88},
      {4040, 80},
      {4000, 70},
      {3973, 61},
      {3952, 54},
      {3934, 51},
      {3909, 44},
      {3891, 42},
      {3870, 38},
      {3858, 32},
      {3845, 25},
      {3786, 21},
      {3746, 12},
      {3710, 10},
      {3670, 8},
      {3660, 6},
      {3600, 0},
  };

  if (millivolts >= curve[0].millivolts) {
    return curve[0].percent;
  }

  const size_t lastIndex = (sizeof(curve) / sizeof(curve[0])) - 1;
  if (millivolts <= curve[lastIndex].millivolts) {
    return curve[lastIndex].percent;
  }

  for (size_t i = 0; i < lastIndex; i++) {
    const BatteryPoint &upper = curve[i];
    const BatteryPoint &lower = curve[i + 1];

    if (millivolts > lower.millivolts) {
      const uint16_t spanMv = upper.millivolts - lower.millivolts;
      const uint8_t spanPercent = upper.percent - lower.percent;
      const uint16_t offsetMv = millivolts - lower.millivolts;
      return lower.percent + static_cast<uint8_t>((offsetMv * spanPercent) / spanMv);
    }
  }

  return 0;
}

void updateBatteryLevel(bool forceNotify = false) {
  uint16_t newMillivolts = BATTERY_TEST_MODE ? BATTERY_TEST_MILLIVOLTS : readBatteryMillivolts();
  uint8_t measuredLevel = BATTERY_TEST_MODE ? BATTERY_TEST_LEVEL : batteryPercentFromMillivolts(newMillivolts);
  bool wasInitialized = batteryInitialized;
  uint8_t previousLevel = batteryLevel;

  batteryMillivolts = newMillivolts;
  if (!batteryInitialized) {
    batteryLevel = measuredLevel;
    batteryInitialized = true;
  } else {
    int delta = static_cast<int>(measuredLevel) - static_cast<int>(batteryLevel);
    if (delta < 0) {
      delta = -delta;
    }

    if (delta >= BATTERY_PERCENT_HYSTERESIS) {
      batteryLevel = measuredLevel;
    }
  }

  bool changed = (!wasInitialized) || (batteryLevel != previousLevel);
  batteryService.write(batteryLevel);

  if (changed || forceNotify) {
    batteryService.notify(batteryLevel);
  }
}

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

bool writeConfigToStorage(const DeviceConfig &cfg) {
  if (!storageReady) {
    return false;
  }

  if (InternalFS.exists(CONFIG_FILE_PATH)) {
    InternalFS.remove(CONFIG_FILE_PATH);
  }

  File file(CONFIG_FILE_PATH, FILE_O_WRITE, InternalFS);
  if (!file) {
    return false;
  }

  size_t written = file.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(DeviceConfig));
  file.close();
  return written == sizeof(DeviceConfig);
}

bool readConfigFromStorage(DeviceConfig &cfg) {
  if (!storageReady) {
    return false;
  }

  File file(CONFIG_FILE_PATH, FILE_O_READ, InternalFS);
  if (!file || file.size() != sizeof(DeviceConfig)) {
    file.close();
    return false;
  }

  int readBytes = file.read(&cfg, sizeof(DeviceConfig));
  file.close();
  return readBytes == sizeof(DeviceConfig);
}

void persistConfig(DeviceConfig &cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);
  writeConfigToStorage(cfg);
  applyConfig(cfg);
}

void loadConfig() {
  DeviceConfig stored;
  if (readConfigFromStorage(stored) && isConfigValid(stored, CONFIG_VERSION)) {
    applyConfig(stored);
    return;
  }

  stored = defaultConfig();
  persistConfig(stored);
}

void resetConfig() {
  DeviceConfig cfg = defaultConfig();
  persistConfig(cfg);
}

void sendConfigFrame() {
  sendFrame(Serial, CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

uint8_t toKeyboardModifiers(uint8_t modifiers) {
  uint8_t result = 0;

  if (modifiers & MODIFIER_CTRL) result |= KEYBOARD_MODIFIER_LEFTCTRL;
  if (modifiers & MODIFIER_SHIFT) result |= KEYBOARD_MODIFIER_LEFTSHIFT;
  if (modifiers & MODIFIER_ALT) result |= KEYBOARD_MODIFIER_LEFTALT;
  if (modifiers & MODIFIER_GUI) result |= KEYBOARD_MODIFIER_LEFTGUI;

  return result;
}

bool isUsbHidActive() {
  return usbInitialized && usbVbusActive && TinyUSBDevice.mounted() && usbHid.ready();
}

bool isRawVbusPresent() {
#if NRF_POWER_HAS_USBREG
  return nrf_power_usbregstatus_vbusdet_get(NRF_POWER) &&
         nrf_power_usbregstatus_outrdy_get(NRF_POWER);
#else
  return TinyUSBDevice.mounted();
#endif
}

bool isVbusPresent() {
  return usbVbusActive;
}

bool sendUsbKeyboardAction(uint8_t keycode, uint8_t modifiers) {
  if (!isUsbHidActive()) {
    return false;
  }

  if (TinyUSBDevice.suspended()) {
    TinyUSBDevice.remoteWakeup();
    delay(4);
  }

  uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
  uint8_t keyboardModifiers = toKeyboardModifiers(modifiers);
  if (!usbHid.keyboardReport(USB_REPORT_ID_KEYBOARD, keyboardModifiers, keycodes)) {
    return false;
  }

  delay(REPORT_DELAY_MS);
  usbHid.keyboardRelease(USB_REPORT_ID_KEYBOARD);
  return true;
}

bool sendBleKeyboardAction(uint8_t keycode, uint8_t modifiers) {
  bool sent = false;
  uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
  uint8_t keyboardModifiers = toKeyboardModifiers(modifiers);

  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected()) {
      continue;
    }

    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }

    if (!hid.keyboardReport(connHandle, keyboardModifiers, keycodes)) {
      continue;
    }

    delay(REPORT_DELAY_MS);
    hid.keyRelease(connHandle);
    sent = true;
  }

  return sent;
}

bool sendConsumerAction(uint16_t usageCode) {
  if (isUsbHidActive()) {
    if (TinyUSBDevice.suspended()) {
      TinyUSBDevice.remoteWakeup();
      delay(4);
    }

    if (!usbHid.sendReport16(USB_REPORT_ID_CONSUMER_CONTROL, usageCode)) {
      return false;
    }

    delay(REPORT_DELAY_MS);
    usbHid.sendReport16(USB_REPORT_ID_CONSUMER_CONTROL, 0);
    return true;
  }

  bool sent = false;

  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected()) {
      continue;
    }

    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }

    if (!hid.consumerKeyPress(connHandle, usageCode)) {
      continue;
    }

    delay(REPORT_DELAY_MS);
    hid.consumerKeyRelease(connHandle);
    sent = true;
  }

  return sent;
}

bool sendGestureAction(const GestureAction &action) {
  if (action.type == ACTION_TYPE_HOTKEY) {
    if (isUsbHidActive()) {
      return sendUsbKeyboardAction(static_cast<uint8_t>(action.code), action.modifiers);
    }

    return sendBleKeyboardAction(static_cast<uint8_t>(action.code), action.modifiers);
  }

  if (action.type == ACTION_TYPE_CONSUMER) {
    return sendConsumerAction(action.code);
  }

  return false;
}

void sendAction(uint8_t gestureCode) {
  GestureAction action = config.longPress;

  if (gestureCode == GESTURE_SINGLE_TAP) {
    action = config.singleTap;
  } else if (gestureCode == GESTURE_DOUBLE_TAP) {
    action = config.doubleTap;
  }

  bool sent = sendGestureAction(action);
  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("gesture=");
    Serial.print(gestureCode);
    Serial.print(" sent=");
    Serial.println(sent ? "yes" : "no");
  }
}

void handleSetConfig(const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    sendError(Serial, STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(DeviceConfig));

  if (!isConfigValid(nextConfig, CONFIG_VERSION)) {
    sendError(Serial, STATUS_BAD_CRC);
    return;
  }

  if (!isActionValid(nextConfig.singleTap) ||
      !isActionValid(nextConfig.doubleTap) ||
      !isActionValid(nextConfig.longPress)) {
    sendError(Serial, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  sendStatusFrame(Serial, CMD_ACK, STATUS_OK);
}

void handleSerial() {
  while (Serial.available() >= 2) {
    if (Serial.read() != FRAME_MAGIC_1) {
      continue;
    }

    if (Serial.read() != FRAME_MAGIC_2) {
      continue;
    }

    uint8_t header[3];
    if (!readExact(Serial, header, sizeof(header))) {
      sendError(Serial, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t version = header[0];
    uint8_t cmd = header[1];
    uint8_t payloadLen = header[2];
    uint8_t payload[sizeof(DeviceConfig)] = {0};

    if (payloadLen > sizeof(payload)) {
      sendError(Serial, STATUS_BAD_PAYLOAD);
      return;
    }

    if (!readExact(Serial, payload, payloadLen)) {
      sendError(Serial, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t receivedCrc = 0;
    if (!readExact(Serial, &receivedCrc, 1)) {
      sendError(Serial, STATUS_BAD_PAYLOAD);
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
      sendError(Serial, STATUS_BAD_COMMAND);
      continue;
    }

    if (receivedCrc != computedCrc) {
      sendError(Serial, STATUS_BAD_CRC);
      continue;
    }

    if (cmd == CMD_GET_CONFIG) {
      markActivity();
      sendConfigFrame();
    } else if (cmd == CMD_SET_CONFIG) {
      markActivity();
      handleSetConfig(payload, payloadLen);
    } else if (cmd == CMD_RESET_CONFIG) {
      markActivity();
      resetConfig();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
      markActivity();
      sendPingFrame(Serial, DEVICE_TYPE_MAGIC_BUTTON, DEVICE_NAME);
    } else {
      sendError(Serial, STATUS_BAD_COMMAND);
    }
  }
}

bool hasActiveBleConnection() {
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (connection && connection->connected()) {
      return true;
    }
  }

  return false;
}

void updateStatusLed(uint32_t now) {
#if defined(LED_BUILTIN)
  if (!isVbusPresent()) {
    digitalWrite(LED_BUILTIN, LOW);
    ledState = false;
    return;
  }

  if (hasActiveBleConnection()) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  if (now - lastBlinkAt < STATUS_BLINK_INTERVAL_MS) {
    return;
  }

  lastBlinkAt = now;
  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
#else
  (void) now;
#endif
}

void updateButton(uint32_t now) {
  int rawState = readButtonState();

  if (rawState != lastRawButtonState) {
    lastRawButtonState = rawState;
    lastDebounceAt = now;
  }

  if (now - lastDebounceAt < DEBOUNCE_TIME) {
    return;
  }

  if (rawState == buttonState) {
    if (buttonState == LOW && !longPressSent && now - pressStartedAt >= LONG_PRESS_TIME) {
      sendAction(GESTURE_LONG_PRESS);
      longPressSent = true;
      waitForDoubleTap = false;
    }

    if (waitForDoubleTap && buttonState == HIGH && now - lastReleaseAt >= DOUBLE_TAP_TIME) {
      sendAction(GESTURE_SINGLE_TAP);
      waitForDoubleTap = false;
    }

    return;
  }

  buttonState = rawState;

  if (buttonState == LOW) {
    pressStartedAt = now;
    longPressSent = false;
    markActivity(now);
    sendButtonEvent(Serial, BUTTON_PRESSED);
    return;
  }

  markActivity(now);
  sendButtonEvent(Serial, BUTTON_RELEASED);

  uint32_t pressDuration = now - pressStartedAt;
  if (longPressSent || pressDuration <= DEBOUNCE_TIME) {
    return;
  }

  if (pressDuration <= DOUBLE_TAP_INIT_TIME) {
    if (waitForDoubleTap && now - lastReleaseAt <= DOUBLE_TAP_TIME) {
      sendAction(GESTURE_DOUBLE_TAP);
      waitForDoubleTap = false;
    } else {
      waitForDoubleTap = true;
      lastReleaseAt = now;
    }
    return;
  }

  if (pressDuration < LONG_PRESS_TIME) {
    sendAction(GESTURE_SINGLE_TAP);
    waitForDoubleTap = false;
  }
}

void startAdvertising() {
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(hid);
  Bluefruit.Advertising.addService(batteryService);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);
}

void connectCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  char peerName[32] = {0};

  if (connection) {
    connection->getPeerName(peerName, sizeof(peerName));
  }

  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("connected: ");
    Serial.println(peerName[0] ? peerName : "unknown");
  }
  markActivity();
  batteryService.notify(connHandle, batteryLevel);
}

void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  (void) connHandle;

  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("disconnected: 0x");
    Serial.println(reason, HEX);
  }
  markActivity();
}

void pairCompleteCallback(uint16_t connHandle, uint8_t authStatus) {
  (void) connHandle;

  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("pairing status: 0x");
    Serial.println(authStatus, HEX);
  }
}

void securedCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  char peerName[32] = {0};

  if (connection) {
    connection->getPeerName(peerName, sizeof(peerName));
  }

  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("secured: ");
    Serial.println(peerName[0] ? peerName : "unknown");
  }
}

void setupBle() {
  Bluefruit.autoConnLed(false);
  Bluefruit.configPrphBandwidth(BANDWIDTH_NORMAL);
  Bluefruit.begin();
  Bluefruit.setTxPower(0);
  Bluefruit.setName(DEVICE_NAME);

  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  Bluefruit.Security.setPairCompleteCallback(pairCompleteCallback);
  Bluefruit.Security.setSecuredCallback(securedCallback);

  deviceInfo.setManufacturer(MANUFACTURER_NAME);
  deviceInfo.setModel(MODEL_NAME);
  deviceInfo.begin();

  batteryService.begin();
  updateBatteryLevel();
  hid.begin();
  startAdvertising();
}

void setupUsbHid() {
  if (usbInitialized) {
    TinyUSBDevice.attach();
    return;
  }

  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }

  usbHid.setPollInterval(2);
  usbHid.setReportDescriptor(USB_HID_REPORT_DESCRIPTOR, sizeof(USB_HID_REPORT_DESCRIPTOR));
  usbHid.setStringDescriptor("Super Magic Button HID");
  usbHid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  usbInitialized = true;
}

void setupStorage() {
  storageReady = InternalFS.begin();
  if (!storageReady) {
    debugPrintln("internal fs unavailable, using volatile config");
  }
}

void updateUsbState() {
  bool vbusPresent = isRawVbusPresent();

  if (vbusPresent && !lastVbusPresent) {
    if (!usbInitialized) {
      NVIC_SystemReset();
    }

    Serial.begin(SERIAL_BAUD);
    delay(50);
    setupUsbHid();
    usbVbusActive = true;
    markActivity();
  } else if (!vbusPresent && lastVbusPresent && usbVbusActive) {
    TinyUSBDevice.detach();
    Serial.end();
    usbVbusActive = false;
    markActivity();
  }

  lastVbusPresent = vbusPresent;
}

void enterSystemOff() {
#if defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, LOW);
#endif
  pulseDebugLed(3, 25, 40);
  Bluefruit.Advertising.stop();
  delay(10);

#if defined(USE_RAW_BUTTON_GPIO)
  nrf_gpio_cfg_sense_input(BUTTON_GPIO, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

  uint8_t softDeviceEnabled = 0;
  (void) sd_softdevice_is_enabled(&softDeviceEnabled);

  if (softDeviceEnabled) {
    (void) sd_power_system_off();
  } else {
    NRF_POWER->SYSTEMOFF = 1;
  }
#else
  systemOff(BUTTON_PIN, LOW);
#endif
}

bool shouldEnterSystemOff(uint32_t now) {
  if (isVbusPresent()) {
    return false;
  }

  if (hasActiveBleConnection()) {
    return false;
  }

  if (buttonState == LOW || lastRawButtonState == LOW) {
    return false;
  }

  if (waitForDoubleTap) {
    return false;
  }

  return now - lastActivityAt >= SYSTEM_OFF_TIMEOUT_MS;
}

uint8_t sleepBlockReason() {
  if (isVbusPresent()) {
    return 4;
  }

  if (hasActiveBleConnection()) {
    return 5;
  }

  if (buttonState == LOW || lastRawButtonState == LOW) {
    return 6;
  }

  if (waitForDoubleTap) {
    return 7;
  }

  return 0;
}

void idleSleep(uint32_t now) {
  if (shouldEnterSystemOff(now)) {
    enterSystemOff();
    return;
  }

  if (now - lastActivityAt >= SYSTEM_OFF_TIMEOUT_MS) {
    uint8_t reason = sleepBlockReason();
    if (reason != 0 && now - lastSleepDebugAt >= SLEEP_DEBUG_REPEAT_MS) {
      lastSleepDebugAt = now;
      pulseDebugLed(reason, DEBUG_REASON_PULSE_MS, DEBUG_REASON_GAP_MS);
    }
  }

  if (Serial.available() > 0) {
    return;
  }

  if (buttonState == LOW || lastRawButtonState == LOW || waitForDoubleTap) {
    delay(ACTIVE_POLL_DELAY_MS);
    return;
  }

  waitForEvent();
}

}  // namespace

void setup() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif
  setupDebugLed();

  const uint32_t resetReason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = resetReason;
  if (resetReason & POWER_RESETREAS_OFF_Msk) {
    pulseDebugLed(2);
  } else {
    pulseDebugLed(1);
  }

  setupButtonInput();
  setupButtonGround();

  setupStorage();
  loadConfig();
  lastVbusPresent = isRawVbusPresent();
  if (lastVbusPresent) {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    setupUsbHid();
    usbVbusActive = true;
  }
  setupBle();
  markActivity();

  if (ENABLE_SERIAL_DEBUG) {
    Serial.println();
    Serial.println("Magic Button NRF");
    Serial.print("advertising as: ");
    Serial.println(DEVICE_NAME);
#if defined(USE_RAW_BUTTON_GPIO)
    Serial.print("button gpio: P");
    Serial.print(BUTTON_PIN_PORT);
    Serial.print(".");
    Serial.println(BUTTON_PIN_NUMBER);
#endif
#ifdef SAADC_CH_PSELP_PSELP_VDDHDIV5
    Serial.println("battery source: VDDH/5");
#elif defined(SAADC_CH_PSELP_PSELP_VDD)
    Serial.println("battery source: VDD");
#endif
  }
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  if (usbInitialized && usbVbusActive) {
    TinyUSBDevice.task();
  }
#endif

  updateUsbState();

  uint32_t now = millis();
  handleSerial();
  updateButton(now);
  updateStatusLed(now);

  if (now - lastBatteryUpdateAt >= BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdateAt = now;
    updateBatteryLevel();
  }

  idleSleep(now);
}
