
#include <arduino.h> // Need some data types and millis
#include "EsicReceiver.h"


EsicReceiver::EsicReceiver() {
  _mess[0] = '\0';
}

uint8_t EsicReceiver::eDecode() {
  // House 1, Channel 1, Battery 0, 23.5'C, 59%
  // (1100) 0001 0011 0011 1011 0100 1001 1001 1000
  // (----) HHHH CC-- BRRR RRRR ?TTT TTTT TTTT SSPP

  uint32_t  tempSignal = _signal;

  uint8_t   result = 0;

  tempSignal >>= 2;
  _seq =         (0x03 & ((uint8_t)tempSignal)) + 1;
  tempSignal >>= 2;
  _primary =     0x07FF & ((uint16_t)tempSignal);
  tempSignal >>= 12;
  _secondary =   0x7F & ((uint8_t)tempSignal);
  tempSignal >>= 7;
  _battery =     0x01 & ((uint8_t)tempSignal);
  tempSignal >>= 3;
  _channel =     (0x03 & ((uint8_t)tempSignal)) + 1;
  tempSignal >>= 2;
  _house =       ((uint8_t)tempSignal);

  _primary = (_primary - 800) * 10 / 16;

  if( // Skip if retransmission
    0 <= (int32_t)(millis() - _millis)
    || _house != _houseOld
    || _channel != _channelOld
  ){
    // "New" signal
    if(
      _houseOld == _house
      && _channelOld == _channel
    ){
      // Flagg identical house and channel as last        
      result = 2; // * so that only if message created
    } else {
      // New house or channel
      _houseOld = _house;
      _channelOld = _channel;
      result = 1;
    }
    //Serial.println(_signal,BIN);
  }
  // Block retransmissions within + x milliseconds
  // (Bursts of 3, ~every minute)
  _millis = millis() + 500;

  return result;
}

char* EsicReceiver::getMessage() {
  int8_t tempInt =  sprintf(_mess,"H=%u,C=%u,T=%i.%i,RH=%u,B=%u,S=%u;%c",
          _house, _channel, (_primary/10), abs(_primary%10), _secondary, _battery, _seq, '\0');
  
  if(tempInt < 1) _mess[0] = '\0';
            
  return _mess;
}

uint8_t EsicReceiver::checkParity() {
  // Two paritys, one for even and one for odd signals
  uint32_t  tempSignal = _signal;
  uint8_t   eParity = 0,
            i = 16;

  while(0 < i--) {
    eParity ^= ((uint8_t)tempSignal);
    tempSignal >>= 2;
  }
  return (0x03 == (0x03 & eParity));
}

uint8_t EsicReceiver::eReceive(uint16_t duration) {
  // T approx 2000us => T/2 approx 1000us

  const uint16_t  shortMin = 100,
                  shortMax = 1400,
                  longMin = 1500,
                  longMax = 2900;
  const uint8_t   const1 = 0x0C,
                  const2 = 0x03;
  static uint8_t  eCount,
                  eTogg;
  boolean         result = 0;

  if(duration < shortMin || longMax < duration) {
    // Wrong duration! Reset if needed
    if(0 != eCount)   eCount = 0;
    if(0 != eTogg)    eTogg = 0;
  } else {
    // OK duration
    if(duration < longMin) {
      // 1
      eTogg ^= 1;
      if(0 == eTogg) {
        _signal <<= 1;
        _signal |= 1;
        eCount++;
      }
    } else if(0 == eTogg) {
      _signal <<= 1;
      eCount++;
    } else {
      // Should always have been 2 short in a row. Reset if needed
      if(0 != eCount)   eCount = 0;
      if(0 != eTogg)    eTogg = 0;
    }
    
    if(4 == eCount && const1 != (0x0F & (uint8_t)_signal)) {
      eCount = eTogg = 0;
      //Serial.println(F("Fail:\tC 1"));
    } else if(12 == eCount && const2 != (0x03 & (uint8_t)_signal)) {
      eCount = eTogg = 0;
      //Serial.println(F("Fail:\tC 2"));
      //Serial.println(((uint16_t)esicSignal | 0x8000), BIN);
    } else if(36 == eCount) { // Expected number of bits
      if(this->checkParity()) {
        result = 1;
      }
      eCount = eTogg = 0;
    }
  }

  return result;
}
