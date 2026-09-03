/*
 * editor_core.h - Platform-neutral text buffer and editing logic.
 *
 * This module contains no Zephyr, OS, or I/O dependencies so it can be
 * unit tested on any host. The document is stored as a flat byte buffer
 * with an index of line start offsets; all storage is provided by the
 * caller, so no dynamic allocation is required.
 */
#ifndef EDITOR_CORE_H
#define EDITOR_CORE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ED_OK = 0,          /* success */
    ED_FULL,            /* buffer or line index capacity reached */
    ED_RANGE,           /* argument outside the document */
    ED_INVALID          /* invalid argument / not initialised */
} ED_Result;

typedef struct {
    char    *buf;         /* caller-provided text storage */
    size_t   buf_cap;     /* capacity of buf in bytes */
    size_t   len;         /* bytes currently used in buf */
    size_t  *lines;       /* caller-provided line start offsets */
    size_t   lines_cap;   /* capacity of lines */
    size_t   line_count;  /* number of lines in the document */
    size_t   cur;         /* cursor as an absolute buffer index */
    size_t   goal_col;    /* remembered column for vertical movement */
    bool     dirty;       /* modified since last load/save */
} ED_Doc;

/* Bind caller-provided storage and reset to an empty document. */
void ED_init(ED_Doc *doc, char *buf, size_t buf_cap,
             size_t *lines, size_t lines_cap);

/* Replace the document with the given bytes (line endings are
 * normalised: CRLF and lone CR become LF). */
ED_Result ED_set_text(ED_Doc *doc, const char *data, size_t len);

/* Current document contents (bytes are in doc->buf, length returned). */
size_t ED_get_text(const ED_Doc *doc);

/* Editing operations. All update the line index and the dirty flag. */
ED_Result ED_insert_char(ED_Doc *doc, char ch);
ED_Result ED_insert_text(ED_Doc *doc, const char *s, size_t len);
ED_Result ED_newline(ED_Doc *doc);
ED_Result ED_backspace(ED_Doc *doc);    /* delete before cursor */
ED_Result ED_delete(ED_Doc *doc);       /* delete at cursor */

/* Cursor movement. */
void ED_cursor_home(ED_Doc *doc);       /* start of line */
void ED_cursor_end(ED_Doc *doc);        /* end of line */
void ED_move_left(ED_Doc *doc);
void ED_move_right(ED_Doc *doc);
void ED_move_up(ED_Doc *doc);
void ED_move_down(ED_Doc *doc);
void ED_move_page_up(ED_Doc *doc, size_t page_rows);
void ED_move_page_down(ED_Doc *doc, size_t page_rows);

/* Cursor and line queries. */
void         ED_get_cursor(const ED_Doc *doc, size_t *row, size_t *col);
size_t       ED_line_count(const ED_Doc *doc);
size_t       ED_line_len(const ED_Doc *doc, size_t row);
const char  *ED_line_text(const ED_Doc *doc, size_t row);
bool         ED_is_dirty(const ED_Doc *doc);
void         ED_clear_dirty(ED_Doc *doc);

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_CORE_H */
