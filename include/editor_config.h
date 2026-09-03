/*
 * editor_config.h - Compile-time configuration and limits for the ted editor.
 *
 * All editor state is caller-provided static storage; these limits define
 * the maximum footprint so memory use is predictable on embedded targets.
 * Override any value at build time (e.g. -DEDITOR_MAX_BYTES=8192).
 */
#ifndef EDITOR_CONFIG_H
#define EDITOR_CONFIG_H

/* Maximum file size held in RAM while editing (bytes). */
#ifndef EDITOR_MAX_BYTES
#define EDITOR_MAX_BYTES        4096
#endif

/* Maximum number of lines tracked by the line index. */
#ifndef EDITOR_MAX_LINES
#define EDITOR_MAX_LINES        256
#endif

/* Default terminal size. The host/Zephyr backend may report the real
 * size via TERM_set_size(); these are safe fallbacks for serial consoles. */
#ifndef EDITOR_TERM_COLS
#define EDITOR_TERM_COLS        80
#endif

#ifndef EDITOR_TERM_ROWS
#define EDITOR_TERM_ROWS        24
#endif

/* Display width of a tab stop. */
#ifndef EDITOR_TAB_WIDTH
#define EDITOR_TAB_WIDTH        4
#endif

/* Non-printable characters are rendered as this glyph. */
#ifndef EDITOR_UNPRINTABLE_GLYPH
#define EDITOR_UNPRINTABLE_GLYPH '.'
#endif

/* Longest status line message. */
#ifndef EDITOR_STATUS_MAX
#define EDITOR_STATUS_MAX       96
#endif

#endif /* EDITOR_CONFIG_H */
