#ifndef LIB_ANSI_TYPES_H
#define LIB_ANSI_TYPES_H

/*
 * Types for the ANSI/VT100 escape sequence parser and state.
 *
 * The context stores parser state, parameter buffers, saved cursor
 * location (for save/restore commands) and a flag to enable/disable
 * cursor rendering behavior.
 */
typedef enum {
	ANSI_NORMAL,
	ANSI_ESC,
	ANSI_CSI,
} ansi_state_t;

typedef struct {
	ansi_state_t state;	/* current parser state */
	int ansi_private;	/* private parameter (e.g., '?') */
	char ansi_buf[64];	/* temporary buffer for parsing sequences */
	int ansi_buf_idx;	/* current index in ansi_buf */
	int ansi_params[16];	/* parsed numeric parameters */
	int ansi_param_count;	/* number of parsed parameters */
	u16 saved_row;		/* saved cursor row (VT100 save/restore) */
	u16 saved_col;		/* saved cursor column */
	bool cursor_enabled;	/* whether cursor rendering is enabled */
} ansi_context_t;

#endif /* LIB_ANSI_TYPES_H */