/*
 * editor_view.c - Viewport and scroll state for the editor.
 *
 * Pure geometry: given the document and terminal size, decide which
 * lines are on screen and where the hardware cursor goes. Tabs expand
 * to the next multiple of EDITOR_TAB_WIDTH for display purposes only.
 */
#include "editor_config.h"
#include "editor_view.h"

void EDV_init(ED_View *v, size_t rows, size_t cols)
{
    if (v == NULL) {
        return;
    }
    v->rows = (rows == 0) ? EDITOR_TERM_ROWS : rows;
    v->cols = (cols == 0) ? EDITOR_TERM_COLS : cols;
    v->top = 0;
    v->left = 0;
}

void EDV_set_size(ED_View *v, size_t rows, size_t cols)
{
    if (v == NULL) {
        return;
    }
    v->rows = (rows == 0) ? 1 : rows;
    v->cols = (cols == 0) ? 1 : cols;
}

size_t EDV_text_rows(const ED_View *v)
{
    if (v == NULL || v->rows <= 1) {
        return 1;   /* always leave room for the status line */
    }
    return v->rows - 1;
}

size_t EDV_text_cols(const ED_View *v)
{
    if (v == NULL || v->cols == 0) {
        return 1;
    }
    return v->cols;
}

size_t EDV_display_col(const ED_Doc *doc, size_t row, size_t char_off)
{
    const char *text = ED_line_text(doc, row);
    size_t len = ED_line_len(doc, row);
    size_t i, col = 0;

    if (char_off > len) {
        char_off = len;
    }
    for (i = 0; i < char_off; i++) {
        if (text[i] == '\t') {
            col += EDITOR_TAB_WIDTH - (col % EDITOR_TAB_WIDTH);
        } else {
            col++;
        }
    }
    return col;
}

void EDV_ensure_cursor_visible(ED_View *v, const ED_Doc *doc)
{
    size_t row, col, disp;

    if (v == NULL || doc == NULL) {
        return;
    }
    ED_get_cursor(doc, &row, &col);
    disp = EDV_display_col(doc, row, col);

    if (row < v->top) {
        v->top = row;
    } else if (row >= v->top + EDV_text_rows(v)) {
        v->top = row - EDV_text_rows(v) + 1;
    }

    if (disp < v->left) {
        v->left = disp;
    } else if (disp >= v->left + EDV_text_cols(v)) {
        v->left = disp - EDV_text_cols(v) + 1;
    }
}

void EDV_visible_range(const ED_View *v, const ED_Doc *doc,
                       size_t *first, size_t *last)
{
    size_t end;

    if (v == NULL || doc == NULL) {
        if (first != NULL) {
            *first = 0;
        }
        if (last != NULL) {
            *last = 0;
        }
        return;
    }
    end = v->top + EDV_text_rows(v);
    if (end > ED_line_count(doc)) {
        end = ED_line_count(doc);
    }
    if (first != NULL) {
        *first = v->top;
    }
    if (last != NULL) {
        *last = end;
    }
}

void EDV_cursor_screen_pos(const ED_View *v, const ED_Doc *doc,
                           size_t *row, size_t *col)
{
    size_t r, c;

    if (v == NULL || doc == NULL) {
        if (row != NULL) {
            *row = 0;
        }
        if (col != NULL) {
            *col = 0;
        }
        return;
    }
    ED_get_cursor(doc, &r, &c);
    if (row != NULL) {
        *row = (r >= v->top) ? r - v->top : 0;
    }
    if (col != NULL) {
        size_t disp = EDV_display_col(doc, r, c);
        *col = (disp >= v->left) ? disp - v->left : 0;
    }
}
