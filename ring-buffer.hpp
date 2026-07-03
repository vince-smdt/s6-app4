#pragma once

#include <Arduino.h>
#include <stdint.h>

class ByteRingBuffer {
public:
  static constexpr size_t Capacity = 256;

  ByteRingBuffer() : _head(0), _tail(0) {}

  // Returns false if full
  bool push(uint8_t value) {
    uint32_t next = (_head + 1) % Capacity;

    if (next == _tail)
      return false; // Full

    _buffer[_head] = value;
    _head = next;

    return true;
  }

  // Returns false if empty
  bool pop(uint8_t &value) {
    if (_head == _tail)
      return false;

    value = _buffer[_tail];
    _tail = (_tail + 1) % Capacity;

    return true;
  }

  bool empty() const { return _head == _tail; }

  bool full() const { return ((_head + 1) % Capacity) == _tail; }

  size_t size() const {
    if (_head >= _tail)
      return _head - _tail;

    return Capacity - _tail + _head;
  }

  void clear() { _head = _tail = 0; }

private:
  uint8_t _buffer[Capacity];

  volatile uint32_t _head;
  volatile uint32_t _tail;
};
