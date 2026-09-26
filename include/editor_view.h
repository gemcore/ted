/*
 * editor_view.h - Viewport and scroll state for the editor.
 *
 * Converts ED_Doc state into screen coordinates: which document lines
 * are visible, where the hardware cursor belongs, and how far lines
 * must be horizontally scrolled. No terminal or OS dependencies.
 */
#ifndef EDITOR_VIEW_H
#define EDITOR_VIEW_H

#include <stddef.h>
#include "editor_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t cols;      /* terminal width in columns */
    size_t rows;      /* terminal height in rows (including status row) */
    size_t top;       /* first visible document row */
    size_t left;      /* horizontal scroll offset in columns */
} ED_View;

/* rows is the full terminal height; one row is reserved for the status
 * line, so text_rows = rows - 1 (clamped at 1). */
void   EDV_init(ED_View *v, size_t rows, size_t cols);
void   EDV_set_size(ED_View *v, size_t rows, size_t cols);

size_t EDV_text_rows(const ED_View *v);
size_t EDV_text_cols(const ED_View *v);

/* Adjust scroll offsets so the document cursor is visible. */
void   EDV_ensure_cursor_visible(ED_View *v, const ED_Doc *doc);

/* Visible document row range: [first, last), last clamped to the
 * document line count. */
void   EDV_visible_range(const ED_View *v, const ED_Doc *doc,
                         size_t *first, size_t *last);

/* Screen position of the document cursor (0-based) after scrolling.
 * Requires EDV_ensure_cursor_visible() to have been applied. */
void   EDV_cursor_screen_pos(const ED_View *v, const ED_Doc *doc,
                             size_t *row, size_t *col);

/* Display column of a character offset within a line, expanding tabs. */
size_t EDV_display_col(const ED_Doc *doc, size_t row, size_t char_off);

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_VIEW_H */
