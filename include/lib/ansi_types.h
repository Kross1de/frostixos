#ifndef LIB_ANSI_TYPES_H
#define LIB_ANSI_TYPES_H

typedef enum { ANSI_NORMAL, ANSI_ESC, ANSI_CSI } ansi_state_t;

typedef struct {
  ansi_state_t state;
  int ansi_private;
  char ansi_buf[64];
  int ansi_buf_idx;
  int ansi_params[16];
  int ansi_param_count;
  u16 saved_row; /* For VT100 save/restore cursor */
  u16 saved_col;
  bool cursor_enabled; /* For VT100 mode control */
} ansi_context_t;

#endif