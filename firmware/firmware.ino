#include <FastLED.h>
#include <HID-Project.h>
#include <EEPROM.h>

#define BTN_PIN 6

#define DATA_PIN 10
#define NUM_LEDS 2
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
CRGB baseColor = CRGB(250, 255, 210);

const uint16_t MULTI_TAP_TIMEOUT = 250;
const uint16_t LONG_PRESS = 600;
const uint16_t DEBOUNCE = 10;

const uint32_t SERIAL_BAUD = 115200;
const uint8_t FRAME_MAGIC_1 = 0x4F;
const uint8_t FRAME_MAGIC_2 = 0x53;
const uint8_t PROTOCOL_VERSION = 1;

enum Command : uint8_t {
  CMD_GET_CONFIG = 0x01,
  CMD_SET_CONFIG = 0x02,
  CMD_RESET_CONFIG = 0x03,
  CMD_PING = 0x04,
  CMD_CONFIG = 0x81,
  CMD_ACK = 0x82,
  CMD_PONG = 0x84,
  CMD_ERROR = 0xFF
};

enum StatusCode : uint8_t {
  STATUS_OK = 0x00,
  STATUS_BAD_PAYLOAD = 0x01,
  STATUS_BAD_CRC = 0x02,
  STATUS_BAD_COMMAND = 0x03
};

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  uint16_t singleTapCode;
  uint16_t doubleTapCode;
  uint16_t tripleTapCode;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t crc;
};

const uint8_t CONFIG_VERSION = 1;
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

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTapCode = MEDIA_PLAY_PAUSE;
  cfg.doubleTapCode = MEDIA_NEXT;
  cfg.tripleTapCode = MEDIA_PREVIOUS;
  cfg.red = 250;
  cfg.green = 255;
  cfg.blue = 210;
  cfg.crc = 0;
  return cfg;
}

uint8_t crc8Update(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++) {
    crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }
  return crc;
}

uint8_t computeConfigCrc(const DeviceConfig &cfg) {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&cfg);
  uint8_t crc = 0;
  for (uint8_t i = 0; i < sizeof(DeviceConfig) - 1; i++) {
    crc = crc8Update(crc, raw[i]);
  }
  return crc;
}

bool isConfigValid(const DeviceConfig &cfg) {
  return cfg.version == CONFIG_VERSION && cfg.crc == computeConfigCrc(cfg);
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

  if (!isConfigValid(stored)) {
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

void sendFrame(uint8_t cmd, const uint8_t *payload, uint8_t payloadLen) {
  uint8_t crc = 0;
  crc = crc8Update(crc, PROTOCOL_VERSION);
  crc = crc8Update(crc, cmd);
  crc = crc8Update(crc, payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    crc = crc8Update(crc, payload[i]);
  }

  Serial.write(FRAME_MAGIC_1);
  Serial.write(FRAME_MAGIC_2);
  Serial.write(PROTOCOL_VERSION);
  Serial.write(cmd);
  Serial.write(payloadLen);
  if (payloadLen > 0) {
    Serial.write(payload, payloadLen);
  }
  Serial.write(crc);
}

void sendConfigFrame() {
  sendFrame(CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

void sendStatusFrame(uint8_t cmd, uint8_t status) {
  uint8_t payload[1] = {status};
  sendFrame(cmd, payload, sizeof(payload));
}

void sendError(uint8_t status) {
  sendStatusFrame(CMD_ERROR, status);
}

void blinkFeedback(uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    fill_solid(leds, NUM_LEDS, CRGB::White);
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

  blinkFeedback(brightnessStep == 0 ? 3 : 1);
}

void sendAction(uint8_t taps) {
  uint16_t code = config.tripleTapCode;

  if (taps == 1) code = config.singleTapCode;
  else if (taps == 2) code = config.doubleTapCode;

  Consumer.write(code);
}

void updateButton() {

  bool state = digitalRead(BTN_PIN);
  uint32_t now = millis();

  if (state != lastState && (now - lastChange) > DEBOUNCE) {

    lastChange = now;
    lastState = state;

    if (state == LOW) {
      pressStart = now;
      longPressHandled = false;
    }

    if (state == HIGH) {
      if (!longPressHandled) {
        tapCount++;
        lastRelease = now;
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

  uint8_t b1 = beatsin8(15, 110, 255, 0, 0);
  uint8_t b2 = beatsin8(15, 110, 255, 0, 88);

  b1 = scale8(b1, baseBrightness);
  b2 = scale8(b2, baseBrightness);

  leds[0] = baseColor;
  leds[0].nscale8_video(b1);

  leds[1] = baseColor;
  leds[1].nscale8_video(b2);

  FastLED.show();
}

void updateBaseColor() {
  baseColor = CRGB(config.red, config.green, config.blue);
}

bool readExact(uint8_t *buffer, uint8_t len) {
  uint32_t startedAt = millis();
  uint8_t offset = 0;

  while (offset < len) {
    if (Serial.available()) {
      buffer[offset++] = Serial.read();
      startedAt = millis();
      continue;
    }

    if (millis() - startedAt > 100) {
      return false;
    }
  }

  return true;
}

void handleSetConfig(const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    sendError(STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(DeviceConfig));

  if (!isConfigValid(nextConfig)) {
    sendError(STATUS_BAD_CRC);
    return;
  }

  persistConfig(nextConfig);
  updateBaseColor();
  sendStatusFrame(CMD_ACK, STATUS_OK);
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
    if (!readExact(header, sizeof(header))) {
      sendError(STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t version = header[0];
    uint8_t cmd = header[1];
    uint8_t payloadLen = header[2];
    uint8_t payload[sizeof(DeviceConfig)] = {0};

    if (payloadLen > sizeof(payload)) {
      sendError(STATUS_BAD_PAYLOAD);
      return;
    }

    if (!readExact(payload, payloadLen)) {
      sendError(STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t receivedCrc = 0;
    if (!readExact(&receivedCrc, 1)) {
      sendError(STATUS_BAD_PAYLOAD);
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
      sendError(STATUS_BAD_COMMAND);
      continue;
    }

    if (receivedCrc != computedCrc) {
      sendError(STATUS_BAD_CRC);
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
      sendStatusFrame(CMD_PONG, STATUS_OK);
    } else {
      sendError(STATUS_BAD_COMMAND);
    }
  }
}

void setup() {

  pinMode(BTN_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear();

  Consumer.begin();

  loadConfig();
  updateBaseColor();

  Serial.begin(SERIAL_BAUD);
}

void loop() {
  handleSerial();
  updateButton();
  updateLEDs();

  delay(16);
}
