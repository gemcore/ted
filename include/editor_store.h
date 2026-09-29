/*
 * editor_store.h - Bounded piece-table storage for large documents.
 *
 * The original document remains in its backing store. Inserted bytes are
 * held in caller-provided memory until save; no storage writes occur here.
 */
#ifndef EDITOR_STORE_H
#define EDITOR_STORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*ED_StoreReadAt)(void *ctx, size_t offset, char *dst,
                                 size_t len);

typedef enum {
    ED_STORE_OK = 0,
    ED_STORE_FULL,
    ED_STORE_RANGE,
    ED_STORE_IO,
    ED_STORE_INVALID
} ED_StoreResult;

typedef struct {
    size_t start;
    size_t len;
    bool added;
} ED_StorePiece;

typedef struct {
    size_t row;
    size_t offset;
} ED_StoreLineAnchor;

typedef struct {
    ED_StoreReadAt read_at;
    void *source_ctx;
    size_t source_len;
    size_t len;
    size_t line_count;
    ED_StorePiece *pieces;
    size_t piece_count;
    size_t pieces_cap;
    char *added;
    size_t added_len;
    size_t added_cap;
    char *cache;
    size_t cache_cap;
    size_t cache_start;
    size_t cache_len;
    ED_StoreLineAnchor *anchors;
    size_t anchor_count;
    size_t anchors_cap;
    bool cache_valid;
} ED_Store;

void ED_Store_init(ED_Store *store, ED_StoreReadAt read_at, void *source_ctx,
                   size_t source_len, ED_StorePiece *pieces,
                   size_t pieces_cap, char *added, size_t added_cap,
                   char *cache, size_t cache_cap,
                   ED_StoreLineAnchor *anchors, size_t anchors_cap);

size_t ED_Store_length(const ED_Store *store);
size_t ED_Store_line_count(const ED_Store *store);
ED_StoreResult ED_Store_read(ED_Store *store, size_t offset, char *dst,
                             size_t len);
ED_StoreResult ED_Store_insert(ED_Store *store, size_t offset,
                               const char *data, size_t len);
ED_StoreResult ED_Store_delete(ED_Store *store, size_t offset, size_t len);
ED_StoreResult ED_Store_line_bounds(ED_Store *store, size_t row,
                                    size_t *start, size_t *len);
ED_StoreResult ED_Store_position_at(ED_Store *store, size_t offset,
                                    size_t *row, size_t *col);

#ifdef __cplusplus
}
#endif

#endif /* EDITOR_STORE_H */