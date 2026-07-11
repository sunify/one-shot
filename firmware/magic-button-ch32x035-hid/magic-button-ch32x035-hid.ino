#include <Arduino.h>
#include <USBHIDKeyboard.h>

#if defined(PB11)
constexpr uint8_t BUTTON_PIN = PB11;
#elif defined(PIN_PB11)
constexpr uint8_t BUTTON_PIN = PIN_PB11;
#else
#error "This Arduino core does not define PB11/PIN_PB11."
#endif

constexpr uint16_t DEBOUNCE_MS = 25;
constexpr uint16_t REARM_MS = 250;
constexpr uint8_t HID_KEY_S = 0x16;
constexpr uint8_t RAW_HID_KEY_OFFSET = 136;

int stableState = HIGH;
int lastRawState = HIGH;
uint32_t lastRawChangeAt = 0;
uint32_t lastActionAt = 0;

void sendAltS() {
  Keyboard.releaseAll();
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press(RAW_HID_KEY_OFFSET + HID_KEY_S);
  delay(12);
  Keyboard.releaseAll();
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  stableState = digitalRead(BUTTON_PIN);
  lastRawState = stableState;
  lastRawChangeAt = millis();

  Keyboard.begin();
  Keyboard.releaseAll();
}

void loop() {
  const int rawState = digitalRead(BUTTON_PIN);
  const uint32_t now = millis();

  if (rawState != lastRawState) {
    lastRawState = rawState;
    lastRawChangeAt = now;
    return;
  }

  if (rawState == stableState || now - lastRawChangeAt < DEBOUNCE_MS) {
    return;
  }

  stableState = rawState;
  if (stableState == LOW && now - lastActionAt >= REARM_MS) {
    sendAltS();
    lastActionAt = now;
  }
}
