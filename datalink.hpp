#pragma once

#include "crc16.hpp"
#include "frame.hpp"
#include "manchester.hpp"

namespace datalink {

class Sender {
public:
  void begin(uint8_t pin, uint32_t halfBitUs) {
    _tx.begin(pin, halfBitUs);
  }

  void sendPreamble() {
    _tx.sendByte(PREAMBLE);
  }

  void sendStart(uint8_t nPackets) {
    sendFrame(FrameType::START, 0, 0, nPackets);
  }

  void sendDataFrame(uint8_t *payload, uint8_t len, uint8_t seq) {
    // _frame.payload = payload; // TODO: FIX THIS BUG, need to do something with payload
    sendFrame(FrameType::DATA, seq, len, 0);
  }

  void sendEnd() {
    sendFrame(FrameType::END, 0, 0, 0);
  }

  void sendNack(uint8_t seq) {
    sendFrame(FrameType::NACK, 0, 0, seq);
  }

private:
  void sendFrame(FrameType type, uint8_t seq, uint8_t len, uint8_t dyn) {
    _frame.header.type = type;
    _frame.header.seq = seq;
    _frame.header.len = len;
    _frame.header.dyn = dyn;
    _frame.crc = crc16::computeFrame(_frame);

    _tx.sendByte(START_CODE);
    _tx.sendBuffer(reinterpret_cast<uint8_t*>(&_frame.header), sizeof(_frame.header));
    _tx.sendBuffer(_frame.payload, _frame.header.len);
    _tx.sendBuffer(reinterpret_cast<uint8_t*>(&_frame.crc), sizeof(_frame.crc));
    _tx.sendByte(END_CODE);
  }

private:
  Frame _frame;
  manchester::Sender _tx;
};

class Receiver {
public:
    void begin(uint8_t pin, uint32_t halfBitUs) {
        _rx.begin(pin, halfBitUs);
        reset();
    }

    void onEdge(bool level) {
        _rx.onEdge(level);
    }

    bool getFrame(Frame& frame) {
        uint8_t byte;

        while (_rx.getByte(byte)) {
            if (processByte(byte)) {
                frame = _frame;
                reset();
                return true;
            }
        }

        return false;
    }

private:
    enum class State {
        WaitStart,
        Header,
        Payload,
        CRCLow,
        CRCHigh,
        WaitEnd
    };

    bool processByte(uint8_t byte) {
        switch (_state) {

        case State::WaitStart:
            if (byte == START_CODE) {
              resetFrame();
              _state = State::Header;
            } else {
              Serial.print("Expected START, got:");
              Serial.println(byte, HEX);
            }
            break;

        case State::Header: {
            auto* hdr = reinterpret_cast<uint8_t*>(&_frame.header);
            hdr[_headerIndex++] = byte;

            if (_headerIndex == sizeof(FrameHeader)) {
                if (_frame.header.len > MAX_PAYLOAD_SIZE) {
                    reset();
                    break;
                }

                if (_frame.header.len == 0)
                    _state = State::CRCLow;
                else
                    _state = State::Payload;
            }
            break;
        }

        case State::Payload:
            _frame.payload[_payloadIndex++] = byte;

            if (_payloadIndex == _frame.header.len)
                _state = State::CRCLow;

            break;

        case State::CRCLow:
            _frame.crc = byte;
            _state = State::CRCHigh;
            break;

        case State::CRCHigh:
            _frame.crc |= static_cast<uint16_t>(byte) << 8;
            _state = State::WaitEnd;
            break;

        case State::WaitEnd:
            if (byte != END_CODE) {
                Serial.println("Expected END");
                reset();
                break;
            }

            if (crc16::computeFrame(_frame) != _frame.crc) {
                Serial.print("Wrong CRC, expected ");
                Serial.print(crc16::computeFrame(_frame), HEX);
                Serial.print(", got ");
                Serial.println(_frame.crc, HEX);
                reset();
                break;
            }

            return true;
        }

        return false;
    }

    void resetFrame() {
        _headerIndex = 0;
        _payloadIndex = 0;
        _frame.crc = 0;
    }

    void reset() {
        _state = State::WaitStart;
        resetFrame();
    }

private:
    manchester::Receiver _rx;

    Frame _frame;

    State _state = State::WaitStart;

    uint8_t _headerIndex = 0;
    uint8_t _payloadIndex = 0;
};

} // namespace datalink
