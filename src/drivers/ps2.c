#include <arch/i386/idt.h>
#include <arch/i386/pic.h>
#include <ctype.h>
#include <drivers/ps2.h>
#include <drivers/serial.h>
#include <lib/terminal.h>
#include <misc/logger.h>

typedef struct {
  char normal;
  char shifted;
} key_t;

static key_t us_keymap[128] = {
    [0x1C] = {'\n', '\n'}, [0x39] = {' ', ' '}, [0x0E] = {'\b', '\b'},
    [0x0F] = {'\t', '\t'}, [0x01] = {0, 0},     [0x3A] = {0, 0},
    [0x2A] = {0, 0},       [0x36] = {0, 0},     [0x1D] = {0, 0},
    [0x38] = {0, 0},       [0x02] = {'1', '!'}, [0x03] = {'2', '@'},
    [0x04] = {'3', '#'},   [0x05] = {'4', '$'}, [0x06] = {'5', '%'},
    [0x07] = {'6', '^'},   [0x08] = {'7', '&'}, [0x09] = {'8', '*'},
    [0x0A] = {'9', '('},   [0x0B] = {'0', ')'}, [0x0C] = {'-', '_'},
    [0x0D] = {'=', '+'},   [0x10] = {'q', 'Q'}, [0x11] = {'w', 'W'},
    [0x12] = {'e', 'E'},   [0x13] = {'r', 'R'}, [0x14] = {'t', 'T'},
    [0x15] = {'y', 'Y'},   [0x16] = {'u', 'U'}, [0x17] = {'i', 'I'},
    [0x18] = {'o', 'O'},   [0x19] = {'p', 'P'}, [0x1A] = {'[', '{'},
    [0x1B] = {']', '}'},   [0x1E] = {'a', 'A'}, [0x1F] = {'s', 'S'},
    [0x20] = {'d', 'D'},   [0x21] = {'f', 'F'}, [0x22] = {'g', 'G'},
    [0x23] = {'h', 'H'},   [0x24] = {'j', 'J'}, [0x25] = {'k', 'K'},
    [0x26] = {'l', 'L'},   [0x27] = {';', ':'}, [0x28] = {'\'', '"'},
    [0x2B] = {'\\', '|'},  [0x2C] = {'z', 'Z'}, [0x2D] = {'x', 'X'},
    [0x2E] = {'c', 'C'},   [0x2F] = {'v', 'V'}, [0x30] = {'b', 'B'},
    [0x31] = {'n', 'N'},   [0x32] = {'m', 'M'}, [0x33] = {',', '<'},
    [0x34] = {'.', '>'},   [0x35] = {'/', '?'}, [0x29] = {'`', '~'},
};

static bool left_shift = false;
static bool right_shift = false;
static bool caps_lock = false;

static void keyboard_handler(registers_t *regs) {
  uint8_t scancode = inb(0x60);

  // log(LOG_INFO, "Scancode: 0x%02x", scancode);

  bool release = (scancode & 0x80) != 0;
  uint8_t code = scancode & 0x7F;

  if (release) {
    switch (code) {
    case 0x2A:
      left_shift = false;
      break;
    case 0x36:
      right_shift = false;
      break;
    }
  } else {
    switch (code) {
    case 0x2A:
      left_shift = true;
      break;
    case 0x36:
      right_shift = true;
      break;
    case 0x3A:
      caps_lock = !caps_lock;
      break;
    default:
      if (code < 128 && us_keymap[code].normal != 0) {
        bool shift_active = left_shift || right_shift;
        bool use_upper = (caps_lock != shift_active);
        char c;
        if (isalpha(us_keymap[code].normal)) {
          c = use_upper ? (char)toupper((int)us_keymap[code].normal)
                        : us_keymap[code].normal;
        } else {
          c = shift_active ? us_keymap[code].shifted : us_keymap[code].normal;
        }
        if (c != 0) {
          terminal_putchar(&g_terminal, c);
	  if (c == '\b') {
	    serial_write_string("\b \b");
	  } else {
            serial_write_char(c);
	  }
        }
      }
      break;
    }
  }

  pic_send_eoi(1);
}

kernel_status_t ps2_keyboard_init(void) {
  outb(0x64, 0xAD);

  while (inb(0x64) & 0x01) {
    inb(0x60);
  }

  outb(0x64, 0xAE);

  isr_register_handler(33, keyboard_handler);
  pic_unmask(1);

  log(LOG_OKAY, "PS/2 keyboard initialized.");
  return KERNEL_OK;
}
