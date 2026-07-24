#include <Adafruit_SPIFlash.h>
#include "nrf_gpio.h"
#include "nrf_power.h"

namespace {

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlash flash(&flashTransport);

bool deepPowerDownOptionalQspiFlash() {
  if (!flash.begin()) {
    return false;
  }

  flashTransport.runCommand(0xB9);
  delay(10);

  const uint32_t idAfter = flash.getJEDECID();
  flash.end();
  return idAfter == 0xFFFFFF || idAfter == 0xFFFFFFFF;
}

void shutDownBoardPeripherals() {
  // UICPal/Super nRF52840 RGB LED is common-anode and active-low.
  const uint8_t ledPins[] = {LED_RED, LED_GREEN, LED_BLUE};
  for (const uint8_t ledPin : ledPins) {
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, HIGH);
    nrf_gpio_cfg_sense_input(g_ADigitalPinMap[ledPin],
                             NRF_GPIO_PIN_PULLUP,
                             NRF_GPIO_PIN_NOSENSE);
  }

  // This clone has no Sense IMU/microphone and no P0.14 divider switch.
  // Its permanent 1M/1M BAT divider terminates at P0.31.
  nrf_gpio_cfg(
      NRF_GPIO_PIN_MAP(0, 31),
      NRF_GPIO_PIN_DIR_INPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_S0S1,
      NRF_GPIO_PIN_NOSENSE);

  // P0.13 changes the LGS4056H charging-current resistor only while USB is
  // present. Leave it electrically disconnected for the battery-only test.
  nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 13));

  // Seeed's low-power reference explicitly sends the optional onboard QSPI
  // flash to deep power-down before System OFF.
  deepPowerDownOptionalQspiFlash();

#if defined(NRF_SAADC)
  NRF_SAADC->ENABLE = 0;
#endif
#if defined(NRF_USBD)
  NRF_USBD->ENABLE = 0;
#endif
}

[[noreturn]] void enterSystemOffImmediately() {
  shutDownBoardPeripherals();

  // No GPIO wake source in this diagnostic build. This distinguishes true
  // System OFF current from an unintended reset/wake loop.
  nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, 3));
  NRF_P0->LATCH = 0xFFFFFFFF;
  NRF_POWER->DCDCEN = 0;
#if defined(POWER_DCDCEN0_DCDCEN_Msk)
  NRF_POWER->DCDCEN0 = 0;
#endif
  NRF_POWER->SYSTEMOFF = 1;
  __DSB();

  while (true) {
    __WFE();
  }
}

}  // namespace

void setup() {
  enterSystemOffImmediately();
}

void loop() {
  enterSystemOffImmediately();
}
