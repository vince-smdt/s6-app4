#pragma once

#include "crd16.hpp"
#include "frame.hpp"
#include "manchester.hpp"

namespace datalink {

class Sender {
public:
  void sendPreamble() {
    _tx.sendByte(PREAMBLE);
  }
  
  void sendStart(uint8_t n_packets) {
    _frame.header.type = FrameType::START;
    _frame.header.seq = 0;
    _frame.header.len = 0;
    _frame.header.dyn = n_packets;
    _frame.crc = crc16::computeFrame(_frame);
  }

  void sendDataFrame(uint8_t *payload, uint8_t len, uint8_t seq) {
    _frame.header.type = FrameType::DATA;
    _frame.header.seq = seq;
    _frame.header.len = len;
    _frame.header.dyn = 0;
    _frame.crc = crc16::computeFrame(_frame);
  }

  void sendEnd() {
    _frame.header.type = FrameType::END;
    _frame.header.seq = 0;
    _frame.header.len = 0;
    _frame.header.dyn = n_packets;
    _frame.crc = crc16::computeFrame(_frame);
  }

  void sendNack(uint8_t seq) {
    _frame.header.type = FrameType::NACK;
    _frame.header.seq = 0;
    _frame.header.len = 0;
    _frame.header.dyn = seq;
    _frame.crc = crc16::computeFrame(_frame);
  }

private:
  void sendFrame() {
    _tx.sendByte(START_CODE);
    _tx.sendBuffer((uint8_t*)_frame.header, sizeof(_frame.header));
    _tx.sendBuffer(_frame.payload, _frame.header.len);
    _tx.sendBuffer((uint8_t*)_frame.crc, sizeof(_frame.crc));
    _tx.sendByte(END_CODE);
  }

private:
  Frame _frame;
  manchester::Sender _tx;
};

class Receiver {
public:
private:
};

} // namespace datalink
