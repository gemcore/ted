/*
 * cmd_edit.c - Shell command entry point:  edit <file>
 *
 * Owns the static editor storage (sized by editor_config.h) and starts an
 * editing session on the caller's terminal. Under Zephyr the `edit`
 * command is registered with the Zephyr shell and terminal I/O goes
 * through the shell's fprintf backend; elsewhere the host shell provides
 * the TERM callbacks and calls Cmd_edit() directly.
 */
#include "editor_config.h"
#include "editor_session.h"
#include "cmd_edit.h"

/* Static storage: the editor never allocates. */
static char   s_buf[EDITOR_MAX_BYTES];
static size_t s_lines[EDITOR_MAX_LINES];

int Cmd_edit(TERM *term, int argc, char *argv[])
{
    ED_SessionConfig cfg;
    ED_SessionResult rc;

    if (term == NULL || argc < 2 || argv == NULL || argv[1] == NULL ||
        argv[1][0] == '\0') {
        return -1;
    }

    cfg.path = argv[1];
    cfg.term = term;
    cfg.buf = s_buf;
    cfg.buf_cap = sizeof(s_buf);
    cfg.lines = s_lines;
    cfg.lines_cap = EDITOR_MAX_LINES;
    cfg.term_rows = EDITOR_TERM_ROWS;
    cfg.term_cols = EDITOR_TERM_COLS;

    rc = ED_Session_run(&cfg);
    switch (rc) {
    case ED_SESSION_OK:
    case ED_SESSION_DISCARDED:
        return 0;
    default:
        return -2;
    }
}

#ifdef __ZEPHYR__

#include <zephyr/shell/shell.h>

/* Zephyr shell backend I/O: characters typed at the shell arrive via the
 * shell's receive handler while the editor owns the screen. Output uses
 * shell_fprintf so it works on any shell backend (UART, RTT, ...). */
static const struct shell *s_shell;

static size_t zephyr_term_write(void *ctx, const char *data, size_t len)
{
    const struct shell *sh = (ctx != NULL) ? (const struct shell *)ctx : s_shell;
    size_t i;

    for (i = 0; i < len; i++) {
        shell_fprintf(sh, SHELL_NORMAL, "%c", data[i]);
    }
    return len;
}

static int zephyr_term_read(void *ctx)
{
    /* The Zephyr shell consumes input bytes itself; a production build
     * should register a raw receive callback (e.g. via the UART backend
     * or a dedicated input thread) and feed this from a ring buffer.
     * The editor architecture keeps this behind TERM_ReadFn so only this
     * one function needs platform work. */
    (void)ctx;
    return -1;
}

static int cmd_edit_zephyr(const struct shell *sh, size_t argc, char **argv)
{
    TERM term;

    if (argc < 2) {
        shell_error(sh, "usage: edit <file>");
        return -EINVAL;
    }

    s_shell = sh;
    TERM_init(&term, zephyr_term_write, zephyr_term_read, (void *)sh);
    return Cmd_edit(&term, (int)argc, argv);
}

SHELL_CMD_ARG_REGISTER(edit, NULL, "edit a file: edit <file>",
                       cmd_edit_zephyr, 2, 0);

#endif /* __ZEPHYR__ */
