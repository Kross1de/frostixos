#include <drivers/screen.h>
#include <drivers/time.h>
#include <drivers/vbe.h>
#include <kernel/kernel.h>
#include <lib/terminal.h>
#include <printf.h>
#include <string.h>

time_t g_current_time;
u64 g_ticks = 0;
const u32 pit_frequency = 100;
static time_t g_prev_time = {0};

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

static u8 bcd_to_bin(u8 val) { return ((val & 0xF0) >> 4) * 10 + (val & 0x0F); }

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
  if (!is_binary && century != 0) {
    century = bcd_to_bin(century);
  }
  time->year += (century != 0 ? century : 20) * 100;
}

void draw_status(void) {
  char buf[32];
  sprintf(buf, "%04u-%02u-%02u %02u:%02u:%02u", g_current_time.year,
          g_current_time.month, g_current_time.day, g_current_time.hour,
          g_current_time.minute, g_current_time.second);

  const font_t *font = g_terminal.font;
  u16 font_w = font->width;
  u16 font_h = font->height;
  size_t len = strlen(buf);
  u16 screen_w = screen_get_width();
  u16 screen_h = screen_get_height();
  u16 y = (screen_h / font_h - 1) * font_h;
  u16 x = screen_w - (len * font_w);

  vbe_fill_rect(x, y, len * font_w, font_h, g_terminal.bg_color);

  screen_draw_string(x, y, buf, g_terminal.fg_color, g_terminal.bg_color);

  g_prev_time = g_current_time;
}

void time_init(void) {
  rtc_read(&g_current_time);
  g_ticks = 0;
  g_prev_time = g_current_time;
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
        rtc_read(&g_current_time);
      }
    }
  }

  static u8 last_second = 255;
  if (last_second != g_current_time.second) {
    draw_status();
    last_second = g_current_time.second;
  }
}

time_t time_get_current(void) { return g_current_time; }

u64 time_get_uptime_ms(void) { return (g_ticks * 1000) / pit_frequency; }