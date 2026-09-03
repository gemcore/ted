/*
 * editor_session.h - One interactive editing run.
 *
 * Ties together the editor core, view, VT100 terminal, and filesystem:
 * loads the file, runs the event loop (draw, read key, apply action),
 * saves on request, and restores the terminal on exit. All storage is
 * supplied by the caller through ED_SessionConfig.
 */
#ifndef EDITOR_SESSION_H
#define EDITOR_SESSION_H

#include <stddef.h>
#include "editor_config.h"
#include "editor_core.h"
#include "editor_view.h"
#include "term_vt100.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ED_SESSION_OK = 0,      /* quit, file saved or unchanged */
    ED_SESSION_DISCARDED,   /* quit without saving changes */
    ED_SESSION_IO_ERROR,    /* load/save failed */
    ED_SESSION_ARG_ERROR    /* bad configuration */
} ED_SessionResult;

typedef struct {
    const char *path;                 /* file to edit (required) */
    TERM       *term;                 /* initialised terminal (required) */
    char       *buf;                  /* text storage (required) */
    size_t      buf_cap;
    size_t     *lines;                /* line index storage (required) */
    size_t      lines_cap;
    size_t      term_rows;            /* terminal size, 0 = defaults */
    size_t      term_cols;
} ED_SessionConfig;

/* Run the editor until the user quits. Blocks reading keys from
 * cfg->term. The terminal is put into/taken out of editor mode by this
 * call. */
ED_SessionResult ED_Session_run(const ED_SessionConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_SESSION_H */
