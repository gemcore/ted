/*
 * test_editor_session.c - Integration tests for the editor event loop.
 *
 * Drives ED_Session_run() with a scripted input stream over a loopback
 * terminal and a real file in /tmp as the filesystem.
 */
#include <stdio.h>
#include <string.h>

#include "editor_session.h"
#include "test_common.h"

#define CAP   256
#define LINES 16

static char   out[16384];
static size_t out_len;
static char   script[512];
static size_t script_len;
static size_t script_pos;

static size_t loop_write(void *ctx, const char *data, size_t len)
{
    (void)ctx;
    if (out_len + len < sizeof(out)) {
        memcpy(out + out_len, data, len);
        out_len += len;
        out[out_len] = '\0';
    }
    return len;
}

static int loop_read(void *ctx)
{
    (void)ctx;
    if (script_pos >= script_len) {
        return -1;
    }
    return (unsigned char)script[script_pos++];
}

static TERM term;
static char   buf[CAP];
static size_t lines[LINES];

static void setup_script(const char *keys, size_t n)
{
    memcpy(script, keys, n);
    script_len = n;
    script_pos = 0;
    out_len = 0;
    out[0] = '\0';
    TERM_init(&term, loop_write, loop_read, NULL);
}

static ED_SessionConfig cfg;

static void cfg_for(const char *path)
{
    cfg.path = path;
    cfg.term = &term;
    cfg.buf = buf;
    cfg.buf_cap = CAP;
    cfg.lines = lines;
    cfg.lines_cap = LINES;
    cfg.term_rows = 10;
    cfg.term_cols = 40;
}

static void write_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");

    CHECK(fp != NULL);
    if (fp != NULL) {
        fwrite(text, 1, strlen(text), fp);
        fclose(fp);
    }
}

static char *read_file(const char *path, char *dst, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t n = 0;

    if (fp != NULL) {
        n = fread(dst, 1, cap - 1, fp);
        fclose(fp);
    }
    dst[n] = '\0';
    return dst;
}

int main(void)
{
    const char *path = "/tmp/ted_session_test.txt";
    char filebuf[CAP];

    /* Type "Hi", save, quit: file must contain the typed text. */
    remove(path);
    setup_script("Hi\x17\x18", 4);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
    CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "Hi");

    /* Quit without saving discards changes. */
    write_file(path, "keep");
    setup_script("X\x18n\r", 4);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_DISCARDED);
    CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "keep");

    /* Editing an existing file and saving persists the edit. */
    write_file(path, "ab\ncd\n");
    /* End of line 0, Enter, 'Z', save, quit. */
    setup_script("\x1b[F" "\r" "Z" "\x17\x18", 7);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
    CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "ab\nZ\ncd\n");

    /* Backspace at line start joins lines. */
    write_file(path, "ab\ncd");
    setup_script("\x1b[B" "\x7f" "\x17\x18", 6);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
    CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "abcd");

    /* Output uses VT100 sequences and shows a status line. */
    write_file(path, "x");
    setup_script("\x18", 1);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
    CHECK(strstr(out, "\x1b[?1049h") != NULL);   /* alternate screen */
    CHECK(strstr(out, "\x1b[7m") != NULL);       /* status bar */
    CHECK(strstr(out, path) != NULL);            /* file name shown */

    /* An oversized file must not render stale caller buffer contents. */
    {
        char oversized[CAP + 1];

        memset(oversized, 'x', sizeof(oversized));
        write_file(path, oversized);
        memset(buf, 'z', sizeof(buf));
        setup_script("\x18", 1);
        cfg_for(path);
        CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
        CHECK(strstr(out, "xxxx") != NULL);
        CHECK(strstr(out, "zzzz") == NULL);
    }

    /* Input stream ending without Ctrl-X still exits cleanly. */
    write_file(path, "x");
    setup_script("", 0);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);

    /* Bad configuration is rejected. */
    {
        ED_SessionConfig bad;

        cfg_for(path);
        bad = cfg;
        bad.buf = NULL;
        CHECK(ED_Session_run(&bad) == ED_SESSION_ARG_ERROR);
    }

    /* Unnamed buffer: Ctrl-W asks for a name, then saves there. */
    remove(path);
    {
        static const char keys[] = "Hi\x17/tmp/ted_session_test.txt\r\x18";

        setup_script(keys, sizeof(keys) - 1);
        cfg_for(NULL);
        CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
        CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "Hi");
        CHECK(strstr(out, "Write to: ") != NULL);
    }

    /* Exit with changes: Ctrl-C cancels, 'y' saves and exits. */
    write_file(path, "a");
    setup_script("b\x18\x03\x18y\r", 6);
    cfg_for(path);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_OK);
    CHECK_STR(read_file(path, filebuf, sizeof(filebuf)), "ba");

    /* Unnamed buffer: 'y' on exit asks for a name; empty Enter is ignored,
     * Ctrl-C cancels. */
    remove(path);
    setup_script("Q\x18y\r\r\x03", 6);
    cfg_for(NULL);
    CHECK(ED_Session_run(&cfg) == ED_SESSION_DISCARDED);
    CHECK(read_file(path, filebuf, sizeof(filebuf))[0] == '\0');

    remove(path);
    TEST_SUMMARY();
}
