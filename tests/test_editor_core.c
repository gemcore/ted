/*
 * test_editor_core.c - Unit tests for the platform-neutral editor core.
 */
#include <string.h>

#include "editor_core.h"
#include "test_common.h"

#define CAP   64
#define LINES 8

static char   buf[CAP];
static size_t lines[LINES];
static ED_Doc doc;

static void setup(void)
{
    ED_init(&doc, buf, CAP, lines, LINES);
}

static void setup_text(const char *text)
{
    setup();
    ED_set_text(&doc, text, strlen(text));
}

static void check_cursor(size_t row, size_t col)
{
    size_t r = 999, c = 999;

    ED_get_cursor(&doc, &r, &c);
    CHECK(r == row);
    CHECK(c == col);
}

static void test_empty_doc(void)
{
    setup();
    CHECK(ED_line_count(&doc) == 1);
    CHECK(ED_line_len(&doc, 0) == 0);
    CHECK(ED_get_text(&doc) == 0);
    CHECK(!ED_is_dirty(&doc));
    check_cursor(0, 0);
}

static void test_load_multiline(void)
{
    setup_text("hello\nworld\n");
    CHECK(ED_line_count(&doc) == 3);
    CHECK(ED_line_len(&doc, 0) == 5);
    CHECK(ED_line_len(&doc, 1) == 5);
    CHECK(ED_line_len(&doc, 2) == 0);
    CHECK(memcmp(ED_line_text(&doc, 1), "world", 5) == 0);
    CHECK(!ED_is_dirty(&doc));          /* load is not an edit */
}

static void test_load_normalises_crlf(void)
{
    setup_text("a\r\nb\rc\n");
    CHECK(ED_get_text(&doc) == 6);
    CHECK(memcmp(buf, "a\nb\nc\n", 6) == 0);
    CHECK(ED_line_count(&doc) == 4);
}

static void test_insert_and_dirty(void)
{
    setup();
    CHECK(ED_insert_char(&doc, 'a') == ED_OK);
    CHECK(ED_insert_char(&doc, 'b') == ED_OK);
    CHECK(ED_get_text(&doc) == 2);
    CHECK(memcmp(buf, "ab", 2) == 0);
    CHECK(ED_is_dirty(&doc));
    check_cursor(0, 2);
}

static void test_insert_into_middle_of_line(void)
{
    setup_text("ac");
    ED_move_right(&doc);
    CHECK(ED_insert_char(&doc, 'b') == ED_OK);
    CHECK(memcmp(buf, "abc", 3) == 0);
    check_cursor(0, 2);
}

static void test_newline_splits_line(void)
{
    setup_text("abcd");
    ED_move_right(&doc);
    ED_move_right(&doc);
    CHECK(ED_newline(&doc) == ED_OK);
    CHECK(ED_line_count(&doc) == 2);
    CHECK(ED_line_len(&doc, 0) == 2);
    CHECK(ED_line_len(&doc, 1) == 2);
    CHECK(memcmp(buf, "ab\ncd", 5) == 0);
    check_cursor(1, 0);
}

static void test_backspace_joins_lines(void)
{
    setup_text("ab\ncd");
    ED_move_down(&doc);                 /* cursor at start of "cd" */
    check_cursor(1, 0);
    CHECK(ED_backspace(&doc) == ED_OK);
    CHECK(ED_line_count(&doc) == 1);
    CHECK(memcmp(buf, "abcd", 4) == 0);
    check_cursor(0, 2);
}

static void test_delete_at_cursor(void)
{
    setup_text("abc");
    CHECK(ED_delete(&doc) == ED_OK);
    CHECK(memcmp(buf, "bc", 2) == 0);
    check_cursor(0, 0);
}

static void test_backspace_at_start_is_range_error(void)
{
    setup_text("abc");
    CHECK(ED_backspace(&doc) == ED_RANGE);
    CHECK(ED_get_text(&doc) == 3);
}

static void test_delete_at_end_is_range_error(void)
{
    setup_text("abc");
    ED_cursor_end(&doc);
    CHECK(ED_delete(&doc) == ED_RANGE);
    CHECK(ED_get_text(&doc) == 3);
}

static void test_buffer_full(void)
{
    setup();
    for (int i = 0; i < (int)CAP; i++) {
        CHECK(ED_insert_char(&doc, 'x') == ED_OK);
    }
    CHECK(ED_insert_char(&doc, 'y') == ED_FULL);
    CHECK(ED_get_text(&doc) == CAP);
}

static void test_line_index_full(void)
{
    setup();
    for (int i = 0; i < (int)LINES - 1; i++) {
        CHECK(ED_newline(&doc) == ED_OK);
    }
    CHECK(ED_line_count(&doc) == LINES);
    CHECK(ED_newline(&doc) == ED_FULL);
    CHECK(ED_line_count(&doc) == LINES);
}

static void test_cursor_movement_basic(void)
{
    setup_text("ab\ncd\nef");
    ED_move_right(&doc);
    check_cursor(0, 1);
    ED_move_down(&doc);
    check_cursor(1, 1);
    ED_move_left(&doc);
    check_cursor(1, 0);
    ED_move_up(&doc);
    check_cursor(0, 0);
    ED_move_up(&doc);                   /* already at top: no-op */
    check_cursor(0, 0);
}

static void test_left_right_cross_line_boundaries(void)
{
    setup_text("ab\ncd");
    ED_cursor_end(&doc);                /* end of "ab" */
    check_cursor(0, 2);
    ED_move_right(&doc);                /* over the '\n' */
    check_cursor(1, 0);
    ED_move_left(&doc);                 /* back over the '\n' */
    check_cursor(0, 2);
}

static void test_goal_column_preserved(void)
{
    setup_text("abcd\nx\nabcde");
    ED_move_right(&doc);
    ED_move_right(&doc);
    ED_move_right(&doc);                /* col 3 */
    ED_move_down(&doc);                 /* clamps to col 1 on "x" */
    check_cursor(1, 1);
    ED_move_down(&doc);                 /* goal col 3 fits on "abcde" */
    check_cursor(2, 3);
    ED_move_up(&doc);                   /* clamps again, goal kept */
    check_cursor(1, 1);
}

static void test_home_end(void)
{
    setup_text("hello\nworld");
    ED_move_down(&doc);
    ED_cursor_end(&doc);
    check_cursor(1, 5);
    ED_cursor_home(&doc);
    check_cursor(1, 0);
}

static void test_page_movement(void)
{
    setup_text("l0\nl1\nl2\nl3\nl4\nl5");
    ED_move_page_down(&doc, 4);
    check_cursor(4, 0);
    ED_move_page_down(&doc, 4);         /* clamps to last line */
    check_cursor(5, 0);
    ED_move_page_up(&doc, 2);
    check_cursor(3, 0);
    ED_move_page_up(&doc, 99);          /* clamps to first line */
    check_cursor(0, 0);
}

static void test_insert_text(void)
{
    setup();
    CHECK(ED_insert_text(&doc, "one\ntwo", 7) == ED_OK);
    CHECK(ED_line_count(&doc) == 2);
    check_cursor(1, 3);
}

static void test_clear_dirty(void)
{
    setup_text("x");
    ED_insert_char(&doc, 'y');
    CHECK(ED_is_dirty(&doc));
    ED_clear_dirty(&doc);
    CHECK(!ED_is_dirty(&doc));
}

int main(void)
{
    test_empty_doc();
    test_load_multiline();
    test_load_normalises_crlf();
    test_insert_and_dirty();
    test_insert_into_middle_of_line();
    test_newline_splits_line();
    test_backspace_joins_lines();
    test_delete_at_cursor();
    test_backspace_at_start_is_range_error();
    test_delete_at_end_is_range_error();
    test_buffer_full();
    test_line_index_full();
    test_cursor_movement_basic();
    test_left_right_cross_line_boundaries();
    test_goal_column_preserved();
    test_home_end();
    test_page_movement();
    test_insert_text();
    test_clear_dirty();
    TEST_SUMMARY();
}
