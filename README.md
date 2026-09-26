# ted - Text Editor

A small, command-driven text editor, refactored for Zephyr OS and embedded
targets. The original `ted.cpp` was a Windows console application; the new
implementation in `include/` + `src/` removes all Windows console APIs and
desktop assumptions while keeping the spirit of the original editor.

## Architecture

The editor is split into small, single-responsibility modules. Only the
edges (terminal I/O callbacks, filesystem, shell glue) are platform aware;
the core is pure C with no OS or heap dependencies — all state lives in
caller-provided static storage sized by `editor_config.h`.

```
include/  src/
  editor_config.h     Compile-time limits (max bytes/lines, screen size)
  editor_core.[ch]    Text buffer, cursor, insert/delete/split/join.
                      Platform-neutral and unit testable.
  editor_view.[ch]    Viewport/scroll state and visible-window math.
  term_vt100.[ch]     VT100 rendering and raw-byte key decoding via
                      pluggable read/write callbacks (no OS calls).
  fs_lfs.[ch]         File load/save wrapper: Zephyr fs API (LittleFS)
                      under __ZEPHYR__, stdio on host builds.
  editor_session.[ch] Event loop tying core/view/terminal/fs together:
                      load, draw, read key, apply action, save, quit.
  cmd_edit.[ch]       Shell command entry point: `edit <file>`.
```

Data flow:

```
 open   cmd_edit -> editor_session -> fs_lfs -> editor_core
 render editor_session -> editor_view -> term_vt100
 input  term_vt100 (key decode) -> editor_session -> editor_core
 save   editor_session -> editor_core buffer -> fs_lfs
```

## Key bindings

| Key            | Action                  |
| -------------- | ----------------------- |
| printable      | insert character        |
| Enter          | split line              |
| Backspace/Del  | delete before/at cursor |
| Tab            | insert spaces to tab stop |
| Arrows         | move cursor             |
| Home/End       | line start/end          |
| PgUp/PgDn      | move by a page          |
| Ctrl-S         | save                    |
| Ctrl-Q         | quit                    |

## Zephyr porting

The modules are organized so a Zephyr port only has to provide the edges:

- **Terminal**: wire `TERM_WriteFn`/`TERM_ReadFn` to the shell/UART.
  `cmd_edit.c` already registers `edit` with the Zephyr shell
  (`SHELL_CMD_ARG_REGISTER`) and writes through `shell_fprintf` when built
  with `__ZEPHYR__`; input needs a raw byte source (e.g. a UART receive
  ring buffer) because the Zephyr shell normally consumes typed bytes.
- **Filesystem**: `fs_lfs.c` uses `<zephyr/fs/fs.h>` under `__ZEPHYR__`;
  mount LittleFS and pass paths such as `/lfs/notes.txt`.
- **Sizing**: override `EDITOR_MAX_BYTES` / `EDITOR_MAX_LINES` at build
  time to fit the target's RAM budget.

No part of the new code uses `GetStdHandle`, `PeekConsoleInput`,
`ReadConsoleInput`, `SetConsoleMode`, or any other Windows console API.
The legacy Windows prototype is kept for reference only:

- `ted.cpp`, `stdafx.h`, `targetver.h`, `ted.sln`, `ted.vcxproj*`

## Host build and tests

The modules are plain C99 and can be exercised on any host:

```sh
make test     # builds and runs the unit tests in tests/
```

The tests cover the editor core (buffer editing, cursor movement, goal
column, capacity limits), view scrolling, VT100 rendering/key decoding,
and an end-to-end session driven through a loopback terminal with a real
file as the filesystem.
