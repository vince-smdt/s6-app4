#pragma once

#include "frame.hpp"

namespace crc16 {

uint16_t compute(const uint8_t* data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x8005;
      else
        crc <<= 1;
    }
  }
  return crc;
}

uint16_t computeFrame(const Frame& frame) {
  const uint8_t* header = reinterpret_cast<const uint8_t*>(&frame.header);
  size_t totalLen = sizeof(FrameHeader) + frame.header.len;
  return compute(header, totalLen);
}

} // namespace crc16
