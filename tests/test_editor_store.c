/* Unit tests for the bounded piece-table store. */
#include <string.h>

#include "editor_store.h"
#include "test_common.h"

#define PIECES 16
#define ADD_CAP 32
#define CACHE_CAP 4
#define ANCHORS 4

static const char source[] = "one\ntwo\nthree";
static ED_StorePiece pieces[PIECES];
static char added[ADD_CAP];
static char cache[CACHE_CAP];
static ED_StoreLineAnchor anchors[ANCHORS];
static ED_Store store;

static void check_text(const char *expected);
static void setup_source(const char *text);

static size_t read_source(void *ctx, size_t offset, char *dst, size_t len)
{
    const char *text = (const char *)ctx;
    size_t text_len = strlen(text);

    if (offset >= text_len) {
        return 0;
    }
    if (len > text_len - offset) {
        len = text_len - offset;
    }
    memcpy(dst, text + offset, len);
    return len;
}

static void setup(void)
{
    setup_source(source);
}

static void setup_source(const char *text)
{
    memset(pieces, 0, sizeof(pieces));
    ED_Store_init(&store, read_source, (void *)text, strlen(text),
                  pieces, PIECES, added, sizeof(added), cache, sizeof(cache),
                  anchors, ANCHORS);
}

static void test_crlf_lines_and_delete(void)
{
    static const char text[] = "one\r\ntwo\r\nthree";
    size_t start, len, row, col;

    setup_source(text);
    CHECK(ED_Store_line_count(&store) == 3);
    CHECK(ED_Store_line_bounds(&store, 1, &start, &len) == ED_STORE_OK);
    CHECK(start == 5 && len == 3);
    CHECK(ED_Store_position_at(&store, 5, &row, &col) == ED_STORE_OK);
    CHECK(row == 1 && col == 0);
    CHECK(ED_Store_delete(&store, 3, 2) == ED_STORE_OK);
    check_text("onetwo\r\nthree");
    CHECK(ED_Store_line_count(&store) == 2);
}

static void test_deleted_edit_memory_is_reused(void)
{
    size_t i;

    setup();
    for (i = 0; i < 40; i++) {
        CHECK(ED_Store_insert(&store, 0, "x", 1) == ED_STORE_OK);
        CHECK(ED_Store_delete(&store, 0, 1) == ED_STORE_OK);
    }
    check_text(source);
}

static void check_text(const char *expected)
{
    char actual[64];
    size_t len = ED_Store_length(&store);

    CHECK(len == strlen(expected));
    CHECK(ED_Store_read(&store, 0, actual, len) == ED_STORE_OK);
    CHECK(memcmp(actual, expected, len) == 0);
}

static void test_read_and_lines(void)
{
    size_t start, len;

    setup();
    check_text(source);
    CHECK(ED_Store_line_count(&store) == 3);
    CHECK(ED_Store_line_bounds(&store, 1, &start, &len) == ED_STORE_OK);
    CHECK(start == 4 && len == 3);
}

static void test_insert_delete_and_cache_invalidation(void)
{
    setup();
    CHECK(ED_Store_insert(&store, 4, "new\n", 4) == ED_STORE_OK);
    check_text("one\nnew\ntwo\nthree");
    CHECK(ED_Store_line_count(&store) == 4);
    CHECK(ED_Store_delete(&store, 4, 4) == ED_STORE_OK);
    check_text(source);
    CHECK(ED_Store_line_count(&store) == 3);
}

static void test_bounds_and_capacity(void)
{
    setup();
    CHECK(ED_Store_delete(&store, 0, ED_Store_length(&store) + 1) ==
          ED_STORE_RANGE);
    CHECK(ED_Store_insert(&store, ED_Store_length(&store) + 1, "x", 1) ==
          ED_STORE_RANGE);
    CHECK(ED_Store_insert(&store, 0, "012345678901234567890123456789012", 33) ==
          ED_STORE_FULL);
}

int main(void)
{
    test_read_and_lines();
    test_insert_delete_and_cache_invalidation();
    test_bounds_and_capacity();
    test_crlf_lines_and_delete();
    test_deleted_edit_memory_is_reused();
    TEST_SUMMARY();
}