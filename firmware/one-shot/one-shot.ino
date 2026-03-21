#include <FastLED.h>
#include <HID-Project.h>
#include <EEPROM.h>
#include "device_protocol.h"

#define BTN_GROUND_PIN 9
#define BTN_INPUT_PIN 10

#define DATA_PIN A3
#define NUM_LEDS 2
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
CRGB baseColor = CRGB(250, 255, 210);

const uint16_t MULTI_TAP_TIMEOUT = 250;
const uint16_t QUICK_TAP_MAX_PRESS = 100;
const uint16_t LONG_PRESS = 600;
const uint16_t DEBOUNCE = 10;

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction singleTap;
  GestureAction doubleTap;
  GestureAction tripleTap;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t breathingEnabled;
  uint8_t crc;
};

const uint8_t CONFIG_VERSION = 3;
const int EEPROM_ADDRESS = 0;
DeviceConfig config;

bool lastState = HIGH;
uint32_t lastChange = 0;

uint32_t pressStart = 0;
uint32_t lastRelease = 0;

uint8_t tapCount = 0;
bool longPressHandled = false;

uint8_t brightnessStep = 0;
uint8_t brightnessLevels[] = {255, 191, 128, 64, 0};

uint32_t releaseBoostStart = 0;
const uint16_t BOOST_DURATION = 500;

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTap = {ACTION_TYPE_CONSUMER, MEDIA_PLAY_PAUSE, 0};
  cfg.doubleTap = {ACTION_TYPE_CONSUMER, MEDIA_NEXT, 0};
  cfg.tripleTap = {ACTION_TYPE_CONSUMER, MEDIA_PREVIOUS, 0};
  cfg.red = 250;
  cfg.green = 255;
  cfg.blue = 210;
  cfg.breathingEnabled = 1;
  cfg.crc = 0;
  return cfg;
}

void applyConfig(const DeviceConfig &cfg) {
  config = cfg;
}

void persistConfig(DeviceConfig &cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);
  EEPROM.put(EEPROM_ADDRESS, cfg);
  applyConfig(cfg);
}

void loadConfig() {
  DeviceConfig stored;
  EEPROM.get(EEPROM_ADDRESS, stored);

  if (!isConfigValid(stored, CONFIG_VERSION)) {
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
  sendFrame(Serial, CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

void powerOnBlink() {
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
}

void blinkFeedback(uint8_t count) {
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

void sendGestureAction(uint8_t actionType, uint16_t actionCode, uint8_t modifiers) {
  if (actionType == ACTION_TYPE_HOTKEY) {
    pressModifiers(modifiers);
    Keyboard.press(static_cast<KeyboardKeycode>(actionCode));
    delay(12);
    Keyboard.releaseAll();
    return;
  }

  Consumer.write(actionCode);
}

void sendAction(uint8_t taps) {
  GestureAction action = config.tripleTap;

  if (taps == 1) {
    action = config.singleTap;
  } else if (taps == 2) {
    action = config.doubleTap;
  }

  sendGestureAction(action.type, action.code, action.modifiers);
}

void updateButton() {

  bool state = digitalRead(BTN_INPUT_PIN);
  uint32_t now = millis();

  if (state != lastState && (now - lastChange) > DEBOUNCE) {

    lastChange = now;
    lastState = state;

    if (state == LOW) {
      sendButtonEvent(Serial, BUTTON_PRESSED);
      pressStart = now;
      longPressHandled = false;
    }

    if (state == HIGH) {
      sendButtonEvent(Serial, BUTTON_RELEASED);
      releaseBoostStart = now;
      if (!longPressHandled) {
        uint32_t pressDuration = now - pressStart;

        // Only short taps enter the multi-tap window. A slower first release
        // is treated as an immediate single tap to reduce perceived latency.
        if (tapCount == 0 && pressDuration > QUICK_TAP_MAX_PRESS) {
          sendAction(1);
        } else {
          tapCount++;
          lastRelease = now;
        }
      }
    }
  }

  if (!longPressHandled && lastState == LOW && (now - pressStart) > LONG_PRESS) {

    nextBrightness();
    longPressHandled = true;
    tapCount = 0;
  }

  if (tapCount > 0 && (now - lastRelease) > MULTI_TAP_TIMEOUT) {

    sendAction(tapCount);
    tapCount = 0;
  }
}

void updateLEDs() {
  uint8_t baseBrightness = brightnessLevels[brightnessStep];

  if (!config.breathingEnabled) {
    fill_solid(leds, NUM_LEDS, baseColor);
    FastLED.setBrightness(baseBrightness);
    FastLED.show();
    return;
  }

  uint8_t b1 = beatsin8(15, 110, 255, 0, 0);
  uint8_t b2 = beatsin8(15, 110, 255, 0, 88);

  // On release: blend in a fast pulse that fades in and out over BOOST_DURATION
  if (releaseBoostStart > 0) {
    uint32_t elapsed = millis() - releaseBoostStart;
    if (elapsed < BOOST_DURATION) {
      uint8_t t = elapsed * 255 / BOOST_DURATION;
      uint8_t mix = cubicwave8(t);  // 0→peak→0 smoothly
      uint8_t fast1 = beatsin8(180, 110, 255, 0, 0);
      uint8_t fast2 = beatsin8(180, 110, 255, 0, 88);
      b1 = lerp8by8(b1, fast1, mix);
      b2 = lerp8by8(b2, fast2, mix);
    }
  }

  b1 = scale8(b1, baseBrightness);
  b2 = scale8(b2, baseBrightness);

  CHSV baseHSV = rgb2hsv_approximate(baseColor);
  uint8_t hueShift = beatsin8(4, 0, 255);

  CHSV hsv0 = baseHSV;
  hsv0.hue += hueShift;
  // hsv0.val = b1;

  CHSV hsv1 = baseHSV;
  hsv1.hue += hueShift + 30;
  // hsv1.val = b2;

  hsv2rgb_rainbow(hsv0, leds[0]);
  hsv2rgb_rainbow(hsv1, leds[1]);

  FastLED.show();
}

void updateBaseColor() {
  baseColor = CRGB(config.red, config.green, config.blue);
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
      !isActionValid(nextConfig.tripleTap)) {
    sendError(Serial, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  updateBaseColor();
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
      updateBaseColor();
      sendConfigFrame();
    } else if (cmd == CMD_PING) {
      sendPingFrame(Serial, DEVICE_TYPE_ONE_SHOT);
    } else {
      sendError(Serial, STATUS_BAD_COMMAND);
    }
  }
}

void setup() {

  pinMode(BTN_GROUND_PIN, OUTPUT);
  digitalWrite(BTN_GROUND_PIN, LOW);
  pinMode(BTN_INPUT_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();

  Consumer.begin();
  Keyboard.begin();

  loadConfig();
  updateBaseColor();

  Serial.begin(SERIAL_BAUD);

  powerOnBlink();
}

void loop() {
  handleSerial();
  updateButton();
  updateLEDs();

  delay(16);
}
