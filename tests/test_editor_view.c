/*
 * test_editor_view.c - Unit tests for viewport/scroll calculations.
 */
#include <string.h>

#include "editor_config.h"
#include "editor_view.h"
#include "test_common.h"

static char   buf[256];
static size_t lines[32];
static ED_Doc doc;
static ED_View view;

static void setup(const char *text, size_t rows, size_t cols)
{
    ED_init(&doc, buf, sizeof(buf), lines, 32);
    ED_set_text(&doc, text, strlen(text));
    EDV_init(&view, rows, cols);
}

int main(void)
{
    /* Defaults come from editor_config.h when 0 is passed. */
    EDV_init(&view, 0, 0);
    CHECK(view.rows == EDITOR_TERM_ROWS);
    CHECK(view.cols == EDITOR_TERM_COLS);

    setup("a\nb\nc\nd\ne\nf", 5, 20);
    CHECK(EDV_text_rows(&view) == 4);   /* one row reserved for status */
    CHECK(EDV_text_cols(&view) == 20);

    /* Visible range with no scrolling. */
    {
        size_t first = 99, last = 99;

        EDV_visible_range(&view, &doc, &first, &last);
        CHECK(first == 0);
        CHECK(last == 4);               /* only text_rows rows are visible */
    }

    /* Cursor below the window scrolls down. */
    for (int i = 0; i < 5; i++) {
        ED_move_down(&doc);
    }
    EDV_ensure_cursor_visible(&view, &doc);
    CHECK(view.top == 2);               /* rows 2..5 visible (4 rows) */
    {
        size_t first = 99, last = 99;
        size_t sr = 99, sc = 99;

        EDV_visible_range(&view, &doc, &first, &last);
        CHECK(first == 2);
        CHECK(last == 6);               /* clamped to the line count */
        EDV_cursor_screen_pos(&view, &doc, &sr, &sc);
        CHECK(sr == 3);                 /* last text row */
        CHECK(sc == 0);
    }

    /* Cursor above the window scrolls up. */
    ED_move_page_up(&doc, 99);
    EDV_ensure_cursor_visible(&view, &doc);
    CHECK(view.top == 0);
    {
        size_t sr = 99, sc = 99;

        EDV_cursor_screen_pos(&view, &doc, &sr, &sc);
        CHECK(sr == 0);
    }

    /* Horizontal scroll when the cursor is beyond the right edge. */
    setup("abcdefghijklmnopqrstuvwxyz", 5, 10);
    for (int i = 0; i < 20; i++) {
        ED_move_right(&doc);
    }
    EDV_ensure_cursor_visible(&view, &doc);
    CHECK(view.left == 11);             /* col 20 visible in cols 11..20 */
    {
        size_t sr = 99, sc = 99;

        EDV_cursor_screen_pos(&view, &doc, &sr, &sc);
        CHECK(sc == 9);                 /* rightmost text column */
    }

    /* Tabs expand to tab stops. */
    setup("a\tb", 5, 20);
    CHECK(EDV_display_col(&doc, 0, 0) == 0);
    CHECK(EDV_display_col(&doc, 0, 1) == 1);
    CHECK(EDV_display_col(&doc, 0, 2) == 1 + EDITOR_TAB_WIDTH -
                                        (1 % EDITOR_TAB_WIDTH));
    CHECK(EDV_display_col(&doc, 0, 3) == EDITOR_TAB_WIDTH + 1);

    /* Rows/cols of 0 or 1 are clamped to something usable. */
    EDV_set_size(&view, 1, 0);
    CHECK(EDV_text_rows(&view) == 1);
    CHECK(EDV_text_cols(&view) == 1);

    TEST_SUMMARY();
}
