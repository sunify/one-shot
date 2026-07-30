#pragma once

#include <Arduino.h>
#include "internal/wch_usbhid_internal.h"
#include "Print.h"

// Key modifiers
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RIGHT_CTRL  0x84
#define KEY_RIGHT_SHIFT 0x85
#define KEY_RIGHT_ALT   0x86
#define KEY_RIGHT_GUI   0x87

#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_INSERT      0xD1
#define KEY_DELETE      0xD4
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6
#define KEY_HOME        0xD2
#define KEY_END         0xD5
#define KEY_CAPS_LOCK   0xC1
#define KEY_F1          0xC2
#define KEY_F2          0xC3
#define KEY_F3          0xC4
#define KEY_F4          0xC5
#define KEY_F5          0xC6
#define KEY_F6          0xC7
#define KEY_F7          0xC8
#define KEY_F8          0xC9
#define KEY_F9          0xCA
#define KEY_F10         0xCB
#define KEY_F11         0xCC
#define KEY_F12         0xCD

// HID Report Structure
typedef struct {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keys[6];
} KeyReport;

class USBHIDKeyboard_ : public Print {
public:
  USBHIDKeyboard_(void);
  void begin(void);
  void end(void);
  
  size_t write(uint8_t k);
  size_t press(uint8_t k);
  size_t release(uint8_t k);
  void releaseAll(void);
  size_t consumerPress(uint16_t usage);
  size_t consumerRelease(void);
  size_t consumerWrite(uint16_t usage);

  void setDelay(uint32_t ms) { _delay = ms; }
  
  // Raw Report Access
  void sendReport(KeyReport* keys);

private:
  KeyReport _keyReport;
  uint32_t _delay;
};

extern USBHIDKeyboard_ Keyboard;

extern "C" {
uint8_t USB_featureReportAvailable(void);
uint8_t USB_readFeatureReport(uint8_t* buffer, uint8_t max_len);
void USB_setFeatureReportResponse(const uint8_t* buffer, uint8_t len);
uint32_t USB_writeVendorInputReport(const uint8_t* buffer, uint32_t len);
}
