#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <FreeRTOS.h>
#include <task.h>
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "device_protocol.h"

using namespace Adafruit_LittleFS_Namespace;

namespace {

const char *DEVICE_NAME = "rrrraw";
const char *CONFIG_FILE_PATH = "/rrrraw.cfg";
const uint8_t CONFIG_VERSION = 2;
const uint16_t DEBOUNCE_MS = 12;
const uint16_t LONG_PRESS_MS = 600;
const uint16_t CONFIG_CHORD_HOLD_MS = 2000;
const uint32_t CONFIG_ADVERTISING_TIMEOUT_MS = 60UL * 1000UL;
const uint32_t IDLE_SLEEP_TIMEOUT_MS = 15UL * 1000UL;
const uint32_t DEEP_SLEEP_TIMEOUT_MS = 4UL * 60UL * 60UL * 1000UL;
const uint32_t DISCONNECTED_DEEP_SLEEP_TIMEOUT_MS = 90UL * 60UL * 1000UL;
const uint32_t MAX_IDLE_BLOCK_MS = 15UL * 1000UL;
const uint32_t PENDING_WAKE_ACTION_MAX_AGE_MS = 2UL * 60UL * 1000UL;
const uint16_t PENDING_WAKE_ACTION_READY_DELAY_MS = 500;
const uint32_t BATTERY_UPDATE_INTERVAL_MS = 5UL * 60UL * 1000UL;
const uint8_t BATTERY_SAMPLE_COUNT = 3;
const uint16_t ACTIVE_INPUT_POLL_MS = 1;
const uint16_t BLE_CONN_INTERVAL_MIN = 9;
const uint16_t BLE_CONN_INTERVAL_MAX = 12;
const uint16_t BLE_IDLE_CONN_INTERVAL = 12;
const uint16_t BLE_IDLE_CONN_SLAVE_LATENCY = 15;
const uint16_t BLE_CONN_SUPERVISION_TIMEOUT = 400;
const uint32_t BLE_IDLE_PARAMS_DELAY_MS = 2000;
// Production mode: expose the configuration connection only after the
// two outer buttons have been held for CONFIG_CHORD_HOLD_MS.
const bool ALWAYS_ADVERTISE_CONFIG = false;
const uint8_t BUTTON_COUNT = 4;

// Defaults use exposed nice!nano-compatible GPIOs and avoid NFC, reset,
// external-flash and the modified SuperMini power-control pin P0.13.
#ifndef RRRRAW_BUTTON1_PORT
#define RRRRAW_BUTTON1_PORT 0
#define RRRRAW_BUTTON1_PIN 17
#endif
#ifndef RRRRAW_BUTTON2_PORT
#define RRRRAW_BUTTON2_PORT 0
#define RRRRAW_BUTTON2_PIN 20
#endif
#ifndef RRRRAW_BUTTON3_PORT
#define RRRRAW_BUTTON3_PORT 0
#define RRRRAW_BUTTON3_PIN 22
#endif
#ifndef RRRRAW_ENCODER_BUTTON_PORT
#define RRRRAW_ENCODER_BUTTON_PORT 0
#define RRRRAW_ENCODER_BUTTON_PIN 24
#endif
#ifndef RRRRAW_ENCODER_A_PORT
#define RRRRAW_ENCODER_A_PORT 1
#define RRRRAW_ENCODER_A_PIN 0
#endif
#ifndef RRRRAW_ENCODER_B_PORT
#define RRRRAW_ENCODER_B_PORT 0
#define RRRRAW_ENCODER_B_PIN 11
#endif

const uint32_t BUTTON_GPIOS[BUTTON_COUNT] = {
    NRF_GPIO_PIN_MAP(RRRRAW_BUTTON1_PORT, RRRRAW_BUTTON1_PIN),
    NRF_GPIO_PIN_MAP(RRRRAW_BUTTON2_PORT, RRRRAW_BUTTON2_PIN),
    NRF_GPIO_PIN_MAP(RRRRAW_BUTTON3_PORT, RRRRAW_BUTTON3_PIN),
    NRF_GPIO_PIN_MAP(RRRRAW_ENCODER_BUTTON_PORT, RRRRAW_ENCODER_BUTTON_PIN),
};
const uint32_t ENCODER_A_GPIO = NRF_GPIO_PIN_MAP(RRRRAW_ENCODER_A_PORT, RRRRAW_ENCODER_A_PIN);
const uint32_t ENCODER_B_GPIO = NRF_GPIO_PIN_MAP(RRRRAW_ENCODER_B_PORT, RRRRAW_ENCODER_B_PIN);

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction button1Single;
  GestureAction button1Long;
  GestureAction button2Single;
  GestureAction button2Long;
  GestureAction button3Single;
  GestureAction button3Long;
  GestureAction encoderPressSingle;
  GestureAction encoderPressLong;
  GestureAction encoderCW;
  GestureAction encoderCCW;
  uint8_t crc;
};

static_assert(sizeof(DeviceConfig) == 42, "Unexpected rrrraw config size");

struct ButtonRuntime {
  bool rawPressed;
  bool pressed;
  bool longTriggered;
  uint8_t heldModifiers;
  uint32_t rawChangedAt;
  uint32_t pressedAt;
};

BLEDis deviceInfo;
BLEHidAdafruit hid;
BLEBas batteryService;
BLEUart configBle;
DeviceConfig config;
ButtonRuntime buttons[BUTTON_COUNT] = {};
bool storageReady = false;
uint8_t encoderState = 0;
int8_t encoderAccumulator = 0;
int16_t pendingEncoderSteps = 0;
bool configAdvertisingActive = false;
uint32_t configAdvertisingUntil = 0;
uint32_t configChordStartedAt = 0;
bool configChordTriggered = false;
uint8_t batteryLevel = 100;
uint32_t lastBatteryUpdateAt = 0;
uint32_t bleSecuredAt = 0;
bool bleIdleParamsRequested = false;
bool usbVbusActive = false;
bool lastVbusPresent = false;
bool inputWakeInterruptsAttached = false;
bool sleeping = false;
uint32_t lastActivityAt = 0;
int8_t pendingWakeButton = -1;
uint32_t pendingWakeActionAt = 0;
TaskHandle_t loopTaskHandle = nullptr;

void exitSleep();

void markActivity(uint32_t now = millis()) {
  lastActivityAt = now;
}

bool isRawVbusPresent() {
#if NRF_POWER_HAS_USBREG
  return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
#else
  return TinyUSBDevice.mounted();
#endif
}

void updateUsbState() {
  const bool vbusPresent = isRawVbusPresent();
  if (vbusPresent && !lastVbusPresent) {
    // TinyUSB is only initialized during setup when VBUS is already present.
    // Reboot once on insertion so USB serial comes up cleanly.
    NVIC_SystemReset();
  } else if (!vbusPresent && lastVbusPresent && usbVbusActive) {
    TinyUSBDevice.detach();
    Serial.end();
    usbVbusActive = false;
    markActivity();
  }
  lastVbusPresent = vbusPresent;
}

void wakeLoopTask() {
  if (loopTaskHandle) xTaskNotifyGive(loopTaskHandle);
}

void inputWakeInterrupt() {
  if (!loopTaskHandle) return;
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(loopTaskHandle, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

bool remapInterruptChannel(int interruptMask, uint8_t port, uint8_t pin) {
  if (interruptMask == 0) return false;
  const uint8_t channel = static_cast<uint8_t>(__builtin_ctz(static_cast<unsigned>(interruptMask)));
  uint32_t configValue = NRF_GPIOTE->CONFIG[channel];
  configValue &= ~(GPIOTE_CONFIG_PSEL_Msk | GPIOTE_CONFIG_PORT_Msk);
  configValue |= ((uint32_t) pin << GPIOTE_CONFIG_PSEL_Pos) |
                 ((uint32_t) port << GPIOTE_CONFIG_PORT_Pos);
  NRF_GPIOTE->CONFIG[channel] = configValue;
  return true;
}

void setupInputWakeInterrupts() {
  loopTaskHandle = xTaskGetCurrentTaskHandle();
  const uint8_t ports[] = {
      RRRRAW_BUTTON1_PORT,
      RRRRAW_BUTTON2_PORT,
      RRRRAW_BUTTON3_PORT,
      RRRRAW_ENCODER_BUTTON_PORT,
      RRRRAW_ENCODER_A_PORT,
      RRRRAW_ENCODER_B_PORT,
  };
  const uint8_t pins[] = {
      RRRRAW_BUTTON1_PIN,
      RRRRAW_BUTTON2_PIN,
      RRRRAW_BUTTON3_PIN,
      RRRRAW_ENCODER_BUTTON_PIN,
      RRRRAW_ENCODER_A_PIN,
      RRRRAW_ENCODER_B_PIN,
  };

  inputWakeInterruptsAttached = true;
  for (uint8_t index = 0; index < 6; index++) {
    const int interruptMask = attachInterrupt(index, inputWakeInterrupt, CHANGE);
    if (!remapInterruptChannel(interruptMask, ports[index], pins[index])) {
      inputWakeInterruptsAttached = false;
    }
  }
}

void ensureBleFastProfile() {
  if (!bleIdleParamsRequested) return;
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (!connection || !connection->connected() || !connection->secured()) continue;
    connection->requestConnectionParameter(
        BLE_CONN_INTERVAL_MIN, 0, BLE_CONN_SUPERVISION_TIMEOUT);
  }
  bleIdleParamsRequested = false;
  if (bleSecuredAt != 0) bleSecuredAt = millis();
}

void updateBlePowerProfile(uint32_t now) {
  if (bleIdleParamsRequested || bleSecuredAt == 0 ||
      now - bleSecuredAt < BLE_IDLE_PARAMS_DELAY_MS) return;
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (!connection || !connection->connected() || !connection->secured()) continue;
    connection->requestConnectionParameter(
        BLE_IDLE_CONN_INTERVAL,
        BLE_IDLE_CONN_SLAVE_LATENCY,
        BLE_CONN_SUPERVISION_TIMEOUT);
  }
  bleIdleParamsRequested = true;
}

void connectCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  if (connection) {
    connection->requestConnectionParameter(
        BLE_CONN_INTERVAL_MIN, 0, BLE_CONN_SUPERVISION_TIMEOUT);
  }
  bleSecuredAt = 0;
  bleIdleParamsRequested = false;
  exitSleep();
  markActivity();
  wakeLoopTask();
}

void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  (void) connHandle;
  (void) reason;
  bleSecuredAt = 0;
  bleIdleParamsRequested = false;
  if (!sleeping) markActivity();
  wakeLoopTask();
}

void securedCallback(uint16_t connHandle) {
  (void) connHandle;
  bleSecuredAt = millis();
  bleIdleParamsRequested = false;
  exitSleep();
  markActivity();
  wakeLoopTask();
}

void flushProtocolTransport(Stream &transport) {
  if (&transport == &configBle) configBle.flushTXD();
}

void sendProtocolFrame(Stream &transport, uint8_t command, const uint8_t *payload, uint8_t length) {
  sendFrame(transport, command, payload, length);
  flushProtocolTransport(transport);
}

void sendProtocolStatus(Stream &transport, uint8_t command, uint8_t status) {
  const uint8_t payload[1] = {status};
  sendProtocolFrame(transport, command, payload, sizeof(payload));
}

void sendProtocolError(Stream &transport, uint8_t status) {
  sendProtocolStatus(transport, CMD_ERROR, status);
}

void sendProtocolPing(Stream &transport) {
  const uint8_t payload[2] = {STATUS_OK, DEVICE_TYPE_RRRRAW};
  sendProtocolFrame(transport, CMD_PONG, payload, sizeof(payload));
}

GestureAction &singleAction(uint8_t index) {
  switch (index) {
    case 0: return config.button1Single;
    case 1: return config.button2Single;
    case 2: return config.button3Single;
    default: return config.encoderPressSingle;
  }
}

GestureAction &longAction(uint8_t index) {
  switch (index) {
    case 0: return config.button1Long;
    case 1: return config.button2Long;
    case 2: return config.button3Long;
    default: return config.encoderPressLong;
  }
}

bool isModifierOnly(const GestureAction &action) {
  return action.type == ACTION_TYPE_HOTKEY && action.code == 0 && action.modifiers != 0;
}

uint8_t keyboardModifiers(uint8_t modifiers) {
  uint8_t result = 0;
  if (modifiers & MODIFIER_CTRL) result |= KEYBOARD_MODIFIER_LEFTCTRL;
  if (modifiers & MODIFIER_SHIFT) result |= KEYBOARD_MODIFIER_LEFTSHIFT;
  if (modifiers & MODIFIER_ALT) result |= KEYBOARD_MODIFIER_LEFTALT;
  if (modifiers & MODIFIER_GUI) result |= KEYBOARD_MODIFIER_LEFTGUI;
  return result;
}

uint8_t activeModifierMask() {
  uint8_t result = 0;
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    result |= buttons[index].heldModifiers;
  }
  return result;
}

template <typename Callback>
void forEachSecuredConnection(Callback callback) {
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (connection && connection->connected() && connection->secured()) callback(handle);
  }
}

void sendKeyboardState() {
  uint8_t keys[6] = {0};
  const uint8_t modifiers = keyboardModifiers(activeModifierMask());
  forEachSecuredConnection([&](uint16_t handle) { hid.keyboardReport(handle, modifiers, keys); });
}

void setHeldModifiers(uint8_t index, uint8_t modifiers) {
  buttons[index].heldModifiers = modifiers;
  sendKeyboardState();
}

void sampleEncoder();

void delayWithEncoderSampling(uint16_t durationMs) {
  const uint32_t startedAt = millis();
  while (millis() - startedAt < durationMs) {
    sampleEncoder();
    delay(1);
  }
}

void sendHotkey(const GestureAction &action) {
  ensureBleFastProfile();
  uint8_t keys[6] = {static_cast<uint8_t>(action.code), 0, 0, 0, 0, 0};
  const uint8_t pressedModifiers = keyboardModifiers(activeModifierMask() | action.modifiers);
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (!connection || !connection->connected()) continue;
    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }
    hid.keyboardReport(handle, pressedModifiers, keys);
  }
  delayWithEncoderSampling(12);
  sendKeyboardState();
}

void sendConsumer(uint16_t code) {
  ensureBleFastProfile();
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (!connection || !connection->connected()) continue;
    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }
    hid.consumerKeyPress(handle, code);
    delayWithEncoderSampling(12);
    hid.consumerKeyRelease(handle);
  }
}

void sendMouse(const GestureAction &action, int8_t direction) {
  ensureBleFastProfile();
  const uint8_t axis = action.code & 0xFF;
  const int8_t amount = constrain(static_cast<int16_t>(action.code >> 8) * direction, -127, 127);
  const bool withModifiers = action.modifiers != 0 || activeModifierMask() != 0;
  if (withModifiers) {
    uint8_t keys[6] = {0};
    const uint8_t modifiers = keyboardModifiers(activeModifierMask() | action.modifiers);
    forEachSecuredConnection([&](uint16_t handle) { hid.keyboardReport(handle, modifiers, keys); });
  }
  forEachSecuredConnection([&](uint16_t handle) {
    if (axis == 0) hid.mouseScroll(handle, amount);
    else if (axis == 1) hid.mouseMove(handle, amount, 0);
    else if (axis == 2) hid.mouseMove(handle, 0, amount);
  });
  if (withModifiers) {
    delayWithEncoderSampling(12);
    sendKeyboardState();
  }
}

void sendAction(const GestureAction &action, int8_t direction = 1) {
  if (action.type == ACTION_TYPE_HOTKEY) sendHotkey(action);
  else if (action.type == ACTION_TYPE_CONSUMER) sendConsumer(action.code);
  else if (action.type == ACTION_TYPE_MOUSE) sendMouse(action, direction);
}

DeviceConfig defaultConfig() {
  DeviceConfig next = {};
  next.version = CONFIG_VERSION;
  next.button1Single = {ACTION_TYPE_HOTKEY, 0x1E, 0};
  next.button1Long = {ACTION_TYPE_HOTKEY, 0x1F, 0};
  next.button2Single = {ACTION_TYPE_HOTKEY, 0x20, 0};
  next.button2Long = {ACTION_TYPE_HOTKEY, 0x21, 0};
  next.button3Single = {ACTION_TYPE_HOTKEY, 0x22, 0};
  next.button3Long = {ACTION_TYPE_HOTKEY, 0x23, 0};
  next.encoderPressSingle = {ACTION_TYPE_HOTKEY, 0x24, 0};
  next.encoderPressLong = {ACTION_TYPE_HOTKEY, 0x25, 0};
  next.encoderCW = {ACTION_TYPE_HOTKEY, 0x26, 0};
  next.encoderCCW = {ACTION_TYPE_HOTKEY, 0x27, 0};
  next.crc = computeConfigCrc(next);
  return next;
}

bool configActionsValid(const DeviceConfig &candidate) {
  const GestureAction *actions = &candidate.button1Single;
  for (uint8_t index = 0; index < 10; index++) {
    if (!isActionValid(actions[index])) return false;
  }
  return true;
}

void saveConfig(DeviceConfig &next) {
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) buttons[index].heldModifiers = 0;
  sendKeyboardState();
  next.version = CONFIG_VERSION;
  next.crc = computeConfigCrc(next);
  config = next;
  if (!storageReady) return;
  if (InternalFS.exists(CONFIG_FILE_PATH)) InternalFS.remove(CONFIG_FILE_PATH);
  File file(CONFIG_FILE_PATH, FILE_O_WRITE, InternalFS);
  if (file) {
    file.write(reinterpret_cast<const uint8_t *>(&config), sizeof(config));
    file.close();
  }
}

void loadConfig() {
  storageReady = InternalFS.begin();
  if (storageReady) {
    File file(CONFIG_FILE_PATH, FILE_O_READ, InternalFS);
    DeviceConfig stored = {};
    if (file && file.size() == sizeof(stored) && file.read(&stored, sizeof(stored)) == sizeof(stored) &&
        isConfigValid(stored, CONFIG_VERSION) && configActionsValid(stored)) {
      file.close();
      config = stored;
      return;
    }
    file.close();
  }
  DeviceConfig defaults = defaultConfig();
  saveConfig(defaults);
}

uint16_t readBatteryMillivolts() {
  analogReadResolution(12);
  analogReference(AR_INTERNAL_1_2);

  // The internal VDD input is VDD/4. Discard the first conversion so the
  // SAADC sampling capacitor can settle, then average fresh samples.
  analogReadVDD();
  uint32_t rawSum = 0;
  for (uint8_t index = 0; index < BATTERY_SAMPLE_COUNT; index++) {
    rawSum += analogReadVDD();
  }

  const uint32_t rawAverage = rawSum / BATTERY_SAMPLE_COUNT;
  return static_cast<uint16_t>((rawAverage * 1200UL * 4UL) / 4095UL);
}

uint8_t batteryPercentFromMillivolts(uint16_t millivolts) {
  struct BatteryPoint {
    uint16_t millivolts;
    uint8_t percent;
  };

  static const BatteryPoint curve[] = {
      {3050, 100},
      {3000, 90},
      {2950, 75},
      {2900, 60},
      {2850, 45},
      {2800, 30},
      {2700, 15},
      {2600, 5},
      {2500, 0},
  };

  if (millivolts >= curve[0].millivolts) return curve[0].percent;
  const size_t lastIndex = (sizeof(curve) / sizeof(curve[0])) - 1;
  if (millivolts <= curve[lastIndex].millivolts) return curve[lastIndex].percent;

  for (size_t index = 0; index < lastIndex; index++) {
    const BatteryPoint &upper = curve[index];
    const BatteryPoint &lower = curve[index + 1];
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
  const uint8_t measuredLevel = batteryPercentFromMillivolts(readBatteryMillivolts());
  const bool changed = measuredLevel != batteryLevel;
  batteryLevel = measuredLevel;
  batteryService.write(batteryLevel);
  if (forceNotify || changed) batteryService.notify(batteryLevel);
}

void sendButtonState(Stream &transport, uint8_t index, uint8_t state) {
  const uint8_t payload[2] = {index, state};
  sendProtocolFrame(transport, CMD_BUTTON_EVENT, payload, sizeof(payload));
}

void broadcastButtonState(uint8_t index, bool pressed) {
  const uint8_t state = pressed ? BUTTON_PRESSED : BUTTON_RELEASED;
  if (usbVbusActive && Serial) sendButtonState(Serial, index, state);
  if (configBle.notifyEnabled()) sendButtonState(configBle, index, state);
}

void sendEncoderEvent(Stream &transport, int8_t direction) {
  const uint8_t payload[2] = {
      3,
      direction > 0 ? ENCODER_CLOCKWISE : ENCODER_COUNTERCLOCKWISE,
  };
  sendProtocolFrame(transport, CMD_ENCODER_EVENT, payload, sizeof(payload));
}

void broadcastEncoderEvent(int8_t direction) {
  if (usbVbusActive && Serial) sendEncoderEvent(Serial, direction);
  if (configBle.notifyEnabled()) sendEncoderEvent(configBle, direction);
}

void updateButton(uint8_t index, uint32_t now) {
  ButtonRuntime &runtime = buttons[index];
  const bool rawPressed = nrf_gpio_pin_read(BUTTON_GPIOS[index]) == 0;
  if (rawPressed != runtime.rawPressed) {
    exitSleep();
    markActivity(now);
    runtime.rawPressed = rawPressed;
    runtime.rawChangedAt = now;
  }
  if (rawPressed != runtime.pressed && now - runtime.rawChangedAt >= DEBOUNCE_MS) {
    runtime.pressed = rawPressed;
    broadcastButtonState(index, rawPressed);
    if (rawPressed) {
      runtime.pressedAt = now;
      runtime.longTriggered = false;
      if (isModifierOnly(singleAction(index))) {
        setHeldModifiers(index, singleAction(index).modifiers);
      }
    } else {
      if (runtime.heldModifiers != 0) {
        setHeldModifiers(index, 0);
      } else if (!runtime.longTriggered) {
        sendAction(singleAction(index));
      }
    }
  }
  if (!runtime.pressed || runtime.longTriggered || runtime.heldModifiers != 0) return;
  if (now - runtime.pressedAt < LONG_PRESS_MS) return;
  runtime.longTriggered = true;
  if (isModifierOnly(longAction(index))) setHeldModifiers(index, longAction(index).modifiers);
  else sendAction(longAction(index));
}

void sampleEncoder() {
  static const int8_t transitions[16] = {
      0, -1, 1, 0, 1, 0, 0, -1,
      -1, 0, 0, 1, 0, 1, -1, 0,
  };
  const uint8_t next = (nrf_gpio_pin_read(ENCODER_A_GPIO) << 1) | nrf_gpio_pin_read(ENCODER_B_GPIO);
  if (next != encoderState) {
    exitSleep();
    markActivity();
  }
  encoderAccumulator += transitions[(encoderState << 2) | next];
  encoderState = next;
  if (encoderAccumulator >= 4) {
    encoderAccumulator -= 4;
    if (pendingEncoderSteps < 127) pendingEncoderSteps++;
  } else if (encoderAccumulator <= -4) {
    encoderAccumulator += 4;
    if (pendingEncoderSteps > -127) pendingEncoderSteps--;
  }
}

void updateEncoder() {
  sampleEncoder();
  if (pendingEncoderSteps > 0) {
    pendingEncoderSteps--;
    broadcastEncoderEvent(1);
    sendAction(config.encoderCW, 1);
  } else if (pendingEncoderSteps < 0) {
    pendingEncoderSteps++;
    broadcastEncoderEvent(-1);
    sendAction(config.encoderCCW, -1);
  }
}

void startAdvertising();

void startConfigAdvertising(uint32_t now) {
  if (Bluefruit.connected() >= 2) return;
  configAdvertisingActive = true;
  configAdvertisingUntil = now + CONFIG_ADVERTISING_TIMEOUT_MS;
  startAdvertising();
}

void updateConfigAdvertising(uint32_t now) {
  if (ALWAYS_ADVERTISE_CONFIG) {
    if (Bluefruit.connected() < 2 && !Bluefruit.Advertising.isRunning()) startAdvertising();
    return;
  }

  if (configAdvertisingActive && static_cast<int32_t>(now - configAdvertisingUntil) >= 0) {
    configAdvertisingActive = false;
  }
  if (configAdvertisingActive) {
    if (Bluefruit.connected() < 2 && !Bluefruit.Advertising.isRunning()) startAdvertising();
    return;
  }
  if (!configAdvertisingActive && Bluefruit.connected() > 0 && Bluefruit.Advertising.isRunning()) {
    Bluefruit.Advertising.stop();
  }
}

void updateConfigChord(uint32_t now) {
  if (!buttons[0].pressed || !buttons[2].pressed) {
    configChordStartedAt = 0;
    configChordTriggered = false;
    return;
  }

  // The outer-button chord is reserved and must not leak configured actions.
  buttons[0].longTriggered = true;
  buttons[2].longTriggered = true;
  if (buttons[0].heldModifiers || buttons[2].heldModifiers) {
    buttons[0].heldModifiers = 0;
    buttons[2].heldModifiers = 0;
    sendKeyboardState();
  }

  if (configChordStartedAt == 0) configChordStartedAt = now;
  if (!configChordTriggered && now - configChordStartedAt >= CONFIG_CHORD_HOLD_MS) {
    configChordTriggered = true;
    startConfigAdvertising(now);
  }
}

void sendDeviceInfo(Stream &transport) {
  const uint8_t payload[17] = {
      0,
      255, 255, 255,
      222, 222, 222,
      153, 153, 153,
      119, 119, 119,
      1,
      1,
      0, 0,
  };
  sendProtocolFrame(transport, CMD_DEVICE_INFO, payload, sizeof(payload));
}

void handleSetConfig(Stream &transport, const uint8_t *payload, uint8_t payloadLength) {
  if (payloadLength != sizeof(DeviceConfig)) {
    sendProtocolError(transport, STATUS_BAD_PAYLOAD);
    return;
  }
  DeviceConfig next;
  memcpy(&next, payload, sizeof(next));
  if (!isConfigValid(next, CONFIG_VERSION)) {
    sendProtocolError(transport, STATUS_BAD_CRC);
    return;
  }
  if (!configActionsValid(next)) {
    sendProtocolError(transport, STATUS_BAD_PAYLOAD);
    return;
  }
  saveConfig(next);
  sendProtocolStatus(transport, CMD_ACK, STATUS_OK);
}

void handleProtocol(Stream &transport) {
  while (transport.available() >= 2) {
    if (transport.read() != FRAME_MAGIC_1 || transport.read() != FRAME_MAGIC_2) continue;
    uint8_t header[3];
    if (!readExact(transport, header, sizeof(header))) return;
    const uint8_t version = header[0];
    const uint8_t command = header[1];
    const uint8_t payloadLength = header[2];
    uint8_t payload[sizeof(DeviceConfig)] = {};
    if (payloadLength > sizeof(payload) || !readExact(transport, payload, payloadLength)) {
      sendProtocolError(transport, STATUS_BAD_PAYLOAD);
      return;
    }
    uint8_t receivedCrc;
    if (!readExact(transport, &receivedCrc, 1)) return;
    uint8_t crc = 0;
    crc = crc8Update(crc, version);
    crc = crc8Update(crc, command);
    crc = crc8Update(crc, payloadLength);
    for (uint8_t index = 0; index < payloadLength; index++) crc = crc8Update(crc, payload[index]);
    if (version != PROTOCOL_VERSION || crc != receivedCrc) {
      sendProtocolError(transport, crc != receivedCrc ? STATUS_BAD_CRC : STATUS_BAD_COMMAND);
      continue;
    }
    if (command == CMD_GET_CONFIG) {
      sendProtocolFrame(transport, CMD_CONFIG, reinterpret_cast<uint8_t *>(&config), sizeof(config));
    } else if (command == CMD_SET_CONFIG) {
      handleSetConfig(transport, payload, payloadLength);
    } else if (command == CMD_RESET_CONFIG) {
      DeviceConfig defaults = defaultConfig();
      saveConfig(defaults);
      sendProtocolFrame(transport, CMD_CONFIG, reinterpret_cast<uint8_t *>(&config), sizeof(config));
    } else if (command == CMD_PING) {
      sendProtocolPing(transport);
    } else if (command == CMD_GET_DEVICE_INFO) {
      sendDeviceInfo(transport);
    } else {
      sendProtocolError(transport, STATUS_BAD_COMMAND);
    }
  }
}

void startAdvertising() {
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(configBle);
  Bluefruit.ScanResponse.addService(hid);
  Bluefruit.ScanResponse.addService(batteryService);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 160);
  Bluefruit.Advertising.setFastTimeout(5);
  Bluefruit.Advertising.start(0);
}

void setupBle() {
  Bluefruit.autoConnLed(false);
  // The config response is 48 bytes including framing. A larger characteristic
  // max length lets BLEUart split it into notifications even when ATT stays at 23.
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin(2, 0);
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_DISABLE);
#if defined(NRF52840_XXAA)
  sd_power_dcdc0_mode_set(NRF_POWER_DCDC_DISABLE);
#endif
  Bluefruit.setTxPower(-8);
  Bluefruit.setName(DEVICE_NAME);
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);
  Bluefruit.Security.setSecuredCallback(securedCallback);
  deviceInfo.setManufacturer("lunev");
  deviceInfo.setModel(DEVICE_NAME);
  deviceInfo.begin();
  batteryService.begin();
  updateBatteryLevel();
  lastBatteryUpdateAt = millis();
  configBle.begin();
  configBle.bufferTXD(true);
  configBle.setRxCallback([](uint16_t connHandle) {
    (void) connHandle;
    wakeLoopTask();
  });
  hid.begin();
  Bluefruit.Periph.setConnInterval(BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX);
  Bluefruit.Periph.setConnSlaveLatency(0);
  Bluefruit.Periph.setConnSupervisionTimeout(BLE_CONN_SUPERVISION_TIMEOUT);
  startAdvertising();
}

bool anyButtonActive() {
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    if (buttons[index].rawPressed || buttons[index].pressed) return true;
  }
  return false;
}

bool hasActiveBleConnection() {
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (connection && connection->connected()) return true;
  }
  return false;
}

bool hasSecuredBleConnection() {
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (connection && connection->connected() && connection->secured()) return true;
  }
  return false;
}

void flushPendingWakeAction(uint32_t now) {
  if (pendingWakeButton < 0) return;
  if (now - pendingWakeActionAt >= PENDING_WAKE_ACTION_MAX_AGE_MS) {
    pendingWakeButton = -1;
    return;
  }
  if (!hasSecuredBleConnection() || bleSecuredAt == 0 ||
      now - bleSecuredAt < PENDING_WAKE_ACTION_READY_DELAY_MS) return;

  const uint8_t buttonIndex = static_cast<uint8_t>(pendingWakeButton);
  pendingWakeButton = -1;
  sendAction(singleAction(buttonIndex));
}

uint32_t currentDeepSleepTimeoutMs() {
  return hasActiveBleConnection()
      ? DEEP_SLEEP_TIMEOUT_MS
      : DISCONNECTED_DEEP_SLEEP_TIMEOUT_MS;
}

void enterSleep() {
  if (sleeping) return;
  sleeping = true;
  Bluefruit.Advertising.restartOnDisconnect(false);
  if (!hasActiveBleConnection()) Bluefruit.Advertising.stop();
}

void exitSleep() {
  if (!sleeping) return;
  sleeping = false;
  Bluefruit.Advertising.restartOnDisconnect(true);
  if (!hasActiveBleConnection()) startAdvertising();
  markActivity();
}

void disableAllGpioSense() {
  for (uint8_t pin = 0; pin < 32; pin++) {
    NRF_P0->PIN_CNF[pin] =
        (NRF_P0->PIN_CNF[pin] & ~GPIO_PIN_CNF_SENSE_Msk) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
  }
#if defined(NRF_P1)
  for (uint8_t pin = 0; pin < 32; pin++) {
    NRF_P1->PIN_CNF[pin] =
        (NRF_P1->PIN_CNF[pin] & ~GPIO_PIN_CNF_SENSE_Msk) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
  }
#endif
}

void restoreInputPins() {
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    nrf_gpio_cfg_input(BUTTON_GPIOS[index], NRF_GPIO_PIN_PULLUP);
  }
  nrf_gpio_cfg_input(ENCODER_A_GPIO, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_input(ENCODER_B_GPIO, NRF_GPIO_PIN_PULLUP);
}

bool prepareSystemOffWake() {
  if (anyButtonActive()) return false;

  disableAllGpioSense();
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    nrf_gpio_cfg_sense_input(
        BUTTON_GPIOS[index], NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  }

  const nrf_gpio_pin_sense_t encoderASense = nrf_gpio_pin_read(ENCODER_A_GPIO)
      ? NRF_GPIO_PIN_SENSE_LOW
      : NRF_GPIO_PIN_SENSE_HIGH;
  const nrf_gpio_pin_sense_t encoderBSense = nrf_gpio_pin_read(ENCODER_B_GPIO)
      ? NRF_GPIO_PIN_SENSE_LOW
      : NRF_GPIO_PIN_SENSE_HIGH;
  nrf_gpio_cfg_sense_input(ENCODER_A_GPIO, NRF_GPIO_PIN_PULLUP, encoderASense);
  nrf_gpio_cfg_sense_input(ENCODER_B_GPIO, NRF_GPIO_PIN_PULLUP, encoderBSense);

  NRF_P0->LATCH = 0xFFFFFFFF;
#if defined(NRF_P1)
  NRF_P1->LATCH = 0xFFFFFFFF;
#endif
  NRF_GPIOTE->EVENTS_PORT = 0;
  delay(10);

  const bool wakePending = anyButtonActive() || NRF_P0->LATCH != 0
#if defined(NRF_P1)
      || NRF_P1->LATCH != 0
#endif
      ;
  if (wakePending) {
    disableAllGpioSense();
    restoreInputPins();
    NRF_P0->LATCH = 0xFFFFFFFF;
#if defined(NRF_P1)
    NRF_P1->LATCH = 0xFFFFFFFF;
#endif
    return false;
  }
  return true;
}

void disconnectAllBleConnections() {
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (connection && connection->connected()) connection->disconnect();
  }
}

void enterSystemOff() {
  if (!prepareSystemOffWake()) {
    markActivity();
    return;
  }

  Bluefruit.Advertising.restartOnDisconnect(false);
  disconnectAllBleConnections();
  delay(30);
  Bluefruit.Advertising.stop();
  delay(10);

#if defined(NRF_SAADC)
  NRF_SAADC->ENABLE = 0;
#endif
#if defined(NRF_UARTE0)
  NRF_UARTE0->ENABLE = 0;
#endif
#if defined(NRF_UARTE1)
  NRF_UARTE1->ENABLE = 0;
#endif

  uint8_t softDeviceEnabled = 0;
  (void) sd_softdevice_is_enabled(&softDeviceEnabled);
  if (softDeviceEnabled) sd_power_system_off();
  else NRF_POWER->SYSTEMOFF = 1;
  __DSB();
  while (true) __WFE();
}

bool shouldEnterSleep(uint32_t now) {
  if (usbVbusActive || anyButtonActive() || pendingEncoderSteps != 0 ||
      configAdvertisingActive || pendingWakeButton >= 0) {
    return false;
  }
  return now - lastActivityAt >= IDLE_SLEEP_TIMEOUT_MS;
}

uint32_t millisecondsUntil(uint32_t now, uint32_t deadline) {
  const int32_t remaining = static_cast<int32_t>(deadline - now);
  return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

void shortenWait(uint32_t &waitMs, uint32_t candidateMs) {
  if (candidateMs < waitMs) waitMs = candidateMs;
}

uint32_t nextLoopWaitMs(uint32_t now) {
  uint32_t waitMs = MAX_IDLE_BLOCK_MS;
  shortenWait(
      waitMs,
      millisecondsUntil(
          now,
          lastActivityAt + (sleeping ? currentDeepSleepTimeoutMs() : IDLE_SLEEP_TIMEOUT_MS)));
  shortenWait(waitMs, millisecondsUntil(now, lastBatteryUpdateAt + BATTERY_UPDATE_INTERVAL_MS));
  if (!bleIdleParamsRequested && bleSecuredAt != 0) {
    shortenWait(waitMs, millisecondsUntil(now, bleSecuredAt + BLE_IDLE_PARAMS_DELAY_MS));
  }
  if (configAdvertisingActive) {
    shortenWait(waitMs, millisecondsUntil(now, configAdvertisingUntil));
  }
  if (pendingWakeButton >= 0) {
    shortenWait(
        waitMs,
        millisecondsUntil(now, pendingWakeActionAt + PENDING_WAKE_ACTION_MAX_AGE_MS));
    if (bleSecuredAt != 0) {
      shortenWait(
          waitMs,
          millisecondsUntil(now, bleSecuredAt + PENDING_WAKE_ACTION_READY_DELAY_MS));
    }
  }
  return waitMs;
}

void blockUntilNextInput(uint32_t now) {
  if (!inputWakeInterruptsAttached || usbVbusActive || Serial.available() > 0 ||
      anyButtonActive() || pendingEncoderSteps != 0) {
    delay(ACTIVE_INPUT_POLL_MS);
    return;
  }

  const uint32_t waitMs = nextLoopWaitMs(now);
  if (waitMs == 0) {
    taskYIELD();
    return;
  }
  const TickType_t ticks = pdMS_TO_TICKS(waitMs);
  ulTaskNotifyTake(pdTRUE, ticks > 0 ? ticks : 1);
}

void idleSleep(uint32_t now) {
  if (sleeping && !usbVbusActive && !configAdvertisingActive && pendingWakeButton < 0 &&
      now - lastActivityAt >= currentDeepSleepTimeoutMs()) {
    enterSystemOff();
    return;
  }
  if (shouldEnterSleep(now)) enterSleep();
  blockUntilNextInput(now);
}

}  // namespace

void setup() {
  const uint32_t p0Latch = NRF_P0->LATCH;
#if defined(NRF_P1)
  const uint32_t p1Latch = NRF_P1->LATCH;
#endif
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    const uint32_t gpio = BUTTON_GPIOS[index];
    const bool latched = gpio < 32
        ? (p0Latch & (1UL << gpio)) != 0
#if defined(NRF_P1)
        : (p1Latch & (1UL << (gpio - 32))) != 0
#else
        : false
#endif
        ;
    if (latched && pendingWakeButton < 0) pendingWakeButton = index;
  }
  NRF_P0->LATCH = 0xFFFFFFFF;
#if defined(NRF_P1)
  NRF_P1->LATCH = 0xFFFFFFFF;
#endif

  // Modified SuperMini/nice!nano clone: the external VCC gate pull-up is
  // removed, so P0.13 can keep the unused regulator rail off deterministically.
  nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0, 13));
  nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0, 13));
  lastVbusPresent = isRawVbusPresent();
  if (lastVbusPresent) {
    Serial.begin(SERIAL_BAUD);
    usbVbusActive = true;
  }
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    nrf_gpio_cfg_input(BUTTON_GPIOS[index], NRF_GPIO_PIN_PULLUP);
    buttons[index].rawPressed = nrf_gpio_pin_read(BUTTON_GPIOS[index]) == 0;
    buttons[index].pressed = buttons[index].rawPressed;
    if (pendingWakeButton == static_cast<int8_t>(index)) buttons[index].longTriggered = true;
  }
  nrf_gpio_cfg_input(ENCODER_A_GPIO, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_input(ENCODER_B_GPIO, NRF_GPIO_PIN_PULLUP);
  encoderState = (nrf_gpio_pin_read(ENCODER_A_GPIO) << 1) | nrf_gpio_pin_read(ENCODER_B_GPIO);
  setupInputWakeInterrupts();
  loadConfig();
  setupBle();
  markActivity();
  if (pendingWakeButton >= 0) pendingWakeActionAt = millis();
}

void loop() {
  updateUsbState();
  const uint32_t now = millis();
  if ((usbVbusActive && Serial.available() > 0) || configBle.available() > 0) {
    exitSleep();
    markActivity(now);
  }
  if (usbVbusActive) handleProtocol(Serial);
  handleProtocol(configBle);
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) updateButton(index, now);
  updateConfigChord(now);
  updateConfigAdvertising(now);
  updateEncoder();
  updateBlePowerProfile(now);
  flushPendingWakeAction(now);
  if (now - lastBatteryUpdateAt >= BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdateAt = now;
    updateBatteryLevel();
  }
  idleSleep(now);
}
