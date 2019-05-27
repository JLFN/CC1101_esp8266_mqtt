/*
  There are many implementations out there but I've found them difficult to  follow
  and debug. This is quite straight forward I think.

  For a description of the protocol, search for nexa - homeeasy - etc. protocol,
  or check http://tech.jolowe.se/home-automation-rf-protocol-update/ 
  It's well documented out there.

  TODO:
  Add support for dim level if and when needed
  
*/
#ifndef NEXATX_H
#define NEXATX_H

#include <arduino.h> // Need some data types and digitalRead / -Write

class NexaTx {
  public:
    NexaTx(uint8_t pin);
    void nexaTransmitt(uint32_t tId, bool turnOn, uint8_t unit, uint8_t retrans);
    
  private:
    uint8_t _pin;

    // Different timings to test
    const uint16_t  _highT =   275,
                    _shortL =  275,
                    _longL =   1225,
                    _periodT = 250;

    void nexaSendBit(bool bitToSend);
    void nexaSendPair(bool bitToSend);
    void nexaSendCommand(uint32_t tId, bool turnOn, uint8_t unit);
};

#endif

