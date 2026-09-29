/*
 * fs_lfs.h - File load/save wrapper for the editor.
 *
 * Under Zephyr (__ZEPHYR__) this uses the Zephyr filesystem API
 * (<zephyr/fs/fs.h>), which typically mounts LittleFS for on-device flash
 * storage. On a host build it falls back to stdio so the same code can be
 * exercised by unit tests. Random-access reads and streamed saves avoid
 * loading the source file into RAM.
 */
#ifndef FS_LFS_H
#define FS_LFS_H

#include <stdbool.h>
#include <stddef.h>
#include "editor_config.h"

#ifdef __ZEPHYR__
#include <zephyr/fs/fs.h>
#else
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FS_LFS_OK = 0,
    FS_LFS_NOT_FOUND,   /* path does not exist (load only) */
    FS_LFS_TOO_LARGE,   /* file exceeds the caller's buffer */
    FS_LFS_IO           /* any other I/O error */
} FS_LFS_Result;

typedef struct {
#ifdef __ZEPHYR__
    struct fs_file_t file;
#else
    FILE *file;
#endif
    size_t size;
    bool open;
    char path[EDITOR_PATH_MAX];
} FS_LFS_File;

typedef size_t (*FS_LFS_ReadAt)(void *ctx, size_t offset, char *dst, size_t len);

FS_LFS_Result FS_LFS_open_read(const char *path, FS_LFS_File *file);
void FS_LFS_close(FS_LFS_File *file);
size_t FS_LFS_read_at(void *ctx, size_t offset, char *dst, size_t len);
FS_LFS_Result FS_LFS_save_stream(const char *path, size_t len,
                                 FS_LFS_ReadAt read_at, void *ctx,
                                 FS_LFS_File *source);

/* Read the whole file into buf. *len receives the byte count on success.
 * A missing file is reported as FS_LFS_NOT_FOUND so the caller can start
 * with an empty buffer. */
FS_LFS_Result FS_LFS_load(const char *path, char *buf, size_t cap, size_t *len);

/* Write len bytes to path, replacing any existing file. */
FS_LFS_Result FS_LFS_save(const char *path, const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FS_LFS_H */
