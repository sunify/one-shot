#include <FastLED.h>
#include <HID-Project.h>

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

bool lastState = HIGH;
uint32_t lastChange = 0;

uint32_t pressStart = 0;
uint32_t lastRelease = 0;

uint8_t tapCount = 0;
bool longPressHandled = false;

uint8_t brightnessStep = 0;
uint8_t brightnessLevels[] = {255, 191, 128, 64, 0};

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
  if (taps == 1) Consumer.write(MEDIA_PLAY_PAUSE);
  else if (taps == 2) Consumer.write(MEDIA_NEXT);
  else if (taps >= 3) Consumer.write(MEDIA_PREVIOUS);
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

void setup() {

  pinMode(BTN_PIN, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  Consumer.begin();

  Serial.begin(9600);
}

void loop() {

  updateButton();
  updateLEDs();

  delay(16);
}