#include <drivers/time.h>
#include <kernel/kernel.h>
#include <printf.h>

time_t g_current_time;
u64 g_ticks = 0;
const u32 pit_frequensy = 100;

#define CMOS_INDEX 0x70
#define CMOS_DATA 0x71
#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_DAY 0x07
#define RTC_MONTH 0x08
#define RTC_YEAR 0x09
#define RTC_STATUS_A 0x0A
#define RTC_STATUS_B 0x0B
#define RTC_CENTURY 0x32

static u8 cmos_read(u8 reg) {
  outb(CMOS_INDEX, reg);
  return inb(CMOS_DATA);
}

static u8 bcd_to_bin(u8 val) {
  return ((val & 0xF0) >> 1) + ((val & 0xF0) >> 3) + (val & 0x0F);
}

static void rtc_read(time_t *time) {
  while (cmos_read(RTC_STATUS_A) & 0x80)
    ;

  u8 status_b = cmos_read(RTC_STATUS_B);
  bool is_binary = (status_b & 0x04) != 0;
  bool is_24h = (status_b & 0x02) != 0;

  time->second = cmos_read(RTC_SECONDS);
  time->minute = cmos_read(RTC_MINUTES);
  time->hour = cmos_read(RTC_HOURS);
  time->day = cmos_read(RTC_DAY);
  time->month = cmos_read(RTC_MONTH);
  time->year = cmos_read(RTC_YEAR);

  if (!is_binary) {
    time->second = bcd_to_bin(time->second);
    time->minute = bcd_to_bin(time->minute);
    time->hour = bcd_to_bin(time->hour);
    time->day = bcd_to_bin(time->day);
    time->month = bcd_to_bin(time->month);
    time->year = bcd_to_bin(time->year);
  }

  if (!is_24h) {
    bool is_pm = (time->hour & 0x80) != 0;
    time->hour &= 0x7F;
    if (is_pm && time->hour != 12)
      time->hour += 12;
    else if (!is_pm && time->hour == 12)
      time->hour = 0;
  }

  u8 century = cmos_read(RTC_CENTURY);
  time->year += (century != 0 ? bcd_to_bin(century) : 20) * 100;
}

void time_init(void) {
  rtc_read(&g_current_time);
  g_ticks = 0;
  printf("Current time: %04u-%02u-%02u %02u:%02u:%02u\n", g_current_time.year,
         g_current_time.month, g_current_time.day, g_current_time.hour,
         g_current_time.minute, g_current_time.second);
}

void time_update(void) {
  g_current_time.second++;
  if (g_current_time.second >= 60) {
    g_current_time.second = 0;
    g_current_time.minute++;
    if (g_current_time.minute >= 60) {
      g_current_time.minute = 0;
      g_current_time.hour++;
      if (g_current_time.hour >= 24) {
        g_current_time.hour = 0;
      }
    }
  }
}

time_t time_get_current(void) { return g_current_time; }

u64 time_get_uptime_ms(void) { return (g_ticks * 1000) / pit_frequensy; }