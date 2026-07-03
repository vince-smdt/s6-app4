#pragma once

#include "config.hpp"

#define MAX_PAYLOAD_SIZE 80
#define PREAMBLE 0x55
#define START_CODE 0x7E
#define END_CODE 0x7E

enum class FrameType : uint8_t {
  START = 0x01,
  DATA = 0x02,
  END = 0x03,
  NACK = 0x04
};

struct FrameHeader {
  FrameType type;
  uint8_t seq;
  uint8_t len;
  uint8_t dyn;
};

struct Frame {
  FrameHeader header;
  uint8_t payload[MAX_PAYLOAD_SIZE];
  uint16_t crc;
};
