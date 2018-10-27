#ifndef _TIME_H_
#define _TIME_H_

#include <Arduino.h>

// Poor man's RTC.

typedef unsigned long int uint32;

namespace time {
  void set(uint32 t);
  uint32 get_current();
}

#endif
