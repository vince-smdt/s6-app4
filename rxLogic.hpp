#pragma once

#include "frame.hpp"
#include "crc16.hpp"
#include "manchester.hpp"

namespace rxLogic {

  enum class FrameState : int {
    PRE,
    START,
    HEADER,
    DATA,
    CONTROL,
    END
  };

  enum class HeaderState : int {
    COMM,
    SEQ,
    LEN,
    DYN
  };

  FrameState state = FrameState::PRE;
  HeaderState stateEntete = HeaderState::COMM;

  const int MAXPACKETLENGTH = 80  ;
  uint8_t packetBuffer[MAXPACKETLENGTH];
  uint16_t bytesReceived = 0;
  bool isError = false;

  uint8_t crc1;
  bool crc1Empty = true;
  uint16_t crc;

  FrameHeader header;

  void reset(){
    state = FrameState::PRE;
    stateEntete = HeaderState::COMM;

    bytesReceived = 0;
    memset(packetBuffer, 0, MAXPACKETLENGTH);
    crc1Empty = true;
  
    isError = false;
  }

  void headerStateMngr(uint8_t dataRX){
    switch(stateEntete){
      case HeaderState::COMM:
        //Type comm
        header.type = (FrameType)dataRX;
        stateEntete = HeaderState::SEQ;
        break;
      case HeaderState::SEQ:
        //Num de séquence
        if(dataRX != (header.seq + 1) || (header.type == FrameType::DATA && dataRX > header.dyn)){
          isError = true;
        }

        header.seq = dataRX;
        stateEntete = HeaderState::LEN;
        break;
      case HeaderState::LEN:
        //Longueur
        header.len = dataRX;
        stateEntete = HeaderState::DYN;
        break;
      case HeaderState::DYN:
        //Volume dynamique
        if(header.type == FrameType::DATA || header.type == FrameType::NACK){
          header.dyn = dataRX;
        }
        stateEntete = HeaderState::COMM;

        if(header.type == FrameType::DATA){
          state = FrameState::DATA;
        }else{
          state = FrameState::CONTROL;
        }
        break;
    }
  }

  void rxStateMngr(uint8_t dataRX){
    switch(state){
      case FrameState::PRE:
        //Preambule
        if(dataRX == 0x55){
          state = FrameState::START;
        }
        break;

      case FrameState::START:
        //Start
        if(dataRX == 0x7E){
          state = FrameState::HEADER;
        }
        break;

      case FrameState::HEADER:
        headerStateMngr(dataRX);
        break;

      case FrameState::DATA:
        //Data
        if (bytesReceived < MAXPACKETLENGTH)
        {
          if(!isError){
            packetBuffer[bytesReceived++] = dataRX;
            if(bytesReceived == header.len)
            {
              state = FrameState::CONTROL;
            }
          }
        }else{
          state = FrameState::CONTROL;
        }
        break;

      case FrameState::CONTROL:
        //Controle
        if(crc1Empty){
          crc1 = dataRX;
          crc1Empty = false;
        }else{
          crc = (static_cast<uint16_t>(crc1) << 8) | dataRX ;

          Frame frame;
          frame.header = header;
          memcpy(frame.payload, packetBuffer, header.len);
          frame.crc = crc;
          crc16::computeFrame(frame);
          state = FrameState::END;
        }
        break;

      case FrameState::END:
        if(dataRX == 0x7E){
          if(header.type == FrameType::DATA){
            Serial.write(packetBuffer, header.len);
          }
          reset();
        }
        break;
    }
  }

  void read(){
    while (manchester::available()) {
      Serial.print("RX: 0x");
      uint8_t dataRX = manchester::read();
      Serial.println(dataRX, HEX);
  
      rxStateMngr(dataRX);
    }
  }
}