#pragma once

#define MAX_PAYLOAD_SIZE 80
#define PREAMBLE 0x55
#define START 0x7E
#define END 0x7E

enum FrameType : uint8_t {
  Start = 0x01,
  Data = 0x02,
  End = 0x03,
  Nack = 0x04
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
