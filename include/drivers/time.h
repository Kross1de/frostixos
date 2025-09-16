#ifndef DRIVERS_TIME_H
#define DRIVERS_TIME_H

#include <kernel/kernel.h>

typedef struct {
  u32 year;  // full year
  u8 month;  // 1-12
  u8 day;    // 1-31
  u8 hour;   // 0-23
  u8 minute; // 0-59
  u8 second; // 0-59
} time_t;

extern u64 g_ticks;
extern const u32 pit_frequensy;

void time_init(void);
void time_update(void);
time_t time_get_current(void);
u64 time_get_uptime_ms(void);

#endif
