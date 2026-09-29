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
 * Without a file name, Ctrl-W asks for one; Ctrl-X with unsaved changes
 * asks whether to save. Ctrl-C cancels a prompt.
 */
#include <stdio.h>
#include <string.h>

#include "editor_config.h"
#include "editor_session.h"
#include "fs_lfs.h"

static void set_status(char *status, size_t cap, const char *path,
                       const ED_Doc *doc, const char *msg)
{
    size_t row, col;

    ED_get_cursor(doc, &row, &col);
    snprintf(status, cap, "%s%s | Ln %lu, Col %lu | ^W write ^X exit%s%s",
             path[0] != '\0' ? path : "(no name)",
             ED_is_dirty(doc) ? " [+]" : "",
             (unsigned long)(row + 1), (unsigned long)(col + 1),
             msg != NULL ? " | " : "",
             msg != NULL ? msg : "");
}

/* Keys that only move the cursor and never change the text. */
static bool is_motion_key(TERM_KeyType type)
{
    switch (type) {
    case TERM_KEY_UP:
    case TERM_KEY_DOWN:
    case TERM_KEY_LEFT:
    case TERM_KEY_RIGHT:
    case TERM_KEY_HOME:
    case TERM_KEY_END:
    case TERM_KEY_PAGE_UP:
    case TERM_KEY_PAGE_DOWN:
        return true;
    default:
        return false;
    }
}

/* Read a line on the status row. Ctrl-C cancels; empty Enter is ignored. */
static bool prompt_line(TERM *t, const ED_View *v, const char *prompt,
                        char *out, size_t cap)
{
    size_t len = 0;
    TERM_Key key;

    out[0] = '\0';
    for (;;) {
        TERM_hide_cursor(t);
        TERM_move_to(t, EDV_text_rows(v), 0);
        TERM_reverse_video(t);
        TERM_write(t, prompt);
        TERM_write(t, out);
        TERM_clear_line(t);
        TERM_normal_video(t);
        TERM_show_cursor(t);

        if (!TERM_read_key(t, &key)) {
            return false;
        }
        switch (key.type) {
        case TERM_KEY_CTRL_C:
            return false;
        case TERM_KEY_ENTER:
            if (len > 0) {
                return true;
            }
            break;
        case TERM_KEY_BACKSPACE:
            if (len > 0) {
                out[--len] = '\0';
            }
            break;
        case TERM_KEY_CHAR:
            if (len + 1 < cap) {
                out[len++] = key.ch;
                out[len] = '\0';
            }
            break;
        default:
            break;
        }
    }
}

static const char *save_doc(ED_Doc *doc, TERM *t, const ED_View *v,
                            char *path, size_t path_cap)
{
    if (path[0] == '\0' &&
        !prompt_line(t, v, "Write to: ", path, path_cap)) {
        return "cancelled";
    }
    if (FS_LFS_save(path, doc->buf, ED_get_text(doc)) != FS_LFS_OK) {
        return "save failed";
    }
    ED_clear_dirty(doc);
    return "saved";
}

/* Apply one editing key. Returns the message to show in the status line
 * (or NULL) after the action. */
static const char *apply_key(ED_Doc *doc, ED_View *view, TERM *term,
                             const TERM_Key *key, char *path,
                             size_t path_cap, bool *quit)
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
        return save_doc(doc, term, view, path, path_cap);
    case TERM_KEY_CTRL_X: {
        char ans[4];
        const char *m;

        if (!ED_is_dirty(doc)) {
            *quit = true;
            break;
        }
        if (!prompt_line(term, view, "Save changes? (y/n): ", ans,
                         sizeof(ans))) {
            return "cancelled";
        }
        if (ans[0] == 'n' || ans[0] == 'N') {
            *quit = true;
            break;
        }
        if (ans[0] != 'y' && ans[0] != 'Y') {
            return "cancelled";
        }
        m = save_doc(doc, term, view, path, path_cap);
        if (strcmp(m, "saved") == 0) {
            *quit = true;
        }
        return m;
    }
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
    bool full_redraw = true;
    char status[EDITOR_STATUS_MAX];
    char path[EDITOR_PATH_MAX];
    const char *msg = NULL;
    size_t loaded_len = 0;
    FS_LFS_Result frc;

    if (cfg == NULL || cfg->term == NULL ||
        cfg->buf == NULL || cfg->lines == NULL || cfg->buf_cap == 0 ||
        cfg->lines_cap == 0) {
        return ED_SESSION_ARG_ERROR;
    }
    path[0] = '\0';
    if (cfg->path != NULL) {
        if (strlen(cfg->path) >= sizeof(path)) {
            return ED_SESSION_ARG_ERROR;
        }
        strcpy(path, cfg->path);
    }

    ED_init(&doc, cfg->buf, cfg->buf_cap, cfg->lines, cfg->lines_cap);
    EDV_init(&view, cfg->term_rows, cfg->term_cols);

    frc = (path[0] != '\0') ?
          FS_LFS_load(path, cfg->buf, cfg->buf_cap, &loaded_len) :
          FS_LFS_NOT_FOUND;
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
        size_t old_top = view.top, old_left = view.left;

        EDV_ensure_cursor_visible(&view, &doc);
        set_status(status, sizeof(status), path, &doc, msg);
        if (full_redraw || view.top != old_top || view.left != old_left) {
            TERM_render(cfg->term, &doc, &view, status);
        } else {
            TERM_render_cursor(cfg->term, &doc, &view, status);
        }

        msg = NULL;
        if (!TERM_read_key(cfg->term, &key)) {
            break;  /* input stream ended: leave the editor */
        }
        if (key.type == TERM_KEY_NONE) {
            full_redraw = false;
            continue;
        }
        full_redraw = !is_motion_key(key.type);
        msg = apply_key(&doc, &view, cfg->term, &key, path, sizeof(path),
                        &quit);
    }

    TERM_exit(cfg->term);

    return ED_is_dirty(&doc) ? ED_SESSION_DISCARDED : ED_SESSION_OK;
}
