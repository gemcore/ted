/*
 * term_vt100.c - VT100 terminal rendering and key decoding.
 *
 * Rendering emits ANSI/VT100 escape sequences through the caller's write
 * callback. Input decodes raw bytes (including CSI/SS3 escape sequences
 * sent by common terminal emulators) into editor key events. No OS calls.
 */
#include <stdint.h>

#include "editor_config.h"
#include "term_vt100.h"

#define ESC 0x1B
#define CSI "\x1b["

/* Escape decoder states (stored in TERM.esc_state). */
enum {
    ST_GROUND = 0,
    ST_ESC,         /* got ESC */
    ST_CSI,         /* got ESC [ */
    ST_CSI_PARAM,   /* got ESC [ digit */
    ST_SS3          /* got ESC O */
};

void TERM_init(TERM *t, TERM_WriteFn write, TERM_ReadFn read, void *ctx)
{
    if (t == NULL) {
        return;
    }
    t->write = write;
    t->read = read;
    t->ctx = ctx;
    t->esc_state = ST_GROUND;
    t->esc_param = 0;
}

void TERM_write_n(TERM *t, const char *s, size_t len)
{
    if (t == NULL || t->write == NULL || s == NULL) {
        return;
    }
    t->write(t->ctx, s, len);
}

void TERM_write(TERM *t, const char *s)
{
    const char *p;

    if (s == NULL) {
        return;
    }
    for (p = s; *p != '\0'; p++) {
        /* measure */
    }
    TERM_write_n(t, s, (size_t)(p - s));
}

static void term_write_uint(TERM *t, size_t value)
{
    char tmp[10];
    size_t i = sizeof(tmp);

    if (value == 0) {
        TERM_write_n(t, "0", 1);
        return;
    }
    while (value > 0 && i > 0) {
        tmp[--i] = (char)('0' + (value % 10));
        value /= 10;
    }
    TERM_write_n(t, &tmp[i], sizeof(tmp) - i);
}

void TERM_enter(TERM *t)
{
    TERM_write(t, CSI "?1049h");  /* alternate screen buffer */
    TERM_write(t, CSI "?25l");    /* hide cursor */
    TERM_clear_screen(t);
}

void TERM_exit(TERM *t)
{
    TERM_write(t, CSI "0m");      /* normal video */
    TERM_write(t, CSI "?25h");    /* show cursor */
    TERM_write(t, CSI "?1049l");  /* restore main screen buffer */
}

void TERM_clear_screen(TERM *t)
{
    TERM_write(t, CSI "H");
    TERM_write(t, CSI "2J");
}

void TERM_clear_line(TERM *t)
{
    TERM_write(t, CSI "K");
}

void TERM_move_to(TERM *t, size_t row, size_t col)
{
    TERM_write(t, CSI "");
    term_write_uint(t, row + 1);
    TERM_write_n(t, ";", 1);
    term_write_uint(t, col + 1);
    TERM_write_n(t, "H", 1);
}

void TERM_hide_cursor(TERM *t)
{
    TERM_write(t, CSI "?25l");
}

void TERM_show_cursor(TERM *t)
{
    TERM_write(t, CSI "?25h");
}

void TERM_reverse_video(TERM *t)
{
    TERM_write(t, CSI "7m");
}

void TERM_normal_video(TERM *t)
{
    TERM_write(t, CSI "0m");
}

static char term_display_glyph(char ch)
{
    if (ch >= 32 && ch <= 126) {
        return ch;
    }
    return EDITOR_UNPRINTABLE_GLYPH;
}

void TERM_render(TERM *t, const ED_Doc *doc, const ED_View *view,
                 const char *status)
{
    size_t first, last, row, cols, text_rows;

    if (t == NULL || doc == NULL || view == NULL) {
        return;
    }
    EDV_visible_range(view, doc, &first, &last);
    cols = EDV_text_cols(view);
    text_rows = EDV_text_rows(view);

    TERM_hide_cursor(t);
    TERM_move_to(t, 0, 0);

    for (row = 0; row < text_rows; row++) {
        size_t drow = first + row;

        TERM_clear_line(t);
        if (drow < last) {
            const char *text = ED_line_text(doc, drow);
            size_t len = ED_line_len(doc, drow);
            size_t i, disp = 0, shown = 0;

            for (i = 0; i < len && shown < cols; i++) {
                char ch = text[i];

                if (ch == '\t') {
                    size_t next = disp + EDITOR_TAB_WIDTH -
                                  (disp % EDITOR_TAB_WIDTH);
                    while (disp < next && shown < cols) {
                        if (disp >= view->left) {
                            TERM_write_n(t, " ", 1);
                            shown++;
                        }
                        disp++;
                    }
                } else {
                    if (disp >= view->left) {
                        char g = term_display_glyph(ch);
                        TERM_write_n(t, &g, 1);
                        shown++;
                    }
                    disp++;
                }
            }
        } else {
            TERM_write_n(t, "~", 1);    /* past end of document */
        }
        if (row + 1 < text_rows) {
            TERM_write_n(t, "\r\n", 2);
        }
    }

    /* Status line on the last terminal row, reverse video. */
    TERM_move_to(t, text_rows, 0);
    TERM_reverse_video(t);
    if (status != NULL) {
        size_t shown = 0;
        const char *p;

        for (p = status; *p != '\0' && shown < cols; p++, shown++) {
            TERM_write_n(t, p, 1);
        }
        while (shown < cols) {
            TERM_write_n(t, " ", 1);
            shown++;
        }
    } else {
        size_t shown;
        for (shown = 0; shown < cols; shown++) {
            TERM_write_n(t, " ", 1);
        }
    }
    TERM_normal_video(t);

    /* Place the hardware cursor at the editing point. */
    {
        size_t sr, sc;

        EDV_cursor_screen_pos(view, doc, &sr, &sc);
        TERM_move_to(t, sr, sc);
    }
    TERM_show_cursor(t);
}

/* ------------------------------------------------------------------ */
/* Key decoding                                                        */
/* ------------------------------------------------------------------ */

static void key_simple(TERM_Key *key, TERM_KeyType type)
{
    key->type = type;
    key->ch = '\0';
}

bool TERM_feed_byte(TERM *t, int byte, TERM_Key *key)
{
    if (t == NULL || key == NULL || byte < 0) {
        return false;
    }

    switch (t->esc_state) {
    case ST_ESC:
        if (byte == '[') {
            t->esc_state = ST_CSI;
        } else if (byte == 'O') {
            t->esc_state = ST_SS3;
        } else {
            t->esc_state = ST_GROUND;   /* unrecognised: drop */
        }
        return false;

    case ST_CSI:
        if (byte >= '0' && byte <= '9') {
            t->esc_param = byte - '0';
            t->esc_state = ST_CSI_PARAM;
            return false;
        }
        t->esc_state = ST_GROUND;
        switch (byte) {
        case 'A': key_simple(key, TERM_KEY_UP);    return true;
        case 'B': key_simple(key, TERM_KEY_DOWN);  return true;
        case 'C': key_simple(key, TERM_KEY_RIGHT); return true;
        case 'D': key_simple(key, TERM_KEY_LEFT);  return true;
        case 'H': key_simple(key, TERM_KEY_HOME);  return true;
        case 'F': key_simple(key, TERM_KEY_END);   return true;
        default:  key_simple(key, TERM_KEY_NONE);  return true;
        }

    case ST_CSI_PARAM:
        /* ESC [ <digit> ~ style sequences */
        t->esc_state = ST_GROUND;
        if (byte == '~') {
            switch (t->esc_param) {
            case 1: key_simple(key, TERM_KEY_HOME);      return true;
            case 3: key_simple(key, TERM_KEY_DELETE);    return true;
            case 4: key_simple(key, TERM_KEY_END);       return true;
            case 5: key_simple(key, TERM_KEY_PAGE_UP);   return true;
            case 6: key_simple(key, TERM_KEY_PAGE_DOWN); return true;
            case 7: key_simple(key, TERM_KEY_HOME);      return true;
            case 8: key_simple(key, TERM_KEY_END);       return true;
            default: key_simple(key, TERM_KEY_NONE);     return true;
            }
        }
        key_simple(key, TERM_KEY_NONE);
        return true;

    case ST_SS3:
        t->esc_state = ST_GROUND;
        switch (byte) {
        case 'A': key_simple(key, TERM_KEY_UP);    return true;
        case 'B': key_simple(key, TERM_KEY_DOWN);  return true;
        case 'C': key_simple(key, TERM_KEY_RIGHT); return true;
        case 'D': key_simple(key, TERM_KEY_LEFT);  return true;
        case 'H': key_simple(key, TERM_KEY_HOME);  return true;
        case 'F': key_simple(key, TERM_KEY_END);   return true;
        default:  key_simple(key, TERM_KEY_NONE);  return true;
        }

    default:
        t->esc_state = ST_GROUND;
        break;
    }

    /* ST_GROUND */
    switch (byte) {
    case ESC:
        t->esc_state = ST_ESC;
        return false;
    case '\r':
    case '\n':
        key_simple(key, TERM_KEY_ENTER);
        return true;
    case 0x7F:
    case 0x08:
        key_simple(key, TERM_KEY_BACKSPACE);
        return true;
    case '\t':
        key_simple(key, TERM_KEY_TAB);
        return true;
    case 0x13:  /* Ctrl-S */
        key_simple(key, TERM_KEY_CTRL_S);
        return true;
    case 0x11:  /* Ctrl-Q */
        key_simple(key, TERM_KEY_CTRL_Q);
        return true;
    default:
        if (byte >= 32 && byte <= 126) {
            key->type = TERM_KEY_CHAR;
            key->ch = (char)byte;
            return true;
        }
        key_simple(key, TERM_KEY_NONE);  /* other control bytes ignored */
        return true;
    }
}

bool TERM_read_key(TERM *t, TERM_Key *key)
{
    int byte;

    if (t == NULL || key == NULL || t->read == NULL) {
        return false;
    }
    for (;;) {
        byte = t->read(t->ctx);
        if (byte < 0) {
            return false;   /* stream ended */
        }
        if (TERM_feed_byte(t, byte, key)) {
            return true;
        }
    }
}
