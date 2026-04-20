#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "nrf_gpio.h"
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
const uint8_t BATTERY_SAMPLE_COUNT = 3;
const uint8_t BATTERY_PERCENT_HYSTERESIS = 2;
const bool BATTERY_TEST_MODE = false;
const uint8_t BATTERY_TEST_LEVEL = 77;
const uint16_t BATTERY_TEST_MILLIVOLTS = 3850;
const float VDDH_MV_PER_LSB = 3.66210938F;

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
bool ledState = false;
bool batteryInitialized = false;
uint8_t batteryLevel = 100;
uint16_t batteryMillivolts = 4200;
uint32_t batteryRawAdc = 0;

void debugPrintln(const char *message) {
  if (!ENABLE_SERIAL_DEBUG) {
    return;
  }

  Serial.println(message);
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

  return static_cast<uint16_t>(batteryRawAdc * VDDH_MV_PER_LSB);
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

  return static_cast<uint16_t>(batteryRawAdc * 0.73242188F);
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
      {4120, 95},
      {4080, 90},
      {4040, 84},
      {4000, 78},
      {3960, 70},
      {3920, 62},
      {3880, 54},
      {3840, 46},
      {3800, 38},
      {3760, 30},
      {3720, 22},
      {3680, 16},
      {3640, 12},
      {3600, 9},
      {3500, 4},
      {3300, 0},
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
  return TinyUSBDevice.mounted() && usbHid.ready();
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
      sendConfigFrame();
    } else if (cmd == CMD_SET_CONFIG) {
      handleSetConfig(payload, payloadLen);
    } else if (cmd == CMD_RESET_CONFIG) {
      resetConfig();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
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
    sendButtonEvent(Serial, BUTTON_PRESSED);
    return;
  }

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
  batteryService.notify(connHandle, batteryLevel);
}

void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  (void) connHandle;

  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("disconnected: 0x");
    Serial.println(reason, HEX);
  }
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
}

void setupStorage() {
  storageReady = InternalFS.begin();
  if (!storageReady) {
    debugPrintln("internal fs unavailable, using volatile config");
  }
}

}  // namespace

void setup() {
#if defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif

  setupButtonInput();
  setupButtonGround();
  Serial.begin(SERIAL_BAUD);
  delay(50);

  setupStorage();
  loadConfig();
  setupUsbHid();
  setupBle();

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
  TinyUSBDevice.task();
#endif

  uint32_t now = millis();
  handleSerial();
  updateButton(now);
  updateStatusLed(now);

  if (now - lastBatteryUpdateAt >= BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdateAt = now;
    updateBatteryLevel();
  }
}
