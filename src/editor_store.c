/*
 * editor_store.c - Allocation-free piece-table document storage.
 */
#include <string.h>

#include "editor_store.h"

#define ED_STORE_LINE_STRIDE 32

static bool store_valid(const ED_Store *store)
{
    return store != NULL && store->pieces != NULL && store->pieces_cap > 0 &&
           store->added != NULL && store->cache != NULL &&
           store->cache_cap > 0;
}

static size_t piece_source_read(ED_Store *store, const ED_StorePiece *piece,
                               size_t offset, char *dst, size_t len)
{
    size_t available = piece->len - offset;

    if (len > available) {
        len = available;
    }
    if (piece->added) {
        memcpy(dst, store->added + piece->start + offset, len);
        return len;
    }
    if (store->read_at == NULL) {
        return 0;
    }
    return store->read_at(store->source_ctx, piece->start + offset, dst, len);
}

static ED_StoreResult raw_read(ED_Store *store, size_t offset, char *dst,
                               size_t len)
{
    size_t i, logical = 0, copied = 0;

    for (i = 0; i < store->piece_count && copied < len; i++) {
        ED_StorePiece *piece = &store->pieces[i];
        size_t within, take, got;

        if (offset >= logical + piece->len) {
            logical += piece->len;
            continue;
        }
        within = (offset > logical) ? offset - logical : 0;
        take = piece->len - within;
        if (take > len - copied) {
            take = len - copied;
        }
        got = piece_source_read(store, piece, within, dst + copied, take);
        if (got != take) {
            return ED_STORE_IO;
        }
        copied += take;
        offset += take;
        logical += piece->len;
    }
    return copied == len ? ED_STORE_OK : ED_STORE_RANGE;
}

static ED_StoreResult get_byte(ED_Store *store, size_t offset, char *value)
{
    size_t start, len;

    if (offset >= store->len) {
        return ED_STORE_RANGE;
    }
    if (!store->cache_valid || offset < store->cache_start ||
        offset >= store->cache_start + store->cache_len) {
        start = (offset / store->cache_cap) * store->cache_cap;
        len = store->len - start;
        if (len > store->cache_cap) {
            len = store->cache_cap;
        }
        if (raw_read(store, start, store->cache, len) != ED_STORE_OK) {
            store->cache_valid = false;
            return ED_STORE_IO;
        }
        store->cache_start = start;
        store->cache_len = len;
        store->cache_valid = true;
    }
    *value = store->cache[offset - store->cache_start];
    return ED_STORE_OK;
}

static size_t newline_width(ED_Store *store, size_t offset)
{
    char ch, next;

    if (get_byte(store, offset, &ch) != ED_STORE_OK) {
        return 0;
    }
    if (ch == '\n') {
        return 1;
    }
    if (ch != '\r') {
        return 0;
    }
    if (offset + 1 < store->len &&
        get_byte(store, offset + 1, &next) == ED_STORE_OK && next == '\n') {
        return 2;
    }
    return 1;
}

static size_t scan_lines(ED_Store *store, bool add_anchors)
{
    size_t offset = 0, row = 0;
    size_t next_anchor = (store->anchors_cap > 0) ?
                         store->source_len / (store->anchors_cap + 1) : 0;

    if (next_anchor == 0 && store->anchors_cap > 0) {
        next_anchor = 1;
    }
    while (offset < store->len) {
        size_t width = newline_width(store, offset);

        if (width > 0) {
            offset += width;
            row++;
            if (add_anchors && store->anchors != NULL &&
                store->anchor_count < store->anchors_cap &&
                offset >= next_anchor) {
                store->anchors[store->anchor_count].row = row;
                store->anchors[store->anchor_count].offset = offset;
                store->anchor_count++;
                next_anchor = store->source_len *
                              (store->anchor_count + 1) /
                              (store->anchors_cap + 1);
            }
        } else {
            offset++;
        }
    }
    return row + 1;
}

static ED_StoreResult split_at(ED_Store *store, size_t offset, size_t *index)
{
    size_t i, logical = 0;

    for (i = 0; i < store->piece_count; i++) {
        ED_StorePiece piece = store->pieces[i];
        size_t boundary = logical + piece.len;

        if (offset == logical) {
            *index = i;
            return ED_STORE_OK;
        }
        if (offset == boundary) {
            *index = i + 1;
            return ED_STORE_OK;
        }
        if (offset < boundary) {
            size_t left_len = offset - logical;

            if (store->piece_count == store->pieces_cap) {
                return ED_STORE_FULL;
            }
            memmove(store->pieces + i + 2, store->pieces + i + 1,
                    (store->piece_count - i - 1) * sizeof(*store->pieces));
            store->pieces[i].len = left_len;
            store->pieces[i + 1] = piece;
            store->pieces[i + 1].start += left_len;
            store->pieces[i + 1].len -= left_len;
            store->piece_count++;
            *index = i + 1;
            return ED_STORE_OK;
        }
        logical = boundary;
    }
    if (offset == store->len) {
        *index = store->piece_count;
        return ED_STORE_OK;
    }
    return ED_STORE_RANGE;
}

static bool can_join(const ED_StorePiece *left, const ED_StorePiece *right)
{
    return left->added == right->added &&
           left->start + left->len == right->start;
}

static void coalesce(ED_Store *store)
{
    size_t i = 0;

    while (i + 1 < store->piece_count) {
        if (can_join(&store->pieces[i], &store->pieces[i + 1])) {
            store->pieces[i].len += store->pieces[i + 1].len;
            memmove(store->pieces + i + 1, store->pieces + i + 2,
                    (store->piece_count - i - 2) * sizeof(*store->pieces));
            store->piece_count--;
        } else {
            i++;
        }
    }
}

static void compact_added(ED_Store *store)
{
    size_t packed = 0, last_end = 0, found = 0;

    while (found < store->piece_count) {
        size_t i, selected = store->piece_count;
        size_t selected_start = (size_t)-1;

        for (i = 0; i < store->piece_count; i++) {
            ED_StorePiece *piece = &store->pieces[i];

            if (piece->added && piece->start >= last_end &&
                piece->start < selected_start) {
                selected = i;
                selected_start = piece->start;
            }
        }
        if (selected == store->piece_count) {
            break;
        }
        if (selected_start != packed) {
            memmove(store->added + packed,
                    store->added + selected_start,
                    store->pieces[selected].len);
            store->pieces[selected].start = packed;
        }
        packed += store->pieces[selected].len;
        last_end = selected_start + store->pieces[selected].len;
        found++;
    }
    store->added_len = packed;
}

static bool split_required(const ED_Store *store, size_t offset)
{
    size_t i, logical = 0;

    for (i = 0; i < store->piece_count; i++) {
        logical += store->pieces[i].len;
        if (offset < logical && offset > logical - store->pieces[i].len) {
            return true;
        }
    }
    return false;
}

static void invalidate_after(ED_Store *store, size_t offset)
{
    while (store->anchor_count > 0 &&
           store->anchors[store->anchor_count - 1].offset >= offset) {
        store->anchor_count--;
    }
    store->cache_valid = false;
}

void ED_Store_init(ED_Store *store, ED_StoreReadAt read_at, void *source_ctx,
                   size_t source_len, ED_StorePiece *pieces,
                   size_t pieces_cap, char *added, size_t added_cap,
                   char *cache, size_t cache_cap,
                   ED_StoreLineAnchor *anchors, size_t anchors_cap)
{
    if (store == NULL) {
        return;
    }
    memset(store, 0, sizeof(*store));
    store->read_at = read_at;
    store->source_ctx = source_ctx;
    store->source_len = source_len;
    store->len = source_len;
    store->line_count = 1;
    store->pieces = pieces;
    store->pieces_cap = pieces_cap;
    store->added = added;
    store->added_cap = added_cap;
    store->cache = cache;
    store->cache_cap = cache_cap;
    store->anchors = anchors;
    store->anchors_cap = anchors_cap;
    if (source_len > 0 && pieces != NULL && pieces_cap > 0) {
        pieces[0].start = 0;
        pieces[0].len = source_len;
        pieces[0].added = false;
        store->piece_count = 1;
    }
    if (store->cache_cap > 0) {
        store->line_count = scan_lines(store, true);
    }
}

size_t ED_Store_length(const ED_Store *store)
{
    return store_valid(store) ? store->len : 0;
}

size_t ED_Store_line_count(const ED_Store *store)
{
    return store_valid(store) ? store->line_count : 0;
}

ED_StoreResult ED_Store_read(ED_Store *store, size_t offset, char *dst,
                             size_t len)
{
    size_t i;

    if (!store_valid(store) || (dst == NULL && len > 0)) {
        return ED_STORE_INVALID;
    }
    if (offset > store->len || len > store->len - offset) {
        return ED_STORE_RANGE;
    }
    for (i = 0; i < len; i++) {
        ED_StoreResult rc = get_byte(store, offset + i, &dst[i]);

        if (rc != ED_STORE_OK) {
            return rc;
        }
    }
    return ED_STORE_OK;
}

ED_StoreResult ED_Store_insert(ED_Store *store, size_t offset,
                               const char *data, size_t len)
{
    size_t index, i, extra;
    ED_StoreResult rc;

    if (!store_valid(store) || (data == NULL && len > 0)) {
        return ED_STORE_INVALID;
    }
    if (offset > store->len) {
        return ED_STORE_RANGE;
    }
    if (len == 0) {
        return ED_STORE_OK;
    }
    if (len > store->added_cap - store->added_len) {
        return ED_STORE_FULL;
    }
    extra = split_required(store, offset) ? 1 : 0;
    if (store->piece_count + extra >= store->pieces_cap) {
        return ED_STORE_FULL;
    }
    rc = split_at(store, offset, &index);
    if (rc != ED_STORE_OK) {
        return rc;
    }
    memcpy(store->added + store->added_len, data, len);
    memmove(store->pieces + index + 1, store->pieces + index,
            (store->piece_count - index) * sizeof(*store->pieces));
    store->pieces[index].start = store->added_len;
    store->pieces[index].len = len;
    store->pieces[index].added = true;
    store->piece_count++;
    store->added_len += len;
    store->len += len;
    for (i = 0; i < len; i++) {
        if (data[i] == '\n') {
            store->line_count++;
        } else if (data[i] == '\r' &&
                   (i + 1 == len || data[i + 1] != '\n')) {
            store->line_count++;
        }
    }
    coalesce(store);
    invalidate_after(store, offset);
    return ED_STORE_OK;
}

ED_StoreResult ED_Store_delete(ED_Store *store, size_t offset, size_t len)
{
    size_t first, last, i, newlines = 0, extra;
    ED_StoreResult rc;

    if (!store_valid(store)) {
        return ED_STORE_INVALID;
    }
    if (offset > store->len || len > store->len - offset) {
        return ED_STORE_RANGE;
    }
    if (len == 0) {
        return ED_STORE_OK;
    }
    extra = (split_required(store, offset) ? 1 : 0) +
            (split_required(store, offset + len) ? 1 : 0);
    if (store->piece_count + extra > store->pieces_cap) {
        return ED_STORE_FULL;
    }
    for (i = 0; i < len; i++) {
        char ch;

        rc = get_byte(store, offset + i, &ch);
        if (rc != ED_STORE_OK) {
            return rc;
        }
        if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && offset + i + 1 < store->len) {
                char next;

                if (get_byte(store, offset + i + 1, &next) == ED_STORE_OK &&
                    next == '\n') {
                    i++;
                }
            }
            newlines++;
        }
    }
    rc = split_at(store, offset, &first);
    if (rc != ED_STORE_OK) {
        return rc;
    }
    rc = split_at(store, offset + len, &last);
    if (rc != ED_STORE_OK) {
        return rc;
    }
    memmove(store->pieces + first, store->pieces + last,
            (store->piece_count - last) * sizeof(*store->pieces));
    store->piece_count -= last - first;
    store->len -= len;
    store->line_count -= newlines;
    coalesce(store);
    compact_added(store);
    invalidate_after(store, offset);
    return ED_STORE_OK;
}

ED_StoreResult ED_Store_line_bounds(ED_Store *store, size_t row,
                                    size_t *start, size_t *len)
{
    size_t row_start = 0, current_row = 0, i, anchor_i;

    if (!store_valid(store) || start == NULL || len == NULL) {
        return ED_STORE_INVALID;
    }
    if (row >= store->line_count) {
        return ED_STORE_RANGE;
    }
    for (anchor_i = 0; anchor_i < store->anchor_count; anchor_i++) {
        if (store->anchors[anchor_i].row > row) {
            break;
        }
        current_row = store->anchors[anchor_i].row;
        row_start = store->anchors[anchor_i].offset;
    }
    for (i = row_start; i < store->len && current_row < row; ) {
        size_t width = newline_width(store, i);

        if (width > 0) {
            current_row++;
            i += width;
            row_start = i;
            if (current_row % ED_STORE_LINE_STRIDE == 0 &&
                store->anchors_cap > 0 &&
                store->anchors != NULL &&
                (store->anchor_count == 0 ||
                 store->anchors[store->anchor_count - 1].row < current_row)) {
                if (store->anchor_count == store->anchors_cap) {
                    memmove(store->anchors, store->anchors + 1,
                            (store->anchors_cap - 1) * sizeof(*store->anchors));
                    store->anchor_count--;
                }
                store->anchors[store->anchor_count].row = current_row;
                store->anchors[store->anchor_count].offset = row_start;
                store->anchor_count++;
            }
        } else {
            i++;
        }
    }
    if (current_row != row) {
        return ED_STORE_IO;
    }
    i = row_start;
    while (i < store->len) {
        size_t width = newline_width(store, i);

        if (width > 0) {
            break;
        }
        i++;
    }
    *start = row_start;
    *len = i - row_start;
    if (*len > 0) {
        char last;

        if (get_byte(store, i - 1, &last) == ED_STORE_OK && last == '\r') {
            (*len)--;
        }
    }
    return ED_STORE_OK;
}

ED_StoreResult ED_Store_position_at(ED_Store *store, size_t offset,
                                    size_t *row, size_t *col)
{
    size_t current_row = 0, line_start = 0, i, anchor_i;

    if (!store_valid(store) || offset > store->len) {
        return ED_STORE_INVALID;
    }
    for (anchor_i = 0; anchor_i < store->anchor_count; anchor_i++) {
        if (store->anchors[anchor_i].offset > offset) {
            break;
        }
        current_row = store->anchors[anchor_i].row;
        line_start = store->anchors[anchor_i].offset;
    }
    for (i = line_start; i < offset; ) {
        size_t width = newline_width(store, i);

        if (width > 0 && i + width <= offset) {
            current_row++;
            i += width;
            line_start = i;
        } else {
            i++;
        }
    }
    if (row != NULL) {
        *row = current_row;
    }
    if (col != NULL) {
        *col = offset - line_start;
        if (offset > line_start) {
            char previous;

            if (get_byte(store, offset - 1, &previous) == ED_STORE_OK &&
                previous == '\r') {
                (*col)--;
            }
        }
    }
    return ED_STORE_OK;
}