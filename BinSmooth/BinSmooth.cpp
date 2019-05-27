// Add hysteres if smoothOver is large enough
// Fix better than now
// Tidy up!

#include <arduino.h> // Need some data types and digitalRead / -Write
#include "BinSmooth.h"

BinSmooth::BinSmooth(uint8_t smoothOver) {
  if(smoothOver < 1)
    smoothOver = 1;
  else if(32 < smoothOver)
    smoothOver = 32;
  
  _smoothHalf = floor(smoothOver/2);
  _mask = (1<<(smoothOver-1));
  _values = 0;
  _sum = 0;

  _smoothHystLow = _smoothHalf - smoothOver/8;
  _smoothHystHigh = _smoothHalf + smoothOver/8;
}

void BinSmooth::newSignal(bool level) {
  _currentLevel = this->calcCurrentLevel(level);
  if(_currentLevel != _lastLevel) {
    this->setNewSignal();
  }
}

void BinSmooth::newSignalGap(bool level) {
  _currentLevel = this->calcCurrentLevel(level);
  if(_currentLevel != _lastLevel && _sum != _smoothHalf) {
    this->setNewSignal();
  }
}

void BinSmooth::newSignalHyst(bool level) {
  _currentLevel = this->calcCurrentLevel(level);
  if(_currentLevel != _lastLevel && (_sum <= _smoothHystLow || _smoothHystHigh <= _sum)) {
    this->setNewSignal();
  }
}

bool BinSmooth::availSignal() {
  if(_availSignal) {
    _availSignal = 0;
    return 1;
  } else {
    return 0;
  }
}

bool BinSmooth::getLevel() {
  return _signalLevel;
}

uint16_t BinSmooth::getDuration() {
  return _signalDuration;
}

void BinSmooth::setNewSignal() {
  _signalDuration = micros() - _micros;
  _micros = micros();
  _signalLevel = _lastLevel;
  _lastLevel = _currentLevel;
  _availSignal = 1;
}

bool BinSmooth::calcCurrentLevel(bool level) {
  if(_mask == (_mask & _values))
    _sum--;
  _values <<= 1;
  if(level) {
    _sum++;
    _values |= 1;
  }
  return (_smoothHalf < _sum);
}
