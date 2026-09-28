/*
 * test_term_vt100.c - Unit tests for VT100 rendering and key decoding.
 */
#include <string.h>

#include "term_vt100.h"
#include "test_common.h"

static char   out[8192];
static size_t out_len;

static size_t capture_write(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    if (out_len + len < sizeof(out)) {
        memcpy(out + out_len, data, len);
        out_len += len;
        out[out_len] = '\0';
    }
    return len;
}

static void capture_reset(void)
{
    out_len = 0;
    out[0] = '\0';
}

static void feed(TERM *t, const char *bytes, size_t n, TERM_Key *keys,
                 size_t *nkeys)
{
    size_t i;

    *nkeys = 0;
    for (i = 0; i < n; i++) {
        TERM_Key k;

        if (TERM_feed_byte(t, (unsigned char)bytes[i], &k)) {
            keys[(*nkeys)++] = k;
        }
    }
}

static void test_single_key(TERM_KeyType expected, const char *seq,
                            size_t len)
{
    TERM t;
    TERM_Key keys[4];
    size_t nkeys = 0;

    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, seq, len, keys, &nkeys);
    CHECK(nkeys == 1);
    if (nkeys == 1 && keys[0].type != expected) {
        printf("  seq=[");
        for (size_t i = 0; i < len; i++) {
            printf("\\x%02x", (unsigned char)seq[i]);
        }
        printf("] decoded as type %d, expected %d\n",
               keys[0].type, expected);
    }
    CHECK(keys[0].type == expected);
}

int main(void)
{
    TERM t;
    TERM_Key keys[8];
    size_t nkeys;

    /* --- Rendering primitives --- */
    capture_reset();
    TERM_init(&t, capture_write, NULL, NULL);
    TERM_move_to(&t, 0, 0);
    CHECK_STR(out, "\x1b[1;1H");

    capture_reset();
    TERM_move_to(&t, 9, 21);
    CHECK_STR(out, "\x1b[10;22H");

    capture_reset();
    TERM_clear_screen(&t);
    CHECK_STR(out, "\x1b[H\x1b[2J");

    capture_reset();
    TERM_enter(&t);
    CHECK(strstr(out, "\x1b[?1049h") != NULL);
    CHECK(strstr(out, "\x1b[?25l") != NULL);

    capture_reset();
    TERM_exit(&t);
    CHECK(strstr(out, "\x1b[?1049l") != NULL);
    CHECK(strstr(out, "\x1b[?25h") != NULL);

    /* --- Full render --- */
    {
        static char buf[64];
        static size_t lines[8];
        ED_Doc doc;
        ED_View view;

        ED_init(&doc, buf, sizeof(buf), lines, 8);
        ED_set_text(&doc, "hi\nthere", 8);
        EDV_init(&view, 5, 10);
        EDV_ensure_cursor_visible(&view, &doc);

        capture_reset();
        TERM_render(&t, &doc, &view, "STATUS");
        CHECK(strstr(out, "hi\r\n") != NULL);
        CHECK(strstr(out, "there") != NULL);
        CHECK(strstr(out, "\x1b[7m") != NULL);   /* status reverse video */
        CHECK(strstr(out, "STATUS") != NULL);
        CHECK(strstr(out, "\x1b[0m") != NULL);
        /* Cursor ends at document position (0,0) -> row 1, col 1. */
        CHECK(strstr(out, "\x1b[1;1H") != NULL);
    }

    /* --- Key decoding: plain characters --- */
    test_single_key(TERM_KEY_ENTER, "\r", 1);
    test_single_key(TERM_KEY_ENTER, "\n", 1);
    test_single_key(TERM_KEY_BACKSPACE, "\x7f", 1);
    test_single_key(TERM_KEY_BACKSPACE, "\x08", 1);
    test_single_key(TERM_KEY_TAB, "\t", 1);
    test_single_key(TERM_KEY_CTRL_W, "\x17", 1);
    test_single_key(TERM_KEY_CTRL_X, "\x18", 1);

    /* CSI cursor keys. */
    test_single_key(TERM_KEY_UP, "\x1b[A", 3);
    test_single_key(TERM_KEY_DOWN, "\x1b[B", 3);
    test_single_key(TERM_KEY_RIGHT, "\x1b[C", 3);
    test_single_key(TERM_KEY_LEFT, "\x1b[D", 3);
    test_single_key(TERM_KEY_HOME, "\x1b[H", 3);
    test_single_key(TERM_KEY_END, "\x1b[F", 3);

    /* SS3 cursor keys. */
    test_single_key(TERM_KEY_UP, "\x1bOA", 3);
    test_single_key(TERM_KEY_DOWN, "\x1bOB", 3);

    /* Tilde sequences. */
    test_single_key(TERM_KEY_HOME, "\x1b[1~", 4);
    test_single_key(TERM_KEY_DELETE, "\x1b[3~", 4);
    test_single_key(TERM_KEY_END, "\x1b[4~", 4);
    test_single_key(TERM_KEY_PAGE_UP, "\x1b[5~", 4);
    test_single_key(TERM_KEY_PAGE_DOWN, "\x1b[6~", 4);

    /* Printable characters come through as TERM_KEY_CHAR. */
    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, "abc", 3, keys, &nkeys);
    CHECK(nkeys == 3);
    CHECK(keys[0].type == TERM_KEY_CHAR && keys[0].ch == 'a');
    CHECK(keys[1].type == TERM_KEY_CHAR && keys[1].ch == 'b');
    CHECK(keys[2].type == TERM_KEY_CHAR && keys[2].ch == 'c');

    /* Escape sequences produce no key until complete. */
    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, "\x1b[", 2, keys, &nkeys);
    CHECK(nkeys == 0);
    feed(&t, "A", 1, keys, &nkeys);
    CHECK(nkeys == 1);
    CHECK(keys[0].type == TERM_KEY_UP);

    /* Interleaved plain input and escape sequences. */
    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, "x\x1b[D" "y", 5, keys, &nkeys);
    CHECK(nkeys == 3);
    CHECK(keys[0].type == TERM_KEY_CHAR && keys[0].ch == 'x');
    CHECK(keys[1].type == TERM_KEY_LEFT);
    CHECK(keys[2].type == TERM_KEY_CHAR && keys[2].ch == 'y');

    /* Unrecognised sequence recovers to ground state. */
    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, "\x1b[Za", 4, keys, &nkeys);
    CHECK(nkeys == 2);
    CHECK(keys[0].type == TERM_KEY_NONE);
    CHECK(keys[1].type == TERM_KEY_CHAR && keys[1].ch == 'a');

    /* Control bytes other than the mapped ones are ignored. */
    TERM_init(&t, capture_write, NULL, NULL);
    feed(&t, "\x01\x02", 2, keys, &nkeys);
    CHECK(nkeys == 2);
    CHECK(keys[0].type == TERM_KEY_NONE);
    CHECK(keys[1].type == TERM_KEY_NONE);

    TEST_SUMMARY();
}
