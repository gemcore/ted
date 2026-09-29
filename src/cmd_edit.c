/*
 * cmd_edit.c - Shell command entry point:  edit [file]
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

/* Static, bounded storage: the source file is never copied into RAM. */
static ED_Store s_store;
static ED_StorePiece s_pieces[EDITOR_MAX_PIECES];
static char s_added[EDITOR_EDIT_BYTES];
static char s_cache[EDITOR_PAGE_CACHE_BYTES];
static ED_StoreLineAnchor s_anchors[EDITOR_LINE_ANCHORS];
static FS_LFS_File s_source;

int Cmd_edit(TERM *term, int argc, char *argv[])
{
    ED_SessionConfig cfg;
    ED_SessionResult rc;

    if (term == NULL || argv == NULL) {
        return -1;
    }

    cfg.path = (argc >= 2 && argv[1] != NULL && argv[1][0] != '\0') ?
               argv[1] : NULL;
    cfg.term = term;
    cfg.buf = NULL;
    cfg.buf_cap = 0;
    cfg.lines = NULL;
    cfg.lines_cap = 0;
    cfg.term_rows = EDITOR_TERM_ROWS;
    cfg.term_cols = EDITOR_TERM_COLS;
    cfg.store = &s_store;
    cfg.pieces = s_pieces;
    cfg.pieces_cap = EDITOR_MAX_PIECES;
    cfg.added = s_added;
    cfg.added_cap = sizeof(s_added);
    cfg.cache = s_cache;
    cfg.cache_cap = sizeof(s_cache);
    cfg.anchors = s_anchors;
    cfg.anchors_cap = EDITOR_LINE_ANCHORS;
    cfg.source = &s_source;

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

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/atomic.h>

#ifndef EDITOR_THREAD_STACK_SIZE
#define EDITOR_THREAD_STACK_SIZE 4096
#endif

#ifndef EDITOR_THREAD_PRIORITY
#define EDITOR_THREAD_PRIORITY   K_LOWEST_APPLICATION_THREAD_PRIO
#endif

#ifndef EDITOR_RX_QUEUE_LEN
#define EDITOR_RX_QUEUE_LEN      64
#endif

/* The editor runs in its own thread; the shell forwards raw input bytes to
 * it through the bypass callback while the editor owns the screen. */
K_THREAD_STACK_DEFINE(s_ed_stack, EDITOR_THREAD_STACK_SIZE);
K_MSGQ_DEFINE(s_rx_q, sizeof(uint8_t), EDITOR_RX_QUEUE_LEN, 1);

static struct k_thread s_ed_thread;
static atomic_t s_busy;
static const struct shell *s_shell;
static char s_path[EDITOR_PATH_MAX];

static void editor_bypass(const struct shell *sh, uint8_t *data, size_t len)
{
    size_t i;

    ARG_UNUSED(sh);
    for (i = 0; i < len; i++) {
        (void)k_msgq_put(&s_rx_q, &data[i], K_NO_WAIT);
    }
}

static size_t zephyr_term_write(void *ctx, const char *data, size_t len)
{
    shell_fprintf((const struct shell *)ctx, SHELL_NORMAL, "%.*s", (int)len,
                  data);
    return len;
}

static int zephyr_term_read(void *ctx)
{
    uint8_t c;

    ARG_UNUSED(ctx);
    if (k_msgq_get(&s_rx_q, &c, K_FOREVER) != 0) {
        return -1;
    }
    return c;
}

static void editor_thread(void *p1, void *p2, void *p3)
{
    const struct shell *sh = s_shell;
    char *argv[] = { "edit", s_path, NULL };
    TERM term;
    int rc;

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    TERM_init(&term, zephyr_term_write, zephyr_term_read, (void *)sh);
    rc = Cmd_edit(&term, s_path[0] != '\0' ? 2 : 1, argv);

    shell_set_bypass(sh, NULL);
    if (rc != 0) {
        shell_error(sh, "edit: failed (%d)", rc);
    }
    atomic_clear(&s_busy);
}

static int cmd_edit_zephyr(const struct shell *sh, size_t argc, char **argv)
{
    const char *name = (argc >= 2) ? argv[1] : "";

    if (strlen(name) >= sizeof(s_path)) {
        shell_error(sh, "edit: path too long");
        return -ENAMETOOLONG;
    }
    if (!atomic_cas(&s_busy, 0, 1)) {
        shell_error(sh, "edit: editor already running");
        return -EBUSY;
    }

    strcpy(s_path, name);
    s_shell = sh;
    k_msgq_purge(&s_rx_q);
    shell_set_bypass(sh, editor_bypass);

    /* Delay start so the shell finishes printing its prompt before the first redraw. */
    k_thread_create(&s_ed_thread, s_ed_stack, K_THREAD_STACK_SIZEOF(s_ed_stack),
                    editor_thread, NULL, NULL, NULL,
                    EDITOR_THREAD_PRIORITY, 0, K_MSEC(50));
    k_thread_name_set(&s_ed_thread, "ted");
    return 0;
}

SHELL_CMD_ARG_REGISTER(edit, NULL, "edit a file: edit [file]",
                       cmd_edit_zephyr, 1, 1);

#endif /* __ZEPHYR__ */