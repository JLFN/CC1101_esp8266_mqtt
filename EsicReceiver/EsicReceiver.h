/*
  1100 0001 0011 0011 1011 0100 1001 1001 1000 - House 1, Channel 1, Battery 0, 23.5'C, 59%
  ---- HHHH CC-- BRRR RRRR -TTT TTTT TTTT SSPP
  https://github.com/NetHome/Coders/blob/master/src/main/java/nu/nethome/coders/decoders/UPMDecoder.java

*/

#ifndef ESICRECEIVER_H
#define ESICRECEIVER_H

#include <arduino.h> // Need some data types and millis

class EsicReceiver {
  public:
    EsicReceiver();
    uint8_t   eReceive(uint16_t duration);
    uint8_t   eDecode();
    char*     getMessage();
    
  private:
    uint32_t  _signal,
              _millis;    // Last updated

    int16_t   _primary;
    
    char      _mess[30];

    uint8_t   _house,
              _houseOld,
              _channel,
              _channelOld,
              _battery,
              _seq,         // Transmission number (1-3)
              _secondary;

    uint8_t   checkParity();

};

#endif
