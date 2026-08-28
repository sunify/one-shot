#include "variant.h"

#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] = {
    45,  // D0: P1.13
    43,  // D1: P1.11
    15,  // D2: P0.15
    2,   // D3: P0.02
    3,   // D4: P0.03
    4,   // D5: P0.04
};

void initVariant() {
  pinMode(PIN_LED1, OUTPUT);
  ledOff(PIN_LED1);
  pinMode(PIN_LED2, OUTPUT);
  ledOff(PIN_LED2);
}
