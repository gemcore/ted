/*
 * fs_lfs.c - File load/save wrapper for the editor.
 *
 * Zephyr builds use the Zephyr filesystem API (typically backed by
 * LittleFS on device flash). Host builds use stdio so the editor logic
 * can be unit tested without a target.
 */
#include "fs_lfs.h"

#ifdef __ZEPHYR__

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/fs/fs.h>

FS_LFS_Result FS_LFS_open_read(const char *path, FS_LFS_File *file)
{
    struct fs_dirent entry;
    int rc;

    if (path == NULL || file == NULL || strlen(path) >= sizeof(file->path)) {
        return FS_LFS_IO;
    }
    memset(file, 0, sizeof(*file));
    rc = fs_stat(path, &entry);
    if (rc == -ENOENT) {
        return FS_LFS_NOT_FOUND;
    }
    if (rc != 0) {
        return FS_LFS_IO;
    }
    fs_file_t_init(&file->file);
    rc = fs_open(&file->file, path, FS_O_READ);
    if (rc != 0) {
        return FS_LFS_IO;
    }
    file->size = (size_t)entry.size;
    strcpy(file->path, path);
    file->open = true;
    return FS_LFS_OK;
}

void FS_LFS_close(FS_LFS_File *file)
{
    if (file != NULL) {
        if (file->open) {
            (void)fs_close(&file->file);
        }
        memset(file, 0, sizeof(*file));
    }
}

size_t FS_LFS_read_at(void *ctx, size_t offset, char *dst, size_t len)
{
    FS_LFS_File *file = (FS_LFS_File *)ctx;
    ssize_t got;

    if (file == NULL || !file->open || dst == NULL || offset > file->size ||
        len > file->size - offset ||
        fs_seek(&file->file, (off_t)offset, FS_SEEK_SET) != 0) {
        return 0;
    }
    got = fs_read(&file->file, dst, len);
    return got < 0 ? 0 : (size_t)got;
}

FS_LFS_Result FS_LFS_save_stream(const char *path, size_t len,
                                 FS_LFS_ReadAt read_at, void *ctx,
                                 FS_LFS_File *source)
{
    char temp[EDITOR_PATH_MAX + 9];
    char chunk[EDITOR_PAGE_CACHE_BYTES];
    struct fs_file_t file;
    size_t offset = 0;
    int rc, n;

    if (path == NULL || read_at == NULL) {
        return FS_LFS_IO;
    }
    n = snprintf(temp, sizeof(temp), "%s.tedtmp", path);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        return FS_LFS_IO;
    }
    fs_file_t_init(&file);
    rc = fs_open(&file, temp, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (rc != 0) {
        return FS_LFS_IO;
    }
    while (offset < len) {
        size_t count = len - offset;
        size_t written = 0;

        if (count > sizeof(chunk)) {
            count = sizeof(chunk);
        }
        if (read_at(ctx, offset, chunk, count) != count) {
            rc = -EIO;
            break;
        }
        while (written < count) {
            ssize_t put = fs_write(&file, chunk + written, count - written);

            if (put <= 0) {
                rc = -EIO;
                break;
            }
            written += (size_t)put;
        }
        if (written != count) {
            break;
        }
        offset += count;
    }
    if (offset == len && fs_sync(&file) != 0) {
        rc = -EIO;
    }
    if (fs_close(&file) != 0 && offset == len) {
        rc = -EIO;
    }
    if (offset != len || rc != 0) {
        (void)fs_unlink(temp);
        return FS_LFS_IO;
    }
    FS_LFS_close(source);
    if (fs_rename(temp, path) != 0) {
        (void)fs_unlink(temp);
        return FS_LFS_IO;
    }
    return FS_LFS_OK;
}

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
    fs_file_t_init(&file);
    rc = fs_open(&file, path, FS_O_READ);
    if (rc != 0) {
        return FS_LFS_IO;
    }
    got = fs_read(&file, buf, (size_t)entry.size > cap ? cap :
                  (size_t)entry.size);
    fs_close(&file);
    if (got < 0) {
        return FS_LFS_IO;
    }
    *len = (size_t)got;
    return ((size_t)entry.size > cap) ? FS_LFS_TOO_LARGE : FS_LFS_OK;
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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

FS_LFS_Result FS_LFS_open_read(const char *path, FS_LFS_File *file)
{
    long size;

    if (path == NULL || file == NULL || strlen(path) >= sizeof(file->path)) {
        return FS_LFS_IO;
    }
    memset(file, 0, sizeof(*file));
    file->file = fopen(path, "rb");
    if (file->file == NULL) {
        return errno == ENOENT ? FS_LFS_NOT_FOUND : FS_LFS_IO;
    }
    if (fseek(file->file, 0, SEEK_END) != 0 ||
        (size = ftell(file->file)) < 0 || fseek(file->file, 0, SEEK_SET) != 0) {
        fclose(file->file);
        file->file = NULL;
        return FS_LFS_IO;
    }
    file->size = (size_t)size;
    strcpy(file->path, path);
    file->open = true;
    return FS_LFS_OK;
}

void FS_LFS_close(FS_LFS_File *file)
{
    if (file != NULL) {
        if (file->open) {
            (void)fclose(file->file);
        }
        memset(file, 0, sizeof(*file));
    }
}

size_t FS_LFS_read_at(void *ctx, size_t offset, char *dst, size_t len)
{
    FS_LFS_File *file = (FS_LFS_File *)ctx;

    if (file == NULL || !file->open || dst == NULL || offset > file->size ||
        len > file->size - offset || offset > LONG_MAX ||
        fseek(file->file, (long)offset, SEEK_SET) != 0) {
        return 0;
    }
    return fread(dst, 1, len, file->file);
}

FS_LFS_Result FS_LFS_save_stream(const char *path, size_t len,
                                 FS_LFS_ReadAt read_at, void *ctx,
                                 FS_LFS_File *source)
{
    char temp[EDITOR_PATH_MAX + 9];
    char chunk[EDITOR_PAGE_CACHE_BYTES];
    FILE *file;
    size_t offset = 0;
    int n;

    if (path == NULL || read_at == NULL) {
        return FS_LFS_IO;
    }
    n = snprintf(temp, sizeof(temp), "%s.tedtmp", path);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        return FS_LFS_IO;
    }
    file = fopen(temp, "wb");
    if (file == NULL) {
        return FS_LFS_IO;
    }
    while (offset < len) {
        size_t count = len - offset;

        if (count > sizeof(chunk)) {
            count = sizeof(chunk);
        }
        if (read_at(ctx, offset, chunk, count) != count ||
            fwrite(chunk, 1, count, file) != count) {
            fclose(file);
            remove(temp);
            return FS_LFS_IO;
        }
        offset += count;
    }
    if (fclose(file) != 0) {
        remove(temp);
        return FS_LFS_IO;
    }
    FS_LFS_close(source);
    if (rename(temp, path) != 0) {
        remove(temp);
        return FS_LFS_IO;
    }
    return FS_LFS_OK;
}

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
    got = fread(buf, 1, (size_t)size > cap ? cap : (size_t)size, fp);
    fclose(fp);
    *len = got;
    if (got != ((size_t)size > cap ? cap : (size_t)size)) {
        return FS_LFS_IO;
    }
    return ((size_t)size > cap) ? FS_LFS_TOO_LARGE : FS_LFS_OK;
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
