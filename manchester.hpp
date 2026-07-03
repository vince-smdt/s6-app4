#pragma once

#include "config.hpp"
#include "ring-buffer.hpp"
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

namespace manchester {

class Sender {
public:
  void begin(uint8_t pin, uint32_t halfBitUs) {
    _pin = pin;
    _halfBitUs = halfBitUs;
  }

  void sendBit(bool value) {
    if (value) {
      digitalWrite(_pin, HIGH);
      delayMicroseconds(_halfBitUs);
      digitalWrite(_pin, LOW);
      delayMicroseconds(_halfBitUs);
    } else {
      digitalWrite(_pin, LOW);
      delayMicroseconds(_halfBitUs);
      digitalWrite(_pin, HIGH);
      delayMicroseconds(_halfBitUs);
    }
  }

  void sendByte(uint8_t byte) {
    for (int8_t i = 7; i >= 0; i--) {
      sendBit((byte >> i) & 1);
    }
  }

  void sendBuffer(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
      Serial.print("Sending byte: ");
      Serial.println(buf[i], HEX);
      sendByte(buf[i]);
    }
  }

private:
  uint8_t _pin;
  uint32_t _halfBitUs;
};

class Receiver {
public:
  void begin(uint8_t pin, int32_t halfBitUs) {
    _pin = pin;
    _fullBitCycles = halfBitUs * 2 * 240; // 240MHz

    _fullMin = (_fullBitCycles * 7) / 8;
    _fullMax = (_fullBitCycles * 9) / 8;

    Serial.print("FullMin: ");
    Serial.println(_fullMin);
    Serial.print("FullMax: ");
    Serial.println(_fullMax);

    _synced = false;
    _timeLastBit = 0;

    _byte = 0;
    _bitCount = 0;
  }

  void onEdge(bool level) {
    uint32_t now = xthal_get_ccount();
    uint32_t dt = now - _timeLastBit;

    bool exceededBitPeriod = dt > _fullMax;

    if (!_synced || (exceededBitPeriod && level == 1)) {
        _synced = true;
        _timeLastBit = now;

        // We caught the first bit, which must be a 0.
        _bitCount = 1;
        _byte = 0;
        return;
    } else if (exceededBitPeriod) {
        unsync();
        return;
    } else if (dt < _fullMin) {
        // Not a bit
        return;
    }

    _timeLastBit = now;
    _byte = (_byte << 1) | !level;
    _bitCount += 1;

    if (_bitCount == 8) {
      _buf.push(_byte);
      _byte = 0;
      _bitCount = 0;
    } else if (_bitCount > 8) {
      unsync();
    }
  }

  bool getByte(uint8_t &byte) {
    return _buf.pop(byte);
  }

  void unsync() { 
    _synced = false;
  }

private:
  uint8_t _pin;
  uint32_t _fullBitCycles;

  // Timing boundaries
  uint32_t _fullMin;
  uint32_t _fullMax;

  // Timing variables
  bool _synced;             // False until we detect first transition
  uint32_t _timeLastBit;    // Cycles count of last edge transition representing a bit

  // Byte tracking
  uint8_t _byte;
  uint8_t _bitCount;
  ByteRingBuffer _buf;
};

} // namespace manchester
