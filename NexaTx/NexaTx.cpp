
#include <arduino.h> // Need some data types and digitalRead / -Write
#include "nexaTx.h"

NexaTx::NexaTx(uint8_t pin){
  _pin = pin;
}

void NexaTx::nexaSendBit(bool bitToSend) {
                  
  digitalWrite(_pin, HIGH);
  delayMicroseconds(_highT);
  digitalWrite(_pin, LOW);
  if(bitToSend)
    delayMicroseconds(_shortL);
  else
    delayMicroseconds(_longL);
}

void NexaTx::nexaSendPair(bool bitToSend) {
  // Inverted manchester bit first
  nexaSendBit(!bitToSend);
  nexaSendBit(bitToSend);
}

void NexaTx::nexaSendCommand(uint32_t tId, bool turnOn, uint8_t unit) {
  const uint16_t  periodT = 250;
  uint8_t         i,
                  bitToSend;

  // Sync - Transmitter id - Group - On/Off/dim - Unit - (Dim level) - Stop bit - pause
  // Send MSB
  
  // Sync
  nexaSendBit(1);
  delayMicroseconds(10*_periodT);

  // Send 26 bit transmitter id
  tId <<= 6;
  i= 26;
  while(0<i--) {
    bitToSend = (0x80000000 == (0x80000000 & tId));
    nexaSendPair(bitToSend);
    tId <<= 1;
  }
  
  // Group message, 0/1 (Always 0 here)
  nexaSendPair(0);

  // Unit on/off (or dimmer flag "00", which breaks the manchester "rule")
  nexaSendPair(turnOn);

  // Send 4 bit unit id
  i= 4;
  while(0<i--) {
    bitToSend = (0x08 == (0x08 & unit));
    nexaSendPair(bitToSend);
    unit <<= 1;
  }

  // Send 4 bit dimmer level (not implemented here) 0-100?

  // Send stop bit and pause
  nexaSendBit(1);
  delayMicroseconds(39*_periodT);

}

void NexaTx::nexaTransmitt(uint32_t tId, bool turnOn, uint8_t unit, uint8_t retrans) {
  while(0 < retrans--) {
    this->nexaSendCommand(tId, turnOn, unit);
  }
}

