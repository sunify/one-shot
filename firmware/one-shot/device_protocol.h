#pragma once

#include <Arduino.h>
#include <Stream.h>

const uint32_t SERIAL_BAUD = 115200;
const uint8_t FRAME_MAGIC_1 = 0x4F;
const uint8_t FRAME_MAGIC_2 = 0x53;
const uint8_t PROTOCOL_VERSION = 1;

const uint8_t DEVICE_TYPE_ONE_SHOT = 0x01;
const uint8_t DEVICE_TYPE_MAGIC_BUTTON = 0x02;

const uint8_t ACTION_TYPE_CONSUMER = 0x01;
const uint8_t ACTION_TYPE_HOTKEY = 0x02;

const uint8_t MODIFIER_CTRL = 0x01;
const uint8_t MODIFIER_SHIFT = 0x02;
const uint8_t MODIFIER_ALT = 0x04;
const uint8_t MODIFIER_GUI = 0x08;

enum Command : uint8_t {
  CMD_GET_CONFIG = 0x01,
  CMD_SET_CONFIG = 0x02,
  CMD_RESET_CONFIG = 0x03,
  CMD_PING = 0x04,
  CMD_CONFIG = 0x81,
  CMD_ACK = 0x82,
  CMD_PONG = 0x84,
  CMD_BUTTON_EVENT = 0x90,
  CMD_ERROR = 0xFF
};

enum StatusCode : uint8_t {
  STATUS_OK = 0x00,
  STATUS_BAD_PAYLOAD = 0x01,
  STATUS_BAD_CRC = 0x02,
  STATUS_BAD_COMMAND = 0x03
};

enum ButtonEventState : uint8_t {
  BUTTON_RELEASED = 0x00,
  BUTTON_PRESSED = 0x01
};

struct __attribute__((packed)) GestureAction {
  uint8_t type;
  uint16_t code;
  uint8_t modifiers;
};

inline uint8_t crc8Update(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8; i++) {
    crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
  }
  return crc;
}

template <typename TConfig>
inline uint8_t computeConfigCrc(const TConfig &cfg) {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&cfg);
  uint8_t crc = 0;
  for (uint8_t i = 0; i < sizeof(TConfig) - 1; i++) {
    crc = crc8Update(crc, raw[i]);
  }
  return crc;
}

template <typename TConfig>
inline bool isConfigValid(const TConfig &cfg, uint8_t configVersion) {
  return cfg.version == configVersion && cfg.crc == computeConfigCrc(cfg);
}

inline void sendFrame(Stream &serialPort, uint8_t cmd, const uint8_t *payload, uint8_t payloadLen) {
  uint8_t crc = 0;
  crc = crc8Update(crc, PROTOCOL_VERSION);
  crc = crc8Update(crc, cmd);
  crc = crc8Update(crc, payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    crc = crc8Update(crc, payload[i]);
  }

  serialPort.write(FRAME_MAGIC_1);
  serialPort.write(FRAME_MAGIC_2);
  serialPort.write(PROTOCOL_VERSION);
  serialPort.write(cmd);
  serialPort.write(payloadLen);
  if (payloadLen > 0) {
    serialPort.write(payload, payloadLen);
  }
  serialPort.write(crc);
}

inline void sendStatusFrame(Stream &serialPort, uint8_t cmd, uint8_t status) {
  uint8_t payload[1] = {status};
  sendFrame(serialPort, cmd, payload, sizeof(payload));
}

inline void sendPingFrame(Stream &serialPort, uint8_t deviceType, const char *productName = nullptr) {
  if (productName) {
    uint8_t nameLen = strlen(productName);
    uint8_t payloadLen = 2 + nameLen;
    uint8_t payload[64];
    payload[0] = STATUS_OK;
    payload[1] = deviceType;
    memcpy(payload + 2, productName, nameLen);
    sendFrame(serialPort, CMD_PONG, payload, payloadLen);
  } else {
    uint8_t payload[2] = {STATUS_OK, deviceType};
    sendFrame(serialPort, CMD_PONG, payload, sizeof(payload));
  }
}

inline void sendButtonEvent(Stream &serialPort, uint8_t state) {
  uint8_t payload[1] = {state};
  sendFrame(serialPort, CMD_BUTTON_EVENT, payload, sizeof(payload));
}

inline void sendError(Stream &serialPort, uint8_t status) {
  sendStatusFrame(serialPort, CMD_ERROR, status);
}

inline bool readExact(Stream &serialPort, uint8_t *buffer, uint8_t len) {
  uint32_t startedAt = millis();
  uint8_t offset = 0;

  while (offset < len) {
    if (serialPort.available()) {
      buffer[offset++] = serialPort.read();
      startedAt = millis();
      continue;
    }

    if (millis() - startedAt > 100) {
      return false;
    }
  }

  return true;
}

inline bool isActionValid(const GestureAction &action) {
  if (action.type == ACTION_TYPE_CONSUMER) {
    return action.modifiers == 0;
  }

  if (action.type == ACTION_TYPE_HOTKEY) {
    return action.code <= 0xFF;
  }

  return false;
}
