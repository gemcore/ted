/*
 * editor_core.c - Platform-neutral text buffer and editing logic.
 *
 * Flat byte buffer plus an index of line start offsets. The cursor is an
 * absolute buffer index; row/col are derived from the line index. No
 * dynamic allocation and no I/O, so the module is fully unit testable.
 */
#include <string.h>

#include "editor_config.h"
#include "editor_core.h"

static bool ed_valid(const ED_Doc *doc)
{
    return doc != NULL && doc->buf != NULL && doc->lines != NULL &&
           doc->lines_cap > 0;
}

/* Byte offset where line row starts. */
static size_t ed_line_start(const ED_Doc *doc, size_t row)
{
    return (row < doc->line_count) ? doc->lines[row] : doc->len;
}

/* Byte offset where line row ends, excluding the '\n'. */
static size_t ed_line_end(const ED_Doc *doc, size_t row)
{
    size_t start = ed_line_start(doc, row);
    size_t end;

    if (row + 1 < doc->line_count) {
        end = doc->lines[row + 1];
        if (end > start && doc->buf[end - 1] == '\n') {
            end--;
        }
    } else {
        end = doc->len;
    }
    return end;
}

static size_t ed_cur_row(const ED_Doc *doc)
{
    size_t row = 0;

    while (row + 1 < doc->line_count && doc->lines[row + 1] <= doc->cur) {
        row++;
    }
    return row;
}

/* Buffer index of (row, col), clamped to the line length. */
static size_t ed_pos_to_index(const ED_Doc *doc, size_t row, size_t col)
{
    size_t start = ed_line_start(doc, row);
    size_t end = ed_line_end(doc, row);
    size_t idx = start + col;

    return (idx > end) ? end : idx;
}

/* Insert raw bytes at the cursor, maintaining the line index. */
static ED_Result ed_insert(ED_Doc *doc, const char *data, size_t n)
{
    size_t newlines = 0;
    size_t i, row;

    if (!ed_valid(doc) || (data == NULL && n > 0)) {
        return ED_INVALID;
    }
    for (i = 0; i < n; i++) {
        if (data[i] == '\n') {
            newlines++;
        }
    }
    if (doc->len + n > doc->buf_cap) {
        return ED_FULL;
    }
    if (doc->line_count + newlines > doc->lines_cap) {
        return ED_FULL;
    }

    memmove(doc->buf + doc->cur + n, doc->buf + doc->cur, doc->len - doc->cur);
    if (n > 0) {
        memcpy(doc->buf + doc->cur, data, n);
    }
    doc->len += n;

    if (newlines > 0) {
        size_t base = doc->cur;

        row = ed_cur_row(doc);
        memmove(doc->lines + row + 1 + newlines, doc->lines + row + 1,
                (doc->line_count - (row + 1)) * sizeof(size_t));
        for (i = row + 1 + newlines; i < doc->line_count + newlines; i++) {
            doc->lines[i] += n;
        }
        for (i = 0; i < n; i++) {
            if (doc->buf[base + i] == '\n') {
                doc->lines[row + 1] = base + i + 1;
                row++;
            }
        }
        doc->line_count += newlines;
    } else {
        /* Shift line starts after the cursor. */
        row = ed_cur_row(doc);
        for (i = row + 1; i < doc->line_count; i++) {
            doc->lines[i] += n;
        }
    }

    doc->cur += n;
    doc->dirty = true;
    return ED_OK;
}

/* Delete n bytes at the cursor, maintaining the line index. */
static ED_Result ed_delete_at(ED_Doc *doc, size_t cur, size_t n)
{
    size_t row, i, end, removed_lines = 0;

    if (!ed_valid(doc) || n == 0 || cur >= doc->len) {
        return ED_RANGE;
    }
    if (cur + n > doc->len) {
        n = doc->len - cur;
    }

    row = 0;
    while (row + 1 < doc->line_count && doc->lines[row + 1] <= cur) {
        row++;
    }
    end = cur + n;
    for (i = row + 1; i < doc->line_count; i++) {
        if (doc->lines[i] <= end) {
            removed_lines++;    /* whole line start swallowed by delete */
        } else {
            doc->lines[i - removed_lines] = doc->lines[i] - n;
        }
    }
    doc->line_count -= removed_lines;

    memmove(doc->buf + cur, doc->buf + cur + n, doc->len - (cur + n));
    doc->len -= n;
    doc->cur = cur;
    doc->dirty = true;
    return ED_OK;
}

void ED_init(ED_Doc *doc, char *buf, size_t buf_cap,
             size_t *lines, size_t lines_cap)
{
    if (doc == NULL) {
        return;
    }
    doc->buf = buf;
    doc->buf_cap = buf_cap;
    doc->len = 0;
    doc->lines = lines;
    doc->lines_cap = lines_cap;
    doc->line_count = 1;
    if (lines != NULL && lines_cap > 0) {
        lines[0] = 0;
    }
    doc->cur = 0;
    doc->goal_col = 0;
    doc->dirty = false;
}

ED_Result ED_set_text(ED_Doc *doc, const char *data, size_t len)
{
    size_t i, out = 0;

    if (!ed_valid(doc) || (data == NULL && len > 0)) {
        return ED_INVALID;
    }

    doc->len = 0;
    doc->line_count = 1;
    doc->lines[0] = 0;

    /* Copy with line-ending normalisation; stop cleanly at capacity. */
    for (i = 0; i < len; i++) {
        char ch = data[i];

        if (ch == '\r') {
            if (i + 1 < len && data[i + 1] == '\n') {
                i++;    /* CRLF -> LF */
            }
            ch = '\n';
        }
        if (ch == '\n') {
            if (out + 1 > doc->buf_cap ||
                doc->line_count + 1 > doc->lines_cap) {
                break;
            }
            doc->lines[doc->line_count++] = out + 1;
        } else {
            if (out + 1 > doc->buf_cap) {
                break;
            }
        }
        doc->buf[out++] = ch;
    }
    doc->len = out;
    doc->cur = 0;
    doc->goal_col = 0;
    doc->dirty = false;
    return ED_OK;
}

size_t ED_get_text(const ED_Doc *doc)
{
    return ed_valid(doc) ? doc->len : 0;
}

ED_Result ED_insert_char(ED_Doc *doc, char ch)
{
    ED_Result rc = ed_insert(doc, &ch, 1);

    if (rc == ED_OK) {
        size_t row = ed_cur_row(doc);
        doc->goal_col = doc->cur - ed_line_start(doc, row);
    }
    return rc;
}

ED_Result ED_insert_text(ED_Doc *doc, const char *s, size_t len)
{
    ED_Result rc = ed_insert(doc, s, len);

    if (rc == ED_OK) {
        size_t row = ed_cur_row(doc);
        doc->goal_col = doc->cur - ed_line_start(doc, row);
    }
    return rc;
}

ED_Result ED_newline(ED_Doc *doc)
{
    return ED_insert_char(doc, '\n');
}

ED_Result ED_backspace(ED_Doc *doc)
{
    if (!ed_valid(doc)) {
        return ED_INVALID;
    }
    if (doc->cur == 0) {
        return ED_RANGE;
    }
    return ed_delete_at(doc, doc->cur - 1, 1);
}

ED_Result ED_delete(ED_Doc *doc)
{
    if (!ed_valid(doc)) {
        return ED_INVALID;
    }
    if (doc->cur >= doc->len) {
        return ED_RANGE;
    }
    return ed_delete_at(doc, doc->cur, 1);
}

void ED_get_cursor(const ED_Doc *doc, size_t *row, size_t *col)
{
    size_t r;

    if (!ed_valid(doc)) {
        if (row != NULL) {
            *row = 0;
        }
        if (col != NULL) {
            *col = 0;
        }
        return;
    }
    r = ed_cur_row(doc);
    if (row != NULL) {
        *row = r;
    }
    if (col != NULL) {
        *col = doc->cur - ed_line_start(doc, r);
    }
}

void ED_cursor_home(ED_Doc *doc)
{
    if (!ed_valid(doc)) {
        return;
    }
    doc->cur = ed_line_start(doc, ed_cur_row(doc));
    doc->goal_col = 0;
}

void ED_cursor_end(ED_Doc *doc)
{
    size_t row;

    if (!ed_valid(doc)) {
        return;
    }
    row = ed_cur_row(doc);
    doc->cur = ed_line_end(doc, row);
    doc->goal_col = doc->cur - ed_line_start(doc, row);
}

void ED_move_left(ED_Doc *doc)
{
    if (!ed_valid(doc) || doc->cur == 0) {
        return;
    }
    doc->cur--;
    doc->goal_col = doc->cur - ed_line_start(doc, ed_cur_row(doc));
}

void ED_move_right(ED_Doc *doc)
{
    if (!ed_valid(doc) || doc->cur >= doc->len) {
        return;
    }
    doc->cur++;
    doc->goal_col = doc->cur - ed_line_start(doc, ed_cur_row(doc));
}

void ED_move_up(ED_Doc *doc)
{
    size_t row;

    if (!ed_valid(doc)) {
        return;
    }
    row = ed_cur_row(doc);
    if (row == 0) {
        return;
    }
    /* goal_col holds the horizontal target set by the last horizontal
     * movement or edit; vertical moves keep it so repeated up/down
     * presses return to the same column when lines allow. */
    doc->cur = ed_pos_to_index(doc, row - 1, doc->goal_col);
}

void ED_move_down(ED_Doc *doc)
{
    size_t row;

    if (!ed_valid(doc)) {
        return;
    }
    row = ed_cur_row(doc);
    if (row + 1 >= doc->line_count) {
        return;
    }
    doc->cur = ed_pos_to_index(doc, row + 1, doc->goal_col);
}

void ED_move_page_up(ED_Doc *doc, size_t page_rows)
{
    size_t row, col, target;

    if (!ed_valid(doc)) {
        return;
    }
    row = ed_cur_row(doc);
    col = doc->cur - ed_line_start(doc, row);
    target = (row > page_rows) ? row - page_rows : 0;
    doc->cur = ed_pos_to_index(doc, target, col);
    doc->goal_col = col;
}

void ED_move_page_down(ED_Doc *doc, size_t page_rows)
{
    size_t row, col, target;

    if (!ed_valid(doc)) {
        return;
    }
    row = ed_cur_row(doc);
    col = doc->cur - ed_line_start(doc, row);
    target = row + page_rows;
    if (target >= doc->line_count) {
        target = doc->line_count - 1;
    }
    doc->cur = ed_pos_to_index(doc, target, col);
    doc->goal_col = col;
}

size_t ED_line_count(const ED_Doc *doc)
{
    return ed_valid(doc) ? doc->line_count : 0;
}

size_t ED_line_len(const ED_Doc *doc, size_t row)
{
    if (!ed_valid(doc) || row >= doc->line_count) {
        return 0;
    }
    return ed_line_end(doc, row) - ed_line_start(doc, row);
}

const char *ED_line_text(const ED_Doc *doc, size_t row)
{
    if (!ed_valid(doc) || row >= doc->line_count) {
        return "";
    }
    return doc->buf + ed_line_start(doc, row);
}

bool ED_is_dirty(const ED_Doc *doc)
{
    return ed_valid(doc) ? doc->dirty : false;
}

void ED_clear_dirty(ED_Doc *doc)
{
    if (ed_valid(doc)) {
        doc->dirty = false;
    }
}
