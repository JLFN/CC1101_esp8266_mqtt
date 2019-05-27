/*
*/

#ifndef BINSMOOTH_H
#define BINSMOOTH_H

#include <arduino.h> // Need some data types and millis

class BinSmooth {
  public:
    BinSmooth(uint8_t smoothOver);
    void      newSignal(bool level);
    void      newSignalGap(bool level);
    void      newSignalHyst(bool level);
    bool      availSignal();
    bool      getLevel();
    uint16_t  getDuration();
    
  private:
    void      setNewSignal();
    bool      calcCurrentLevel(bool level);
    uint32_t  _mask,
              _values,
              _micros;

    uint16_t  _signalDuration;
    uint8_t   _sum,
              _smoothHalf,
              _smoothHystLow,
              _smoothHystHigh,
              _count;

    bool      _currentLevel,
              _lastLevel,
              _signalLevel,
              _availSignal;
              
};

#endif
