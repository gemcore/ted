/*
 * fs_lfs.c - File load/save wrapper for the editor.
 *
 * Zephyr builds use the Zephyr filesystem API (typically backed by
 * LittleFS on device flash). Host builds use stdio so the editor logic
 * can be unit tested without a target.
 */
#include "fs_lfs.h"

#ifdef __ZEPHYR__

#include <zephyr/fs/fs.h>

FS_LFS_Result FS_LFS_load(const char *path, char *buf, size_t cap,
                          size_t *len)
{
    struct fs_file_t file;
    struct fs_dirent entry;
    ssize_t got;
    int rc;

    if (path == NULL || buf == NULL || len == NULL) {
        return FS_LFS_IO;
    }
    *len = 0;

    rc = fs_stat(path, &entry);
    if (rc == -ENOENT) {
        return FS_LFS_NOT_FOUND;
    }
    if (rc != 0) {
        return FS_LFS_IO;
    }
    if ((size_t)entry.size > cap) {
        return FS_LFS_TOO_LARGE;
    }

    fs_file_t_init(&file);
    rc = fs_open(&file, path, FS_O_READ);
    if (rc != 0) {
        return FS_LFS_IO;
    }
    got = fs_read(&file, buf, (size_t)entry.size);
    fs_close(&file);
    if (got < 0) {
        return FS_LFS_IO;
    }
    *len = (size_t)got;
    return FS_LFS_OK;
}

FS_LFS_Result FS_LFS_save(const char *path, const char *buf, size_t len)
{
    struct fs_file_t file;
    ssize_t put;
    int rc;

    if (path == NULL || (buf == NULL && len > 0)) {
        return FS_LFS_IO;
    }

    fs_file_t_init(&file);
    rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (rc != 0) {
        return FS_LFS_IO;
    }
    put = fs_write(&file, buf, len);
    fs_close(&file);
    if (put < 0 || (size_t)put != len) {
        return FS_LFS_IO;
    }
    return FS_LFS_OK;
}

#else /* host build: stdio backend */

#include <stdio.h>

FS_LFS_Result FS_LFS_load(const char *path, char *buf, size_t cap,
                          size_t *len)
{
    FILE *fp;
    long size;
    size_t got;

    if (path == NULL || buf == NULL || len == NULL) {
        return FS_LFS_IO;
    }
    *len = 0;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return FS_LFS_NOT_FOUND;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return FS_LFS_IO;
    }
    if ((size_t)size > cap) {
        fclose(fp);
        return FS_LFS_TOO_LARGE;
    }
    got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    *len = got;
    return FS_LFS_OK;
}

FS_LFS_Result FS_LFS_save(const char *path, const char *buf, size_t len)
{
    FILE *fp;
    size_t put;

    if (path == NULL || (buf == NULL && len > 0)) {
        return FS_LFS_IO;
    }
    fp = fopen(path, "wb");
    if (fp == NULL) {
        return FS_LFS_IO;
    }
    put = fwrite(buf, 1, len, fp);
    if (fclose(fp) != 0 || put != len) {
        return FS_LFS_IO;
    }
    return FS_LFS_OK;
}

#endif /* __ZEPHYR__ */
