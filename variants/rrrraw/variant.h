#ifndef _VARIANT_RRRRAW_
#define _VARIANT_RRRRAW_

#define VARIANT_MCK (64000000ul)
#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

// rrrraw needs six distinct Arduino pin slots to allocate one GPIOTE channel
// for each button/encoder input. The sketch remaps these temporary slots to
// the actual raw nRF GPIOs immediately after attachInterrupt().
#define PINS_COUNT (6)
#define NUM_DIGITAL_PINS (6)
#define NUM_ANALOG_INPUTS (0)
#define NUM_ANALOG_OUTPUTS (0)

#define PIN_LED1 (0)
#define PIN_LED2 (1)
#define LED_BUILTIN PIN_LED1
#define LED_CONN PIN_LED1
#define LED_RED PIN_LED1
#define LED_BLUE PIN_LED2
#define LED_STATE_ON 0

#define PIN_BUTTON1 (2)
#define PIN_DFU (2)
#define ADC_RESOLUTION 14

// Keep Serial1 on an invalid Arduino pin, matching the Raytac base variant.
#define PIN_SERIAL1_RX (6)
#define PIN_SERIAL1_TX (6)

#ifdef __cplusplus
}
#endif

#endif
