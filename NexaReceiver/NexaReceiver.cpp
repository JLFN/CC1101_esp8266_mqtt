
#include <arduino.h> // Need some data types and millis
#include "NexaReceiver.h"


NexaReceiver::NexaReceiver() {
  _mess[0] = '\0';
}

uint8_t NexaReceiver::nDecode() {

  uint32_t  tempSignal = _signal;
  uint8_t   result = 0;
  
// Sync - Transmitter id - Group - On/Off/dim - Unit - (Dim level) - Stop bit - pause
// Sent MSB

  _channel =  0x0F & ((uint8_t)tempSignal);
  tempSignal >>= 4;
  _onOff =    0x01 & ((uint8_t)tempSignal);
  tempSignal >>= 1;
  _group =    0x01 & ((uint8_t)tempSignal);
  tempSignal >>= 1;
  _transmitter = tempSignal;

  if(
    0 <= (int32_t)(millis() - _millis)
    || _transmitterOld != _transmitter
    || _groupOld != _group
    || _onOffOld != _onOff
    || _channelOld != _channel
  ){
    if(
      _transmitterOld == _transmitter
      && _groupOld == _group
      && _onOffOld == _onOff
      && _channelOld == _channel
    ) {
      // Flag identical message
      result = 2;
    } else {
      _transmitterOld = _transmitter;
      _groupOld = _group;
      _onOffOld = _onOff;
      _channelOld = _channel;
      result = 1;
    }
  }
  // To block retransmissions within + x milliseconds
  _millis = millis() + 1000;

  return result;
}

char* NexaReceiver::getMessage() {
  int8_t tempInt =  sprintf(_mess, "S=%u,G=%u,O=%u,R=%u;%c", _transmitter, _group, _onOff, _channel, '\0');
  
  if(tempInt < 1) _mess[0] = '\0';
            
  return _mess;
}

uint8_t NexaReceiver::nReceive(uint16_t duration, uint8_t level) {
/*
  T approx 250us 
  Start:  High T, Low ~10T
  End:    High T, Low ~40T
  (Could be used to start and stop the sampling)
  1 = T high + T low
  0 = T high + ~5T low
  Manchester encoded
  
  Ignoring the dim level versions. Then tri-state on onOff and 4
  additional bits att the end with the dim level.  
*/
  const uint16_t  tMin = 50,
                  tMax = 500,
                  tMax5 = 1800;

  static uint8_t  nCount,
                  tempSig;

  uint8_t         result = 0;

  if(1 == level) {
    if(duration < tMin || tMax < duration) {
      // Wrong duration
      if(nCount) // Only change if not 0
	nCount = 0;
    }
  } else {
    if(duration < tMin || tMax5 < duration) {
      // Wrong duration
      if(nCount)
	nCount = 0;
    } else { // OK duration
      tempSig <<= 1;
      nCount++;
      if(duration < tMax)
        tempSig |= 1;
    
      if(nCount && 0 == nCount % 2) {
        if(0x00 == (0x03 & tempSig) || 0x03 == (0x03 & tempSig)) {
          // Failed manchester
	  nCount = (2 == nCount) ? 1:0;
        } else {
          _signal <<= 1;
          _signal |= (0x01 & tempSig);
        }
        if(64 == nCount) {
          nCount = 0;
          result = 1;
        }
      } // End: 0 == nCount%2
    } // End: OK duration
  } // End: if lvl 0

  return result;
}
