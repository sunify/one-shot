#include "USBHIDKeyboard.h"
#include <string.h>

// ASCII to HID mapping (Partial/US Layout)
// Removed 'const' to ensure RAM access and avoid Flash latency issues
uint8_t _asciimap[128] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x2A, 0x2B, 0x28, 0x00, 0x00, 0x28, 0x00, 0x00, // BS, TAB, LF, CR
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00,
	0x2C,          // ' '
	0x1E|0x80,     // !
	0x34|0x80,     // "
	0x20|0x80,     // #
	0x21|0x80,     // $
	0x22|0x80,     // %
	0x24|0x80,     // &
	0x34,          // '
	0x26|0x80,     // (
	0x27|0x80,     // )
	0x25|0x80,     // *
	0x2E|0x80,     // +
	0x36,          // ,
	0x2D,          // -
	0x37,          // .
	0x38,          // /
	0x27,          // 0
	0x1E,          // 1
	0x1F,          // 2
	0x20,          // 3
	0x21,          // 4
	0x22,          // 5
	0x23,          // 6
	0x24,          // 7
	0x25,          // 8
	0x26,          // 9
	0x33|0x80,     // :
	0x33,          // ;
	0x36|0x80,     // <
	0x2E,          // =
	0x37|0x80,     // >
	0x38|0x80,     // ?
	0x1F|0x80,     // @
	0x04|0x80,     // A
	0x05|0x80,     // B
	0x06|0x80,     // C
	0x07|0x80,     // D
	0x08|0x80,     // E
	0x09|0x80,     // F
	0x0A|0x80,     // G
	0x0B|0x80,     // H
	0x0C|0x80,     // I
	0x0D|0x80,     // J
	0x0E|0x80,     // K
	0x0F|0x80,     // L
	0x10|0x80,     // M
	0x11|0x80,     // N
	0x12|0x80,     // O
	0x13|0x80,     // P
	0x14|0x80,     // Q
	0x15|0x80,     // R
	0x16|0x80,     // S
	0x17|0x80,     // T
	0x18|0x80,     // U
	0x19|0x80,     // V
	0x1A|0x80,     // W
	0x1B|0x80,     // X
	0x1C|0x80,     // Y
	0x1D|0x80,     // Z
	0x2F,          // [
	0x31,          // \ (backslash)
	0x30,          // ]
	0x23|0x80,     // ^
	0x2D|0x80,     // _
	0x35,          // `
	0x04,          // a
	0x05,          // b
	0x06,          // c
	0x07,          // d
	0x08,          // e
	0x09,          // f
	0x0A,          // g
	0x0B,          // h
	0x0C,          // i
	0x0D,          // j
	0x0E,          // k
	0x0F,          // l
	0x10,          // m
	0x11,          // n
	0x12,          // o
	0x13,          // p
	0x14,          // q
	0x15,          // r
	0x16,          // s
	0x17,          // t
	0x18,          // u
	0x19,          // v
	0x1A,          // w
	0x1B,          // x
	0x1C,          // y
	0x1D,          // z
	0x2F|0x80,     // {
	0x31|0x80,     // |
	0x30|0x80,     // }
	0x35|0x80,     // ~
	0x00           // DEL
};

USBHIDKeyboard_ Keyboard;

USBHIDKeyboard_::USBHIDKeyboard_(void) {
  _delay = 0;
}

void USBHIDKeyboard_::begin(void) {
    memset(&_keyReport, 0, sizeof(_keyReport));
    USB_init();
}

void USBHIDKeyboard_::end(void) {
}

void USBHIDKeyboard_::sendReport(KeyReport* keys) {
  uint8_t report[sizeof(KeyReport) + 1];
  report[0] = 1;
  memcpy(report + 1, keys, sizeof(KeyReport));
  USB_write(report, sizeof(report));
}

size_t USBHIDKeyboard_::press(uint8_t k) {
  uint8_t i;
  if (k >= 136) {
    k = k - 136;
  } else if (k >= 128) {
    _keyReport.modifiers |= (1 << (k - 128));
    k = 0;
  } else {
    k = _asciimap[k];
    if (!k) return 0;
    if (k & 0x80) {
      _keyReport.modifiers |= 0x02;
      k &= 0x7F;
    }
  }
  
  if (_keyReport.keys[0] != k && _keyReport.keys[1] != k && 
    _keyReport.keys[2] != k && _keyReport.keys[3] != k &&
    _keyReport.keys[4] != k && _keyReport.keys[5] != k) {

    for (i=0; i<6; i++) {
      if (_keyReport.keys[i] == 0x00) {
        _keyReport.keys[i] = k;
        break;
      }
    }
    if (i == 6) return 0;
  }
  
  sendReport(&_keyReport);
  return 1;
}

size_t USBHIDKeyboard_::release(uint8_t k) {
  uint8_t i;
  if (k >= 136) {
    k = k - 136;
  } else if (k >= 128) {
    _keyReport.modifiers &= ~(1 << (k - 128));
    k = 0;
  } else {
    k = _asciimap[k];
    if (!k) return 0;
    if (k & 0x80) {
      _keyReport.modifiers &= ~(0x02);
      k &= 0x7F;
    }
  }
  
  for (i=0; i<6; i++) {
    if (0 != k && _keyReport.keys[i] == k) {
      _keyReport.keys[i] = 0x00;
    }
  }

  sendReport(&_keyReport);
  return 1;
}

void USBHIDKeyboard_::releaseAll(void) {
  _keyReport.keys[0] = 0;
  _keyReport.keys[1] = 0;
  _keyReport.keys[2] = 0;
  _keyReport.keys[3] = 0;
  _keyReport.keys[4] = 0;
  _keyReport.keys[5] = 0;
  _keyReport.modifiers = 0;
  sendReport(&_keyReport);
}

size_t USBHIDKeyboard_::write(uint8_t c) {
  uint8_t p = press(c);
  release(c);
  if (_delay > 0) delay(_delay);
  return p;
}

size_t USBHIDKeyboard_::consumerPress(uint16_t usage) {
  uint8_t report[3] = {
    2,
    static_cast<uint8_t>(usage & 0xFF),
    static_cast<uint8_t>((usage >> 8) & 0xFF),
  };
  return USB_write(report, sizeof(report)) == sizeof(report) ? 1 : 0;
}

size_t USBHIDKeyboard_::consumerRelease(void) {
  uint8_t report[3] = {2, 0, 0};
  return USB_write(report, sizeof(report)) == sizeof(report) ? 1 : 0;
}

size_t USBHIDKeyboard_::consumerWrite(uint16_t usage) {
  size_t result = consumerPress(usage);
  delay(_delay > 0 ? _delay : 12);
  consumerRelease();
  return result;
}
