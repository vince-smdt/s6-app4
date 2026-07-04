#pragma once

#include "datalink.hpp"

namespace protocol {

class Sender {
public:
    void begin(uint8_t txPin, uint8_t rxPin, uint32_t halfBitUs) {
        _tx.begin(txPin, halfBitUs);
        _rx.begin(rxPin, halfBitUs);
    }

    void onEdge(bool level) {
        _rx.onEdge(level);
    }

    void sendSession(const uint8_t* data, size_t len) {
        if (len == 0) return;

        uint8_t nPackets = (len + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
        uint8_t currentSeq = 1;
        uint8_t nackSeq;

        Serial.println("[S] Sending START...");
        _tx.sendPreamble();
        _tx.sendStart(nPackets);

        while (currentSeq <= nPackets) {
            uint16_t offset = (currentSeq - 1) * MAX_PAYLOAD_SIZE;
            uint8_t pktLen = (offset + MAX_PAYLOAD_SIZE > len) ? (len - offset) : MAX_PAYLOAD_SIZE;

            Serial.printf("[S] Sending DATA seq=%u len=%u\n", currentSeq, pktLen);
            _tx.sendPreamble();
            _tx.sendDataFrame(data + offset, pktLen, currentSeq);

            currentSeq++;

            Serial.print("[S] drainNacks...");
            if (drainNacks(nackSeq)) {
                Serial.printf(" NACK(%u) -> rewind to seq %u\n", nackSeq, nackSeq);
                currentSeq = nackSeq;
            } else {
                Serial.println(" none");
            }
        }

        Serial.println("[S] Sending END...");
        _tx.sendPreamble();
        _tx.sendEnd();
    }

private:
    bool drainNacks(uint8_t& nackSeq) {
        bool gotNack = false;
        const Frame* frame;
        while ((frame = _rx.getFrame()) != nullptr) {
            if (frame->header.type == FrameType::NACK) {
                if (!gotNack || frame->header.dyn < nackSeq) {
                    nackSeq = frame->header.dyn;
                }
                gotNack = true;
            }
        }
        return gotNack;
    }

    datalink::Sender _tx;
    datalink::Receiver _rx;
};

class Receiver {
public:
    enum class Status : int {
        IDLE = 0,
        BUSY = 1,
        COMPLETE = 2
    };

    void begin(uint8_t txPin, uint8_t rxPin, uint32_t halfBitUs) {
        _rx.begin(rxPin, halfBitUs);
        _tx.begin(txPin, halfBitUs);
    }

    void onEdge(bool level) {
        _rx.onEdge(level);
    }

    void setBuffer(uint8_t* buffer, size_t size) {
        _buffer = buffer;
        _bufferSize = size;
    }

    Status poll() {
        const Frame* frame = _rx.getFrame();
        if (!frame) {
            Status s = _sessionActive ? Status::BUSY : Status::IDLE;
            if (s == Status::IDLE) Serial.println("[R] idle");
            return s;
        }

        Serial.printf("[R] got frame type=%02X seq=%u len=%u dyn=%u\n",
            (uint8_t)frame->header.type, frame->header.seq,
            frame->header.len, frame->header.dyn);

        switch (frame->header.type) {

        case FrameType::START:
            _expectedSeq = 1;
            _totalPackets = frame->header.dyn;
            _sessionActive = true;
            _receivedLen = 0;
            Serial.printf("[R] START: expecting %u packets\n", _totalPackets);
            return Status::BUSY;

        case FrameType::DATA: {
            if (!_sessionActive) return Status::IDLE;

            if (frame->header.seq == _expectedSeq) {
                Serial.printf("[R] DATA seq=%u ACCEPTED\n", frame->header.seq);
                if (_receivedLen + frame->header.len <= _bufferSize) {
                    memcpy(_buffer + _receivedLen, frame->payload, frame->header.len);
                }
                _receivedLen += frame->header.len;
                _expectedSeq++;

                if (_expectedSeq > _totalPackets) {
                    Serial.println("[R] All packets received -> COMPLETE");
                    return Status::COMPLETE;
                }
            } else {
                Serial.printf("[R] DATA seq=%u != expected %u -> NACK\n",
                    frame->header.seq, _expectedSeq);
                _tx.sendPreamble();
                _tx.sendNack(_expectedSeq);
            }
            return Status::BUSY;
        }

        case FrameType::END:
            Serial.println("[R] END -> COMPLETE");
            _sessionActive = false;
            return Status::COMPLETE;

        default:
            return _sessionActive ? Status::BUSY : Status::IDLE;
        }
    }

    size_t available() const { return _receivedLen; }
    bool isActive() const { return _sessionActive; }

    void reset() {
        _sessionActive = false;
        _expectedSeq = 0;
        _totalPackets = 0;
        _receivedLen = 0;
    }

private:
    datalink::Receiver _rx;
    datalink::Sender _tx;

    uint8_t _expectedSeq = 0;
    uint8_t _totalPackets = 0;
    bool _sessionActive = false;

    uint8_t* _buffer = nullptr;
    size_t _bufferSize = 0;
    size_t _receivedLen = 0;
};

} // namespace protocol
