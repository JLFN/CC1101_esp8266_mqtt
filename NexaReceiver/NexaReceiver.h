/*
*/

#ifndef NEXARECEIVER_H
#define NEXARECEIVER_H

#include <arduino.h> // Need some data types and millis

class NexaReceiver {
  public:
    NexaReceiver();
    uint8_t   nReceive(uint16_t duration, uint8_t level);
    uint8_t   nDecode();
    char*     getMessage();
    
  private:
    uint32_t  _signal,
              _millis,    // Last updated
              _transmitter,
              _transmitterOld;

    char      _mess[30];

    uint8_t   _channel,
              _channelOld,
              _group,
              _groupOld,
              _onOff,
              _onOffOld;

};

#endif
