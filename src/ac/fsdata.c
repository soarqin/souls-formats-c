/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fsdata.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"
#include "compression/compression_internal.h"

#include <stdlib.h>
#include <string.h>

struct sf_fsdata_file {
    int id;
    uint8_t *bytes;
    size_t size;
};

struct sf_fsdata {
    const sf_allocator_t *alloc;
    int entry_count;
    bool compressed;
    struct sf_fsdata_file *files;
    size_t file_count;
    size_t file_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

sf_result_t sf_fsdata_create(sf_fsdata_t **out, int entry_count, bool compressed,
                             const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(entry_count > 0 && (entry_count % (int)SF_FSDATA_SECTOR_SIZE) == 0);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_fsdata_t *f = (sf_fsdata_t *)sf_xalloc(alloc, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    memset(f, 0, sizeof(*f));
    f->alloc = alloc;
    f->entry_count = entry_count;
    f->compressed = compressed;
    *out = f;
    return SF_OK;
}

void sf_fsdata_destroy(sf_fsdata_t *f) {
    if (!f) return;
    for (size_t i = 0; i < f->file_count; i++)
        sf_xfree(f->alloc, f->files[i].bytes);
    sf_xfree(f->alloc, f->files);
    sf_xfree(f->alloc, f);
}

bool sf_fsdata_is_compressed(const sf_fsdata_t *f) { return f ? f->compressed : false; }
int sf_fsdata_entry_count(const sf_fsdata_t *f) { return f ? f->entry_count : 0; }
size_t sf_fsdata_file_count(const sf_fsdata_t *f) { return f ? f->file_count : 0u; }

sf_result_t sf_fsdata_get_file(const sf_fsdata_t *f, size_t index,
                               int *out_id, const uint8_t **out_bytes, size_t *out_size) {
    SF_CHECK_ARG(f != NULL);
    if (index >= f->file_count) return SF_ERR_OUT_OF_RANGE;
    if (out_id) *out_id = f->files[index].id;
    if (out_bytes) *out_bytes = f->files[index].bytes;
    if (out_size) *out_size = f->files[index].size;
    return SF_OK;
}

sf_result_t sf_fsdata_find_file(const sf_fsdata_t *f, int id, size_t *out_index) {
    SF_CHECK_ARG(f != NULL && out_index != NULL);
    for (size_t i = 0; i < f->file_count; i++) {
        if (f->files[i].id == id) { *out_index = i; return SF_OK; }
    }
    return SF_ERR_NOT_FOUND;
}

sf_result_t sf_fsdata_add_file(sf_fsdata_t *f, int id, const uint8_t *bytes, size_t size) {
    SF_CHECK_ARG(f != NULL);
    SF_CHECK_ARG(id >= 0 && id < f->entry_count);
    size_t idx;
    if (sf_fsdata_find_file(f, id, &idx) == SF_OK) return SF_ERR_INVALID_ARG;

    if (f->file_count >= f->file_cap) {
        size_t new_cap = f->file_cap == 0 ? 8u : f->file_cap * 2u;
        struct sf_fsdata_file *nf = (struct sf_fsdata_file *)sf_xalloc(
            f->alloc, new_cap * sizeof(*nf));
        if (!nf) return SF_ERR_OOM;
        if (f->files) {
            memcpy(nf, f->files, f->file_count * sizeof(*nf));
            sf_xfree(f->alloc, f->files);
        }
        f->files = nf;
        f->file_cap = new_cap;
    }

    uint8_t *copy = NULL;
    if (size > 0) {
        copy = (uint8_t *)sf_xalloc(f->alloc, size);
        if (!copy) return SF_ERR_OOM;
        memcpy(copy, bytes, size);
    }
    f->files[f->file_count].id = id;
    f->files[f->file_count].bytes = copy;
    f->files[f->file_count].size = size;
    f->file_count++;
    return SF_OK;
}

static int cmp_file_id(const void *a, const void *b) {
    const struct sf_fsdata_file *fa = (const struct sf_fsdata_file *)a;
    const struct sf_fsdata_file *fb = (const struct sf_fsdata_file *)b;
    return fa->id - fb->id;
}

static size_t align_up(size_t v, size_t align) {
    return (v + align - 1u) & ~(align - 1u);
}

sf_result_t sf_fsdata_read_from_memory(sf_fsdata_t **out, const void *bytes, size_t size,
                                       int entry_count, bool compressed,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    SF_CHECK_ARG(entry_count > 0 && (entry_count % (int)SF_FSDATA_SECTOR_SIZE) == 0);
    *out = NULL;

    sf_fsdata_t *f = NULL;
    TRY(sf_fsdata_create(&f, entry_count, compressed, alloc));

    int fields_per_entry = compressed ? 3 : 2;
    size_t header_size = (size_t)entry_count * (size_t)fields_per_entry * 4u;
    if (size < header_size) { sf_fsdata_destroy(f); return SF_ERR_INVALID_ARG; }

    const uint8_t *p = (const uint8_t *)bytes;

    for (int i = 0; i < entry_count; i++) {
        size_t off = (size_t)i * (size_t)fields_per_entry * 4u;
        int32_t sector_offset, sector_length;
        memcpy(&sector_offset, p + off, 4);
        if (compressed) {
            int32_t decompressed_sectors;
            memcpy(&decompressed_sectors, p + off + 4, 4);
            memcpy(&sector_length, p + off + 8, 4);
            (void)decompressed_sectors;
        } else {
            memcpy(&sector_length, p + off + 4, 4);
        }

        if (sector_offset == 0 && sector_length == 0) continue;
        if (sector_length <= 0) continue;

        size_t data_off = header_size + (size_t)sector_offset * SF_FSDATA_SECTOR_SIZE;
        size_t data_len = (size_t)sector_length * SF_FSDATA_SECTOR_SIZE;
        if (data_off + data_len > size) { sf_fsdata_destroy(f); return SF_ERR_INVALID_ARG; }

        if (compressed) {
            int32_t decompressed_sectors_val;
            memcpy(&decompressed_sectors_val, p + off + 4, 4);
            size_t decompressed_size = (size_t)decompressed_sectors_val * SF_FSDATA_SECTOR_SIZE;
            void *decompressed = NULL;
            sf_result_t e = sfi_zlib_decompress(p + data_off, data_len,
                                                &decompressed, decompressed_size, f->alloc);
            if (e != SF_OK) { sf_fsdata_destroy(f); return e; }
            sf_result_t ae = sf_fsdata_add_file(f, i, (const uint8_t *)decompressed, decompressed_size);
            sf_xfree(f->alloc, decompressed);
            if (ae != SF_OK) { sf_fsdata_destroy(f); return ae; }
        } else {
            sf_result_t ae = sf_fsdata_add_file(f, i, p + data_off, data_len);
            if (ae != SF_OK) { sf_fsdata_destroy(f); return ae; }
        }
    }

    *out = f;
    return SF_OK;
}

sf_result_t sf_fsdata_write_to_memory(const sf_fsdata_t *f, void **out_bytes,
                                      size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(f != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    struct sf_fsdata_file *sorted = NULL;
    if (f->file_count > 0) {
        sorted = (struct sf_fsdata_file *)sf_xalloc(alloc,
            f->file_count * sizeof(*sorted));
        if (!sorted) return SF_ERR_OOM;
        memcpy(sorted, f->files, f->file_count * sizeof(*sorted));
        qsort(sorted, f->file_count, sizeof(*sorted), cmp_file_id);
    }

    int fields = f->compressed ? 3 : 2;
    size_t header_size = (size_t)f->entry_count * (size_t)fields * 4u;

    size_t data_size = 0;
    for (size_t i = 0; i < f->file_count; i++) {
        size_t file_size = sorted ? sorted[i].size : 0u;
        data_size += align_up(file_size, SF_FSDATA_ALIGNMENT_SIZE);
    }

    size_t total = header_size + data_size;
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, total);
    if (!buf) { sf_xfree(alloc, sorted); return SF_ERR_OOM; }
    memset(buf, 0, total);

    size_t data_cursor = 0;
    size_t file_idx = 0;
    int prev_id = -1;

    for (size_t fi = 0; fi < f->file_count; fi++) {
        struct sf_fsdata_file *file = sorted ? &sorted[fi] : NULL;
        if (!file) break;
        int id = file->id;
        size_t entry_off = (size_t)id * (size_t)fields * 4u;

        int32_t sector_offset = (int32_t)(data_cursor / SF_FSDATA_SECTOR_SIZE);
        int32_t sector_length;

        if (f->compressed) {
            void *compressed = NULL;
            size_t compressed_size = 0;
            sf_result_t e = sfi_zlib_compress(file->bytes, file->size,
                                              &compressed, &compressed_size, alloc);
            if (e != SF_OK) { sf_xfree(alloc, buf); sf_xfree(alloc, sorted); return e; }
            size_t padded = align_up(compressed_size, SF_FSDATA_SECTOR_SIZE);
            sector_length = (int32_t)(padded / SF_FSDATA_SECTOR_SIZE);
            int32_t decompressed_sectors = (int32_t)(align_up(file->size, SF_FSDATA_SECTOR_SIZE) / SF_FSDATA_SECTOR_SIZE);
            memcpy(buf + entry_off, &sector_offset, 4);
            memcpy(buf + entry_off + 4, &decompressed_sectors, 4);
            memcpy(buf + entry_off + 8, &sector_length, 4);
            memcpy(buf + header_size + data_cursor, compressed, compressed_size);
            sf_xfree(alloc, compressed);
            data_cursor += align_up(compressed_size, SF_FSDATA_ALIGNMENT_SIZE);
        } else {
            size_t padded = align_up(file->size, SF_FSDATA_SECTOR_SIZE);
            sector_length = (int32_t)(padded / SF_FSDATA_SECTOR_SIZE);
            memcpy(buf + entry_off, &sector_offset, 4);
            memcpy(buf + entry_off + 4, &sector_length, 4);
            if (file->size > 0)
                memcpy(buf + header_size + data_cursor, file->bytes, file->size);
            data_cursor += align_up(file->size, SF_FSDATA_ALIGNMENT_SIZE);
        }
        (void)prev_id; (void)file_idx;
        prev_id = id;
        file_idx++;
    }

    sf_xfree(alloc, sorted);
    *out_bytes = buf;
    *out_size = total;
    return SF_OK;
}
