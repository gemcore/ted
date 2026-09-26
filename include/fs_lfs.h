/*
 * fs_lfs.h - File load/save wrapper for the editor.
 *
 * Under Zephyr (__ZEPHYR__) this uses the Zephyr filesystem API
 * (<zephyr/fs/fs.h>), which typically mounts LittleFS for on-device flash
 * storage. On a host build it falls back to stdio so the same code can be
 * exercised by unit tests. The API is intentionally read-all/write-all:
 * embedded text files are small and are edited entirely in RAM.
 */
#ifndef FS_LFS_H
#define FS_LFS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FS_LFS_OK = 0,
    FS_LFS_NOT_FOUND,   /* path does not exist (load only) */
    FS_LFS_TOO_LARGE,   /* file exceeds the caller's buffer */
    FS_LFS_IO           /* any other I/O error */
} FS_LFS_Result;

/* Read the whole file into buf. *len receives the byte count on success.
 * A missing file is reported as FS_LFS_NOT_FOUND so the caller can start
 * with an empty buffer. */
FS_LFS_Result FS_LFS_load(const char *path, char *buf, size_t cap,
                          size_t *len);

/* Write len bytes to path, replacing any existing file. */
FS_LFS_Result FS_LFS_save(const char *path, const char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* FS_LFS_H */
