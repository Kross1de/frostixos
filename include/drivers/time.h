#ifndef DRIVERS_TIME_H
#define DRIVERS_TIME_H

#include <kernel/kernel.h>

/*
 * Time APIs and uptime tracking.
 *
 * time_t holds a simple broken-down time representation. The implementation
 * should keep g_ticks updated from the PIT and provide current time and
 * uptime helpers.
 */
typedef struct {
	u32 year;  /* full year */
	u8 month;  /* 1-12 */
	u8 day;    /* 1-31 */
	u8 hour;   /* 0-23 */
	u8 minute; /* 0-59 */
	u8 second; /* 0-59 */
} time_t;

/* Global tick counter (milliseconds or ticks depending on implementation). */
extern u64 g_ticks;

/* PIT base frequency in Hz used by the time subsystem */
extern const u32 pit_frequency;

/* Draw on-screen status (e.g., time/status bar). */
void draw_status(void);

/* Initialize time subsystem (hook PIT, read RTC, etc.). */
void time_init(void);

/* Update time state from tick/RTC; typically called from timer IRQ. */
void time_update(void);

/* Return current broken-down time. */
time_t time_get_current(void);

/* Return uptime in milliseconds (or units documented by implementation). */
u64 time_get_uptime_ms(void);

#endif /* DRIVERS_TIME_H */