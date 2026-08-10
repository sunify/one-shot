#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <FreeRTOS.h>
#include <task.h>
#include "nrf_gpio.h"
#include "nrf_power.h"
#include "nrfx_qspi.h"
#include "device_protocol.h"

using namespace Adafruit_LittleFS_Namespace;

namespace {

const char *DEVICE_NAME = "Super Magic Button";
const char *MANUFACTURER_NAME = "Huntflow";
const char *MODEL_NAME = "Super Magic Button";
const char *CONFIG_FILE_PATH = "/magic-button.cfg";
const bool ENABLE_SERIAL_DEBUG = false;
const bool FORCE_BLE_HID_ONLY = false;
const bool RECOVERY_MODE = false;

#ifndef BUTTON_PIN_PORT
#define BUTTON_PIN_PORT 0
#endif

#ifndef BUTTON_PIN_NUMBER
#define BUTTON_PIN_NUMBER 20
#endif

#ifndef BUTTON_INTERRUPT_PIN
// The compact Raytac variant exposes only three Arduino pin slots. Slot 2 is
// used to allocate a core-managed GPIOTE callback, then remapped below to the
// actual raw button GPIO (P0.20 on the nice!nano build).
#define BUTTON_INTERRUPT_PIN 2
#endif

#if defined(BUTTON_PIN_PORT) && defined(BUTTON_PIN_NUMBER)
const uint32_t BUTTON_GPIO = NRF_GPIO_PIN_MAP(BUTTON_PIN_PORT, BUTTON_PIN_NUMBER);
#define USE_RAW_BUTTON_GPIO 1
#endif

#ifndef BUTTON_GROUND_PIN_PORT
#define BUTTON_GROUND_PIN_PORT 0
#endif

#ifndef BUTTON_GROUND_PIN_NUMBER
#define BUTTON_GROUND_PIN_NUMBER 2
#endif

#if BUTTON_GROUND_PIN_PORT >= 0 && BUTTON_GROUND_PIN_NUMBER >= 0
const uint32_t BUTTON_GROUND_GPIO = NRF_GPIO_PIN_MAP(BUTTON_GROUND_PIN_PORT, BUTTON_GROUND_PIN_NUMBER);
#define USE_RAW_BUTTON_GROUND_GPIO 1
#endif

// SuperMini nRF52840 external VCC/LDO cutoff. The faulty 5.6K pull-up is
// removed on this board, so hold the enable gate deterministically low.
const uint32_t SUPERMINI_VCC_CUTOFF_GPIO = NRF_GPIO_PIN_MAP(0, 13);

#ifndef BATTERY_ENABLE_PIN_PORT
#define BATTERY_ENABLE_PIN_PORT -1
#endif

#ifndef BATTERY_ENABLE_PIN_NUMBER
#define BATTERY_ENABLE_PIN_NUMBER -1
#endif

#if BATTERY_ENABLE_PIN_PORT >= 0 && BATTERY_ENABLE_PIN_NUMBER >= 0
const uint32_t BATTERY_ENABLE_GPIO = NRF_GPIO_PIN_MAP(BATTERY_ENABLE_PIN_PORT, BATTERY_ENABLE_PIN_NUMBER);
#define USE_BATTERY_ENABLE_GPIO 1
#endif

#ifndef STATUS_LED_PIN_PORT
#define STATUS_LED_PIN_PORT 0
#endif

#ifndef STATUS_LED_PIN_NUMBER
#define STATUS_LED_PIN_NUMBER 22
#endif

#ifndef STATUS_LED_IS_ACTIVE_LOW
#define STATUS_LED_IS_ACTIVE_LOW 0
#endif

#ifndef STATUS_LED_ENABLED
#define STATUS_LED_ENABLED 1
#endif

#ifndef INITIALIZE_BUILTIN_LED
#define INITIALIZE_BUILTIN_LED 1
#endif

#ifndef DISABLE_NFC_PINS
#define DISABLE_NFC_PINS 1
#endif

#ifndef USB_VBUS_DETECT_ONLY
#define USB_VBUS_DETECT_ONLY 0
#endif

#ifndef ENABLE_DCDC_REGULATOR
#define ENABLE_DCDC_REGULATOR 0
#endif

#ifndef DCDC_BATTERY_ONLY
#define DCDC_BATTERY_ONLY 0
#endif

#ifndef BATTERY_MEASURE_INTERNAL_VDD
#if defined(BOARD_UICPAL_MINI_NRF52840) || defined(USE_BATTERY_ENABLE_GPIO)
#define BATTERY_MEASURE_INTERNAL_VDD 0
#else
#define BATTERY_MEASURE_INTERNAL_VDD 1
#endif
#endif

#ifndef BATTERY_CHEMISTRY_CR2032
#define BATTERY_CHEMISTRY_CR2032 BATTERY_MEASURE_INTERNAL_VDD
#endif

const uint8_t CONFIG_VERSION = 1;
const uint16_t SECOND_TAP_TIMEOUT = 180;
const uint16_t THIRD_TAP_TIMEOUT = 100;
const uint16_t QUICK_TAP_MAX_PRESS = 160;
const uint16_t LONG_PRESS_TIME = 600;
const uint16_t CLEAR_BONDS_HOLD_MS = 10000;
const uint16_t DEBOUNCE_TIME = 10;
const uint16_t REPORT_DELAY_MS = 12;
const uint32_t IDLE_SLEEP_TIMEOUT_MS = 15UL * 1000UL;
const uint32_t DEEP_SLEEP_TIMEOUT_MS = 4UL * 60UL * 60UL * 1000UL;
const uint32_t DISCONNECTED_DEEP_SLEEP_TIMEOUT_MS = 90UL * 60UL * 1000UL;
const uint16_t ACTIVE_POLL_DELAY_MS = 5;
const uint16_t IDLE_POLL_DELAY_MS = 20;
const uint32_t MAX_IDLE_BLOCK_MS = 15UL * 1000UL;
const uint32_t BATTERY_UPDATE_INTERVAL_MS = 300000;
const uint8_t BATTERY_SAMPLE_COUNT = 3;
const uint8_t BATTERY_PERCENT_HYSTERESIS = 1;
const uint8_t BATTERY_USB_CHARGE_STEP_PERCENT = 1;
const int16_t BATTERY_CALIBRATION_OFFSET_MV = 0;
#if BATTERY_MEASURE_INTERNAL_VDD
// On the modified SuperMini, B+, VDDH and VDD are tied together. The SAADC's
// internal VDD input is VDD/4, so no always-on external divider is required.
const uint32_t BATTERY_ADC_PSEL = SAADC_CH_PSELP_PSELP_VDD;
const uint8_t BATTERY_ADC_DIVIDER_MULTIPLIER = 4;
const uint16_t BATTERY_ADC_FULL_SCALE_MV = 1200;
#elif defined(BOARD_UICPAL_MINI_NRF52840)
// The UICPal MINI has no usable onboard BAT measurement path. Use an external
// 1M/1M divider with its midpoint connected to D0 (P0.02/AIN0).
const uint32_t BATTERY_ADC_GPIO = NRF_GPIO_PIN_MAP(0, 2);
const uint32_t BATTERY_ADC_PSEL = SAADC_CH_PSELP_PSELP_AnalogInput0;
const uint8_t BATTERY_ADC_DIVIDER_MULTIPLIER = 2;
const uint16_t BATTERY_ADC_FULL_SCALE_MV = 3000;
#else
const uint32_t BATTERY_ADC_GPIO = NRF_GPIO_PIN_MAP(0, 31);
const uint32_t BATTERY_ADC_PSEL = SAADC_CH_PSELP_PSELP_AnalogInput7;
const uint8_t BATTERY_ADC_DIVIDER_MULTIPLIER = 2;
const uint16_t BATTERY_ADC_FULL_SCALE_MV = 3000;
#endif
const bool IDLE_USES_SYSTEM_OFF = false;
const uint16_t BLE_ADVERTISING_INTERVAL_FAST = 32;
const uint16_t BLE_ADVERTISING_INTERVAL_SLOW = 160;
const uint16_t BLE_ADVERTISING_FAST_TIMEOUT_SEC = 5;
const uint16_t BLE_CONN_INTERVAL_MIN = 9;
const uint16_t BLE_CONN_INTERVAL_MAX = 12;
const uint16_t BLE_CONN_SLAVE_LATENCY = 0;
const uint16_t BLE_CONN_SUPERVISION_TIMEOUT = 400;
const int8_t BLE_TX_POWER_FAST_DBM = -8;
const uint16_t BLE_IDLE_CONN_INTERVAL_MIN = 12;
const uint16_t BLE_IDLE_CONN_INTERVAL_MAX = 12;
const uint16_t BLE_IDLE_CONN_SLAVE_LATENCY = 15;
const uint16_t BLE_IDLE_CONN_SUPERVISION_TIMEOUT = 400;
const int8_t BLE_TX_POWER_IDLE_DBM = -8;
const uint32_t BLE_IDLE_PARAMS_DELAY_MS = 2000;
const uint32_t STATUS_LED_GPIO = NRF_GPIO_PIN_MAP(STATUS_LED_PIN_PORT, STATUS_LED_PIN_NUMBER);
const bool STATUS_LED_ACTIVE_LOW = STATUS_LED_IS_ACTIVE_LOW != 0;
const uint32_t DEBUG_LED_GPIO = STATUS_LED_GPIO;
const uint16_t DEBUG_LED_PULSE_MS = 40;
const uint16_t DEBUG_LED_GAP_MS = 70;
const uint16_t DEBUG_REASON_PULSE_MS = 80;
const uint16_t DEBUG_REASON_GAP_MS = 220;
const uint32_t SLEEP_DEBUG_REPEAT_MS = 2000;
const uint32_t PENDING_GESTURE_MAX_AGE_MS = 120000;
const uint32_t PENDING_GESTURE_RETRY_INTERVAL_MS = 200;
const uint8_t BLE_HID_READY_STABLE_COUNT = 5;
const uint8_t WAKE_GPIOTE_CHANNEL = 7;

enum UsbHidReportId : uint8_t {
  USB_REPORT_ID_KEYBOARD = 1,
  USB_REPORT_ID_CONSUMER_CONTROL = 2,
};

const uint8_t USB_HID_REPORT_DESCRIPTOR[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(USB_REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(USB_REPORT_ID_CONSUMER_CONTROL)),
};

enum GestureCode : uint8_t {
  GESTURE_SINGLE_TAP = 1,
  GESTURE_DOUBLE_TAP = 2,
  GESTURE_LONG_PRESS = 3
};

struct __attribute__((packed)) DeviceConfig {
  uint8_t version;
  GestureAction singleTap;
  GestureAction doubleTap;
  GestureAction longPress;
  uint8_t crc;
};

class MagicButtonBleHid : public BLEHidAdafruit {
 public:
  void setInputReportCccdCallback(BLECharacteristic::write_cccd_cb_t callback) {
    if (_chr_inputs) {
      _chr_inputs[0].setCccdWriteCallback(callback);
      _chr_inputs[1].setCccdWriteCallback(callback);
    }
    if (_chr_boot_keyboard_input) {
      _chr_boot_keyboard_input->setCccdWriteCallback(callback);
    }
  }

  bool keyboardNotifyEnabled(uint16_t connHandle) {
    if (isBootMode()) {
      return _chr_boot_keyboard_input && _chr_boot_keyboard_input->notifyEnabled(connHandle);
    }

    return _chr_inputs && _chr_inputs[0].notifyEnabled(connHandle);
  }

  bool consumerNotifyEnabled(uint16_t connHandle) {
    return !isBootMode() && _chr_inputs && _chr_inputs[1].notifyEnabled(connHandle);
  }

  bool isKeyboardInputReport(BLECharacteristic *characteristic) {
    return characteristic &&
           ((_chr_inputs && characteristic == &_chr_inputs[0]) ||
            (_chr_boot_keyboard_input && characteristic == _chr_boot_keyboard_input));
  }

  bool isConsumerInputReport(BLECharacteristic *characteristic) {
    return characteristic && _chr_inputs && characteristic == &_chr_inputs[1];
  }
};

class MagicButtonBatteryService : public BLEBas {
 public:
  void setLevelCccdCallback(BLECharacteristic::write_cccd_cb_t callback) {
    _battery.setCccdWriteCallback(callback);
  }
};

BLEDis deviceInfo;
MagicButtonBleHid hid;
MagicButtonBatteryService batteryService;
BLEUart configBle;
Adafruit_USBD_HID usbHid;
DeviceConfig config;

bool storageReady = false;
bool buttonState = HIGH;
bool lastRawButtonState = HIGH;
bool longPressSent = false;
bool bondsClearedThisPress = false;
uint32_t lastDebounceAt = 0;
uint32_t pressStartedAt = 0;
uint32_t lastReleaseAt = 0;
uint8_t tapCount = 0;
uint32_t lastActivityAt = 0;
uint32_t lastSleepDebugAt = 0;
uint32_t lastConnParamsCheckAt = 0;
uint32_t lastBatteryUpdateAt = 0;
uint32_t bleSecuredAt = 0;
uint8_t pendingGestureCode = 0;
uint32_t pendingGestureAt = 0;
uint32_t lastPendingGestureRetryAt = 0;
uint8_t pendingBleReadyStableCount = 0;
bool pendingWakeGesture = false;
bool sleeping = false;
bool usbInitialized = false;
bool usbVbusActive = false;
bool lastVbusPresent = false;
bool wokeFromButtonLatch = false;
uint32_t lastHeartbeatAt = 0;
bool bleIdleParamsRequested = false;
bool keyboardNotifySubscribedThisConnection = false;
bool consumerNotifySubscribedThisConnection = false;
bool batteryInitialized = false;
TaskHandle_t loopTaskHandle = nullptr;
bool buttonInterruptAttached = false;
bool bleReadySeen = false;
uint8_t batteryLevel = 100;
uint16_t batteryMillivolts = 4200;

const uint32_t HEARTBEAT_INTERVAL_MS = 2500;

void setupDebugLed() {
  nrf_gpio_cfg_output(DEBUG_LED_GPIO);
  nrf_gpio_pin_clear(DEBUG_LED_GPIO);
}

void setStatusLed(bool enabled) {
#if !STATUS_LED_ENABLED
  enabled = false;
#endif
  bool driveHigh = STATUS_LED_ACTIVE_LOW ? !enabled : enabled;
  if (driveHigh) {
    nrf_gpio_pin_set(STATUS_LED_GPIO);
  } else {
    nrf_gpio_pin_clear(STATUS_LED_GPIO);
  }
}

void setupStatusLed() {
  nrf_gpio_cfg_output(STATUS_LED_GPIO);
  setStatusLed(false);
}

void setDebugLed(bool enabled) {
  setStatusLed(enabled);
}

void pulseDebugLed(uint8_t pulses, uint16_t pulseMs = DEBUG_LED_PULSE_MS, uint16_t gapMs = DEBUG_LED_GAP_MS) {
  for (uint8_t i = 0; i < pulses; i++) {
    setDebugLed(true);
    delay(pulseMs);
    setDebugLed(false);

    if (i + 1 < pulses) {
      delay(gapMs);
    }
  }
}

void debugPrintln(const char *message) {
  if (!ENABLE_SERIAL_DEBUG) {
    return;
  }

  Serial.println(message);
}

bool shouldLogSerial() {
  return false;
  return ENABLE_SERIAL_DEBUG || usbVbusActive;
}

bool isUsbHidActive();
int readButtonState();

void wakeLoopTask() {
  if (loopTaskHandle) {
    xTaskNotifyGive(loopTaskHandle);
  }
}

void buttonPressInterrupt() {
  if (!loopTaskHandle) {
    return;
  }

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(loopTaskHandle, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void markActivity(uint32_t now = millis()) {
  lastActivityAt = now;
}

bool shouldQueueWakeGesture() {
  return !isUsbHidActive() && wokeFromButtonLatch;
}

void resetBleHidReadyState() {
  pendingBleReadyStableCount = 0;
  keyboardNotifySubscribedThisConnection = false;
  consumerNotifySubscribedThisConnection = false;
}

void ensureBleFastProfile(uint32_t now = millis()) {
  Bluefruit.setTxPower(BLE_TX_POWER_FAST_DBM);
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected() || !connection->secured()) {
      continue;
    }

    connection->requestConnectionParameter(
        BLE_CONN_INTERVAL_MIN,
        BLE_CONN_SLAVE_LATENCY,
        BLE_CONN_SUPERVISION_TIMEOUT);
  }
  bleIdleParamsRequested = false;
  if (bleSecuredAt != 0) {
    bleSecuredAt = now;
  }
}

void hidInputCccdCallback(uint16_t connHandle, BLECharacteristic *characteristic, uint16_t value) {
  (void) connHandle;

  bool notifyEnabled = (value & BLE_GATT_HVX_NOTIFICATION) != 0;
  if (hid.isKeyboardInputReport(characteristic)) {
    keyboardNotifySubscribedThisConnection = notifyEnabled;
  } else if (hid.isConsumerInputReport(characteristic)) {
    consumerNotifySubscribedThisConnection = notifyEnabled;
  }

  if (notifyEnabled && pendingGestureCode != 0) {
    lastPendingGestureRetryAt = 0;
  }
  wakeLoopTask();
}

void batteryLevelCccdCallback(
    uint16_t connHandle,
    BLECharacteristic *characteristic,
    uint16_t value) {
  (void) characteristic;
  if ((value & BLE_GATT_HVX_NOTIFICATION) != 0) {
    batteryService.notify(connHandle, batteryLevel);
  }
  wakeLoopTask();
}

void queuePendingGesture(uint8_t gestureCode, uint32_t now = millis(), bool wakeGesture = false) {
  if (pendingGestureCode == 0) {
    pendingGestureAt = now;
    pendingBleReadyStableCount = 0;
    pendingWakeGesture = wakeGesture;
  }
  pendingGestureCode = gestureCode;
  lastPendingGestureRetryAt = now;
}

void setupButtonInput() {
#if defined(USE_RAW_BUTTON_GPIO)
  nrf_gpio_cfg_input(BUTTON_GPIO, NRF_GPIO_PIN_PULLUP);
#else
  pinMode(BUTTON_PIN, INPUT_PULLUP);
#endif
}

void setupButtonInterrupt() {
#if BUTTON_INTERRUPT_PIN >= 0
  loopTaskHandle = xTaskGetCurrentTaskHandle();
  const int interruptMask = attachInterrupt(BUTTON_INTERRUPT_PIN, buttonPressInterrupt, FALLING);
  buttonInterruptAttached = interruptMask != 0;

#if defined(USE_RAW_BUTTON_GPIO)
  if (buttonInterruptAttached) {
    const uint8_t channel = static_cast<uint8_t>(__builtin_ctz(static_cast<unsigned>(interruptMask)));
    uint32_t config = NRF_GPIOTE->CONFIG[channel];
    config &= ~(GPIOTE_CONFIG_PSEL_Msk | GPIOTE_CONFIG_PORT_Msk);
    config |= ((uint32_t) BUTTON_PIN_NUMBER << GPIOTE_CONFIG_PSEL_Pos) |
              ((uint32_t) BUTTON_PIN_PORT << GPIOTE_CONFIG_PORT_Pos);
    NRF_GPIOTE->CONFIG[channel] = config;
  }
#endif
#endif
}

int readButtonState() {
#if defined(USE_RAW_BUTTON_GPIO)
  return nrf_gpio_pin_read(BUTTON_GPIO) ? HIGH : LOW;
#else
  return digitalRead(BUTTON_PIN);
#endif
}

void setupButtonGround() {
#if defined(USE_RAW_BUTTON_GROUND_GPIO)
  nrf_gpio_cfg_output(BUTTON_GROUND_GPIO);
  nrf_gpio_pin_clear(BUTTON_GROUND_GPIO);
#endif
}

void holdButtonGroundLow() {
#if defined(USE_RAW_BUTTON_GROUND_GPIO)
  nrf_gpio_cfg_output(BUTTON_GROUND_GPIO);
  nrf_gpio_pin_clear(BUTTON_GROUND_GPIO);
#endif
}

void holdSuperMiniExternalVccOff() {
  // Set the output latch before changing direction to avoid an enable pulse.
  nrf_gpio_pin_clear(SUPERMINI_VCC_CUTOFF_GPIO);
  nrf_gpio_cfg_output(SUPERMINI_VCC_CUTOFF_GPIO);
}

void setupBatteryMeasurement() {
#if defined(USE_BATTERY_ENABLE_GPIO)
  // XIAO nRF52840 uses an active-low switch for its battery divider.
  // Keep it enabled while the firmware may sample P0.31. Sink-only prevents
  // accidentally driving this battery-connected node high.
  nrf_gpio_cfg(
      BATTERY_ENABLE_GPIO,
      NRF_GPIO_PIN_DIR_OUTPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_S0D1,
      NRF_GPIO_PIN_NOSENSE);
  nrf_gpio_pin_clear(BATTERY_ENABLE_GPIO);
#endif
}

void prepareBatteryPinsForSystemOff() {
#if defined(BOARD_UICPAL_MINI_NRF52840)
  // Leave the external divider's ADC input buffer disconnected after sampling.
  nrf_gpio_cfg(
      BATTERY_ADC_GPIO,
      NRF_GPIO_PIN_DIR_INPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_S0S1,
      NRF_GPIO_PIN_NOSENSE);
#endif
}

uint16_t readBatteryAdcMillivolts() {
#if defined(NRF_SAADC)
  setupBatteryMeasurement();
#if !BATTERY_MEASURE_INTERNAL_VDD
  nrf_gpio_cfg(
      BATTERY_ADC_GPIO,
      NRF_GPIO_PIN_DIR_INPUT,
      NRF_GPIO_PIN_INPUT_DISCONNECT,
      NRF_GPIO_PIN_NOPULL,
      NRF_GPIO_PIN_S0S1,
      NRF_GPIO_PIN_NOSENSE);
#endif

  volatile int16_t raw = 0;
  NRF_SAADC->ENABLE = (SAADC_ENABLE_ENABLE_Enabled << SAADC_ENABLE_ENABLE_Pos);
  NRF_SAADC->RESOLUTION = SAADC_RESOLUTION_VAL_12bit;
  NRF_SAADC->OVERSAMPLE = SAADC_OVERSAMPLE_OVERSAMPLE_Bypass;

  for (uint8_t i = 0; i < 8; i++) {
    NRF_SAADC->CH[i].PSELN = SAADC_CH_PSELP_PSELP_NC;
    NRF_SAADC->CH[i].PSELP = SAADC_CH_PSELP_PSELP_NC;
  }

  NRF_SAADC->CH[0].CONFIG =
      ((SAADC_CH_CONFIG_RESP_Bypass << SAADC_CH_CONFIG_RESP_Pos) & SAADC_CH_CONFIG_RESP_Msk) |
      ((SAADC_CH_CONFIG_RESN_Bypass << SAADC_CH_CONFIG_RESN_Pos) & SAADC_CH_CONFIG_RESN_Msk) |
#if BATTERY_MEASURE_INTERNAL_VDD
      ((SAADC_CH_CONFIG_GAIN_Gain1_2 << SAADC_CH_CONFIG_GAIN_Pos) & SAADC_CH_CONFIG_GAIN_Msk) |
#else
      ((SAADC_CH_CONFIG_GAIN_Gain1_5 << SAADC_CH_CONFIG_GAIN_Pos) & SAADC_CH_CONFIG_GAIN_Msk) |
#endif
      ((SAADC_CH_CONFIG_REFSEL_Internal << SAADC_CH_CONFIG_REFSEL_Pos) & SAADC_CH_CONFIG_REFSEL_Msk) |
      ((SAADC_CH_CONFIG_TACQ_40us << SAADC_CH_CONFIG_TACQ_Pos) & SAADC_CH_CONFIG_TACQ_Msk) |
      ((SAADC_CH_CONFIG_MODE_SE << SAADC_CH_CONFIG_MODE_Pos) & SAADC_CH_CONFIG_MODE_Msk) |
      ((SAADC_CH_CONFIG_BURST_Disabled << SAADC_CH_CONFIG_BURST_Pos) & SAADC_CH_CONFIG_BURST_Msk);
  NRF_SAADC->CH[0].PSELN = SAADC_CH_PSELP_PSELP_NC;
  NRF_SAADC->CH[0].PSELP = BATTERY_ADC_PSEL;
  // Discard the first conversion so the SAADC sampling capacitor can settle,
  // then average several fresh conversions.
  int32_t rawSum = 0;
  for (uint8_t sampleIndex = 0; sampleIndex <= BATTERY_SAMPLE_COUNT; sampleIndex++) {
    raw = 0;
    NRF_SAADC->RESULT.PTR = reinterpret_cast<uint32_t>(&raw);
    NRF_SAADC->RESULT.MAXCNT = 1;

    NRF_SAADC->EVENTS_STARTED = 0;
    NRF_SAADC->TASKS_START = 1;
    while (!NRF_SAADC->EVENTS_STARTED) {
    }

    NRF_SAADC->EVENTS_END = 0;
    NRF_SAADC->TASKS_SAMPLE = 1;
    while (!NRF_SAADC->EVENTS_END) {
    }

    NRF_SAADC->EVENTS_STOPPED = 0;
    NRF_SAADC->TASKS_STOP = 1;
    while (!NRF_SAADC->EVENTS_STOPPED) {
    }

    if (sampleIndex > 0 && raw > 0) {
      rawSum += raw;
    }
  }

  NRF_SAADC->ENABLE = (SAADC_ENABLE_ENABLE_Disabled << SAADC_ENABLE_ENABLE_Pos);
  uint32_t rawAverage = static_cast<uint32_t>(rawSum / BATTERY_SAMPLE_COUNT);
  uint32_t batteryMv =
      (rawAverage * BATTERY_ADC_FULL_SCALE_MV * BATTERY_ADC_DIVIDER_MULTIPLIER) / 4095;
  return static_cast<uint16_t>(batteryMv + BATTERY_CALIBRATION_OFFSET_MV);
#else
  return batteryMillivolts;
#endif
}

uint16_t readBatteryMillivolts() {
  return readBatteryAdcMillivolts();
}

uint8_t batteryPercentFromMillivolts(uint16_t millivolts) {
  struct BatteryPoint {
    uint16_t millivolts;
    uint8_t percent;
  };

#if BATTERY_CHEMISTRY_CR2032
  static const BatteryPoint curve[] = {
      {3050, 100},
      {3000, 90},
      {2950, 75},
      {2900, 60},
      {2850, 45},
      {2800, 30},
      {2700, 15},
      {2600, 5},
      {2500, 0},
  };
#else
  static const BatteryPoint curve[] = {
      {4130, 100},
      {4100, 96},
      {4050, 88},
      {4000, 78},
      {3950, 68},
      {3900, 58},
      {3850, 45},
      {3800, 35},
      {3750, 26},
      {3700, 18},
      {3650, 12},
      {3600, 8},
      {3550, 4},
      {3500, 0},
  };
#endif

  if (millivolts >= curve[0].millivolts) {
    return curve[0].percent;
  }

  const size_t lastIndex = (sizeof(curve) / sizeof(curve[0])) - 1;
  if (millivolts <= curve[lastIndex].millivolts) {
    return curve[lastIndex].percent;
  }

  for (size_t i = 0; i < lastIndex; i++) {
    const BatteryPoint &upper = curve[i];
    const BatteryPoint &lower = curve[i + 1];

    if (millivolts > lower.millivolts) {
      const uint16_t spanMv = upper.millivolts - lower.millivolts;
      const uint8_t spanPercent = upper.percent - lower.percent;
      const uint16_t offsetMv = millivolts - lower.millivolts;
      return lower.percent + static_cast<uint8_t>((offsetMv * spanPercent) / spanMv);
    }
  }

  return 0;
}

void updateBatteryLevel(bool forceNotify = false) {
  uint16_t newMillivolts = readBatteryMillivolts();
  uint8_t previousLevel = batteryLevel;

  batteryMillivolts = newMillivolts;
  if (usbVbusActive && batteryInitialized && !BATTERY_CHEMISTRY_CR2032) {
    uint8_t measuredLevel = batteryPercentFromMillivolts(newMillivolts);
    if (measuredLevel < batteryLevel) {
      int delta = static_cast<int>(batteryLevel) - static_cast<int>(measuredLevel);
      if (delta >= BATTERY_PERCENT_HYSTERESIS) {
        batteryLevel = measuredLevel;
      }
    } else if (measuredLevel > batteryLevel) {
      uint8_t nextLevel = batteryLevel + BATTERY_USB_CHARGE_STEP_PERCENT;
      batteryLevel = nextLevel > measuredLevel ? measuredLevel : nextLevel;
    }
    batteryService.write(batteryLevel);
    if (forceNotify || batteryLevel != previousLevel) {
      batteryService.notify(batteryLevel);
    }
    return;
  }

  uint8_t measuredLevel = batteryPercentFromMillivolts(newMillivolts);
  if (!batteryInitialized) {
    batteryLevel = measuredLevel;
    batteryInitialized = true;
  } else {
    int delta = static_cast<int>(measuredLevel) - static_cast<int>(batteryLevel);
    if (delta < 0) {
      delta = -delta;
    }

    if (delta >= BATTERY_PERCENT_HYSTERESIS) {
      batteryLevel = measuredLevel;
    }
  }

  batteryService.write(batteryLevel);
  if (forceNotify || batteryLevel != previousLevel) {
    batteryService.notify(batteryLevel);
  }
}

DeviceConfig defaultConfig() {
  DeviceConfig cfg;
  cfg.version = CONFIG_VERSION;
  cfg.singleTap = {ACTION_TYPE_HOTKEY, 0x16, MODIFIER_ALT};
  cfg.doubleTap = {ACTION_TYPE_HOTKEY, 0x29, 0};
  cfg.longPress = {ACTION_TYPE_HOTKEY, 0x28, MODIFIER_CTRL | MODIFIER_GUI};
  cfg.crc = 0;
  return cfg;
}

void applyConfig(const DeviceConfig &cfg) {
  config = cfg;
}

bool writeConfigToStorage(const DeviceConfig &cfg) {
  if (!storageReady) {
    return false;
  }

  if (InternalFS.exists(CONFIG_FILE_PATH)) {
    InternalFS.remove(CONFIG_FILE_PATH);
  }

  File file(CONFIG_FILE_PATH, FILE_O_WRITE, InternalFS);
  if (!file) {
    return false;
  }

  size_t written = file.write(reinterpret_cast<const uint8_t *>(&cfg), sizeof(DeviceConfig));
  file.close();
  return written == sizeof(DeviceConfig);
}

bool readConfigFromStorage(DeviceConfig &cfg) {
  if (!storageReady) {
    return false;
  }

  File file(CONFIG_FILE_PATH, FILE_O_READ, InternalFS);
  if (!file || file.size() != sizeof(DeviceConfig)) {
    file.close();
    return false;
  }

  int readBytes = file.read(&cfg, sizeof(DeviceConfig));
  file.close();
  return readBytes == sizeof(DeviceConfig);
}

void persistConfig(DeviceConfig &cfg) {
  cfg.version = CONFIG_VERSION;
  cfg.crc = computeConfigCrc(cfg);
  writeConfigToStorage(cfg);
  applyConfig(cfg);
}

void loadConfig() {
  DeviceConfig stored;
  if (readConfigFromStorage(stored) && isConfigValid(stored, CONFIG_VERSION)) {
    applyConfig(stored);
    return;
  }

  stored = defaultConfig();
  persistConfig(stored);
}

void resetConfig() {
  DeviceConfig cfg = defaultConfig();
  persistConfig(cfg);
}

void sendConfigFrame(Stream &transport) {
  sendFrame(transport, CMD_CONFIG, reinterpret_cast<const uint8_t *>(&config), sizeof(DeviceConfig));
}

uint8_t toKeyboardModifiers(uint8_t modifiers) {
  uint8_t result = 0;

  if (modifiers & MODIFIER_CTRL) result |= KEYBOARD_MODIFIER_LEFTCTRL;
  if (modifiers & MODIFIER_SHIFT) result |= KEYBOARD_MODIFIER_LEFTSHIFT;
  if (modifiers & MODIFIER_ALT) result |= KEYBOARD_MODIFIER_LEFTALT;
  if (modifiers & MODIFIER_GUI) result |= KEYBOARD_MODIFIER_LEFTGUI;

  return result;
}

bool isUsbHidActive() {
  if (FORCE_BLE_HID_ONLY) {
    return false;
  }
  return usbInitialized && usbVbusActive && TinyUSBDevice.mounted() && usbHid.ready();
}

bool isRawVbusPresent() {
#if NRF_POWER_HAS_USBREG
#if USB_VBUS_DETECT_ONLY
  return nrf_power_usbregstatus_vbusdet_get(NRF_POWER);
#else
  return nrf_power_usbregstatus_vbusdet_get(NRF_POWER) &&
         nrf_power_usbregstatus_outrdy_get(NRF_POWER);
#endif
#else
  return TinyUSBDevice.mounted();
#endif
}

void configureDcdcForPowerSource(bool vbusPresent) {
#if ENABLE_DCDC_REGULATOR
  const bool enable = !DCDC_BATTERY_ONLY || !vbusPresent;
  sd_power_dcdc_mode_set(enable ? NRF_POWER_DCDC_ENABLE : NRF_POWER_DCDC_DISABLE);
#else
  (void) vbusPresent;
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_DISABLE);
#if defined(NRF52840_XXAA)
  // The nice!nano-compatible bootloader may leave the high-voltage REG0
  // converter enabled. Early nRF52840 silicon has a ~300 uA System ON idle
  // penalty in this mode (errata 197), so explicitly force its LDO path.
  sd_power_dcdc0_mode_set(NRF_POWER_DCDC_DISABLE);
#endif
#endif
}

void putUnusedExternalFlashInDeepPowerDown() {
#if defined(BOARD_UICPAL_MINI_NRF52840)
  const uint8_t sck = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_SCK]);
  const uint8_t cs = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_CS]);
  const uint8_t io0 = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_IO0]);
  const uint8_t io1 = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_IO1]);
  const uint8_t io2 = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_IO2]);
  const uint8_t io3 = static_cast<uint8_t>(g_ADigitalPinMap[PIN_QSPI_IO3]);
  const nrfx_qspi_config_t qspiConfig = {
      .xip_offset = 0,
      .pins = {
          .sck_pin = sck,
          .csn_pin = cs,
          .io0_pin = io0,
          .io1_pin = io1,
          .io2_pin = io2,
          .io3_pin = io3,
      },
      .prot_if = {
          .readoc = NRF_QSPI_READOC_READ4O,
          .writeoc = NRF_QSPI_WRITEOC_PP4O,
          .addrmode = NRF_QSPI_ADDRMODE_24BIT,
          .dpmconfig = false,
      },
      .phy_if = {
          .sck_delay = 10,
          .dpmen = false,
          .spi_mode = NRF_QSPI_MODE_0,
          .sck_freq = NRF_QSPI_FREQ_32MDIV16,
      },
      .irq_priority = 7,
  };

  if (nrfx_qspi_init(&qspiConfig, nullptr, nullptr) == NRFX_SUCCESS) {
    const nrf_qspi_cinstr_conf_t command = {
        .opcode = 0xB9,
        .length = NRF_QSPI_CINSTR_LEN_1B,
        .io2_level = true,
        .io3_level = true,
        .wipwait = false,
        .wren = false,
    };
    (void) nrfx_qspi_cinstr_xfer(&command, nullptr, nullptr);
    delayMicroseconds(10);
    nrfx_qspi_uninit();
  }

  // P25Q16H specifies its deep-power-down current with every input held at a
  // CMOS level. Keep CS high and avoid floating QSPI inputs in System OFF.
  nrf_gpio_cfg_output(sck);
  nrf_gpio_pin_clear(sck);
  nrf_gpio_cfg_output(cs);
  nrf_gpio_pin_set(cs);
  nrf_gpio_cfg_output(io0);
  nrf_gpio_pin_clear(io0);
  nrf_gpio_cfg_output(io1);
  nrf_gpio_pin_clear(io1);
  nrf_gpio_cfg_output(io2);
  nrf_gpio_pin_set(io2);
  nrf_gpio_cfg_output(io3);
  nrf_gpio_pin_set(io3);
#endif
}

bool isVbusPresent() {
  return usbVbusActive;
}

bool hasActiveUsbSession() {
  return usbInitialized && usbVbusActive && TinyUSBDevice.mounted();
}

bool sendUsbKeyboardAction(uint8_t keycode, uint8_t modifiers) {
  if (!isUsbHidActive()) {
    return false;
  }

  if (TinyUSBDevice.suspended()) {
    TinyUSBDevice.remoteWakeup();
    delay(4);
  }

  uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
  uint8_t keyboardModifiers = toKeyboardModifiers(modifiers);
  if (!usbHid.keyboardReport(USB_REPORT_ID_KEYBOARD, keyboardModifiers, keycodes)) {
    return false;
  }

  delay(REPORT_DELAY_MS);
  usbHid.keyboardRelease(USB_REPORT_ID_KEYBOARD);
  return true;
}

bool sendBleKeyboardAction(uint8_t keycode, uint8_t modifiers) {
  bool sent = false;
  bool sawConnection = false;
  uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
  uint8_t keyboardModifiers = toKeyboardModifiers(modifiers);
  ensureBleFastProfile();

  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected()) {
      continue;
    }

    sawConnection = true;

    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }

    if (!hid.keyboardReport(connHandle, keyboardModifiers, keycodes)) {
      if (shouldLogSerial()) {
        Serial.print("[ble-tx] keyboardReport=false handle=");
        Serial.println(connHandle);
      }
      continue;
    }

    delay(REPORT_DELAY_MS);
    hid.keyRelease(connHandle);
    sent = true;
  }

  if (!sawConnection && shouldLogSerial()) {
    Serial.println("[ble-tx] no active connection");
  }

  return sent;
}

bool sendConsumerAction(uint16_t usageCode) {
  if (isUsbHidActive()) {
    if (TinyUSBDevice.suspended()) {
      TinyUSBDevice.remoteWakeup();
      delay(4);
    }

    if (!usbHid.sendReport16(USB_REPORT_ID_CONSUMER_CONTROL, usageCode)) {
      return false;
    }

    delay(REPORT_DELAY_MS);
    usbHid.sendReport16(USB_REPORT_ID_CONSUMER_CONTROL, 0);
    return true;
  }

  bool sent = false;
  ensureBleFastProfile();

  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected()) {
      continue;
    }

    if (!connection->secured()) {
      connection->requestPairing();
      continue;
    }

    if (!hid.consumerKeyPress(connHandle, usageCode)) {
      continue;
    }

    delay(REPORT_DELAY_MS);
    hid.consumerKeyRelease(connHandle);
    sent = true;
  }

  return sent;
}

bool sendGestureAction(const GestureAction &action) {
  if (action.type == ACTION_TYPE_HOTKEY) {
    if (isUsbHidActive()) {
      return sendUsbKeyboardAction(static_cast<uint8_t>(action.code), action.modifiers);
    }

    return sendBleKeyboardAction(static_cast<uint8_t>(action.code), action.modifiers);
  }

  if (action.type == ACTION_TYPE_CONSUMER) {
    return sendConsumerAction(action.code);
  }

  return false;
}

GestureAction actionForGesture(uint8_t gestureCode) {
  GestureAction action = config.longPress;

  if (gestureCode == GESTURE_SINGLE_TAP) {
    action = config.singleTap;
  } else if (gestureCode == GESTURE_DOUBLE_TAP) {
    action = config.doubleTap;
  }

  return action;
}

void sendAction(uint8_t gestureCode) {
  uint32_t now = millis();
  GestureAction action = actionForGesture(gestureCode);

  bool sent = sendGestureAction(action);
  if (ENABLE_SERIAL_DEBUG) {
    Serial.print("gesture=");
    Serial.print(gestureCode);
    Serial.print(" sent=");
    Serial.println(sent ? "yes" : "no");
  }

  if (sent) {
    pendingGestureCode = 0;
    pendingWakeGesture = false;
  } else if (!isUsbHidActive()) {
    queuePendingGesture(gestureCode, now);
  }
}

bool hasActiveBleConnection();
bool hasSecuredBleConnection();
bool hasBleReadyForAction(const GestureAction &action);
GestureAction actionForGesture(uint8_t gestureCode);
void exitSleep();

void flushPendingGesture(uint32_t now) {
  if (pendingGestureCode == 0) {
    return;
  }
  if (now - pendingGestureAt >= PENDING_GESTURE_MAX_AGE_MS) {
    pendingGestureCode = 0;
    pendingWakeGesture = false;
    return;
  }
  if (now - lastPendingGestureRetryAt < PENDING_GESTURE_RETRY_INTERVAL_MS) {
    return;
  }
  GestureAction action = actionForGesture(pendingGestureCode);
  if (pendingWakeGesture) {
    if (!hasBleReadyForAction(action)) {
      pendingBleReadyStableCount = 0;
      return;
    }
    if (pendingBleReadyStableCount < BLE_HID_READY_STABLE_COUNT) {
      pendingBleReadyStableCount++;
      return;
    }
  } else if (!hasSecuredBleConnection()) {
    return;
  }
  uint8_t code = pendingGestureCode;
  pendingGestureCode = 0;
  pendingWakeGesture = false;
  pendingBleReadyStableCount = 0;
  sendAction(code);
}

void handleSetConfig(Stream &transport, const uint8_t *payload, uint8_t payloadLen) {
  if (payloadLen != sizeof(DeviceConfig)) {
    sendError(transport, STATUS_BAD_PAYLOAD);
    return;
  }

  DeviceConfig nextConfig;
  memcpy(&nextConfig, payload, sizeof(DeviceConfig));

  if (!isConfigValid(nextConfig, CONFIG_VERSION)) {
    sendError(transport, STATUS_BAD_CRC);
    return;
  }

  if (!isActionValid(nextConfig.singleTap) ||
      !isActionValid(nextConfig.doubleTap) ||
      !isActionValid(nextConfig.longPress)) {
    sendError(transport, STATUS_BAD_PAYLOAD);
    return;
  }

  persistConfig(nextConfig);
  sendStatusFrame(transport, CMD_ACK, STATUS_OK);
}

void handleProtocolTransport(Stream &transport) {
  while (transport.available() >= 2) {
    if (transport.read() != FRAME_MAGIC_1) {
      continue;
    }

    if (transport.read() != FRAME_MAGIC_2) {
      continue;
    }

    uint8_t header[3];
    if (!readExact(transport, header, sizeof(header))) {
      sendError(transport, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t version = header[0];
    uint8_t cmd = header[1];
    uint8_t payloadLen = header[2];
    uint8_t payload[sizeof(DeviceConfig)] = {0};

    if (payloadLen > sizeof(payload)) {
      sendError(transport, STATUS_BAD_PAYLOAD);
      return;
    }

    if (!readExact(transport, payload, payloadLen)) {
      sendError(transport, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t receivedCrc = 0;
    if (!readExact(transport, &receivedCrc, 1)) {
      sendError(transport, STATUS_BAD_PAYLOAD);
      return;
    }

    uint8_t computedCrc = 0;
    computedCrc = crc8Update(computedCrc, version);
    computedCrc = crc8Update(computedCrc, cmd);
    computedCrc = crc8Update(computedCrc, payloadLen);
    for (uint8_t i = 0; i < payloadLen; i++) {
      computedCrc = crc8Update(computedCrc, payload[i]);
    }

    if (version != PROTOCOL_VERSION) {
      sendError(transport, STATUS_BAD_COMMAND);
      continue;
    }

    if (receivedCrc != computedCrc) {
      sendError(transport, STATUS_BAD_CRC);
      continue;
    }

    if (cmd == CMD_GET_CONFIG) {
      markActivity();
      sendConfigFrame(transport);
    } else if (cmd == CMD_SET_CONFIG) {
      markActivity();
      handleSetConfig(transport, payload, payloadLen);
    } else if (cmd == CMD_RESET_CONFIG) {
      markActivity();
      resetConfig();
      sendConfigFrame(transport);
    } else if (cmd == CMD_PING) {
      markActivity();
      // Keep BLE responses within the default 20-byte ATT payload. The
      // browser already receives DEVICE_NAME from the Bluetooth device.
      const char *productName = &transport == &configBle ? nullptr : DEVICE_NAME;
      sendPingFrame(transport, DEVICE_TYPE_MAGIC_BUTTON, productName);
    } else {
      sendError(transport, STATUS_BAD_COMMAND);
    }
  }
}

bool hasActiveBleConnection() {
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (connection && connection->connected()) {
      return true;
    }
  }

  return false;
}

uint32_t currentDeepSleepTimeoutMs() {
  return hasActiveBleConnection()
      ? DEEP_SLEEP_TIMEOUT_MS
      : DISCONNECTED_DEEP_SLEEP_TIMEOUT_MS;
}

bool hasSecuredBleConnection() {
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (connection && connection->connected() && connection->secured()) {
      return true;
    }
  }

  return false;
}

bool hasBleReadyForAction(const GestureAction &action) {
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected() || !connection->secured()) {
      continue;
    }

    if (action.type == ACTION_TYPE_HOTKEY &&
        keyboardNotifySubscribedThisConnection &&
        hid.keyboardNotifyEnabled(connHandle)) {
      return true;
    }
    if (action.type == ACTION_TYPE_CONSUMER &&
        consumerNotifySubscribedThisConnection &&
        hid.consumerNotifyEnabled(connHandle)) {
      return true;
    }
  }

  return false;
}

void disconnectAllBleConnections() {
  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected()) {
      continue;
    }

    connection->disconnect();
  }
}

void startAdvertising();

void emitHeartbeat(uint32_t now) {
  if (!shouldLogSerial()) {
    return;
  }

  if (now - lastHeartbeatAt < HEARTBEAT_INTERVAL_MS) {
    return;
  }
  lastHeartbeatAt = now;

  uint8_t connectedCount = 0;
  uint8_t securedCount = 0;
  for (uint16_t h = 0; h < BLE_MAX_CONNECTION; h++) {
    BLEConnection *c = Bluefruit.Connection(h);
    if (!c || !c->connected()) {
      continue;
    }
    connectedCount++;
    if (c->secured()) {
      securedCount++;
    }
  }

  Serial.print("[hb] up=");
  Serial.print(now / 1000);
  Serial.print("s sleep=");
  Serial.print(sleeping ? 1 : 0);
  Serial.print(" conn=");
  Serial.print(connectedCount);
  Serial.print(" sec=");
  Serial.print(securedCount);
  Serial.print(" btn=");
  Serial.print(buttonState == LOW ? 'L' : 'H');
  Serial.print('/');
  Serial.print(lastRawButtonState == LOW ? 'L' : 'H');
  Serial.print(" pend=");
  Serial.print(pendingGestureCode);
  Serial.print(" taps=");
  Serial.print(tapCount);
  Serial.print(" idle=");
  Serial.print(now - lastActivityAt);
  Serial.println("ms");
}

void clearBondsAndRestartAdvertising() {
  if (shouldLogSerial()) {
    Serial.println("[ble] clearing bonds, restarting advertising");
  }
  bleReadySeen = false;
  disconnectAllBleConnections();
  delay(200);
  Bluefruit.Periph.clearBonds();
  startAdvertising();
}

void updateStatusLed(uint32_t now) {
  if (hasBleReadyForAction(config.singleTap)) {
    bleReadySeen = true;
  }
  bool clearBondsHoldActive =
      buttonState == LOW &&
      (bondsClearedThisPress || now - pressStartedAt >= CLEAR_BONDS_HOLD_MS);
  setStatusLed(clearBondsHoldActive || pendingGestureCode != 0 || !bleReadySeen);
}

void updateButton(uint32_t now) {
  int rawState = readButtonState();

  if (sleeping && rawState == LOW) {
    exitSleep();
  }

  if (rawState != lastRawButtonState) {
    lastRawButtonState = rawState;
    lastDebounceAt = now;
  }

  if (now - lastDebounceAt < DEBOUNCE_TIME) {
    return;
  }

  if (rawState == buttonState) {
    if (buttonState == LOW && !longPressSent && now - pressStartedAt >= LONG_PRESS_TIME) {
      sendAction(GESTURE_LONG_PRESS);
      longPressSent = true;
      tapCount = 0;
    }

    if (buttonState == LOW && !bondsClearedThisPress && now - pressStartedAt >= CLEAR_BONDS_HOLD_MS) {
      bondsClearedThisPress = true;
      clearBondsAndRestartAdvertising();
    }

    const uint16_t tapTimeout = tapCount >= 2 ? THIRD_TAP_TIMEOUT : SECOND_TAP_TIMEOUT;
    if (tapCount > 0 && buttonState == HIGH && now - lastReleaseAt >= tapTimeout) {
      sendAction(tapCount == 1 ? GESTURE_SINGLE_TAP : GESTURE_DOUBLE_TAP);
      tapCount = 0;
    }

    return;
  }

  buttonState = rawState;

  if (buttonState == LOW) {
    pressStartedAt = now;
    longPressSent = false;
    bondsClearedThisPress = false;
    markActivity(now);
    sendButtonEvent(Serial, BUTTON_PRESSED);
    return;
  }

  markActivity(now);
  sendButtonEvent(Serial, BUTTON_RELEASED);

  uint32_t pressDuration = now - pressStartedAt;
  if (longPressSent || pressDuration <= DEBOUNCE_TIME) {
    return;
  }

  // Same feel as One Shot: a slower first short press fires immediately,
  // while quick taps enter the multi-tap window.
  if (tapCount == 0 && pressDuration > QUICK_TAP_MAX_PRESS) {
    sendAction(GESTURE_SINGLE_TAP);
  } else {
    tapCount++;
    lastReleaseAt = now;
  }
}

void startAdvertising() {
  Bluefruit.Advertising.stop();
  Bluefruit.Advertising.clearData();
  Bluefruit.ScanResponse.clearData();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(hid);
  Bluefruit.Advertising.addService(configBle);
  Bluefruit.ScanResponse.addService(batteryService);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(BLE_ADVERTISING_INTERVAL_FAST, BLE_ADVERTISING_INTERVAL_SLOW);
  Bluefruit.Advertising.setFastTimeout(BLE_ADVERTISING_FAST_TIMEOUT_SEC);
  Bluefruit.Advertising.start(0);
}

void printConnParams(BLEConnection *connection, const char *tag) {
  if (!shouldLogSerial()) {
    return;
  }

  if (!connection) {
    return;
  }
  uint16_t interval = connection->getConnectionInterval();
  uint16_t latency = connection->getSlaveLatency();
  uint16_t timeout = connection->getSupervisionTimeout();
  Serial.print("[connparams ");
  Serial.print(tag);
  Serial.print("] interval=");
  Serial.print(interval * 1.25f, 2);
  Serial.print("ms latency=");
  Serial.print(latency);
  Serial.print(" timeout=");
  Serial.print(timeout * 10);
  Serial.println("ms");
}

void connectCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  char peerName[32] = {0};

  if (connection) {
    Bluefruit.setTxPower(BLE_TX_POWER_FAST_DBM);
    bleIdleParamsRequested = false;
    bleSecuredAt = 0;
    connection->getPeerName(peerName, sizeof(peerName));
    connection->requestConnectionParameter(BLE_CONN_INTERVAL_MIN, BLE_CONN_SLAVE_LATENCY, BLE_CONN_SUPERVISION_TIMEOUT);
    resetBleHidReadyState();
    if (!connection->secured()) {
      connection->requestPairing();
    }
  }

  if (shouldLogSerial()) {
    Serial.print("[ble] connect handle=");
    Serial.print(connHandle);
    Serial.print(" peer=");
    Serial.println(peerName[0] ? peerName : "unknown");
  }
  printConnParams(connection, "connect");
  markActivity();
  batteryService.notify(connHandle, batteryLevel);
  if (pendingGestureCode != 0) {
    lastPendingGestureRetryAt = 0;
  }
  wakeLoopTask();
}

void disconnectCallback(uint16_t connHandle, uint8_t reason) {
  resetBleHidReadyState();
  bleIdleParamsRequested = false;
  bleSecuredAt = 0;
  Bluefruit.setTxPower(BLE_TX_POWER_FAST_DBM);
  if (shouldLogSerial()) {
    Serial.print("[ble] disconnect handle=");
    Serial.print(connHandle);
    Serial.print(" reason=0x");
    Serial.println(reason, HEX);
  }
  // A disconnect while already idle switches to the shorter disconnected
  // System OFF policy without treating the disconnect as user activity.
  if (!sleeping) {
    markActivity();
  }
  wakeLoopTask();
}

void pairCompleteCallback(uint16_t connHandle, uint8_t authStatus) {
  if (shouldLogSerial()) {
    Serial.print("[ble] pair handle=");
    Serial.print(connHandle);
    Serial.print(" status=0x");
    Serial.println(authStatus, HEX);
  }
}

void securedCallback(uint16_t connHandle) {
  BLEConnection *connection = Bluefruit.Connection(connHandle);
  char peerName[32] = {0};

  if (connection) {
    connection->getPeerName(peerName, sizeof(peerName));
  }

  if (shouldLogSerial()) {
    Serial.print("[ble] secured handle=");
    Serial.print(connHandle);
    Serial.print(" peer=");
    Serial.println(peerName[0] ? peerName : "unknown");
  }
  printConnParams(connection, "secured");
  markActivity();
  resetBleHidReadyState();
  bleSecuredAt = millis();
  bleIdleParamsRequested = false;
  if (pendingGestureCode != 0) {
    lastPendingGestureRetryAt = 0;
  }
  wakeLoopTask();
}

void updateBlePowerProfile(uint32_t now) {
  if (bleIdleParamsRequested || bleSecuredAt == 0 || pendingGestureCode != 0) {
    return;
  }
  if (now - bleSecuredAt < BLE_IDLE_PARAMS_DELAY_MS) {
    return;
  }

  for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
    BLEConnection *connection = Bluefruit.Connection(connHandle);
    if (!connection || !connection->connected() || !connection->secured()) {
      continue;
    }

    connection->requestConnectionParameter(
        BLE_IDLE_CONN_INTERVAL_MIN,
        BLE_IDLE_CONN_SLAVE_LATENCY,
        BLE_IDLE_CONN_SUPERVISION_TIMEOUT);
  }

  Bluefruit.setTxPower(BLE_TX_POWER_IDLE_DBM);
  bleIdleParamsRequested = true;
}

void setupBle() {
  Bluefruit.autoConnLed(false);
  Bluefruit.configPrphBandwidth(BANDWIDTH_LOW);
  Bluefruit.begin();
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);
  configureDcdcForPowerSource(isRawVbusPresent());
  Bluefruit.setTxPower(BLE_TX_POWER_FAST_DBM);
  Bluefruit.setName(DEVICE_NAME);
  if (RECOVERY_MODE) {
    Bluefruit.Periph.clearBonds();
  }

  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

  Bluefruit.Security.setPairCompleteCallback(pairCompleteCallback);
  Bluefruit.Security.setSecuredCallback(securedCallback);

  deviceInfo.setManufacturer(MANUFACTURER_NAME);
  deviceInfo.setModel(MODEL_NAME);
  deviceInfo.begin();

  batteryService.begin();
  batteryService.setLevelCccdCallback(batteryLevelCccdCallback);
  updateBatteryLevel();
  configBle.begin();
  configBle.setRxCallback([](uint16_t connHandle) {
    (void) connHandle;
    wakeLoopTask();
  });
  hid.begin();
  hid.setInputReportCccdCallback(hidInputCccdCallback);
  Bluefruit.Periph.setConnInterval(BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX);
  Bluefruit.Periph.setConnSlaveLatency(BLE_CONN_SLAVE_LATENCY);
  Bluefruit.Periph.setConnSupervisionTimeout(BLE_CONN_SUPERVISION_TIMEOUT);
  startAdvertising();
}

void setupUsbHid() {
  if (usbInitialized) {
    TinyUSBDevice.attach();
    return;
  }

#if defined(BOARD_UICPAL_MINI_NRF52840) || defined(ARDUINO_Seeed_XIAO_nRF52840_Sense)
  // Seeed's bundled TinyUSB is initialized by the core before setup() and
  // predates isInitialized().
#else
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.begin(0);
  }
#endif

  usbHid.setPollInterval(2);
  usbHid.setReportDescriptor(USB_HID_REPORT_DESCRIPTOR, sizeof(USB_HID_REPORT_DESCRIPTOR));
  usbHid.setStringDescriptor("Super Magic Button HID");
  usbHid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }

  usbInitialized = true;
}

void setupStorage() {
  storageReady = InternalFS.begin();
  if (!storageReady) {
    debugPrintln("internal fs unavailable, using volatile config");
  }
}

void updateUsbState() {
  bool vbusPresent = isRawVbusPresent();

  if (vbusPresent && !lastVbusPresent) {
    configureDcdcForPowerSource(true);
    if (!usbInitialized) {
      NVIC_SystemReset();
    }

    if (sleeping) {
      exitSleep();
    }

    Serial.begin(SERIAL_BAUD);
    delay(50);
    setupUsbHid();
    usbVbusActive = true;
    markActivity();
  } else if (!vbusPresent && lastVbusPresent && usbVbusActive) {
    TinyUSBDevice.detach();
    Serial.end();
    usbVbusActive = false;
    configureDcdcForPowerSource(false);
    markActivity();
  }

  lastVbusPresent = vbusPresent;
}

void armButtonWakeSense() {
#if defined(USE_RAW_BUTTON_GPIO)
  if (buttonInterruptAttached) {
    return;
  }
  nrf_gpio_cfg_input(BUTTON_GPIO, NRF_GPIO_PIN_PULLUP);

  NRF_GPIOTE->CONFIG[WAKE_GPIOTE_CHANNEL] =
      ((uint32_t) GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
      ((uint32_t) BUTTON_PIN_NUMBER << GPIOTE_CONFIG_PSEL_Pos) |
      ((uint32_t) BUTTON_PIN_PORT << GPIOTE_CONFIG_PORT_Pos) |
      ((uint32_t) GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos);
  NRF_GPIOTE->EVENTS_IN[WAKE_GPIOTE_CHANNEL] = 0;
  NRF_GPIOTE->INTENSET = (1UL << WAKE_GPIOTE_CHANNEL);
  NVIC_EnableIRQ(GPIOTE_IRQn);
#endif
}

void disarmButtonWakeSense() {
#if defined(USE_RAW_BUTTON_GPIO)
  if (buttonInterruptAttached) {
    return;
  }
  NRF_GPIOTE->INTENCLR = (1UL << WAKE_GPIOTE_CHANNEL);
  NRF_GPIOTE->CONFIG[WAKE_GPIOTE_CHANNEL] = 0;
  NRF_GPIOTE->EVENTS_IN[WAKE_GPIOTE_CHANNEL] = 0;
#endif
}

void disableAllGpioSense() {
  for (uint8_t pin = 0; pin < 32; pin++) {
    NRF_P0->PIN_CNF[pin] =
        (NRF_P0->PIN_CNF[pin] & ~GPIO_PIN_CNF_SENSE_Msk) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
  }
#if defined(NRF_P1)
  for (uint8_t pin = 0; pin < 32; pin++) {
    NRF_P1->PIN_CNF[pin] =
        (NRF_P1->PIN_CNF[pin] & ~GPIO_PIN_CNF_SENSE_Msk) |
        (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
  }
#endif
}

bool prepareSystemOffButtonWake() {
#if defined(USE_RAW_BUTTON_GPIO)
  if (readButtonState() == LOW) {
    return false;
  }

  disableAllGpioSense();
  nrf_gpio_cfg_sense_input(BUTTON_GPIO, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
  NRF_P0->LATCH = 0xFFFFFFFF;
#if defined(NRF_P1)
  NRF_P1->LATCH = 0xFFFFFFFF;
#endif
  NRF_GPIOTE->EVENTS_PORT = 0;
  delay(100);

#if BUTTON_PIN_PORT == 0
  if (readButtonState() == LOW || (NRF_P0->LATCH & (1UL << BUTTON_PIN_NUMBER)) != 0) {
    setupButtonInput();
    NRF_P0->LATCH = 0xFFFFFFFF;
    return false;
  }
#endif
#endif

  return true;
}

void enterSleep() {
  if (sleeping) {
    return;
  }
  const bool connected = hasActiveBleConnection();
  sleeping = true;

#if INITIALIZE_BUILTIN_LED && defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, LOW);
#endif
  setStatusLed(false);

  Bluefruit.Advertising.restartOnDisconnect(false);
  if (!connected) {
    Bluefruit.Advertising.stop();
  }
  armButtonWakeSense();
}

void exitSleep() {
  if (!sleeping) {
    return;
  }
  sleeping = false;
  disarmButtonWakeSense();

  Bluefruit.Advertising.restartOnDisconnect(true);
  if (!hasActiveBleConnection()) {
    startAdvertising();
  }
  markActivity();
}

void enterSystemOff() {
#if defined(USE_RAW_BUTTON_GPIO)
  if (!prepareSystemOffButtonWake()) {
    markActivity();
    return;
  }
#endif

#if INITIALIZE_BUILTIN_LED && defined(LED_BUILTIN)
  digitalWrite(LED_BUILTIN, LOW);
#endif
  setStatusLed(false);
  disarmButtonWakeSense();
  holdButtonGroundLow();
  disconnectAllBleConnections();
  delay(30);
  Bluefruit.Advertising.stop();
  delay(10);
  putUnusedExternalFlashInDeepPowerDown();
  prepareBatteryPinsForSystemOff();

  uint8_t softDeviceEnabled = 0;
  (void) sd_softdevice_is_enabled(&softDeviceEnabled);

#if defined(NRF_SAADC)
  NRF_SAADC->ENABLE = 0;
#endif
#if defined(NRF_UARTE0)
  NRF_UARTE0->ENABLE = 0;
#endif
#if defined(NRF_UARTE1)
  NRF_UARTE1->ENABLE = 0;
#endif
#if defined(NRF_PWM0)
  NRF_PWM0->ENABLE = 0;
#endif
#if defined(NRF_PWM1)
  NRF_PWM1->ENABLE = 0;
#endif
#if defined(NRF_PWM2)
  NRF_PWM2->ENABLE = 0;
#endif
#if defined(NRF_PWM3)
  NRF_PWM3->ENABLE = 0;
#endif

#if defined(USE_RAW_BUTTON_GPIO)
  nrf_gpio_cfg_default(STATUS_LED_GPIO);
  holdButtonGroundLow();

  if (softDeviceEnabled) {
    sd_power_system_off();
  } else {
    NRF_POWER->SYSTEMOFF = 1;
  }
  __DSB();
  while (true) {
    __WFE();
  }
#else
  systemOff(BUTTON_PIN, LOW);
#endif
}

bool shouldEnterSleep(uint32_t now) {
  if (hasActiveUsbSession()) {
    return false;
  }

  if (buttonState == LOW || lastRawButtonState == LOW) {
    return false;
  }

  if (tapCount > 0) {
    return false;
  }

  if (pendingGestureCode != 0) {
    return false;
  }

  return now - lastActivityAt >= IDLE_SLEEP_TIMEOUT_MS;
}

uint8_t sleepBlockReason() {
  if (hasActiveUsbSession()) {
    return 4;
  }

  if (buttonState == LOW || lastRawButtonState == LOW) {
    return 6;
  }

  if (tapCount > 0) {
    return 7;
  }

  if (pendingGestureCode != 0) {
    return 8;
  }

  return 0;
}

uint32_t millisecondsUntil(uint32_t now, uint32_t deadline) {
  int32_t remaining = static_cast<int32_t>(deadline - now);
  return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

void shortenWait(uint32_t &waitMs, uint32_t candidateMs) {
  if (candidateMs < waitMs) {
    waitMs = candidateMs;
  }
}

uint32_t nextLoopWaitMs(uint32_t now) {
  uint32_t waitMs = MAX_IDLE_BLOCK_MS;

  if (pendingGestureCode != 0) {
    shortenWait(
        waitMs,
        millisecondsUntil(now, lastPendingGestureRetryAt + PENDING_GESTURE_RETRY_INTERVAL_MS));
    shortenWait(
        waitMs,
        millisecondsUntil(now, pendingGestureAt + PENDING_GESTURE_MAX_AGE_MS));
  }

  if (!bleIdleParamsRequested && bleSecuredAt != 0) {
    shortenWait(waitMs, millisecondsUntil(now, bleSecuredAt + BLE_IDLE_PARAMS_DELAY_MS));
  }

  shortenWait(
      waitMs,
      millisecondsUntil(
          now,
          lastActivityAt + (sleeping ? currentDeepSleepTimeoutMs() : IDLE_SLEEP_TIMEOUT_MS)));
  shortenWait(waitMs, millisecondsUntil(now, lastBatteryUpdateAt + BATTERY_UPDATE_INTERVAL_MS));
  return waitMs;
}

void blockUntilNextWork(uint32_t now) {
  if (!buttonInterruptAttached || hasActiveUsbSession() || Serial.available() > 0) {
    delay(IDLE_POLL_DELAY_MS);
    return;
  }

  uint32_t waitMs = nextLoopWaitMs(now);
  if (waitMs == 0) {
    taskYIELD();
    return;
  }

  TickType_t ticks = pdMS_TO_TICKS(waitMs);
  ulTaskNotifyTake(pdTRUE, ticks > 0 ? ticks : 1);
}

void idleSleep(uint32_t now) {
  if (RECOVERY_MODE) {
    return;
  }

  if (IDLE_USES_SYSTEM_OFF && shouldEnterSleep(now)) {
    enterSystemOff();
    return;
  }

  if (sleeping &&
      !hasActiveUsbSession() &&
      pendingGestureCode == 0 &&
      now - lastActivityAt >= currentDeepSleepTimeoutMs()) {
    enterSystemOff();
    return;
  }

  if (shouldEnterSleep(now)) {
    enterSleep();
  }

  if (now - lastActivityAt >= IDLE_SLEEP_TIMEOUT_MS) {
    uint8_t reason = sleepBlockReason();
    if (ENABLE_SERIAL_DEBUG && reason != 0 && now - lastSleepDebugAt >= SLEEP_DEBUG_REPEAT_MS) {
      lastSleepDebugAt = now;
      pulseDebugLed(reason, DEBUG_REASON_PULSE_MS, DEBUG_REASON_GAP_MS);
    }
  }

  if (Serial.available() > 0) {
    return;
  }

  if (buttonState == LOW || lastRawButtonState == LOW || tapCount > 0) {
    delay(ACTIVE_POLL_DELAY_MS);
    return;
  }

  // Let the Arduino loop task block so FreeRTOS can run its tickless idle
  // path. A falling-edge GPIOTE interrupt wakes it immediately on a press;
  // while a gesture is in progress the fast polling path above stays active.
  blockUntilNextWork(now);
}

}  // namespace

void disableNfcPinsIfNeeded() {
#if DISABLE_NFC_PINS
  if ((NRF_UICR->NFCPINS & UICR_NFCPINS_PROTECT_Msk) ==
      (UICR_NFCPINS_PROTECT_NFC << UICR_NFCPINS_PROTECT_Pos)) {
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_UICR->NFCPINS &= ~UICR_NFCPINS_PROTECT_Msk;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
    NVIC_SystemReset();
  }
#endif
}

void setup() {
  disableNfcPinsIfNeeded();
  holdSuperMiniExternalVccOff();
#if INITIALIZE_BUILTIN_LED && defined(LED_BUILTIN)
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
#endif
  setupStatusLed();

  const uint32_t resetReason = NRF_POWER->RESETREAS;
  const uint32_t p0Latch = NRF_P0->LATCH;
  NRF_POWER->RESETREAS = resetReason;
#if defined(USE_RAW_BUTTON_GPIO) && BUTTON_PIN_PORT == 0
  wokeFromButtonLatch = (p0Latch & (1UL << BUTTON_PIN_NUMBER)) != 0;
  NRF_P0->LATCH = 0xFFFFFFFF;
#else
  wokeFromButtonLatch = false;
#endif
  if (ENABLE_SERIAL_DEBUG) {
    pulseDebugLed(1);
  }

  setupButtonInput();
  setupButtonInterrupt();
  setupButtonGround();
  setupBatteryMeasurement();

  setupStorage();
  loadConfig();
  lastVbusPresent = isRawVbusPresent();
  if (lastVbusPresent) {
    Serial.begin(SERIAL_BAUD);
    delay(50);
    setupUsbHid();
    usbVbusActive = true;
  }
  setupBle();
  markActivity();
  if (!RECOVERY_MODE && shouldQueueWakeGesture()) {
    uint32_t now = millis();
    queuePendingGesture(GESTURE_SINGLE_TAP, now, true);
  }

  if (ENABLE_SERIAL_DEBUG || usbVbusActive) {
    Serial.println();
    Serial.println("=== Magic Button NRF boot ===");
  }

  if (ENABLE_SERIAL_DEBUG) {
    Serial.println();
    Serial.println("Magic Button NRF");
    Serial.print("advertising as: ");
    Serial.println(DEVICE_NAME);
#if defined(USE_RAW_BUTTON_GPIO)
    Serial.print("button gpio: P");
    Serial.print(BUTTON_PIN_PORT);
    Serial.print(".");
    Serial.println(BUTTON_PIN_NUMBER);
#endif
#if BATTERY_MEASURE_INTERNAL_VDD
    Serial.println("battery source: VDD");
#else
    Serial.println("battery source: external divider");
#endif
  }
}

void loop() {
#ifdef TINYUSB_NEED_POLLING_TASK
  if (usbInitialized && usbVbusActive) {
    TinyUSBDevice.task();
  }
#endif

  updateUsbState();

  uint32_t now = millis();
  handleProtocolTransport(Serial);
  handleProtocolTransport(configBle);
  updateButton(now);
  flushPendingGesture(now);
  updateStatusLed(now);
  updateBlePowerProfile(now);
  emitHeartbeat(now);

  if (now - lastBatteryUpdateAt >= BATTERY_UPDATE_INTERVAL_MS) {
    lastBatteryUpdateAt = now;
    updateBatteryLevel();
  }

  if (shouldLogSerial() && now - lastConnParamsCheckAt >= 5000) {
    lastConnParamsCheckAt = now;
    bool anyConnected = false;
    for (uint16_t connHandle = 0; connHandle < BLE_MAX_CONNECTION; connHandle++) {
      BLEConnection *connection = Bluefruit.Connection(connHandle);
      if (!connection || !connection->connected()) {
        continue;
      }
      anyConnected = true;
      printConnParams(connection, "poll");
    }
    if (!anyConnected) {
      Serial.println("[connparams poll] no active BLE connection");
    }
  }

  idleSleep(now);
}
