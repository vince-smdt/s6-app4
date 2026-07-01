#pragma once

#include "Arduino.h"
#include <soc/gpio_struct.h>
#include <soc/gpio_reg.h>

namespace manchester {

class Transceiver {
public:
  void beginTX(uint8_t pin, uint32_t halfPeriodUs = 100) {
    _txPin = pin;
    _txPinMask = 1UL << pin;
    _txHalfUs = halfPeriodUs;
    pinMode(pin, OUTPUT);
    GPIO.out_w1tc = _txPinMask;
  }

  void setOutput(bool value) {
    if (value)
      GPIO.out_w1ts = _txPinMask;
    else
      GPIO.out_w1tc = _txPinMask;
  }

  void sendBit(bool value) {
    if (value) {
      GPIO.out_w1ts = _txPinMask;
      delayMicroseconds(_txHalfUs);
      GPIO.out_w1tc = _txPinMask;
      delayMicroseconds(_txHalfUs);
    } else {
      GPIO.out_w1tc = _txPinMask;
      delayMicroseconds(_txHalfUs);
      GPIO.out_w1ts = _txPinMask;
      delayMicroseconds(_txHalfUs);
    }
  }

  void sendByte(uint8_t byte) {
    for (int i = 7; i >= 0; i--)
      sendBit((byte >> i) & 1);
  }

  void syncTX() {
    GPIO.out_w1tc = _txPinMask;
  }

  void beginRX(uint8_t pin, uint32_t halfPeriodUs = 100) {
    _rxPin = pin;
    _rxPinMask = 1UL << pin;

    _rxHalfUs = halfPeriodUs;
    _rxHalfCycles = _rxHalfUs * ESP.getCpuFreqMHz();
    _rxMinHalfCycles = _rxHalfCycles * 3 / 5;
    _rxMaxHalfCycles = _rxHalfCycles * 7 / 5;
    _rxMinFullCycles = _rxHalfCycles * 8 / 5;
    _rxMaxFullCycles = _rxHalfCycles * 12 / 5;
    _lastEdgeCycles = xthal_get_ccount();

    _rqHead = 0;
    _rqTail = 0;

    pinMode(pin, INPUT);
    syncRX();
  }

  void IRAM_ATTR onEdge() {
    uint32_t now = xthal_get_ccount();
    uint32_t dtCycles = now - _lastEdgeCycles;
    _lastEdgeCycles = now;

    if (!_synced) {
      _synced = true;
      _boundarySeen = false;
    } else if (_boundarySeen) {
      _boundarySeen = false;
    } else if (dtCycles >= _rxMinHalfCycles && dtCycles <= _rxMaxHalfCycles) {
      _boundarySeen = true;
      return;
    } else if (dtCycles >= _rxMinFullCycles && dtCycles <= _rxMaxFullCycles) {
    } else {
      syncRX();
      return;
    }

    bool bit = !(GPIO.in & _rxPinMask);

    if (_rxState == RxState::SEARCH_SYNC) {
      _rxShift = (_rxShift << 1) | bit;

      if (_rxShift == PREAMBLE) {
        _rxCurrent = 0;
        _rxBitCount = 0;
        _rxState = RxState::RECEIVE;
      }
    } else {
      _rxCurrent = (_rxCurrent << 1) | bit;

      if (++_rxBitCount == 8) {
        uint8_t n = (_rqHead + 1) & 15;

        if (n != _rqTail) {
          _rqBuf[_rqHead] = _rxCurrent;
          _rqHead = n;
        }

        _rxCurrent = 0;
        _rxBitCount = 0;
      }
    }
  }

  bool available() {
    return _rqHead != _rqTail;
  }

  uint8_t read() {
    uint8_t b = _rqBuf[_rqTail];
    _rqTail = (_rqTail + 1) & 15;
    return b;
  }

private:
  enum class RxState {
    SEARCH_SYNC,
    RECEIVE
  };

  void syncRX() {
    _synced = false;
    _boundarySeen = false;
    _rxShift = 0;
    _rxBitCount = 0;
    _rxCurrent = 0;
    _rxState = RxState::SEARCH_SYNC;
  }

  // TX state
  uint32_t _txHalfUs = 100;
  uint8_t  _txPin = 0;
  uint32_t _txPinMask = 0;

  // RX state
  uint32_t _rxHalfUs = 100;
  uint32_t _rxHalfCycles = 0;
  uint32_t _rxMinHalfCycles = 0;
  uint32_t _rxMaxHalfCycles = 0;
  uint32_t _rxMinFullCycles = 0;
  uint32_t _rxMaxFullCycles = 0;

  uint8_t  _rxPin = 0;
  uint32_t _rxPinMask = 0;
  bool _synced = false;
  bool _boundarySeen = false;
  uint32_t _lastEdgeCycles = 0;
  uint8_t _rxShift = 0;
  uint8_t _rxBitCount = 0;
  uint8_t _rxCurrent = 0;
  RxState _rxState = RxState::SEARCH_SYNC;

  uint8_t _rqBuf[16]{};
  volatile uint8_t _rqHead = 0;
  volatile uint8_t _rqTail = 0;
};

} // namespace manchester
