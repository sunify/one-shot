#include <Arduino.h>
#include <nrf.h>
#include <nrf_sdm.h>

namespace {

bool softDeviceWasEnabled = false;
bool debugWasEnabled = false;

void holdP0High(uint8_t pin) {
  NRF_P0->OUTSET = (1UL << pin);
  NRF_P0->PIN_CNF[pin] =
      (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void holdP0Low(uint8_t pin) {
  NRF_P0->OUTCLR = (1UL << pin);
  NRF_P0->PIN_CNF[pin] =
      (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
}

void putExternalFlashInDeepPowerDown() {
  constexpr uint8_t sck = 21;
  constexpr uint8_t cs = 25;
  constexpr uint8_t io0 = 20;

  holdP0Low(sck);
  holdP0High(cs);
  holdP0Low(io0);
  holdP0High(22);  // IO2/WP inactive.
  holdP0High(23);  // IO3/HOLD inactive.

  delayMicroseconds(2);
  NRF_P0->OUTCLR = (1UL << cs);

  constexpr uint8_t command = 0xB9;
  for (int8_t bit = 7; bit >= 0; --bit) {
    if (command & (1U << bit)) {
      NRF_P0->OUTSET = (1UL << io0);
    } else {
      NRF_P0->OUTCLR = (1UL << io0);
    }
    delayMicroseconds(1);
    NRF_P0->OUTSET = (1UL << sck);
    delayMicroseconds(1);
    NRF_P0->OUTCLR = (1UL << sck);
  }

  NRF_P0->OUTSET = (1UL << cs);
  delayMicroseconds(20);
}

void disconnectAllGpio() {
  const uint32_t disconnectedInput =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);

  for (uint8_t pin = 0; pin < 32; ++pin) {
    NRF_P0->PIN_CNF[pin] = disconnectedInput;
    NRF_P1->PIN_CNF[pin] = disconnectedInput;
  }

  // Preserve the safe levels deliberately established by initVariant().
  holdP0High(14);  // Battery divider disabled.
  holdP0High(25);  // External flash deselected.
  holdP0Low(21);   // External flash SCK at a CMOS level.
  holdP0Low(20);   // External flash IO0 at a CMOS level.
  holdP0Low(24);   // External flash IO1 at a CMOS level.
  holdP0High(22);  // External flash IO2/WP inactive.
  holdP0High(23);  // External flash IO3/HOLD inactive.
  holdP0Low(28);   // D2 diagnostic: LOW means System OFF was not left.
  if (debugWasEnabled) {
    holdP0High(29);  // P0.29 HIGH: the CPU debug domain was enabled.
  } else {
    holdP0Low(29);   // P0.29 LOW: the CPU debug domain was disabled.
  }

  // D1/P0.03 is the only wake source in this diagnostic. Shorting it to GND
  // must cause an OFF reset if the SYSTEMOFF write was actually accepted.
  NRF_P0->PIN_CNF[3] =
      (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
      (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) |
      (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) |
      (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) |
      (GPIO_PIN_CNF_SENSE_Low << GPIO_PIN_CNF_SENSE_Pos);

  holdP0High(26);  // RGB red off (common anode).
  holdP0High(30);  // RGB green off (common anode).
  holdP0High(6);   // RGB blue off (common anode).
}

[[noreturn]] void indicateUnexpectedSystemOffWake() {
  __disable_irq();
  disconnectAllGpio();
  holdP0High(28);  // D2 diagnostic: HIGH means a wake from System OFF occurred.

  while (true) {
    __WFE();
  }
}

[[noreturn]] void enterSystemOff() {
  __disable_irq();

  NRF_QSPI->TASKS_DEACTIVATE = 1;
  *(volatile uint32_t *)0x40029010UL = 1UL;
  *(volatile uint32_t *)0x40029054UL = 1UL;
  NRF_QSPI->ENABLE = QSPI_ENABLE_ENABLE_Disabled;
  putExternalFlashInDeepPowerDown();

  NRF_USBD->ENABLE = USBD_ENABLE_ENABLE_Disabled;
  NRF_SAADC->ENABLE = SAADC_ENABLE_ENABLE_Disabled;
  NRF_PDM->ENABLE = PDM_ENABLE_ENABLE_Disabled;
  NRF_PWM0->ENABLE = PWM_ENABLE_ENABLE_Disabled;
  NRF_PWM1->ENABLE = PWM_ENABLE_ENABLE_Disabled;
  NRF_PWM2->ENABLE = PWM_ENABLE_ENABLE_Disabled;
  NRF_PWM3->ENABLE = PWM_ENABLE_ENABLE_Disabled;
  NRF_SPIM0->ENABLE = SPIM_ENABLE_ENABLE_Disabled;
  NRF_SPIM1->ENABLE = SPIM_ENABLE_ENABLE_Disabled;
  NRF_SPIM2->ENABLE = SPIM_ENABLE_ENABLE_Disabled;
  NRF_SPIM3->ENABLE = SPIM_ENABLE_ENABLE_Disabled;
  NRF_TWIM0->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
  NRF_TWIM1->ENABLE = TWIM_ENABLE_ENABLE_Disabled;
  NRF_UARTE0->ENABLE = UARTE_ENABLE_ENABLE_Disabled;
  NRF_UARTE1->ENABLE = UARTE_ENABLE_ENABLE_Disabled;

  disconnectAllGpio();

  NRF_POWER->SYSTEMOFF = 1;
  __DSB();

  // This instruction is unreachable if SYSTEMOFF was accepted. HIGH therefore
  // proves that the CPU continued executing after the power-down request.
  holdP0High(28);

  while (true) {
    __WFE();
  }
}

// Run before Arduino main(), initVariant(), FreeRTOS, TinyUSB, and ordinary
// static constructors. SystemInit and the C runtime's RAM setup are the only
// startup stages that have already run at this point.
[[gnu::constructor(101), gnu::used, noreturn]] void earlySystemOff() {
  debugWasEnabled =
      (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0;

  const uint32_t resetReason = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = resetReason;
  if ((resetReason & POWER_RESETREAS_OFF_Msk) != 0) {
    indicateUnexpectedSystemOffWake();
  }

  uint8_t softDeviceEnabled = 0;
  if (sd_softdevice_is_enabled(&softDeviceEnabled) == NRF_SUCCESS &&
      softDeviceEnabled != 0) {
    softDeviceWasEnabled = true;
    (void) sd_softdevice_disable();
  }

  enterSystemOff();
}

}  // namespace

void setup() {
  enterSystemOff();
}

void loop() {
}
