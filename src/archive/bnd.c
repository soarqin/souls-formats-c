/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — legacy BND archive container.
 * Mirrors SoulsFormats/Formats/Binder/BND/BND.cs.
 */

#include "souls_formats/sf_bnd.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sf_bnd {
    const sf_allocator_t *alloc;
    sf_bnd_file_t       *files;
    size_t               file_count;
    size_t               file_capacity;
    char                *name_pool;
    size_t               name_pool_size;
    int32_t              internal_version;
    uint16_t             format0;
    uint16_t             format1;
    char                *root_file_path;
};

typedef struct bnd_file_header {
    int32_t  id;
    uint32_t data_offset;
    uint32_t file_size;
    char    *name_utf8;
} bnd_file_header_t;

static bool bnd_name_in_pool(const char *name, const char *pool, size_t pool_size) {
    if (!name || !pool || pool_size == 0) return false;
    uintptr_t n = (uintptr_t)name;
    uintptr_t p = (uintptr_t)pool;
    return n >= p && n < p + pool_size;
}

static void bnd_file_free(sf_bnd_file_t *f, const sf_allocator_t *a,
                          const char *pool, size_t pool_size) {
    if (!f) return;
    if (!bnd_name_in_pool(f->name_utf8, pool, pool_size)) sf_xfree(a, (void *)f->name_utf8);
    sf_xfree(a, (void *)f->data);
    memset(f, 0, sizeof(*f));
}

static sf_result_t bnd_file_dup(sf_bnd_file_t *dst, const sf_bnd_file_t *src,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(dst != NULL);
    SF_CHECK_ARG(src != NULL);
    memset(dst, 0, sizeof(*dst));
    dst->id = src->id;
    dst->size = src->size;
    if (src->name_utf8) {
        dst->name_utf8 = sf_strdup(a, src->name_utf8);
        if (!dst->name_utf8) return SF_ERR_OOM;
    }
    if (src->data && src->size > 0) {
        uint8_t *buf = (uint8_t *)sf_xalloc(a, src->size);
        if (!buf) {
            sf_xfree(a, (void *)dst->name_utf8);
            memset(dst, 0, sizeof(*dst));
            return SF_ERR_OOM;
        }
        memcpy(buf, src->data, src->size);
        dst->data = buf;
    } else {
        dst->size = 0;
    }
    return SF_OK;
}

static sf_result_t bnd_key(char out[64], const char *prefix, size_t index) {
    int n = snprintf(out, 64, "%s_%zu", prefix, index);
    return (n > 0 && n < 64) ? SF_OK : SF_ERR_INTERNAL;
}

static sf_result_t bnd_bulk_copy_names(sf_bnd_file_t *files, bnd_file_header_t *headers,
                                       size_t n, char **out_pool, size_t *out_pool_size,
                                       const sf_allocator_t *a) {
    SF_CHECK_ARG(out_pool != NULL);
    SF_CHECK_ARG(out_pool_size != NULL);
    *out_pool = NULL;
    *out_pool_size = 0;
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        if (!headers[i].name_utf8) continue;
        size_t len = strlen(headers[i].name_utf8);
        if (total > SIZE_MAX - len - 1) return SF_ERR_OUT_OF_RANGE;
        total += len + 1;
    }
    if (total == 0) return SF_OK;
    char *pool = (char *)sf_xalloc(a, total);
    if (!pool) return SF_ERR_OOM;
    char *p = pool;
    for (size_t i = 0; i < n; i++) {
        if (!headers[i].name_utf8) continue;
        strcpy(p, headers[i].name_utf8);
        files[i].name_utf8 = p;
        p += strlen(p) + 1;
    }
    *out_pool = pool;
    *out_pool_size = total;
    return SF_OK;
}

static void bnd_free_headers(bnd_file_header_t *headers, size_t n, const sf_allocator_t *a) {
    if (!headers) return;
    for (size_t i = 0; i < n; i++) sf_xfree(a, headers[i].name_utf8);
    sf_xfree(a, headers);
}

bool sf_bnd_is_format(const uint8_t *data, size_t size) {
    return size >= 4 && data && memcmp(data, "BND\0", 4) == 0;
}

sf_result_t sf_bnd_create(sf_bnd_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);
    sf_bnd_t *b = (sf_bnd_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    b->alloc = a;
    b->root_file_path = sf_strdup(a, "");
    if (!b->root_file_path) {
        sf_xfree(a, b);
        return SF_ERR_OOM;
    }
    *out = b;
    return SF_OK;
}

void sf_bnd_destroy(sf_bnd_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    for (size_t i = 0; i < b->file_count; i++) {
        bnd_file_free(&b->files[i], a, b->name_pool, b->name_pool_size);
    }
    sf_xfree(a, b->name_pool);
    sf_xfree(a, b->files);
    sf_xfree(a, b->root_file_path);
    sf_xfree(a, b);
}

static sf_result_t bnd_read_header(sf_binary_reader_t *br, sf_bnd_t *b,
                                   bnd_file_header_t **out_headers, size_t *out_count) {
    SF_CHECK_ARG(br != NULL);
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(out_headers != NULL);
    SF_CHECK_ARG(out_count != NULL);
    *out_headers = NULL;
    *out_count = 0;

    sf_binary_reader_set_big_endian(br, false);
    sf_result_t r = sf_binary_reader_assert_ascii(br, "BND");
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);      if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u16_one(br, 0xFFFF); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u16_one(br, 0);      if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &b->internal_version); if (r != SF_OK) return r;
    r = sf_binary_reader_skip(br, 4); if (r != SF_OK) return r;

    int32_t file_count = 0;
    int32_t root_offset = 0;
    r = sf_binary_reader_read_i32(br, &file_count);  if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &root_offset); if (r != SF_OK) return r;
    if (file_count < 0 || root_offset < 0) return SF_ERR_OUT_OF_RANGE;

    if (root_offset != 0) {
        char *root = NULL;
        r = sf_binary_reader_get_shift_jis(br, root_offset, &root, NULL);
        if (r != SF_OK) return r;
        sf_xfree(b->alloc, b->root_file_path);
        b->root_file_path = root;
    }

    r = sf_binary_reader_read_u16(br, &b->format0); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u16(br, &b->format1); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);     if (r != SF_OK) return r;

    bnd_file_header_t *headers = NULL;
    if (file_count > 0) {
        headers = (bnd_file_header_t *)sf_xalloc(b->alloc, (size_t)file_count * sizeof(*headers));
        if (!headers) return SF_ERR_OOM;
        memset(headers, 0, (size_t)file_count * sizeof(*headers));
    }

    for (int32_t i = 0; i < file_count; i++) {
        uint32_t name_offset = 0;
        r = sf_binary_reader_read_i32(br, &headers[i].id);          if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_u32(br, &headers[i].data_offset); if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_u32(br, &headers[i].file_size);   if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_u32(br, &name_offset);            if (r != SF_OK) goto fail;
        if (name_offset != 0) {
            r = sf_binary_reader_get_shift_jis(br, (int64_t)name_offset,
                                               &headers[i].name_utf8, NULL);
            if (r != SF_OK) goto fail;
        } else {
            char generated[64];
            int n = snprintf(generated, sizeof generated, "File_%d", headers[i].id);
            if (n <= 0 || n >= (int)sizeof generated) { r = SF_ERR_INTERNAL; goto fail; }
            headers[i].name_utf8 = sf_strdup(b->alloc, generated);
            if (!headers[i].name_utf8) { r = SF_ERR_OOM; goto fail; }
        }
    }

    *out_headers = headers;
    *out_count = (size_t)file_count;
    return SF_OK;

fail:
    bnd_free_headers(headers, (size_t)file_count, b->alloc);
    return r;
}

static sf_result_t bnd_populate_from_reader(sf_bnd_t *b, sf_binary_reader_t *br) {
    bnd_file_header_t *headers = NULL;
    size_t n = 0;
    sf_result_t r = bnd_read_header(br, b, &headers, &n);
    if (r != SF_OK) return r;

    if (n > 0) {
        b->files = (sf_bnd_file_t *)sf_xalloc(b->alloc, n * sizeof(*b->files));
        if (!b->files) { r = SF_ERR_OOM; goto cleanup_headers; }
        memset(b->files, 0, n * sizeof(*b->files));
        b->file_capacity = n;
        r = bnd_bulk_copy_names(b->files, headers, n, &b->name_pool,
                                &b->name_pool_size, b->alloc);
        if (r != SF_OK) goto cleanup_files;
    }

    for (size_t i = 0; i < n; i++) {
        b->files[i].id = headers[i].id;
        b->files[i].size = (size_t)headers[i].file_size;
        if (headers[i].file_size > 0) {
            uint8_t *data = (uint8_t *)sf_xalloc(b->alloc, headers[i].file_size);
            if (!data) { r = SF_ERR_OOM; goto cleanup_files; }
            r = sf_binary_reader_get_bytes(br, (int64_t)headers[i].data_offset,
                                           data, headers[i].file_size);
            if (r != SF_OK) { sf_xfree(b->alloc, data); goto cleanup_files; }
            b->files[i].data = data;
        }
        b->file_count++;
    }

    bnd_free_headers(headers, n, b->alloc);
    return SF_OK;

cleanup_files:
    for (size_t i = 0; i < b->file_count; i++) {
        bnd_file_free(&b->files[i], b->alloc, b->name_pool, b->name_pool_size);
    }
    sf_xfree(b->alloc, b->name_pool);
    sf_xfree(b->alloc, b->files);
    b->name_pool = NULL;
    b->name_pool_size = 0;
    b->files = NULL;
    b->file_count = 0;
    b->file_capacity = 0;
cleanup_headers:
    bnd_free_headers(headers, n, b->alloc);
    return r;
}

static sf_result_t bnd_open_decompressed(sf_binary_reader_t *raw, sf_binary_reader_t **out,
                                         bool *out_owns, const sf_allocator_t *a) {
    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof info);
    sf_result_t r = sf_get_decompressed_reader(raw, out, &info, a);
    if (r != SF_OK) return r;
    *out_owns = (info.type != SF_DCX_TYPE_NONE);
    return SF_OK;
}

sf_result_t sf_bnd_read_from_memory(sf_bnd_t **out, const uint8_t *data, size_t size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL || size == 0);
    a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, data, size, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *raw = NULL;
    r = sf_binary_reader_create(&raw, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }
    sf_binary_reader_t *br = NULL;
    bool owns = false;
    r = bnd_open_decompressed(raw, &br, &owns, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw); sf_istream_close(is); return r; }
    sf_bnd_t *b = NULL;
    r = sf_bnd_create(&b, a);
    if (r == SF_OK) {
        r = bnd_populate_from_reader(b, br);
        if (r != SF_OK) { sf_bnd_destroy(b); b = NULL; }
    }
    if (r == SF_OK) *out = b;
    if (owns) sf_binary_reader_destroy(br);
    sf_binary_reader_destroy(raw);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_bnd_read_from_path(sf_bnd_t **out, const wchar_t *path, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *raw = NULL;
    r = sf_binary_reader_create(&raw, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }
    sf_binary_reader_t *br = NULL;
    bool owns = false;
    r = bnd_open_decompressed(raw, &br, &owns, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw); sf_istream_close(is); return r; }
    sf_bnd_t *b = NULL;
    r = sf_bnd_create(&b, a);
    if (r == SF_OK) {
        r = bnd_populate_from_reader(b, br);
        if (r != SF_OK) { sf_bnd_destroy(b); b = NULL; }
    }
    if (r == SF_OK) *out = b;
    if (owns) sf_binary_reader_destroy(br);
    sf_binary_reader_destroy(raw);
    sf_istream_close(is);
    return r;
}

static sf_result_t bnd_write_to_writer(const sf_bnd_t *b, sf_binary_writer_t *bw) {
    sf_result_t r;
    sf_binary_writer_set_big_endian(bw, false);
    r = sf_binary_writer_write_ascii(bw, "BND", false); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);           if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, 0xFFFF);            if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, 0);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, b->internal_version); if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "fileSize");     if (r != SF_OK) return r;
    if (b->file_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_writer_write_i32(bw, (int32_t)b->file_count); if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "rootFilePath"); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, b->format0);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, b->format1);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                 if (r != SF_OK) return r;

    for (size_t i = 0; i < b->file_count; i++) {
        char off_key[64], name_key[64];
        r = bnd_key(off_key, "dataOffset", i); if (r != SF_OK) return r;
        r = bnd_key(name_key, "nameOffset", i); if (r != SF_OK) return r;
        if (b->files[i].size > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_write_i32(bw, b->files[i].id); if (r != SF_OK) return r;
        r = sf_binary_writer_reserve_i32(bw, off_key);      if (r != SF_OK) return r;
        r = sf_binary_writer_write_u32(bw, (uint32_t)b->files[i].size); if (r != SF_OK) return r;
        r = sf_binary_writer_reserve_i32(bw, name_key);     if (r != SF_OK) return r;
    }

    if (b->root_file_path && b->root_file_path[0] != '\0') {
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, "rootFilePath", (int32_t)pos); if (r != SF_OK) return r;
        r = sf_binary_writer_write_shift_jis(bw, b->root_file_path, true); if (r != SF_OK) return r;
    } else {
        r = sf_binary_writer_fill_i32(bw, "rootFilePath", 0); if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < b->file_count; i++) {
        char name_key[64], generated[64];
        const char *name = b->files[i].name_utf8;
        r = bnd_key(name_key, "nameOffset", i); if (r != SF_OK) return r;
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, name_key, (int32_t)pos); if (r != SF_OK) return r;
        if (!name || name[0] == '\0') {
            int n = snprintf(generated, sizeof generated, "File_%zu", i);
            if (n <= 0 || n >= (int)sizeof generated) return SF_ERR_INTERNAL;
            name = generated;
        }
        r = sf_binary_writer_write_shift_jis(bw, name, true); if (r != SF_OK) return r;
    }
    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;

    for (size_t i = 0; i < b->file_count; i++) {
        char off_key[64];
        r = bnd_key(off_key, "dataOffset", i); if (r != SF_OK) return r;
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, off_key, (int32_t)pos); if (r != SF_OK) return r;
        if (b->files[i].size > 0) {
            r = sf_binary_writer_write_bytes(bw, b->files[i].data, b->files[i].size);
            if (r != SF_OK) return r;
        }
        if (i + 1 < b->file_count) {
            r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
        }
    }
    int64_t end = sf_binary_writer_position(bw);
    if (end > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(bw, "fileSize", (int32_t)end);
}

sf_result_t sf_bnd_write_to_memory(const sf_bnd_t *b, uint8_t **out, size_t *out_size,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    a = sf_alloc_or_default(a);
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, a);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, a);
    if (r != SF_OK) { sf_ostream_close(os); return r; }
    r = bnd_write_to_writer(b, bw);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out, out_size);
    else sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_bnd_write_to_path(const sf_bnd_t *b, const wchar_t *path) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(path != NULL);
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, b->alloc);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, b->alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }
    r = bnd_write_to_writer(b, bw);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    else sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

size_t sf_bnd_file_count(const sf_bnd_t *b) { return b ? b->file_count : 0; }

const sf_bnd_file_t *sf_bnd_get_file(const sf_bnd_t *b, size_t index) {
    if (!b || index >= b->file_count) return NULL;
    return &b->files[index];
}

sf_result_t sf_bnd_add_file(sf_bnd_t *b, const sf_bnd_file_t *file) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(file != NULL);
    if (b->file_count == b->file_capacity) {
        size_t new_cap = b->file_capacity ? b->file_capacity * 2 : 8;
        if (new_cap < b->file_capacity) return SF_ERR_OUT_OF_RANGE;
        void *p = sf_xrealloc(b->alloc, b->files, b->file_capacity * sizeof(*b->files),
                              new_cap * sizeof(*b->files));
        if (!p) return SF_ERR_OOM;
        b->files = (sf_bnd_file_t *)p;
        memset(&b->files[b->file_capacity], 0, (new_cap - b->file_capacity) * sizeof(*b->files));
        b->file_capacity = new_cap;
    }
    sf_result_t r = bnd_file_dup(&b->files[b->file_count], file, b->alloc);
    if (r != SF_OK) return r;
    b->file_count++;
    return SF_OK;
}

sf_result_t sf_bnd_remove_file(sf_bnd_t *b, size_t index) {
    SF_CHECK_ARG(b != NULL);
    if (index >= b->file_count) return SF_ERR_OUT_OF_RANGE;
    bnd_file_free(&b->files[index], b->alloc, b->name_pool, b->name_pool_size);
    for (size_t i = index + 1; i < b->file_count; i++) b->files[i - 1] = b->files[i];
    b->file_count--;
    memset(&b->files[b->file_count], 0, sizeof(*b->files));
    return SF_OK;
}

int32_t sf_bnd_get_internal_version(const sf_bnd_t *b) { return b ? b->internal_version : 0; }
uint16_t sf_bnd_get_format0(const sf_bnd_t *b) { return b ? b->format0 : 0; }
uint16_t sf_bnd_get_format1(const sf_bnd_t *b) { return b ? b->format1 : 0; }
const char *sf_bnd_get_root_file_path(const sf_bnd_t *b) { return b ? b->root_file_path : NULL; }

void sf_bnd_set_internal_version(sf_bnd_t *b, int32_t v) { if (b) b->internal_version = v; }
void sf_bnd_set_format0(sf_bnd_t *b, uint16_t v) { if (b) b->format0 = v; }
void sf_bnd_set_format1(sf_bnd_t *b, uint16_t v) { if (b) b->format1 = v; }
void sf_bnd_set_root_file_path(sf_bnd_t *b, const char *path_utf8) {
    if (!b) return;
    char *dup = sf_strdup(b->alloc, path_utf8 ? path_utf8 : "");
    if (!dup) return;
    sf_xfree(b->alloc, b->root_file_path);
    b->root_file_path = dup;
}
