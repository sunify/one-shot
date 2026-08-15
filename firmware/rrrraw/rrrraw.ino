#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "nrf_gpio.h"
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
// Bench mode: keep a second BLE slot discoverable while the HID connection is active.
// Disable before battery-life measurements and the production build.
const bool ALWAYS_ADVERTISE_CONFIG = true;
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
BLEUart configBle;
DeviceConfig config;
ButtonRuntime buttons[BUTTON_COUNT] = {};
bool storageReady = false;
uint8_t encoderState = 0;
int8_t encoderAccumulator = 0;
bool configAdvertisingActive = false;
uint32_t configAdvertisingUntil = 0;
uint32_t configChordStartedAt = 0;
bool configChordTriggered = false;

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

void sendHotkey(const GestureAction &action) {
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
  delay(12);
  sendKeyboardState();
}

void sendConsumer(uint16_t code) {
  for (uint16_t handle = 0; handle < BLE_MAX_CONNECTION; handle++) {
    BLEConnection *connection = Bluefruit.Connection(handle);
    if (!connection || !connection->connected()) continue;
    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }
    hid.consumerKeyPress(handle, code);
    delay(12);
    hid.consumerKeyRelease(handle);
  }
}

void sendMouse(const GestureAction &action, int8_t direction) {
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
    delay(12);
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

void sendButtonState(Stream &transport, uint8_t index, uint8_t state) {
  const uint8_t payload[2] = {index, state};
  sendProtocolFrame(transport, CMD_BUTTON_EVENT, payload, sizeof(payload));
}

void broadcastButtonState(uint8_t index, bool pressed) {
  const uint8_t state = pressed ? BUTTON_PRESSED : BUTTON_RELEASED;
  if (Serial) sendButtonState(Serial, index, state);
  if (configBle.notifyEnabled()) sendButtonState(configBle, index, state);
}

void updateButton(uint8_t index, uint32_t now) {
  ButtonRuntime &runtime = buttons[index];
  const bool rawPressed = nrf_gpio_pin_read(BUTTON_GPIOS[index]) == 0;
  if (rawPressed != runtime.rawPressed) {
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

void updateEncoder() {
  static const int8_t transitions[16] = {
      0, -1, 1, 0, 1, 0, 0, -1,
      -1, 0, 0, 1, 0, 1, -1, 0,
  };
  const uint8_t next = (nrf_gpio_pin_read(ENCODER_A_GPIO) << 1) | nrf_gpio_pin_read(ENCODER_B_GPIO);
  encoderAccumulator += transitions[(encoderState << 2) | next];
  encoderState = next;
  if (encoderAccumulator >= 4) {
    encoderAccumulator = 0;
    sendAction(config.encoderCW, 1);
  } else if (encoderAccumulator <= -4) {
    encoderAccumulator = 0;
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
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(hid);
  Bluefruit.Advertising.addService(configBle);
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
  deviceInfo.setManufacturer("Huntflow");
  deviceInfo.setModel(DEVICE_NAME);
  deviceInfo.begin();
  configBle.begin();
  configBle.bufferTXD(true);
  hid.begin();
  Bluefruit.Periph.setConnInterval(12, 24);
  startAdvertising();
}

}  // namespace

void setup() {
  // Modified SuperMini/nice!nano clone: the external VCC gate pull-up is
  // removed, so P0.13 can keep the unused regulator rail off deterministically.
  nrf_gpio_cfg_output(NRF_GPIO_PIN_MAP(0, 13));
  nrf_gpio_pin_clear(NRF_GPIO_PIN_MAP(0, 13));
  Serial.begin(SERIAL_BAUD);
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) {
    nrf_gpio_cfg_input(BUTTON_GPIOS[index], NRF_GPIO_PIN_PULLUP);
    buttons[index].rawPressed = nrf_gpio_pin_read(BUTTON_GPIOS[index]) == 0;
    buttons[index].pressed = buttons[index].rawPressed;
  }
  nrf_gpio_cfg_input(ENCODER_A_GPIO, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_input(ENCODER_B_GPIO, NRF_GPIO_PIN_PULLUP);
  encoderState = (nrf_gpio_pin_read(ENCODER_A_GPIO) << 1) | nrf_gpio_pin_read(ENCODER_B_GPIO);
  loadConfig();
  setupBle();
}

void loop() {
  const uint32_t now = millis();
  handleProtocol(Serial);
  handleProtocol(configBle);
  for (uint8_t index = 0; index < BUTTON_COUNT; index++) updateButton(index, now);
  updateConfigChord(now);
  updateConfigAdvertising(now);
  updateEncoder();
  delay(2);
}
