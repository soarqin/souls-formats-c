/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — legacy Binder2 (BND2) archive container.
 * Mirrors the upstream SoulsFormats BND2 C# files.
 */

#include "souls_formats/sf_bnd2.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sf_bnd2 {
    const sf_allocator_t       *alloc;
    sf_bnd2_file_t             *files;
    size_t                      file_count;
    size_t                      file_capacity;
    char                       *name_pool;
    size_t                      name_pool_size;
    sf_bnd2_header_info_flags_t header_info_flags;
    sf_bnd2_file_info_flags_t   file_info_flags;
    uint8_t                     unk06;
    uint8_t                     unk07;
    int32_t                     file_version;
    uint16_t                    alignment_size;
    sf_bnd2_file_path_mode_t    file_path_mode;
    uint8_t                     unk1b;
    char                       *base_directory;
};

struct sf_bnd2_reader {
    const sf_allocator_t       *alloc;
    sf_binary_reader_t         *br;
    sf_bnd2_file_header_t      *headers;
    size_t                      file_count;
    char                       *name_pool;
    size_t                      name_pool_size;
    sf_bnd2_header_info_flags_t header_info_flags;
    sf_bnd2_file_info_flags_t   file_info_flags;
    uint8_t                     unk06;
    uint8_t                     unk07;
    int32_t                     file_version;
    uint16_t                    alignment_size;
    sf_bnd2_file_path_mode_t    file_path_mode;
    uint8_t                     unk1b;
    char                       *base_directory;
};

static uint16_t bnd2_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int32_t bnd2_le32s(const uint8_t *p) {
    uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                 ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (int32_t)v;
}

static uint32_t bnd2_le32u(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool sf_bnd2_is_format(const uint8_t *data, size_t size) {
    if (!data || size < 32 || memcmp(data, "BND\0", 4) != 0) return false;
    int32_t file_version = bnd2_le32s(data + 0x08);
    int32_t base_dir_offset = bnd2_le32s(data + 0x14);
    (void)bnd2_le16(data + 0x18);
    uint8_t file_path_mode = data[0x1A];
    uint8_t unk1b = data[0x1B];
    uint32_t unk1c = bnd2_le32u(data + 0x1C);
    bool valid_names_offset = false;
    switch (file_path_mode) {
    case SF_BND2_FILE_PATH_MODE_NAMELESS:
    case SF_BND2_FILE_PATH_MODE_FILE_NAME:
    case SF_BND2_FILE_PATH_MODE_FULL_PATH:
        valid_names_offset = base_dir_offset >= 0 && (size_t)base_dir_offset <= size && base_dir_offset == 0;
        break;
    case SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY:
        valid_names_offset = base_dir_offset >= 0 && (size_t)base_dir_offset <= size;
        break;
    default:
        return false;
    }
    return file_version >= 202 && file_version <= 211 && valid_names_offset &&
           (unk1b == 0 || unk1b == 1) && unk1c == 0;
}

static bool bnd2_name_in_pool(const char *name, const char *pool, size_t pool_size) {
    if (!name || !pool || pool_size == 0) return false;
    uintptr_t n = (uintptr_t)name;
    uintptr_t p = (uintptr_t)pool;
    return n >= p && n < p + pool_size;
}

static void bnd2_file_free(sf_bnd2_file_t *f, const sf_allocator_t *a,
                           const char *pool, size_t pool_size) {
    if (!f) return;
    if (!bnd2_name_in_pool(f->name_utf8, pool, pool_size)) sf_xfree(a, (void *)f->name_utf8);
    sf_xfree(a, (void *)f->data);
    memset(f, 0, sizeof(*f));
}

static sf_result_t bnd2_file_dup(sf_bnd2_file_t *dst, const sf_bnd2_file_t *src,
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

static sf_result_t bnd2_key(char out[64], const char *prefix, size_t index) {
    int n = snprintf(out, 64, "%s_%zu", prefix, index);
    return (n > 0 && n < 64) ? SF_OK : SF_ERR_INTERNAL;
}

static void bnd2_free_headers(sf_bnd2_file_header_t *headers, size_t n,
                              const sf_allocator_t *a) {
    if (!headers) return;
    for (size_t i = 0; i < n; i++) sf_xfree(a, (void *)headers[i].name_utf8);
    sf_xfree(a, headers);
}

static sf_result_t bnd2_bulk_copy_header_names(sf_bnd2_file_header_t *headers, size_t n,
                                               char **out_pool, size_t *out_pool_size,
                                               const sf_allocator_t *a) {
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
        const char *old = headers[i].name_utf8;
        strcpy(p, old);
        headers[i].name_utf8 = p;
        sf_xfree(a, (void *)old);
        p += strlen(p) + 1;
    }
    *out_pool = pool;
    *out_pool_size = total;
    return SF_OK;
}

static sf_result_t bnd2_copy_header_names_to_files(sf_bnd2_file_t *files,
                                                   const sf_bnd2_file_header_t *headers,
                                                   size_t n, char **out_pool,
                                                   size_t *out_pool_size,
                                                   const sf_allocator_t *a) {
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

sf_result_t sf_bnd2_create(sf_bnd2_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);
    sf_bnd2_t *b = (sf_bnd2_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    b->alloc = a;
    b->header_info_flags = 0xFF;
    b->file_info_flags = 0xFF;
    b->file_version = 211;
    b->alignment_size = 2048;
    b->file_path_mode = SF_BND2_FILE_PATH_MODE_FILE_NAME;
    b->base_directory = sf_strdup(a, "");
    if (!b->base_directory) { sf_xfree(a, b); return SF_ERR_OOM; }
    *out = b;
    return SF_OK;
}

void sf_bnd2_destroy(sf_bnd2_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    for (size_t i = 0; i < b->file_count; i++) {
        bnd2_file_free(&b->files[i], a, b->name_pool, b->name_pool_size);
    }
    sf_xfree(a, b->name_pool);
    sf_xfree(a, b->files);
    sf_xfree(a, b->base_directory);
    sf_xfree(a, b);
}

static sf_result_t bnd2_read_header(sf_binary_reader_t *br,
                                    sf_bnd2_header_info_flags_t *header_info_flags,
                                    sf_bnd2_file_info_flags_t *file_info_flags,
                                    uint8_t *unk06, uint8_t *unk07,
                                    int32_t *file_version, uint16_t *alignment_size,
                                    sf_bnd2_file_path_mode_t *file_path_mode,
                                    uint8_t *unk1b, char **base_directory,
                                    sf_bnd2_file_header_t **out_headers,
                                    size_t *out_count, const sf_allocator_t *a) {
    sf_binary_reader_set_big_endian(br, false);
    *out_headers = NULL;
    *out_count = 0;
    sf_result_t r = sf_binary_reader_assert_ascii(br, "BND"); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, header_info_flags); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, file_info_flags);   if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, unk06);             if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, unk07);             if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, file_version);     if (r != SF_OK) return r;
    r = sf_binary_reader_skip(br, 4);                    if (r != SF_OK) return r;
    int32_t file_count = 0;
    int32_t base_dir_offset = 0;
    r = sf_binary_reader_read_i32(br, &file_count);      if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &base_dir_offset); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u16(br, alignment_size);   if (r != SF_OK) return r;
    static const uint8_t path_modes[] = { 0, 1, 2, 3 };
    r = sf_binary_reader_read_enum_8(br, 4, path_modes, file_path_mode); if (r != SF_OK) return r;
    static const uint8_t unk1b_options[] = { 0, 1 };
    r = sf_binary_reader_assert_u8(br, 2, unk1b_options, unk1b); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u32_one(br, 0); if (r != SF_OK) return r;
    if ((*file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) == 0) {
        r = sf_binary_reader_assert_u32_one(br, 0); if (r != SF_OK) return r;
    }
    if (file_count < 0 || base_dir_offset < 0) return SF_ERR_OUT_OF_RANGE;
    sf_xfree(a, *base_directory);
    *base_directory = NULL;
    if (*file_path_mode == SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY &&
        (*file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) != 0) {
        r = sf_binary_reader_get_shift_jis(br, base_dir_offset, base_directory, NULL);
        if (r != SF_OK) return r;
    } else {
        *base_directory = sf_strdup(a, "");
        if (!*base_directory) return SF_ERR_OOM;
    }

    sf_bnd2_file_header_t *headers = NULL;
    if (file_count > 0) {
        headers = (sf_bnd2_file_header_t *)sf_xalloc(a, (size_t)file_count * sizeof(*headers));
        if (!headers) return SF_ERR_OOM;
        memset(headers, 0, (size_t)file_count * sizeof(*headers));
    }
    for (int32_t i = 0; i < file_count; i++) {
        r = sf_binary_reader_read_i32(br, &headers[i].id);     if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &headers[i].offset); if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &headers[i].size);   if (r != SF_OK) goto fail;
        if (headers[i].offset < 0 || headers[i].size < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }
        if ((*file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) != 0) {
            int32_t name_offset = 0;
            r = sf_binary_reader_read_i32(br, &name_offset); if (r != SF_OK) goto fail;
            if (*file_path_mode == SF_BND2_FILE_PATH_MODE_NAMELESS) {
                char generated[64];
                int n = snprintf(generated, sizeof generated, "%d", headers[i].id);
                if (n <= 0 || n >= (int)sizeof generated) { r = SF_ERR_INTERNAL; goto fail; }
                headers[i].name_utf8 = sf_strdup(a, generated);
                if (!headers[i].name_utf8) { r = SF_ERR_OOM; goto fail; }
            } else {
                if (name_offset < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }
                r = sf_binary_reader_get_shift_jis(br, name_offset, (char **)&headers[i].name_utf8, NULL);
                if (r != SF_OK) goto fail;
            }
        } else {
            headers[i].name_utf8 = sf_strdup(a, "");
            if (!headers[i].name_utf8) { r = SF_ERR_OOM; goto fail; }
        }
    }
    *out_headers = headers;
    *out_count = (size_t)file_count;
    return SF_OK;

fail:
    bnd2_free_headers(headers, (size_t)file_count, a);
    return r;
}

static sf_result_t bnd2_populate_from_reader(sf_bnd2_t *b, sf_binary_reader_t *br) {
    sf_bnd2_file_header_t *headers = NULL;
    size_t n = 0;
    sf_result_t r = bnd2_read_header(br, &b->header_info_flags, &b->file_info_flags,
                                    &b->unk06, &b->unk07, &b->file_version,
                                    &b->alignment_size, &b->file_path_mode, &b->unk1b,
                                    &b->base_directory, &headers, &n, b->alloc);
    if (r != SF_OK) return r;
    if (n > 0) {
        b->files = (sf_bnd2_file_t *)sf_xalloc(b->alloc, n * sizeof(*b->files));
        if (!b->files) { r = SF_ERR_OOM; goto cleanup_headers; }
        memset(b->files, 0, n * sizeof(*b->files));
        b->file_capacity = n;
        r = bnd2_copy_header_names_to_files(b->files, headers, n, &b->name_pool,
                                            &b->name_pool_size, b->alloc);
        if (r != SF_OK) goto cleanup_files;
    }
    for (size_t i = 0; i < n; i++) {
        b->files[i].id = headers[i].id;
        b->files[i].size = (size_t)headers[i].size;
        if (headers[i].size > 0) {
            uint8_t *data = (uint8_t *)sf_xalloc(b->alloc, (size_t)headers[i].size);
            if (!data) { r = SF_ERR_OOM; goto cleanup_files; }
            r = sf_binary_reader_get_bytes(br, headers[i].offset, data, (size_t)headers[i].size);
            if (r != SF_OK) { sf_xfree(b->alloc, data); goto cleanup_files; }
            b->files[i].data = data;
        }
        b->file_count++;
    }
    bnd2_free_headers(headers, n, b->alloc);
    return SF_OK;

cleanup_files:
    for (size_t i = 0; i < b->file_count; i++) {
        bnd2_file_free(&b->files[i], b->alloc, b->name_pool, b->name_pool_size);
    }
    sf_xfree(b->alloc, b->name_pool);
    sf_xfree(b->alloc, b->files);
    b->name_pool = NULL;
    b->name_pool_size = 0;
    b->files = NULL;
    b->file_count = 0;
    b->file_capacity = 0;
cleanup_headers:
    bnd2_free_headers(headers, n, b->alloc);
    return r;
}

static sf_result_t bnd2_open_decompressed(sf_binary_reader_t *raw, sf_binary_reader_t **out,
                                          bool *out_owns, const sf_allocator_t *a) {
    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof info);
    sf_result_t r = sf_get_decompressed_reader(raw, out, &info, a);
    if (r != SF_OK) return r;
    *out_owns = (info.type != SF_DCX_TYPE_NONE);
    return SF_OK;
}

sf_result_t sf_bnd2_read_from_memory(sf_bnd2_t **out, const uint8_t *data, size_t size,
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
    r = bnd2_open_decompressed(raw, &br, &owns, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw); sf_istream_close(is); return r; }
    sf_bnd2_t *b = NULL;
    r = sf_bnd2_create(&b, a);
    if (r == SF_OK) {
        r = bnd2_populate_from_reader(b, br);
        if (r != SF_OK) { sf_bnd2_destroy(b); b = NULL; }
    }
    if (r == SF_OK) *out = b;
    if (owns) sf_binary_reader_destroy(br);
    sf_binary_reader_destroy(raw);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_bnd2_read_from_path(sf_bnd2_t **out, const wchar_t *path,
                                   const sf_allocator_t *a) {
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
    r = bnd2_open_decompressed(raw, &br, &owns, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw); sf_istream_close(is); return r; }
    sf_bnd2_t *b = NULL;
    r = sf_bnd2_create(&b, a);
    if (r == SF_OK) {
        r = bnd2_populate_from_reader(b, br);
        if (r != SF_OK) { sf_bnd2_destroy(b); b = NULL; }
    }
    if (r == SF_OK) *out = b;
    if (owns) sf_binary_reader_destroy(br);
    sf_binary_reader_destroy(raw);
    sf_istream_close(is);
    return r;
}

static sf_result_t bnd2_write_name(sf_binary_writer_t *bw, const char *name,
                                   sf_bnd2_file_path_mode_t mode,
                                   const sf_allocator_t *a) {
    if (mode != SF_BND2_FILE_PATH_MODE_FULL_PATH) {
        return sf_binary_writer_write_shift_jis(bw, name ? name : "", true);
    }
    if (name && strlen(name) > 1 && name[1] == ':') {
        return sf_binary_writer_write_shift_jis(bw, name, true);
    }
    size_t name_len = name ? strlen(name) : 0;
    if (name_len > SIZE_MAX - 4) return SF_ERR_OUT_OF_RANGE;
    char *tmp = (char *)sf_xalloc(a, name_len + 4);
    if (!tmp) return SF_ERR_OOM;
    tmp[0] = 'K';
    tmp[1] = ':';
    tmp[2] = '\\';
    if (name_len > 0) memcpy(tmp + 3, name, name_len);
    tmp[name_len + 3] = '\0';
    sf_result_t r = sf_binary_writer_write_shift_jis(bw, tmp, true);
    sf_xfree(a, tmp);
    return r;
}
static sf_result_t bnd2_pad_data(sf_binary_writer_t *bw, uint16_t align) {
    return align > 0 ? sf_binary_writer_pad(bw, align) : SF_OK;
}

static sf_result_t bnd2_write_to_writer(const sf_bnd2_t *b, sf_binary_writer_t *bw) {
    sf_result_t r;
    sf_binary_writer_set_big_endian(bw, false);
    r = sf_binary_writer_write_ascii(bw, "BND", false); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                  if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->header_info_flags); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->file_info_flags);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->unk06);             if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->unk07);             if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, b->file_version);     if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "fileSize");       if (r != SF_OK) return r;
    if (b->file_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_writer_write_i32(bw, (int32_t)b->file_count); if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "baseDirOffset");  if (r != SF_OK) return r;
    r = sf_binary_writer_write_u16(bw, b->alignment_size);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->file_path_mode);    if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, b->unk1b);             if (r != SF_OK) return r;
    r = sf_binary_writer_write_u32(bw, 0);                   if (r != SF_OK) return r;
    if ((b->file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) == 0) {
        r = sf_binary_writer_write_u32(bw, 0); if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < b->file_count; i++) {
        char off_key[64], size_key[64], name_key[64];
        r = bnd2_key(off_key, "fileOffset", i); if (r != SF_OK) return r;
        r = bnd2_key(size_key, "fileSize", i);  if (r != SF_OK) return r;
        r = bnd2_key(name_key, "nameOffset", i); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, b->files[i].id); if (r != SF_OK) return r;
        r = sf_binary_writer_reserve_i32(bw, off_key);      if (r != SF_OK) return r;
        r = sf_binary_writer_reserve_i32(bw, size_key);     if (r != SF_OK) return r;
        if ((b->file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) != 0) {
            if (b->file_path_mode == SF_BND2_FILE_PATH_MODE_NAMELESS) {
                r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
            } else {
                r = sf_binary_writer_reserve_i32(bw, name_key); if (r != SF_OK) return r;
            }
        }
    }

    if ((b->file_info_flags & SF_BND2_FILE_INFO_NAME_OFFSET) != 0) {
        if (b->file_path_mode == SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY) {
            int64_t pos = sf_binary_writer_position(bw);
            if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
            r = sf_binary_writer_fill_i32(bw, "baseDirOffset", (int32_t)pos); if (r != SF_OK) return r;
            r = sf_binary_writer_write_shift_jis(bw, b->base_directory ? b->base_directory : "", true);
            if (r != SF_OK) return r;
        } else {
            r = sf_binary_writer_fill_i32(bw, "baseDirOffset", 0); if (r != SF_OK) return r;
        }
        if (b->file_path_mode != SF_BND2_FILE_PATH_MODE_NAMELESS) {
            for (size_t i = 0; i < b->file_count; i++) {
                char name_key[64];
                r = bnd2_key(name_key, "nameOffset", i); if (r != SF_OK) return r;
                int64_t pos = sf_binary_writer_position(bw);
                if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
                r = sf_binary_writer_fill_i32(bw, name_key, (int32_t)pos); if (r != SF_OK) return r;
                r = bnd2_write_name(bw, b->files[i].name_utf8, b->file_path_mode, b->alloc); if (r != SF_OK) return r;
            }
        }
    } else {
        r = sf_binary_writer_fill_i32(bw, "baseDirOffset", 0); if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < b->file_count; i++) {
        char off_key[64], size_key[64];
        r = bnd2_key(off_key, "fileOffset", i); if (r != SF_OK) return r;
        r = bnd2_key(size_key, "fileSize", i);  if (r != SF_OK) return r;
        r = bnd2_pad_data(bw, b->alignment_size); if (r != SF_OK) return r;
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX || b->files[i].size > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_fill_i32(bw, off_key, (int32_t)pos); if (r != SF_OK) return r;
        r = sf_binary_writer_fill_i32(bw, size_key, (int32_t)b->files[i].size); if (r != SF_OK) return r;
        if (b->files[i].size > 0) {
            r = sf_binary_writer_write_bytes(bw, b->files[i].data, b->files[i].size);
            if (r != SF_OK) return r;
        }
    }
    int64_t end = sf_binary_writer_position(bw);
    if (end > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(bw, "fileSize", (int32_t)end);
}

sf_result_t sf_bnd2_write_to_memory(const sf_bnd2_t *b, uint8_t **out, size_t *out_size,
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
    r = bnd2_write_to_writer(b, bw);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out, out_size);
    else sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_bnd2_write_to_path(const sf_bnd2_t *b, const wchar_t *path) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(path != NULL);
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, b->alloc);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, b->alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }
    r = bnd2_write_to_writer(b, bw);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    else sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

size_t sf_bnd2_file_count(const sf_bnd2_t *b) { return b ? b->file_count : 0; }

const sf_bnd2_file_t *sf_bnd2_get_file(const sf_bnd2_t *b, size_t index) {
    if (!b || index >= b->file_count) return NULL;
    return &b->files[index];
}

sf_result_t sf_bnd2_add_file(sf_bnd2_t *b, const sf_bnd2_file_t *file) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(file != NULL);
    if (b->file_count == b->file_capacity) {
        size_t new_cap = b->file_capacity ? b->file_capacity * 2 : 8;
        if (new_cap < b->file_capacity) return SF_ERR_OUT_OF_RANGE;
        void *p = sf_xrealloc(b->alloc, b->files, b->file_capacity * sizeof(*b->files),
                              new_cap * sizeof(*b->files));
        if (!p) return SF_ERR_OOM;
        b->files = (sf_bnd2_file_t *)p;
        memset(&b->files[b->file_capacity], 0, (new_cap - b->file_capacity) * sizeof(*b->files));
        b->file_capacity = new_cap;
    }
    sf_result_t r = bnd2_file_dup(&b->files[b->file_count], file, b->alloc);
    if (r != SF_OK) return r;
    b->file_count++;
    return SF_OK;
}

sf_result_t sf_bnd2_remove_file(sf_bnd2_t *b, size_t index) {
    SF_CHECK_ARG(b != NULL);
    if (index >= b->file_count) return SF_ERR_OUT_OF_RANGE;
    bnd2_file_free(&b->files[index], b->alloc, b->name_pool, b->name_pool_size);
    for (size_t i = index + 1; i < b->file_count; i++) b->files[i - 1] = b->files[i];
    b->file_count--;
    memset(&b->files[b->file_count], 0, sizeof(*b->files));
    return SF_OK;
}

sf_bnd2_header_info_flags_t sf_bnd2_get_header_info_flags(const sf_bnd2_t *b) { return b ? b->header_info_flags : 0; }
sf_bnd2_file_info_flags_t sf_bnd2_get_file_info_flags(const sf_bnd2_t *b) { return b ? b->file_info_flags : 0; }
uint8_t sf_bnd2_get_unk06(const sf_bnd2_t *b) { return b ? b->unk06 : 0; }
uint8_t sf_bnd2_get_unk07(const sf_bnd2_t *b) { return b ? b->unk07 : 0; }
int32_t sf_bnd2_get_file_version(const sf_bnd2_t *b) { return b ? b->file_version : 0; }
uint16_t sf_bnd2_get_alignment_size(const sf_bnd2_t *b) { return b ? b->alignment_size : 0; }
sf_bnd2_file_path_mode_t sf_bnd2_get_file_path_mode(const sf_bnd2_t *b) { return b ? b->file_path_mode : 0; }
uint8_t sf_bnd2_get_unk1b(const sf_bnd2_t *b) { return b ? b->unk1b : 0; }
const char *sf_bnd2_get_base_directory(const sf_bnd2_t *b) { return b ? b->base_directory : NULL; }

void sf_bnd2_set_header_info_flags(sf_bnd2_t *b, sf_bnd2_header_info_flags_t v) { if (b) b->header_info_flags = v; }
void sf_bnd2_set_file_info_flags(sf_bnd2_t *b, sf_bnd2_file_info_flags_t v) { if (b) b->file_info_flags = v; }
void sf_bnd2_set_unk06(sf_bnd2_t *b, uint8_t v) { if (b) b->unk06 = v; }
void sf_bnd2_set_unk07(sf_bnd2_t *b, uint8_t v) { if (b) b->unk07 = v; }
void sf_bnd2_set_file_version(sf_bnd2_t *b, int32_t v) { if (b) b->file_version = v; }
void sf_bnd2_set_alignment_size(sf_bnd2_t *b, uint16_t v) { if (b) b->alignment_size = v; }
void sf_bnd2_set_file_path_mode(sf_bnd2_t *b, sf_bnd2_file_path_mode_t v) { if (b) b->file_path_mode = v; }
void sf_bnd2_set_unk1b(sf_bnd2_t *b, uint8_t v) { if (b) b->unk1b = v; }
void sf_bnd2_set_base_directory(sf_bnd2_t *b, const char *base_directory_utf8) {
    if (!b) return;
    char *dup = sf_strdup(b->alloc, base_directory_utf8 ? base_directory_utf8 : "");
    if (!dup) return;
    sf_xfree(b->alloc, b->base_directory);
    b->base_directory = dup;
}

static void bnd2_reader_free(sf_bnd2_reader_t *r) {
    if (!r) return;
    const sf_allocator_t *a = r->alloc;
    sf_xfree(a, r->name_pool);
    sf_xfree(a, r->headers);
    sf_xfree(a, r->base_directory);
    if (r->br) sf_binary_reader_destroy(r->br);
    sf_xfree(a, r);
}

void sf_bnd2_reader_close(sf_bnd2_reader_t *r) { bnd2_reader_free(r); }

sf_result_t sf_bnd2_reader_open(sf_bnd2_reader_t **out, const wchar_t *path,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;
    int64_t total = sf_istream_length(is);
    if (total < 0) { sf_istream_close(is); return SF_ERR_IO; }
    uint8_t *raw_buf = total > 0 ? (uint8_t *)sf_xalloc(a, (size_t)total) : NULL;
    if (total > 0 && !raw_buf) { sf_istream_close(is); return SF_ERR_OOM; }
    if (total > 0) {
        r = sf_istream_read(is, raw_buf, (size_t)total);
        if (r != SF_OK) { sf_xfree(a, raw_buf); sf_istream_close(is); return r; }
    }
    sf_istream_close(is);

    sf_binary_reader_t *raw = NULL;
    r = sf_binary_reader_create_from_memory(&raw, false, raw_buf, (size_t)total, a);
    if (r != SF_OK) { sf_xfree(a, raw_buf); return r; }
    sf_binary_reader_t *br = NULL;
    bool owns = false;
    r = bnd2_open_decompressed(raw, &br, &owns, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw); return r; }
    if (owns) {
        sf_binary_reader_destroy(raw);
        raw = NULL;
    }

    sf_bnd2_reader_t *rd = (sf_bnd2_reader_t *)sf_xalloc(a, sizeof(*rd));
    if (!rd) { r = SF_ERR_OOM; goto fail; }
    memset(rd, 0, sizeof(*rd));
    rd->alloc = a;
    rd->base_directory = sf_strdup(a, "");
    if (!rd->base_directory) { sf_xfree(a, rd); r = SF_ERR_OOM; goto fail; }
    r = bnd2_read_header(br, &rd->header_info_flags, &rd->file_info_flags, &rd->unk06,
                         &rd->unk07, &rd->file_version, &rd->alignment_size,
                         &rd->file_path_mode, &rd->unk1b, &rd->base_directory,
                         &rd->headers, &rd->file_count, a);
    if (r != SF_OK) { bnd2_reader_free(rd); goto fail; }
    r = bnd2_bulk_copy_header_names(rd->headers, rd->file_count, &rd->name_pool,
                                    &rd->name_pool_size, a);
    if (r != SF_OK) { bnd2_reader_free(rd); goto fail; }
    rd->br = owns ? br : raw;
    *out = rd;
    return SF_OK;

fail:
    if (owns) sf_binary_reader_destroy(br);
    else sf_binary_reader_destroy(raw);
    return r;
}

size_t sf_bnd2_reader_file_count(const sf_bnd2_reader_t *r) { return r ? r->file_count : 0; }

const sf_bnd2_file_header_t *sf_bnd2_reader_get_file_header(const sf_bnd2_reader_t *r,
                                                           size_t index) {
    if (!r || index >= r->file_count) return NULL;
    return &r->headers[index];
}

sf_result_t sf_bnd2_reader_read_file_by_index(sf_bnd2_reader_t *r, size_t index,
                                              uint8_t **out, size_t *out_size,
                                              const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    if (index >= r->file_count) return SF_ERR_OUT_OF_RANGE;
    a = sf_alloc_or_default(a);
    const sf_bnd2_file_header_t *h = &r->headers[index];
    *out = NULL;
    *out_size = (size_t)h->size;
    if (h->size == 0) return SF_OK;
    uint8_t *buf = (uint8_t *)sf_xalloc(a, (size_t)h->size);
    if (!buf) return SF_ERR_OOM;
    sf_result_t res = sf_binary_reader_get_bytes(r->br, h->offset, buf, (size_t)h->size);
    if (res != SF_OK) { sf_xfree(a, buf); *out_size = 0; return res; }
    *out = buf;
    return SF_OK;
}

sf_result_t sf_bnd2_reader_read_file_by_id(sf_bnd2_reader_t *r, int32_t id,
                                           uint8_t **out, size_t *out_size,
                                           const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    for (size_t i = 0; i < r->file_count; i++) {
        if (r->headers[i].id == id) return sf_bnd2_reader_read_file_by_index(r, i, out, out_size, a);
    }
    return SF_ERR_NOT_FOUND;
}
