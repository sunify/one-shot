#include <Adafruit_TinyUSB.h>
#include "nrf_gpio.h"

namespace {

const uint32_t BATTERY_READ_ENABLE_GPIO = NRF_GPIO_PIN_MAP(0, 14);

void keepBatteryDividerSafe() {
  // XIAO's battery divider enable is active-low. Use sink-only drive as
  // required by the board schematic, so the GPIO never sources this net.
  nrf_gpio_cfg(
      BATTERY_READ_ENABLE_GPIO,
      NRF_GPIO_PIN_DIR_OUTPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_S0D1,
      NRF_GPIO_PIN_NOSENSE);
  nrf_gpio_pin_clear(BATTERY_READ_ENABLE_GPIO);
}

void turnRgbLedOff() {
  const uint32_t ledPins[] = {
      NRF_GPIO_PIN_MAP(0, 26),
      NRF_GPIO_PIN_MAP(0, 6),
      NRF_GPIO_PIN_MAP(0, 30),
  };

  for (uint8_t i = 0; i < 3; i++) {
    nrf_gpio_cfg_output(ledPins[i]);
    nrf_gpio_pin_set(ledPins[i]);
  }
}

}  // namespace

void setup() {
  keepBatteryDividerSafe();
  turnRgbLedOff();
  Serial.begin(115200);
  Serial.println("XIAO recovery firmware");
}

void loop() {
  delay(1000);
}
