#include <Arduino.h>
#include <nrf.h>

// Minimal System OFF floor-current diagnostic for the UICPal MINI clone.
//
// Companion to magic-button-xiao-power-test. That sketch aggressively tears
// down peripherals, bit-bangs the external flash and drives many GPIO to fixed
// levels before System OFF. This one does NONE of that. The two binaries share
// the exact same startup stage (a constructor with priority 101, before
// initVariant(), TinyUSB and setup()), so they differ only in this body.
//
// Question this answers: is the ~0.56 mA System OFF current created by our own
// active configuration (driven pins / peripheral pokes / flash access), or is
// it a load this firmware never touches (external flash chip or silicon)?
//
//   - If this "do nothing" binary reads well below 0.56 mA -> the current is
//     something the power-test binary itself sets up. Bisect from there.
//   - If this binary also reads ~0.56 mA -> our GPIO/peripheral config is not
//     the cause; the load is the external flash or the die.
//
// Before System OFF this only:
//   1. Waits a generous, recoverable USB-DFU window (never SYSTEMOFF instantly).
//   2. Puts every GPIO into the safe high-impedance INPUT_DISCONNECT state:
//      no pin driven, no internal pull, no sense, nothing floating. This is the
//      smallest possible GPIO contribution and cannot source current itself.
//   3. NRF_POWER->SYSTEMOFF = 1.
//
// Recovery: this sketch has no wake source. To reflash, double-tap RESET to
// enter the UF2 bootloader (the bootloader runs before this code).

namespace {

// ~64 MHz core. A volatile decrement loop burns a few cycles per iteration;
// the exact duration is irrelevant as long as it is comfortably over the ~5 s
// minimum DFU window. This lands somewhere around 10-20 s.
void busyWaitRecoveryWindow() {
  for (uint32_t outer = 0; outer < 12; ++outer) {
    for (volatile uint32_t i = 0; i < 10000000UL; ++i) {
      __NOP();
    }
  }
}

void disconnectAllGpioHighZ() {
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
}

[[noreturn]] void enterFloorSystemOff() {
  __disable_irq();

  // Keep the board recoverable: never power down before a long DFU window.
  busyWaitRecoveryWindow();

  // Nothing driven, nothing pulled, nothing floating.
  disconnectAllGpioHighZ();

  NRF_POWER->SYSTEMOFF = 1;
  __DSB();

  while (true) {
    __WFE();
  }
}

// Runs before Arduino main(), initVariant(), FreeRTOS, TinyUSB and ordinary
// static constructors. SystemInit and the C runtime's RAM setup are the only
// startup stages that have already run at this point -- identical to the
// companion power-test sketch.
[[gnu::constructor(101), gnu::used, noreturn]] void earlyFloorSystemOff() {
  enterFloorSystemOff();
}

}  // namespace

void setup() {
  enterFloorSystemOff();
}

void loop() {
}
