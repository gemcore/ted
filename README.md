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

**Terminal settings:** use an Xterm (or VT220) emulation. Plain VT100
emulation has no Home/End keys, so terminals such as SecureCRT send nothing
for them in that mode. Also disable XON/XOFF flow control, otherwise the
terminal swallows Ctrl-S and Ctrl-Q.

## Zephyr porting

The modules are organized so a Zephyr port only has to provide the edges:

- **Terminal**: `cmd_edit.c` registers `edit` with the Zephyr shell
  (`SHELL_CMD_ARG_REGISTER`) and runs the editor in its own thread
  (`ted`). While the editor is open, `shell_set_bypass()` routes raw input
  bytes to a message queue that feeds `TERM_ReadFn`; output goes through
  `shell_fprintf`. On quit the bypass is removed (press Enter to redraw
  the shell prompt). Thread stack, priority, path length and input queue
  depth are set by `EDITOR_THREAD_STACK_SIZE`, `EDITOR_THREAD_PRIORITY`,
  `EDITOR_PATH_MAX` and `EDITOR_RX_QUEUE_LEN`.
- **Filesystem**: `fs_lfs.c` uses `<zephyr/fs/fs.h>` under `__ZEPHYR__`.
  `app/main.c` mounts LittleFS at `/lfs` on the `littlefs_storage`
  partition, so pass paths such as `/lfs/notes.txt`.
- **Sizing**: override `EDITOR_MAX_BYTES` / `EDITOR_MAX_LINES` at build
  time to fit the target's RAM budget.

### Building for nRF Connect SDK

The repository root is a Zephyr application (`CMakeLists.txt`, `prj.conf`,
`app/main.c`), tested with NCS v3.2.4:

```sh
west build -p -b nrf52840dk/nrf52840 .
west flash
```

Then run `edit /lfs/notes.txt` at the shell prompt. `prj.conf` sets
`CONFIG_MAIN_STACK_SIZE=2048` because mounting (and first-time formatting)
LittleFS in `main()` overflows the default 1 KB main stack.

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
