#ifndef NUM_LEDS
#define NUM_LEDS 2
#endif

#if NUM_LEDS > 0
#include <FastLED.h>
#endif
#include <HID-Project.h>
#include <EEPROM.h>
#include <stddef.h>
#include "device_protocol.h"

#ifndef BTN_GROUND_PIN
#define BTN_GROUND_PIN 9
#endif
#ifndef BTN_INPUT_PIN
#define BTN_INPUT_PIN 10
#endif
#ifndef NUM_BUTTONS
#define NUM_BUTTONS 1
#endif
#if NUM_BUTTONS == 2
#ifndef BTN2_INPUT_PIN
#define BTN2_INPUT_PIN 5
#endif
#ifndef RESET_PIN
#define RESET_PIN 4
#endif
#endif

#ifndef DATA_PIN
#define DATA_PIN A3
#endif
#ifndef KEYCAP_R
#define KEYCAP_R 0xFF
#endif
#ifndef KEYCAP_G
#define KEYCAP_G 0xFF
#endif
#ifndef KEYCAP_B
#define KEYCAP_B 0xFF
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
#define TOP_CASE_SHADE_R 0xCF
#endif
#ifndef TOP_CASE_SHADE_G
#define TOP_CASE_SHADE_G 0x00
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
#define TOP_CASE_SHADE_ENABLED 1
#endif

#ifndef THIRD_ACTION_TRIGGER
#define THIRD_ACTION_TRIGGER 0
#endif

#define THIRD_ACTION_TRIGGER_TRIPLE_TAP 0
#define THIRD_ACTION_TRIGGER_LONG_PRESS 1

#if NUM_LEDS > 0
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
CRGB baseColor = CRGB(250, 255, 210);
#endif

const uint16_t MULTI_TAP_TIMEOUT = 250;
const uint16_t QUICK_TAP_MAX_PRESS = 100;
const uint16_t LONG_PRESS = 600;
const uint16_t DEBOUNCE = 10;
const uint8_t RAW_HID_REPORT_SIZE = 64;

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction singleTap;
  GestureAction doubleTap;
  GestureAction tripleTap;
#if NUM_BUTTONS == 2
  GestureAction button2SingleTap;
  GestureAction button2DoubleTap;
  GestureAction button2TripleTap;
#endif
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t animationMode;
  uint8_t sleepTimeout;
#ifdef ROTARY_ENABLED
  GestureAction encoderCW;
  GestureAction encoderCCW;
  uint8_t encoderSensitivity;
#endif
  uint8_t crc;
};

struct __attribute__((packed)) DeviceOptions {
  uint8_t version;
  uint16_t flags;
  uint8_t crc;
};

static_assert(sizeof(DeviceOptions) == 4, "Unexpected device options size");

#if NUM_BUTTONS == 2
static_assert(sizeof(DeviceConfig) == 31, "Unexpected Bebop config size");
static_assert(offsetof(DeviceConfig, button2SingleTap) == 13, "Unexpected Bebop button 2 offset");
static_assert(offsetof(DeviceConfig, red) == 25, "Unexpected Bebop lighting offset");
static_assert(offsetof(DeviceConfig, crc) == 30, "Unexpected Bebop CRC offset");
#elif defined(ROTARY_ENABLED)
static_assert(sizeof(DeviceConfig) == 28, "Unexpected One Shot rotary config size");
static_assert(offsetof(DeviceConfig, encoderCW) == 18, "Unexpected One Shot encoder offset");
#else
static_assert(sizeof(DeviceConfig) == 19, "Unexpected One Shot config size");
static_assert(offsetof(DeviceConfig, red) == 13, "Unexpected One Shot lighting offset");
#endif

#if NUM_BUTTONS == 2
const uint8_t CONFIG_VERSION = 1;
const uint8_t DEVICE_TYPE = DEVICE_TYPE_BEBOP;
#else
const uint8_t CONFIG_VERSION = 6;
const uint8_t DEVICE_TYPE = DEVICE_TYPE_ONE_SHOT;
#endif
const int EEPROM_ADDRESS = 0;
const int EEPROM_OPTIONS_ADDRESS = EEPROM_ADDRESS + sizeof(DeviceConfig);
const uint8_t DEVICE_OPTIONS_VERSION = 1;
#if NUM_BUTTONS == 2
const uint16_t DEVICE_CAPABILITIES = DEVICE_CAPABILITY_TURBO_MODE;
const uint16_t SUPPORTED_OPTION_FLAGS = DEVICE_OPTION_TURBO_MODE;
#else
const uint16_t DEVICE_CAPABILITIES = 0;
const uint16_t SUPPORTED_OPTION_FLAGS = 0;
#endif
DeviceConfig config;
DeviceOptions deviceOptions;
uint8_t rawHidReport[RAW_HID_REPORT_SIZE];

struct ButtonRuntime {
  bool lastState;
  uint32_t lastChange;
  uint32_t pressStart;
  uint32_t lastRelease;
  uint8_t tapCount;
  bool longPressHandled;
};

ButtonRuntime buttons[NUM_BUTTONS];
const uint8_t buttonPins[NUM_BUTTONS] = {
#if NUM_BUTTONS == 2
  BTN2_INPUT_PIN,
  BTN_INPUT_PIN,
#else
  BTN_INPUT_PIN,
#endif
};

#if NUM_BUTTONS == 2
const uint16_t RESET_DELAY = 1000;
uint32_t resetStartedAt = 0;
bool resetChordActive = false;
#endif

uint8_t brightnessStep = 0;
uint8_t brightnessLevels[] = {255, 191, 128, 64, 0};

uint32_t releaseBoostStart = 0;
const uint16_t BOOST_DURATION = 500;

uint32_t lastActivityTime = 0;
bool isSleeping = false;

#ifdef ROTARY_ENABLED
#include <Encoder.h>
Encoder rotaryEncoder(ROTARY_A_PIN, ROTARY_B_PIN);
long rotaryPosition = 0;
const int16_t BASE_VELOCITY = 3;
int16_t phaseVelocity = BASE_VELOCITY;
int8_t phaseDirection = 1;
uint16_t manualPhase = 0;
uint32_t lastBoostTime = 0;
const uint16_t VELOCITY_DECAY_MS = 80;
#endif

void markActivity() {
  lastActivityTime = millis();
  if (isSleeping) {
    isSleeping = false;
    powerOnBlink();
  }
}

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTap = {ACTION_TYPE_CONSUMER, MEDIA_PLAY_PAUSE, 0};
  cfg.doubleTap = {ACTION_TYPE_CONSUMER, MEDIA_NEXT, 0};
  cfg.tripleTap = {ACTION_TYPE_CONSUMER, MEDIA_PREVIOUS, 0};
#if NUM_BUTTONS == 2
  cfg.singleTap = {ACTION_TYPE_HOTKEY, 0x05, 0};
  cfg.button2SingleTap = {ACTION_TYPE_HOTKEY, 0x13, 0};
  cfg.button2DoubleTap = {ACTION_TYPE_CONSUMER, MEDIA_NEXT, 0};
  cfg.button2TripleTap = {ACTION_TYPE_CONSUMER, MEDIA_PREVIOUS, 0};
#endif
  cfg.red = 250;
  cfg.green = 255;
  cfg.blue = 210;
  cfg.animationMode = 1;
  cfg.sleepTimeout = 0;
#ifdef ROTARY_ENABLED
  cfg.encoderCW = {ACTION_TYPE_MOUSE, MOUSE_AXIS_SCROLL | (2 << 8), MODIFIER_GUI};
  cfg.encoderCCW = {ACTION_TYPE_MOUSE, MOUSE_AXIS_SCROLL | (2 << 8), MODIFIER_GUI};
  cfg.encoderSensitivity = 4;
#endif
  cfg.crc = 0;
  return cfg;
}

void applyConfig(const DeviceConfig &cfg) {
  config = cfg;
}

DeviceOptions defaultDeviceOptions() {
  DeviceOptions options = {DEVICE_OPTIONS_VERSION, 0, 0};
  options.crc = computeConfigCrc(options);
  return options;
}

bool isDeviceOptionsValid(const DeviceOptions &options) {
  return isConfigValid(options, DEVICE_OPTIONS_VERSION) &&
         (options.flags & ~SUPPORTED_OPTION_FLAGS) == 0;
}

void applyDeviceOptions(const DeviceOptions &options) {
  deviceOptions = options;
  for (uint8_t index = 0; index < NUM_BUTTONS; index++) {
    buttons[index].tapCount = 0;
    buttons[index].longPressHandled = buttons[index].lastState == LOW;
  }
}

bool turboModeEnabled() {
  return (deviceOptions.flags & DEVICE_OPTION_TURBO_MODE) != 0;
}

void persistConfig(DeviceConfig &cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);
  EEPROM.put(EEPROM_ADDRESS, cfg);
  applyConfig(cfg);
}

void persistDeviceOptions(DeviceOptions &options) {
  options.version = DEVICE_OPTIONS_VERSION;
  options.flags &= SUPPORTED_OPTION_FLAGS;
  options.crc = computeConfigCrc(options);
  EEPROM.put(EEPROM_OPTIONS_ADDRESS, options);
  applyDeviceOptions(options);
}

void loadConfig() {
  DeviceConfig stored;
  EEPROM.get(EEPROM_ADDRESS, stored);

  if (isConfigValid(stored, CONFIG_VERSION)) {
    applyConfig(stored);
  } else {
    stored = defaultConfig();
    persistConfig(stored);
  }

  DeviceOptions storedOptions;
  EEPROM.get(EEPROM_OPTIONS_ADDRESS, storedOptions);
  if (isDeviceOptionsValid(storedOptions)) {
    applyDeviceOptions(storedOptions);
  } else {
    storedOptions = defaultDeviceOptions();
    persistDeviceOptions(storedOptions);
  }
}

void resetConfig() {
  DeviceConfig cfg = defaultConfig();
  persistConfig(cfg);
  DeviceOptions options = defaultDeviceOptions();
  persistDeviceOptions(options);
}

void sendConfigFrame() {
  sendFrame(Serial, CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

void sendRawHidFrame(uint8_t cmd, const uint8_t *payload, uint8_t payloadLen) {
  uint8_t frame[RAW_HID_REPORT_SIZE] = {0};
  uint8_t crc = 0;
  crc = crc8Update(crc, PROTOCOL_VERSION);
  crc = crc8Update(crc, cmd);
  crc = crc8Update(crc, payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    crc = crc8Update(crc, payload[i]);
  }

  frame[0] = FRAME_MAGIC_1;
  frame[1] = FRAME_MAGIC_2;
  frame[2] = PROTOCOL_VERSION;
  frame[3] = cmd;
  frame[4] = payloadLen;
  if (payloadLen > 0) {
    memcpy(frame + 5, payload, payloadLen);
  }
  frame[5 + payloadLen] = crc;
  RawHID.write(frame, sizeof(frame));
}

void sendRawHidStatusFrame(uint8_t cmd, uint8_t status) {
  const uint8_t payload[] = {status};
  sendRawHidFrame(cmd, payload, sizeof(payload));
}

void sendRawHidConfigFrame() {
  sendRawHidFrame(CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

void sendDeviceOptionsFrame() {
  const uint8_t payload[] = {
    DEVICE_OPTIONS_VERSION,
    static_cast<uint8_t>(deviceOptions.flags & 0xFF),
    static_cast<uint8_t>(deviceOptions.flags >> 8),
  };
  sendFrame(Serial, CMD_DEVICE_OPTIONS, payload, sizeof(payload));
}

void sendRawHidDeviceOptionsFrame() {
  const uint8_t payload[] = {
    DEVICE_OPTIONS_VERSION,
    static_cast<uint8_t>(deviceOptions.flags & 0xFF),
    static_cast<uint8_t>(deviceOptions.flags >> 8),
  };
  sendRawHidFrame(CMD_DEVICE_OPTIONS, payload, sizeof(payload));
}

void sendButtonStateEvent(uint8_t buttonIndex, uint8_t state) {
#if NUM_BUTTONS == 2
  const uint8_t payload[] = {buttonIndex, state};
  sendFrame(Serial, CMD_BUTTON_EVENT, payload, sizeof(payload));
  sendRawHidFrame(CMD_BUTTON_EVENT, payload, sizeof(payload));
#else
  sendButtonEvent(Serial, state);
  const uint8_t payload[] = {state};
  sendRawHidFrame(CMD_BUTTON_EVENT, payload, sizeof(payload));
#endif
}

void sendDeviceInfoFrame() {
  uint8_t payload[17];
  payload[0]  = NUM_LEDS;
  payload[1]  = KEYCAP_R;
  payload[2]  = KEYCAP_G;
  payload[3]  = KEYCAP_B;
  payload[4]  = TOP_CASE_R;
  payload[5]  = TOP_CASE_G;
  payload[6]  = TOP_CASE_B;
  payload[7]  = TOP_CASE_SHADE_R;
  payload[8]  = TOP_CASE_SHADE_G;
  payload[9]  = TOP_CASE_SHADE_B;
  payload[10] = BOTTOM_CASE_R;
  payload[11] = BOTTOM_CASE_G;
  payload[12] = BOTTOM_CASE_B;
  payload[13] = TOP_CASE_SHADE_ENABLED;
  payload[14] = THIRD_ACTION_TRIGGER;
  payload[15] = static_cast<uint8_t>(DEVICE_CAPABILITIES & 0xFF);
  payload[16] = static_cast<uint8_t>(DEVICE_CAPABILITIES >> 8);
  sendFrame(Serial, CMD_DEVICE_INFO, payload, sizeof(payload));
}

void sendRawHidDeviceInfoFrame() {
  uint8_t payload[17];
  payload[0]  = NUM_LEDS;
  payload[1]  = KEYCAP_R;
  payload[2]  = KEYCAP_G;
  payload[3]  = KEYCAP_B;
  payload[4]  = TOP_CASE_R;
  payload[5]  = TOP_CASE_G;
  payload[6]  = TOP_CASE_B;
  payload[7]  = TOP_CASE_SHADE_R;
  payload[8]  = TOP_CASE_SHADE_G;
  payload[9]  = TOP_CASE_SHADE_B;
  payload[10] = BOTTOM_CASE_R;
  payload[11] = BOTTOM_CASE_G;
  payload[12] = BOTTOM_CASE_B;
  payload[13] = TOP_CASE_SHADE_ENABLED;
  payload[14] = THIRD_ACTION_TRIGGER;
  payload[15] = static_cast<uint8_t>(DEVICE_CAPABILITIES & 0xFF);
  payload[16] = static_cast<uint8_t>(DEVICE_CAPABILITIES >> 8);
  sendRawHidFrame(CMD_DEVICE_INFO, payload, sizeof(payload));
}

void powerOnBlink() {
#if NUM_LEDS > 0
  fill_solid(leds, NUM_LEDS, baseColor);

  FastLED.setBrightness(30);
  FastLED.show();
  delay(150);
  FastLED.setBrightness(10);
  FastLED.show();
  delay(50);
  FastLED.setBrightness(50);
  FastLED.show();
  delay(50);
  FastLED.setBrightness(0);
  FastLED.show();
  delay(50);
  FastLED.setBrightness(brightnessLevels[brightnessStep]);
  FastLED.show();
#endif
}

void blinkFeedback(uint8_t count) {
#if NUM_LEDS > 0
  CRGB feedbackColor = blend(baseColor, CRGB::White, 96);

  for (uint8_t i = 0; i < count; i++) {
    fill_solid(leds, NUM_LEDS, feedbackColor);
    FastLED.setBrightness(brightnessLevels[brightnessStep]);
    FastLED.show();
    delay(70);

    FastLED.clear();
    FastLED.show();
    delay(60);
  }
#else
  (void)count;
#endif
}

void nextBrightness() {
  brightnessStep++;
  if (brightnessStep >= 5) brightnessStep = 0;

  if (brightnessStep == 0) {
    powerOnBlink();
  } else {
    blinkFeedback(1);
  }
}

void pressModifiers(uint8_t modifiers) {
  if (modifiers & MODIFIER_CTRL) Keyboard.press(KEY_LEFT_CTRL);
  if (modifiers & MODIFIER_SHIFT) Keyboard.press(KEY_LEFT_SHIFT);
  if (modifiers & MODIFIER_ALT) Keyboard.press(KEY_LEFT_ALT);
  if (modifiers & MODIFIER_GUI) Keyboard.press(KEY_LEFT_GUI);
}

void sendGestureAction(uint8_t actionType, uint16_t actionCode, uint8_t modifiers, int8_t direction = 1) {
  if (actionType == ACTION_TYPE_HOTKEY) {
    pressModifiers(modifiers);
    Keyboard.press(static_cast<KeyboardKeycode>(actionCode));
    delay(12);
    Keyboard.releaseAll();
    return;
  }

  if (actionType == ACTION_TYPE_MOUSE) {
    uint8_t axis = actionCode & 0xFF;
    int8_t amount = (int8_t)((actionCode >> 8) & 0xFF) * direction;
    pressModifiers(modifiers);
    if (axis == MOUSE_AXIS_SCROLL) {
      Mouse.move(0, 0, amount);
    } else if (axis == MOUSE_AXIS_MOVE_X) {
      Mouse.move(amount, 0, 0);
    } else if (axis == MOUSE_AXIS_MOVE_Y) {
      Mouse.move(0, amount, 0);
    }
    Keyboard.releaseAll();
    return;
  }

  Consumer.write(actionCode);
}

void sendAction(uint8_t buttonIndex, uint8_t taps) {
  GestureAction action;

#if NUM_BUTTONS == 2
  if (buttonIndex == 1) {
    action = config.button2TripleTap;
    if (taps == 1) {
      action = config.button2SingleTap;
    } else if (taps == 2) {
      action = config.button2DoubleTap;
    }
  } else
#else
  (void)buttonIndex;
#endif
  {
    action = config.tripleTap;
    if (taps == 1) {
      action = config.singleTap;
    } else if (taps == 2) {
      action = config.doubleTap;
    }
  }

  sendGestureAction(action.type, action.code, action.modifiers);
}

#if NUM_BUTTONS == 2
void syncButtonsWithoutActions(uint32_t now) {
  for (uint8_t index = 0; index < NUM_BUTTONS; index++) {
    ButtonRuntime &button = buttons[index];
    bool state = digitalRead(buttonPins[index]);

    if (state != button.lastState) {
      button.lastState = state;
      button.lastChange = now;
      sendButtonStateEvent(index, state == LOW ? BUTTON_PRESSED : BUTTON_RELEASED);
    }

    button.tapCount = 0;
    button.longPressHandled = true;
  }
}

bool updateResetChord() {
  uint32_t now = millis();
  bool firstPressed = digitalRead(buttonPins[0]) == LOW;
  bool secondPressed = digitalRead(buttonPins[1]) == LOW;

  if (!resetChordActive && firstPressed && secondPressed) {
    resetChordActive = true;
    resetStartedAt = now;
  }

  if (!resetChordActive) {
    return false;
  }

  syncButtonsWithoutActions(now);

  if (!firstPressed || !secondPressed) {
    resetStartedAt = 0;
  } else {
    if (resetStartedAt == 0) {
      resetStartedAt = now;
    }
    if (now - resetStartedAt >= RESET_DELAY) {
      pinMode(RESET_PIN, OUTPUT);
      digitalWrite(RESET_PIN, LOW);
    }
  }

  if (!firstPressed && !secondPressed) {
    pinMode(RESET_PIN, INPUT);
    resetChordActive = false;
    resetStartedAt = 0;
  }

  return true;
}
#endif

void updateButton(uint8_t buttonIndex) {
  ButtonRuntime &button = buttons[buttonIndex];

  bool state = digitalRead(buttonPins[buttonIndex]);
  uint32_t now = millis();

  if (state != button.lastState && (now - button.lastChange) > DEBOUNCE) {

    button.lastChange = now;
    button.lastState = state;

    if (state == LOW) {
      markActivity();
      releaseBoostStart = now;
      sendButtonStateEvent(buttonIndex, BUTTON_PRESSED);
      button.pressStart = now;
      button.longPressHandled = false;
      if (turboModeEnabled()) {
        sendAction(buttonIndex, 1);
        button.longPressHandled = true;
        button.tapCount = 0;
      }
    }

    if (state == HIGH) {
      sendButtonStateEvent(buttonIndex, BUTTON_RELEASED);
      if (!turboModeEnabled() && !button.longPressHandled) {
        uint32_t pressDuration = now - button.pressStart;

        // Only short taps enter the multi-tap window. A slower first release
        // is treated as an immediate single tap to reduce perceived latency.
        if (button.tapCount == 0 && pressDuration > QUICK_TAP_MAX_PRESS) {
          sendAction(buttonIndex, 1);
        } else {
          button.tapCount++;
          button.lastRelease = now;
        }
      }
    }
  }

  if (turboModeEnabled()) {
    button.tapCount = 0;
    return;
  }

  if (!button.longPressHandled && button.lastState == LOW && (now - button.pressStart) > LONG_PRESS) {

#if THIRD_ACTION_TRIGGER == THIRD_ACTION_TRIGGER_LONG_PRESS
    sendAction(buttonIndex, 3);
#else
    nextBrightness();
#endif
    button.longPressHandled = true;
    button.tapCount = 0;
  }

  if (button.tapCount > 0 && (now - button.lastRelease) > MULTI_TAP_TIMEOUT) {

    sendAction(buttonIndex, button.tapCount);
    button.tapCount = 0;
  }
}

#ifdef ROTARY_ENABLED
void updateRotary() {
  long newPosition = rotaryEncoder.read() / config.encoderSensitivity;

  if (newPosition != rotaryPosition) {
    long diff = newPosition - rotaryPosition;
    rotaryPosition = newPosition;

    GestureAction action = diff > 0 ? config.encoderCW : config.encoderCCW;
    int8_t dir = diff > 0 ? 1 : -1;

    markActivity();
    phaseDirection = dir;
    phaseVelocity = min(phaseVelocity + 2, (int16_t)12);

    sendGestureAction(action.type, action.code, action.modifiers, dir);
    lastBoostTime = millis();
  }

  // Decay velocity back to base
  uint32_t now = millis();
  if (phaseVelocity > BASE_VELOCITY && (now - lastBoostTime) > VELOCITY_DECAY_MS) {
    phaseVelocity--;
    lastBoostTime = now;
  }

  manualPhase += phaseVelocity * phaseDirection;
}
#endif

void animateBreathing(uint8_t baseBrightness) {
#if NUM_LEDS > 0
  uint8_t phaseStep = NUM_LEDS > 1 ? 88 : 0;

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    uint8_t phase = i * phaseStep;
#ifdef ROTARY_ENABLED
    uint8_t angle = phase + (uint8_t)(manualPhase >> 2);
    uint8_t b = scale8(sin8(angle), 145) + 110;
#else
    uint8_t b = beatsin8(15, 110, 255, 0, phase);
#endif

    if (releaseBoostStart > 0) {
      uint32_t elapsed = millis() - releaseBoostStart;
      if (elapsed < BOOST_DURATION) {
        uint8_t t = elapsed * 255 / BOOST_DURATION;
        uint8_t mix = cubicwave8(t);
#ifdef ROTARY_ENABLED
        uint8_t boostAngle = phase * phaseDirection + (uint8_t)(manualPhase >> 1);
        uint8_t fast = scale8(sin8(boostAngle), 145) + 110;
#else
        uint8_t fast = beatsin8(180, 80, 255, 0, phase);
#endif
        b = lerp8by8(b, fast, mix);
      }
    }

    b = scale8(b, baseBrightness);
    leds[i] = baseColor;
    leds[i].nscale8_video(b);
  }
#else
  (void)baseBrightness;
#endif
}

void updateSleep() {
  if (config.sleepTimeout == 0 || isSleeping) return;

  uint32_t timeoutMs = (uint32_t)config.sleepTimeout * 3600000UL;
  if (millis() - lastActivityTime >= timeoutMs) {
    isSleeping = true;
  }
}

void animateRainbow(uint8_t baseBrightness) {
#if NUM_LEDS > 0
  uint8_t hueBase = beatsin8(4, 0, 255);
  uint8_t hueStep = NUM_LEDS > 1 ? 30 : 0;

  if (releaseBoostStart > 0) {
    uint32_t elapsed = millis() - releaseBoostStart;
    if (elapsed < BOOST_DURATION) {
      uint8_t t = elapsed * 255 / BOOST_DURATION;
      uint8_t mix = cubicwave8(t);
      uint8_t fast = beatsin8(40, 0, 255);
      hueBase = lerp8by8(hueBase, fast, mix / 3);
    }
  }

  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    CHSV hsv(hueBase + i * hueStep, 150, baseBrightness);
    hsv2rgb_rainbow(hsv, leds[i]);
  }
#else
  (void)baseBrightness;
#endif
}

void updateLEDs() {
#if NUM_LEDS > 0
  if (isSleeping) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  uint8_t baseBrightness = brightnessLevels[brightnessStep];

  switch (config.animationMode) {
    case 1:
      animateBreathing(baseBrightness);
      break;
    case 2:
      animateRainbow(baseBrightness);
      break;
    default:
      fill_solid(leds, NUM_LEDS, baseColor);
      FastLED.setBrightness(baseBrightness);
      break;
  }

  FastLED.show();
#endif
}

void updateBaseColor() {
#if NUM_LEDS > 0
  baseColor = CRGB(config.red, config.green, config.blue);
#endif
}

bool areConfigActionsValid(const DeviceConfig &candidate) {
  if (!isActionValid(candidate.singleTap) ||
      !isActionValid(candidate.doubleTap) ||
      !isActionValid(candidate.tripleTap)) {
    return false;
  }

#if NUM_BUTTONS == 2
  if (!isActionValid(candidate.button2SingleTap) ||
      !isActionValid(candidate.button2DoubleTap) ||
      !isActionValid(candidate.button2TripleTap)) {
    return false;
  }
#endif

  return true;
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

  if (!areConfigActionsValid(nextConfig)) {
    sendError(Serial, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  updateBaseColor();
  markActivity();
  sendStatusFrame(Serial, CMD_ACK, STATUS_OK);
}

void handleRawHidSetConfig(const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(DeviceConfig));

  if (!isConfigValid(nextConfig, CONFIG_VERSION)) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_CRC);
    return;
  }

  if (!areConfigActionsValid(nextConfig)) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  updateBaseColor();
  markActivity();
  sendRawHidStatusFrame(CMD_ACK, STATUS_OK);
}

bool decodeDeviceOptions(const uint8_t *payload, uint8_t payloadLen, DeviceOptions &options) {
  if (payloadLen != 3 || payload[0] != DEVICE_OPTIONS_VERSION) {
    return false;
  }

  const uint16_t flags = static_cast<uint16_t>(payload[1]) |
                         (static_cast<uint16_t>(payload[2]) << 8);
  if ((flags & ~SUPPORTED_OPTION_FLAGS) != 0) {
    return false;
  }

  options = {DEVICE_OPTIONS_VERSION, flags, 0};
  return true;
}

void handleSetDeviceOptions(const uint8_t *payload, uint8_t payloadLen) {
  DeviceOptions options;
  if (!decodeDeviceOptions(payload, payloadLen, options)) {
    sendError(Serial, STATUS_BAD_PAYLOAD);
    return;
  }

  persistDeviceOptions(options);
  sendStatusFrame(Serial, CMD_ACK, STATUS_OK);
}

void handleRawHidSetDeviceOptions(const uint8_t *payload, uint8_t payloadLen) {
  DeviceOptions options;
  if (!decodeDeviceOptions(payload, payloadLen, options)) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  persistDeviceOptions(options);
  sendRawHidStatusFrame(CMD_ACK, STATUS_OK);
}

void sendRawHidPingFrame() {
  const char *productName = USB_PRODUCT;
  uint8_t nameLen = strlen(productName);
  if (nameLen > RAW_HID_REPORT_SIZE - 8) {
    nameLen = RAW_HID_REPORT_SIZE - 8;
  }

  uint8_t payload[RAW_HID_REPORT_SIZE] = {0};
  payload[0] = STATUS_OK;
  payload[1] = DEVICE_TYPE;
  memcpy(payload + 2, productName, nameLen);
  sendRawHidFrame(CMD_PONG, payload, 2 + nameLen);
}

void handleRawHid() {
  if (RawHID.available() < RAW_HID_REPORT_SIZE) {
    return;
  }

  uint8_t report[RAW_HID_REPORT_SIZE] = {0};
  for (uint8_t i = 0; i < sizeof(report); i++) {
    const int value = RawHID.read();
    if (value < 0) {
      sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_PAYLOAD);
      return;
    }
    report[i] = static_cast<uint8_t>(value);
  }

  if (report[0] != FRAME_MAGIC_1 || report[1] != FRAME_MAGIC_2) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_PAYLOAD);
    return;
  }

  const uint8_t version = report[2];
  const uint8_t cmd = report[3];
  const uint8_t payloadLen = report[4];
  const uint8_t frameLen = 2 + 3 + payloadLen + 1;

  if (version != PROTOCOL_VERSION || frameLen > RAW_HID_REPORT_SIZE) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_COMMAND);
    return;
  }

  uint8_t computedCrc = 0;
  computedCrc = crc8Update(computedCrc, version);
  computedCrc = crc8Update(computedCrc, cmd);
  computedCrc = crc8Update(computedCrc, payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    computedCrc = crc8Update(computedCrc, report[5 + i]);
  }

  if (computedCrc != report[frameLen - 1]) {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_CRC);
    return;
  }

  const uint8_t *payload = report + 5;

  if (cmd == CMD_GET_CONFIG) {
    sendRawHidConfigFrame();
  } else if (cmd == CMD_SET_CONFIG) {
    handleRawHidSetConfig(payload, payloadLen);
  } else if (cmd == CMD_RESET_CONFIG) {
    resetConfig();
    updateBaseColor();
    sendRawHidConfigFrame();
  } else if (cmd == CMD_PING) {
    sendRawHidPingFrame();
  } else if (cmd == CMD_GET_DEVICE_INFO) {
    sendRawHidDeviceInfoFrame();
  } else if (cmd == CMD_GET_DEVICE_OPTIONS) {
    sendRawHidDeviceOptionsFrame();
  } else if (cmd == CMD_SET_DEVICE_OPTIONS) {
    handleRawHidSetDeviceOptions(payload, payloadLen);
  } else {
    sendRawHidStatusFrame(CMD_ERROR, STATUS_BAD_COMMAND);
  }
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
      updateBaseColor();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
      sendPingFrame(Serial, DEVICE_TYPE, USB_PRODUCT);
    } else if (cmd == CMD_GET_DEVICE_INFO) {
      sendDeviceInfoFrame();
    } else if (cmd == CMD_GET_DEVICE_OPTIONS) {
      sendDeviceOptionsFrame();
    } else if (cmd == CMD_SET_DEVICE_OPTIONS) {
      handleSetDeviceOptions(payload, payloadLen);
    } else {
      sendError(Serial, STATUS_BAD_COMMAND);
    }
  }
}

void setup() {

#if BTN_GROUND_PIN >= 0
  pinMode(BTN_GROUND_PIN, OUTPUT);
  digitalWrite(BTN_GROUND_PIN, LOW);
#endif
  for (uint8_t index = 0; index < NUM_BUTTONS; index++) {
    pinMode(buttonPins[index], INPUT_PULLUP);
    buttons[index].lastState = HIGH;
    buttons[index].lastChange = 0;
    buttons[index].pressStart = 0;
    buttons[index].lastRelease = 0;
    buttons[index].tapCount = 0;
    buttons[index].longPressHandled = false;
  }

#if NUM_BUTTONS == 2
  pinMode(RESET_PIN, INPUT);
#endif

#if NUM_LEDS > 0
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();
#endif

  Consumer.begin();
  Keyboard.begin();
  Mouse.begin();
  RawHID.begin(rawHidReport, sizeof(rawHidReport));

  loadConfig();
  updateBaseColor();

  Serial.begin(SERIAL_BAUD);

  lastActivityTime = millis();
  powerOnBlink();
}

void loop() {
  handleSerial();
  handleRawHid();
#if NUM_BUTTONS == 2
  if (!updateResetChord()) {
#endif
    for (uint8_t index = 0; index < NUM_BUTTONS; index++) {
      updateButton(index);
    }
#if NUM_BUTTONS == 2
  }
#endif
#ifdef ROTARY_ENABLED
  updateRotary();
#endif
  updateSleep();
  updateLEDs();

  delay(16);
}
