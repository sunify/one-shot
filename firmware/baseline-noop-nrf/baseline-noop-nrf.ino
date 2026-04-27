#include "nrf_gpio.h"
#include "nrf_power.h"
#include "nrf_soc.h"

void setup() {
  NRF_P0->LATCH = 0xFFFFFFFF;
  NRF_P1->LATCH = 0xFFFFFFFF;
  NRF_GPIOTE->EVENTS_PORT = 0;

  for (uint8_t pin = 0; pin < 32; pin++) {
    nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(0, pin));
  }
  for (uint8_t pin = 0; pin < 16; pin++) {
    nrf_gpio_cfg_default(NRF_GPIO_PIN_MAP(1, pin));
  }

  NRF_POWER->DCDCEN = 1;

  NRF_POWER->SYSTEMOFF = 1;
  __DSB();
  while (true) {
    __WFE();
  }
}

void loop() {}
