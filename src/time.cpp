#include "time.h"

uint32 timestamp;
uint32 when_last_updated;

void time::set(uint32 t) {
  timestamp = t;
  when_last_updated = millis();
}

uint32 time::get_current() {
  if (when_last_updated == 0) return 0;

  uint32 delta_s = (millis() - when_last_updated) / 1000;
  return timestamp + delta_s;
}
