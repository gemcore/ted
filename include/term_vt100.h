/*
 * term_vt100.h - VT100 terminal rendering and key decoding.
 *
 * All I/O goes through caller-supplied callbacks, so this module has no
 * OS dependencies: on Zephyr the callbacks write to the shell/UART, on a
 * host they can write to stdout or a test harness. Output uses VT100
 * escape sequences only; input is decoded from raw terminal bytes into
 * editor key events (printable characters, control keys, and common
 * ANSI/VT100 escape sequences for arrows and navigation keys).
 */
#ifndef TERM_VT100_H
#define TERM_VT100_H

#include <stddef.h>
#include <stdbool.h>
#include "editor_core.h"
#include "editor_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TERM_KEY_NONE = 0,   /* ignored byte or unrecognised sequence */
    TERM_KEY_CHAR,       /* printable character in ch */
    TERM_KEY_ENTER,
    TERM_KEY_BACKSPACE,
    TERM_KEY_DELETE,
    TERM_KEY_TAB,
    TERM_KEY_UP,
    TERM_KEY_DOWN,
    TERM_KEY_LEFT,
    TERM_KEY_RIGHT,
    TERM_KEY_HOME,
    TERM_KEY_END,
    TERM_KEY_PAGE_UP,
    TERM_KEY_PAGE_DOWN,
    TERM_KEY_CTRL_S,     /* save */
    TERM_KEY_CTRL_Q      /* quit */
} TERM_KeyType;

typedef struct {
    TERM_KeyType type;
    char         ch;    /* valid when type == TERM_KEY_CHAR */
} TERM_Key;

typedef size_t (*TERM_WriteFn)(void *ctx, const char *data, size_t len);
typedef int    (*TERM_ReadFn)(void *ctx);   /* returns byte 0..255 or -1 */

typedef struct {
    TERM_WriteFn write;
    TERM_ReadFn  read;
    void        *ctx;
    int          esc_state;   /* escape sequence decoder state (private) */
    int          esc_param;   /* CSI parameter byte being decoded (private) */
} TERM;

/* Attach I/O callbacks. write is required; read may be NULL if only
 * rendering is used. */
void TERM_init(TERM *t, TERM_WriteFn write, TERM_ReadFn read, void *ctx);

/* Raw output helpers. */
void TERM_write(TERM *t, const char *s);
void TERM_write_n(TERM *t, const char *s, size_t len);

/* VT100 rendering primitives (rows/cols are 1-based on the wire). */
void TERM_enter(TERM *t);          /* alternate screen, hide cursor */
void TERM_exit(TERM *t);           /* show cursor, leave alternate screen */
void TERM_clear_screen(TERM *t);
void TERM_clear_line(TERM *t);
void TERM_move_to(TERM *t, size_t row, size_t col);
void TERM_hide_cursor(TERM *t);
void TERM_show_cursor(TERM *t);
void TERM_reverse_video(TERM *t);
void TERM_normal_video(TERM *t);

/* Full-screen redraw: document lines within the viewport, reverse-video
 * status line on the last row, then cursor placement. */
void TERM_render(TERM *t, const ED_Doc *doc, const ED_View *view,
                 const char *status);

/* Blocking read of one decoded key. Unrecognised bytes are consumed and
 * reported as TERM_KEY_NONE. Returns false only if read is unavailable
 * or the stream ended. */
bool TERM_read_key(TERM *t, TERM_Key *key);

/* Feed one raw byte into the key decoder. Produces a key when one is
 * complete (returns true), or false if more bytes are needed / the byte
 * was ignored. Exposed for tests and polling loops. */
bool TERM_feed_byte(TERM *t, int byte, TERM_Key *key);

#ifdef __cplusplus
}
#endif

#endif /* TERM_VT100_H */
