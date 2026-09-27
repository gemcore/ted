cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ted)

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE
    src/editor_core.c
    src/editor_view.c
    src/term_vt100.c
    src/fs_lfs.c
    src/editor_session.c
    src/cmd_edit.c
    app/main.c
)
