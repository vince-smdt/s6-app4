#pragma once

#include "config.hpp"
#include "ring-buffer.hpp"
#include <Arduino.h>
#include <soc/gpio_struct.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stddef.h>
#include <stdint.h>

namespace manchester {

class Sender {
public:
  void begin(uint8_t pin, uint32_t halfBitUs) {
    _halfBitUs = halfBitUs;
    _bitMask = 1u << pin;
    _done = true;

    esp_timer_create_args_t args = {};
    args.callback = timerCallback;
    args.arg = this;
    args.name = "man_tx";
    esp_timer_create(&args, &_timer);

    _sem = xSemaphoreCreateBinary();
  }

  void sendByte(uint8_t byte) {
    sendBuffer(&byte, 1);
  }

  void sendBuffer(const uint8_t* buf, size_t len) {
    _buf = buf;
    _len = len;
    _byteIdx = 0;
    _bitIdx = 7;
    _halfPhase = 0;
    _done = false;

    bool bit = (_buf[0] >> 7) & 1;
    if (bit) {
      GPIO.out_w1ts = _bitMask;
    } else {
      GPIO.out_w1tc = _bitMask;
    }

    esp_timer_start_periodic(_timer, _halfBitUs);
    xSemaphoreTake(_sem, portMAX_DELAY);
  }

private:
  static void timerCallback(void* arg) {
    static_cast<Sender*>(arg)->handleTimer();
  }

  void handleTimer() {
    if (_done) return;

    if (_halfPhase == 0) {
      bool bit = (_buf[_byteIdx] >> _bitIdx) & 1;
      if (bit) {
        GPIO.out_w1tc = _bitMask;
      } else {
        GPIO.out_w1ts = _bitMask;
      }
      _halfPhase = 1;
    } else {
      _bitIdx--;
      if (_bitIdx < 0) {
        _bitIdx = 7;
        _byteIdx++;
        if (_byteIdx >= _len) {
          _done = true;
          esp_timer_stop(_timer);
          xSemaphoreGive(_sem);
          return;
        }
      }

      bool bit = (_buf[_byteIdx] >> _bitIdx) & 1;
      if (bit) {
        GPIO.out_w1ts = _bitMask;
      } else {
        GPIO.out_w1tc = _bitMask;
      }
      _halfPhase = 0;
    }
  }

  uint32_t _bitMask;
  uint32_t _halfBitUs;

  const uint8_t* _buf;
  size_t _len;
  size_t _byteIdx;
  int8_t _bitIdx;
  uint8_t _halfPhase;
  volatile bool _done;

  esp_timer_handle_t _timer;
  SemaphoreHandle_t _sem;
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
