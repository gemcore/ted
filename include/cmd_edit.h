/*
 * cmd_edit.h - Shell command entry point for the editor.
 *
 * Usage: edit <file>
 *
 * The command owns static editor storage sized by editor_config.h and
 * launches an editor session on the shell's terminal. On Zephyr the
 * `edit` command is registered with the Zephyr shell (via SHELL_CMD in
 * cmd_edit.c) and the shell's fprintf backend is used for terminal I/O.
 * On other platforms the caller wires the TERM callbacks itself and
 * invokes Cmd_edit() from its own shell command table (as ted.cpp does
 * with its "ed" command).
 */
#ifndef CMD_EDIT_H
#define CMD_EDIT_H

#include "term_vt100.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Execute the edit command. argv[1] must be the file path.
 * Returns 0 on success, negative on error. */
int Cmd_edit(TERM *term, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* CMD_EDIT_H */
