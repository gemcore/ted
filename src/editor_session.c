/*
 * editor_session.c - Interactive editing session.
 *
 * Owns the event loop for one edit run: load file, draw, read key, apply
 * action, repeat until save or quit. Keys handled:
 *   printable chars  insert        Backspace  delete before cursor
 *   Enter            split line     Delete     delete at cursor
 *   Tab              insert spaces  arrows     move cursor
 *   Home/End         line start/end PgUp/PgDn  move by a page
 *   Ctrl-W           write          Ctrl-X     exit
 */
#include <stdio.h>

#include "editor_config.h"
#include "editor_session.h"
#include "fs_lfs.h"

static void set_status(char *status, size_t cap, const char *path,
                       const ED_Doc *doc, const char *msg)
{
    size_t row, col;

    ED_get_cursor(doc, &row, &col);
    snprintf(status, cap, "%s%s | Ln %zu, Col %zu | ^W save ^X quit%s%s",
             path != NULL ? path : "(no name)",
             ED_is_dirty(doc) ? " [+]" : "",
             row + 1, col + 1,
             msg != NULL ? " | " : "",
             msg != NULL ? msg : "");
}

/* Apply one editing key. Returns the message to show in the status line
 * (or NULL) after the action. */
static const char *apply_key(ED_Doc *doc, ED_View *view,
                             const TERM_Key *key, const char *path,
                             bool *quit)
{
    switch (key->type) {
    case TERM_KEY_CHAR:
        if (ED_insert_char(doc, key->ch) != ED_OK) {
            return "buffer full";
        }
        break;
    case TERM_KEY_TAB: {
        size_t row, col, i, spaces;

        ED_get_cursor(doc, &row, &col);
        spaces = EDITOR_TAB_WIDTH - (EDV_display_col(doc, row, col) %
                                     EDITOR_TAB_WIDTH);
        for (i = 0; i < spaces; i++) {
            if (ED_insert_char(doc, ' ') != ED_OK) {
                return "buffer full";
            }
        }
        break;
    }
    case TERM_KEY_ENTER:
        if (ED_newline(doc) != ED_OK) {
            return "buffer full";
        }
        break;
    case TERM_KEY_BACKSPACE:   ED_backspace(doc);   break;
    case TERM_KEY_DELETE:      ED_delete(doc);      break;
    case TERM_KEY_UP:          ED_move_up(doc);     break;
    case TERM_KEY_DOWN:        ED_move_down(doc);   break;
    case TERM_KEY_LEFT:        ED_move_left(doc);   break;
    case TERM_KEY_RIGHT:       ED_move_right(doc);  break;
    case TERM_KEY_HOME:        ED_cursor_home(doc); break;
    case TERM_KEY_END:         ED_cursor_end(doc);  break;
    case TERM_KEY_PAGE_UP:
        ED_move_page_up(doc, EDV_text_rows(view));
        break;
    case TERM_KEY_PAGE_DOWN:
        ED_move_page_down(doc, EDV_text_rows(view));
        break;
    case TERM_KEY_CTRL_W:
        if (FS_LFS_save(path, doc->buf, ED_get_text(doc)) == FS_LFS_OK) {
            ED_clear_dirty(doc);
            return "saved";
        }
        return "save failed";
    case TERM_KEY_CTRL_X:
        *quit = true;
        break;
    case TERM_KEY_NONE:
    default:
        break;
    }
    return NULL;
}

ED_SessionResult ED_Session_run(const ED_SessionConfig *cfg)
{
    ED_Doc doc;
    ED_View view;
    TERM_Key key;
    bool quit = false;
    char status[EDITOR_STATUS_MAX];
    const char *msg = NULL;
    size_t loaded_len = 0;
    FS_LFS_Result frc;

    if (cfg == NULL || cfg->path == NULL || cfg->term == NULL ||
        cfg->buf == NULL || cfg->lines == NULL || cfg->buf_cap == 0 ||
        cfg->lines_cap == 0) {
        return ED_SESSION_ARG_ERROR;
    }

    ED_init(&doc, cfg->buf, cfg->buf_cap, cfg->lines, cfg->lines_cap);
    EDV_init(&view, cfg->term_rows, cfg->term_cols);

    frc = FS_LFS_load(cfg->path, cfg->buf, cfg->buf_cap, &loaded_len);
    switch (frc) {
    case FS_LFS_OK:
        ED_set_text(&doc, cfg->buf, loaded_len);
        break;
    case FS_LFS_NOT_FOUND:
        ED_set_text(&doc, NULL, 0);     /* start a new, empty file */
        msg = "new file";
        break;
    case FS_LFS_TOO_LARGE:
        msg = "file truncated";         /* buffer holds what fits */
        ED_set_text(&doc, cfg->buf, loaded_len);
        break;
    default:
        return ED_SESSION_IO_ERROR;
    }

    TERM_enter(cfg->term);

    while (!quit) {
        EDV_ensure_cursor_visible(&view, &doc);
        set_status(status, sizeof(status), cfg->path, &doc, msg);
        TERM_render(cfg->term, &doc, &view, status);

        msg = NULL;
        if (!TERM_read_key(cfg->term, &key)) {
            break;  /* input stream ended: leave the editor */
        }
        if (key.type == TERM_KEY_NONE) {
            continue;
        }
        msg = apply_key(&doc, &view, &key, cfg->path, &quit);
    }

    TERM_exit(cfg->term);

    return ED_is_dirty(&doc) ? ED_SESSION_DISCARDED : ED_SESSION_OK;
}
