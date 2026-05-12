/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BXF3 split-archive container.
 *
 * Mirrors:
 *   SoulsFormats/Formats/Binder/BXF3/BXF3.cs
 *   SoulsFormats/Formats/Binder/BXF3/BXF3Reader.cs
 *
 * BXF3 layout (no DCX wrapper) — two files, BHD and BDT:
 *
 *   BHD ("BHF3"):
 *     off 0x00  ASCII "BHF3"
 *     off 0x04  fixstr[8] version
 *     off 0x0C  byte    Format          (bit-reversed unless BitBigEndian)
 *     off 0x0D  byte    BigEndian
 *     off 0x0E  byte    BitBigEndian
 *     off 0x0F  byte    0
 *     off 0x10  int32   fileCount
 *     off 0x14  int32   0
 *     off 0x18  int32   0
 *     off 0x1C  int32   0
 *     off 0x20  ── per-entry file headers, then names ──
 *
 *   BDT ("BDF3"):
 *     off 0x00  ASCII "BDF3"
 *     off 0x04  fixstr[8] version
 *     off 0x0C  int32   0
 *     off 0x10  ── padded file payloads (offsets stored in BHD) ──
 */

#include "souls_formats/sf_bxf3.h"

#include "archive/binder_common.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================
 * Private struct definitions
 *===========================================================================*/

struct sf_bxf3 {
    const sf_allocator_t *alloc;

    sf_binder_file_t *files;
    size_t            file_count;
    size_t            file_capacity;
    char             *name_pool;
    size_t            name_pool_size;

    char             *version;
    sf_binder_format_t format;
    bool              big_endian;
    bool              bit_big_endian;
};

struct sf_bxf3_reader {
    const sf_allocator_t *alloc;

    sf_binder_file_t          *files;
    sfi_binder_file_header_t  *headers;
    size_t                     file_count;
    char                      *name_pool;
    size_t                     name_pool_size;

    sf_binary_reader_t        *bhd_br;   /* parsed-header reader (decompressed) */
    sf_binary_reader_t        *bdt_br;   /* data file reader (decompressed)     */
    sf_dcx_compression_info_t  bhd_compression;
    sf_dcx_compression_info_t  bdt_compression;

    char             *version;
    sf_binder_format_t format;
    bool              big_endian;
    bool              bit_big_endian;
};

/*===========================================================================
 * Default-version timestamp (mirrors Binder.DateToBinderTimestamp(DateTime.Now))
 *===========================================================================*/

static void bxf3_default_version(char out[9]) {
    time_t now_t = time(NULL);
    struct tm tm_local;
#if defined(_WIN32)
    localtime_s(&tm_local, &now_t);
#else
    struct tm *p = localtime(&now_t);
    if (p) tm_local = *p; else memset(&tm_local, 0, sizeof tm_local);
#endif
    sf_binder_datetime_t dt = {
        .year   = tm_local.tm_year + 1900,
        .month  = tm_local.tm_mon,
        .day    = (tm_local.tm_mday > 0) ? tm_local.tm_mday : 1,
        .hour   = tm_local.tm_hour,
        .minute = tm_local.tm_min,
    };
    if (dt.year < 2000 || dt.year > 2099) {
        dt.year = 2000;
        dt.month = 0;
        dt.day = 1;
        dt.hour = 0;
        dt.minute = 0;
    }
    if (sf_binder_timestamp_format(&dt, out) != SF_OK) {
        memcpy(out, "00A1A0\0\0", 9);
    }
}

/*===========================================================================
 * sf_binder_file_t deep copy / free helpers
 *===========================================================================*/

static sf_result_t bxf3_file_dup(sf_binder_file_t *dst, const sf_binder_file_t *src,
                                 const sf_allocator_t *a) {
    SF_CHECK_ARG(dst != NULL);
    SF_CHECK_ARG(src != NULL);
    *dst = *src;
    dst->name_utf8 = NULL;
    dst->data = NULL;

    if (src->name_utf8) {
        dst->name_utf8 = sf_strdup(a, src->name_utf8);
        if (!dst->name_utf8) return SF_ERR_OOM;
    }
    if (src->data && src->size > 0) {
        uint8_t *buf = (uint8_t *)sf_xalloc(a, src->size);
        if (!buf) {
            sf_xfree(a, (void *)dst->name_utf8);
            dst->name_utf8 = NULL;
            return SF_ERR_OOM;
        }
        memcpy(buf, src->data, src->size);
        dst->data = buf;
    } else {
        dst->size = 0;
    }
    return SF_OK;
}

static void bxf3_file_free(sf_binder_file_t *f, const sf_allocator_t *a) {
    if (!f) return;
    sf_xfree(a, (void *)f->name_utf8);
    sf_xfree(a, (void *)f->data);
    f->name_utf8 = NULL;
    f->data = NULL;
    f->size = 0;
}

static bool bxf3_name_in_pool(const char *name, const char *pool, size_t pool_size) {
    if (!name || !pool || pool_size == 0) return false;
    uintptr_t n     = (uintptr_t)name;
    uintptr_t start = (uintptr_t)pool;
    return n >= start && n < start + pool_size;
}

static void bxf3_file_free_with_name_pool(sf_binder_file_t *f, const sf_allocator_t *a,
                                          const char *pool, size_t pool_size) {
    if (!f) return;
    if (!bxf3_name_in_pool(f->name_utf8, pool, pool_size)) {
        sf_xfree(a, (void *)f->name_utf8);
    }
    sf_xfree(a, (void *)f->data);
    f->name_utf8 = NULL;
    f->data = NULL;
    f->size = 0;
}

static sf_result_t bxf3_bulk_copy_names(sf_binder_file_t *files,
                                        const sfi_binder_file_header_t *headers,
                                        size_t n, char **out_pool,
                                        size_t *out_pool_size,
                                        const sf_allocator_t *a) {
    SF_CHECK_ARG(files != NULL || n == 0);
    SF_CHECK_ARG(headers != NULL || n == 0);
    SF_CHECK_ARG(out_pool != NULL);
    SF_CHECK_ARG(out_pool_size != NULL);

    *out_pool = NULL;
    *out_pool_size = 0;

    size_t pool_size = 0;
    for (size_t i = 0; i < n; i++) {
        if (!headers[i].name_utf8) continue;
        size_t len = strlen(headers[i].name_utf8);
        if (pool_size > SIZE_MAX - len - 1) return SF_ERR_OUT_OF_RANGE;
        pool_size += len + 1;
    }

    if (pool_size == 0) return SF_OK;

    char *pool = (char *)sf_xalloc(a, pool_size);
    if (!pool) return SF_ERR_OOM;

    char *p = pool;
    for (size_t i = 0; i < n; i++) {
        if (!headers[i].name_utf8) continue;
        strcpy(p, headers[i].name_utf8);
        files[i].name_utf8 = p;
        p += strlen(p) + 1;
    }

    *out_pool = pool;
    *out_pool_size = pool_size;
    return SF_OK;
}

/*===========================================================================
 * sf_bxf3_create / destroy
 *===========================================================================*/

sf_result_t sf_bxf3_create(sf_bxf3_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);

    sf_bxf3_t *b = (sf_bxf3_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    b->alloc = a;

    char ver[9];
    bxf3_default_version(ver);
    b->version = sf_strdup(a, ver);
    if (!b->version) {
        sf_xfree(a, b);
        return SF_ERR_OOM;
    }

    b->format = (sf_binder_format_t)(SF_BINDER_FORMAT_IDS | SF_BINDER_FORMAT_NAMES1
                                     | SF_BINDER_FORMAT_NAMES2
                                     | SF_BINDER_FORMAT_COMPRESSION);
    b->big_endian     = false;
    b->bit_big_endian = false;

    *out = b;
    return SF_OK;
}

void sf_bxf3_destroy(sf_bxf3_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    for (size_t i = 0; i < b->file_count; i++) {
        bxf3_file_free_with_name_pool(&b->files[i], a, b->name_pool, b->name_pool_size);
    }
    sf_xfree(a, b->name_pool);
    sf_xfree(a, b->files);
    sf_xfree(a, b->version);
    sf_xfree(a, b);
}

/*===========================================================================
 * Read: BHF/BDF header parsers
 *
 * BDF header (BXF3.cs:215-220): magic "BDF3", fixstr[8] version, int32 0.
 * BHF header (BXF3.cs:222-246): magic "BHF3", fixstr[8] version, format
 * byte (using bit-BE byte read at offset 0xE first), big-endian flag,
 * bit-BE byte, pad, file count, three reserved zeros, then file headers.
 *===========================================================================*/

typedef struct bxf3_props {
    char              *version;
    sf_binder_format_t format;
    bool               big_endian;
    bool               bit_big_endian;
} bxf3_props_t;

static sf_result_t bxf3_read_bdf_header(sf_binary_reader_t *br, char **out_version,
                                        const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL);
    sf_result_t r = sf_binary_reader_assert_ascii(br, "BDF3");
    if (r != SF_OK) return r;
    char *version = NULL;
    r = sf_binary_reader_read_fix_str(br, 8, &version, NULL);
    if (r != SF_OK) return r;
    if (out_version) {
        if (a && a != sf_default_allocator()) {
            char *copy = sf_strdup(a, version);
            sf_free(NULL, version);
            if (!copy) return SF_ERR_OOM;
            *out_version = copy;
        } else {
            *out_version = version;
        }
    } else {
        sf_free(NULL, version);
    }
    return sf_binary_reader_assert_i32_one(br, 0);
}

static sf_result_t bxf3_read_bhf_header(sf_binary_reader_t        *br,
                                        bxf3_props_t              *props,
                                        sfi_binder_file_header_t **out_headers,
                                        size_t                    *out_count,
                                        const sf_allocator_t      *a) {
    SF_CHECK_ARG(br != NULL);
    SF_CHECK_ARG(props != NULL);
    SF_CHECK_ARG(out_headers != NULL);
    SF_CHECK_ARG(out_count != NULL);

    *out_headers = NULL;
    *out_count   = 0;
    memset(props, 0, sizeof(*props));

    sf_result_t r;
    r = sf_binary_reader_assert_ascii(br, "BHF3"); if (r != SF_OK) return r;

    char *version = NULL;
    r = sf_binary_reader_read_fix_str(br, 8, &version, NULL);
    if (r != SF_OK) return r;
    props->version = version;

    bool bit_big_endian = false;
    r = sf_binary_reader_get_bool(br, 0xE, &bit_big_endian);
    if (r != SF_OK) goto fail;
    props->bit_big_endian = bit_big_endian;

    props->format = sfi_binder_read_format(br, bit_big_endian);

    bool be = false;
    r = sf_binary_reader_read_bool(br, &be);                  if (r != SF_OK) goto fail;
    props->big_endian = be;
    r = sf_binary_reader_assert_bool_one(br, bit_big_endian); if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_u8_one  (br, 0);              if (r != SF_OK) goto fail;

    sf_binary_reader_set_big_endian(br,
        be || sf_binder_format_force_big_endian(props->format));

    int32_t file_count = 0;
    r = sf_binary_reader_read_i32(br, &file_count);   if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_i32_one(br, 0);       if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_i32_one(br, 0);       if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_i32_one(br, 0);       if (r != SF_OK) goto fail;

    if (file_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

    sfi_binder_file_header_t *headers = NULL;
    if (file_count > 0) {
        headers = (sfi_binder_file_header_t *)sf_xalloc(a, (size_t)file_count
                                                        * sizeof(*headers));
        if (!headers) { r = SF_ERR_OOM; goto fail; }
        memset(headers, 0, (size_t)file_count * sizeof(*headers));
    }

    for (int32_t i = 0; i < file_count; i++) {
        r = sfi_binder3_read_file_header(br, props->format, bit_big_endian,
                                         &headers[i], a);
        if (r != SF_OK) {
            for (int32_t k = 0; k <= i; k++) sfi_binder_file_header_destroy(&headers[k], a);
            sf_xfree(a, headers);
            goto fail;
        }
    }

    *out_headers = headers;
    *out_count   = (size_t)file_count;
    return SF_OK;

fail:
    sf_xfree(a, props->version);
    props->version = NULL;
    return r;
}

/*===========================================================================
 * Read: pull file payload bytes from a BHD-parsed header by reading the
 * referenced range from the BDT reader. Mirrors BinderFileHeader.ReadFileData
 * (BinderFileHeader.cs:158-176) but with the data sourced from a separate
 * BDT stream.
 *===========================================================================*/

static sf_result_t bxf3_extract_file_data(sf_binary_reader_t              *bdt_br,
                                          const sfi_binder_file_header_t  *h,
                                          uint8_t                        **out_data,
                                          size_t                          *out_size,
                                          sf_dcx_compression_info_t       *out_info,
                                          const sf_allocator_t            *a) {
    SF_CHECK_ARG(bdt_br != NULL);
    SF_CHECK_ARG(h  != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);

    *out_data = NULL;
    *out_size = 0;

    size_t compressed_size = (size_t)h->compressed_size;
    uint8_t *raw = (compressed_size > 0)
                       ? (uint8_t *)sf_xalloc(a, compressed_size)
                       : NULL;
    if (compressed_size > 0 && !raw) return SF_ERR_OOM;

    if (compressed_size > 0) {
        sf_result_t r = sf_binary_reader_get_bytes(bdt_br, (int64_t)h->data_offset,
                                                   raw, compressed_size);
        if (r != SF_OK) {
            sf_xfree(a, raw);
            return r;
        }
    }

    if ((h->flags & SF_BINDER_FILE_FLAG_COMPRESSED) != 0) {
        uint8_t *decoded = NULL;
        size_t   dsize   = 0;
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof info);
        sf_result_t r = sf_dcx_decompress_from_buffer(raw, compressed_size,
                                                      &decoded, &dsize, &info, a);
        sf_xfree(a, raw);
        if (r != SF_OK) return r;
        if (out_info) *out_info = info;
        *out_data = decoded;
        *out_size = dsize;
    } else {
        if (out_info) {
            memset(out_info, 0, sizeof(*out_info));
            out_info->type = SF_DCX_TYPE_ZLIB;
        }
        *out_data = raw;
        *out_size = compressed_size;
    }
    return SF_OK;
}

/*===========================================================================
 * Eager Read implementation
 *===========================================================================*/

static sf_result_t bxf3_populate_from_readers(sf_bxf3_t           *b,
                                              sf_binary_reader_t  *bhd_br,
                                              sf_binary_reader_t  *bdt_br) {
    sf_result_t r = bxf3_read_bdf_header(bdt_br, NULL, b->alloc);
    if (r != SF_OK) return r;

    bxf3_props_t              props;
    sfi_binder_file_header_t *headers = NULL;
    size_t                    n       = 0;
    r = bxf3_read_bhf_header(bhd_br, &props, &headers, &n, b->alloc);
    if (r != SF_OK) return r;

    sf_xfree(b->alloc, b->version);
    b->version        = props.version;
    b->format         = props.format;
    b->big_endian     = props.big_endian;
    b->bit_big_endian = props.bit_big_endian;

    if (n > 0) {
        b->files = (sf_binder_file_t *)sf_xalloc(b->alloc, n * sizeof(*b->files));
        if (!b->files) {
            r = SF_ERR_OOM;
            goto cleanup_headers;
        }
        memset(b->files, 0, n * sizeof(*b->files));
        b->file_capacity = n;
    }

    for (size_t i = 0; i < n; i++) {
        uint8_t *data = NULL;
        size_t   dsize = 0;
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof info);

        r = bxf3_extract_file_data(bdt_br, &headers[i], &data, &dsize, &info, b->alloc);
        if (r != SF_OK) goto cleanup_files;

        sf_binder_file_t *out_f = &b->files[i];
        out_f->id               = headers[i].id;
        out_f->flags            = headers[i].flags;
        out_f->compression_info = info;
        out_f->size             = dsize;
        out_f->data             = data;

        b->file_count++;
    }

    r = bxf3_bulk_copy_names(b->files, headers, n, &b->name_pool,
                             &b->name_pool_size, b->alloc);
    if (r != SF_OK) goto cleanup_files;

    for (size_t i = 0; i < n; i++) sfi_binder_file_header_destroy(&headers[i], b->alloc);
    sf_xfree(b->alloc, headers);
    return SF_OK;

cleanup_files:
    for (size_t i = 0; i < b->file_count; i++) {
        bxf3_file_free_with_name_pool(&b->files[i], b->alloc, b->name_pool,
                                      b->name_pool_size);
    }
    sf_xfree(b->alloc, b->name_pool);
    b->name_pool = NULL;
    b->name_pool_size = 0;
    sf_xfree(b->alloc, b->files);
    b->files = NULL;
    b->file_count = 0;
    b->file_capacity = 0;
cleanup_headers:
    for (size_t i = 0; i < n; i++) sfi_binder_file_header_destroy(&headers[i], b->alloc);
    sf_xfree(b->alloc, headers);
    return r;
}

static sf_result_t bxf3_open_decompressed(sf_binary_reader_t   *raw,
                                          sf_binary_reader_t  **out_reader,
                                          bool                 *out_owns,
                                          sf_dcx_compression_info_t *out_info,
                                          const sf_allocator_t *a) {
    sf_binary_reader_t *unwrapped = NULL;
    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof info);

    sf_result_t r = sf_get_decompressed_reader(raw, &unwrapped, &info, a);
    if (r != SF_OK) return r;

    *out_reader = unwrapped;
    *out_owns   = (unwrapped != raw);
    if (out_info) *out_info = info;
    return SF_OK;
}

sf_result_t sf_bxf3_read_from_memory(sf_bxf3_t **out,
                                     const uint8_t *bhd, size_t bhd_size,
                                     const uint8_t *bdt, size_t bdt_size,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bhd != NULL || bhd_size == 0);
    SF_CHECK_ARG(bdt != NULL || bdt_size == 0);
    a = sf_alloc_or_default(a);

    sf_istream_t *bhd_is = NULL;
    sf_result_t r = sf_istream_open_memory(&bhd_is, bhd, bhd_size, a);
    if (r != SF_OK) return r;

    sf_istream_t *bdt_is = NULL;
    r = sf_istream_open_memory(&bdt_is, bdt, bdt_size, a);
    if (r != SF_OK) { sf_istream_close(bhd_is); return r; }

    sf_binary_reader_t *bhd_raw = NULL;
    r = sf_binary_reader_create(&bhd_raw, bhd_is, false, a);
    if (r != SF_OK) { sf_istream_close(bdt_is); sf_istream_close(bhd_is); return r; }

    sf_binary_reader_t *bdt_raw = NULL;
    r = sf_binary_reader_create(&bdt_raw, bdt_is, false, a);
    if (r != SF_OK) {
        sf_binary_reader_destroy(bhd_raw);
        sf_istream_close(bdt_is); sf_istream_close(bhd_is);
        return r;
    }

    sf_binary_reader_t *bhd_use = NULL;
    sf_binary_reader_t *bdt_use = NULL;
    bool bhd_owns = false, bdt_owns = false;
    r = bxf3_open_decompressed(bhd_raw, &bhd_use, &bhd_owns, NULL, a);
    if (r != SF_OK) goto cleanup_raw;
    r = bxf3_open_decompressed(bdt_raw, &bdt_use, &bdt_owns, NULL, a);
    if (r != SF_OK) {
        if (bhd_owns) sf_binary_reader_destroy(bhd_use);
        goto cleanup_raw;
    }

    sf_bxf3_t *b = NULL;
    r = sf_bxf3_create(&b, a);
    if (r != SF_OK) goto cleanup_use;

    r = bxf3_populate_from_readers(b, bhd_use, bdt_use);
    if (r != SF_OK) { sf_bxf3_destroy(b); b = NULL; goto cleanup_use; }

    *out = b;

cleanup_use:
    if (bhd_owns) sf_binary_reader_destroy(bhd_use);
    if (bdt_owns) sf_binary_reader_destroy(bdt_use);
cleanup_raw:
    sf_binary_reader_destroy(bdt_raw);
    sf_binary_reader_destroy(bhd_raw);
    sf_istream_close(bdt_is);
    sf_istream_close(bhd_is);
    return r;
}

sf_result_t sf_bxf3_read_from_paths(sf_bxf3_t **out,
                                    const wchar_t *bhd_path,
                                    const wchar_t *bdt_path,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bhd_path != NULL);
    SF_CHECK_ARG(bdt_path != NULL);
    a = sf_alloc_or_default(a);

    sf_istream_t *bhd_is = NULL;
    sf_result_t r = sf_istream_open_wfile(&bhd_is, bhd_path, a);
    if (r != SF_OK) return r;

    sf_istream_t *bdt_is = NULL;
    r = sf_istream_open_wfile(&bdt_is, bdt_path, a);
    if (r != SF_OK) { sf_istream_close(bhd_is); return r; }

    sf_binary_reader_t *bhd_raw = NULL;
    r = sf_binary_reader_create(&bhd_raw, bhd_is, false, a);
    if (r != SF_OK) { sf_istream_close(bdt_is); sf_istream_close(bhd_is); return r; }

    sf_binary_reader_t *bdt_raw = NULL;
    r = sf_binary_reader_create(&bdt_raw, bdt_is, false, a);
    if (r != SF_OK) {
        sf_binary_reader_destroy(bhd_raw);
        sf_istream_close(bdt_is); sf_istream_close(bhd_is);
        return r;
    }

    sf_binary_reader_t *bhd_use = NULL;
    sf_binary_reader_t *bdt_use = NULL;
    bool bhd_owns = false, bdt_owns = false;
    r = bxf3_open_decompressed(bhd_raw, &bhd_use, &bhd_owns, NULL, a);
    if (r != SF_OK) goto cleanup_raw;
    r = bxf3_open_decompressed(bdt_raw, &bdt_use, &bdt_owns, NULL, a);
    if (r != SF_OK) {
        if (bhd_owns) sf_binary_reader_destroy(bhd_use);
        goto cleanup_raw;
    }

    sf_bxf3_t *b = NULL;
    r = sf_bxf3_create(&b, a);
    if (r != SF_OK) goto cleanup_use;

    r = bxf3_populate_from_readers(b, bhd_use, bdt_use);
    if (r != SF_OK) { sf_bxf3_destroy(b); b = NULL; goto cleanup_use; }

    *out = b;

cleanup_use:
    if (bhd_owns) sf_binary_reader_destroy(bhd_use);
    if (bdt_owns) sf_binary_reader_destroy(bdt_use);
cleanup_raw:
    sf_binary_reader_destroy(bdt_raw);
    sf_binary_reader_destroy(bhd_raw);
    sf_istream_close(bdt_is);
    sf_istream_close(bhd_is);
    return r;
}

/*===========================================================================
 * Eager Write implementation
 *
 * BXF3 writes BOTH the BDT header AND the BHF header up front, then walks
 * each file once writing per-file data into the BDT writer (its compressed
 * size + offset is stitched back into the BHD reservations made during
 * sfi_binder3_write_file_header).
 *
 * The trick: sfi_binder3_write_file_data appends payload to the same
 * writer it received. For BXF3 we want payloads in the BDT writer but
 * the size/offset reservations are in the BHD writer. The write helpers
 * work the same — we just call them with the bdt writer as the "data
 * sink", since the reservations were registered with the bhd writer.
 *
 * Looking at sfi_binder3_write_file_data: it calls
 * sfi_write_file_data_core(bw, ...) which writes to `bw`, then calls
 * sf_binary_writer_fill_*(bw, "FileCompressedSize<i>", ...) on the same
 * `bw`. That won't work for BXF3 — fills must target the BHD writer
 * where the reservations live.
 *
 * So we inline a BXF3-specific write_file_data: write to bdt, then fill
 * to bhd.
 *===========================================================================*/

static int bxf3_format_index_name(char *buf, size_t cap, const char *prefix, size_t i) {
    return snprintf(buf, cap, "%s%zu", prefix, i);
}

static sf_result_t bxf3_write_file_data(sf_binary_writer_t              *bhd_bw,
                                        sf_binary_writer_t              *bdt_bw,
                                        sf_binder_format_t               f,
                                        const sfi_binder_file_header_t  *h,
                                        const uint8_t                   *raw,
                                        size_t                           entry_index) {
    SF_CHECK_ARG(bhd_bw != NULL);
    SF_CHECK_ARG(bdt_bw != NULL);
    SF_CHECK_ARG(h != NULL);

    size_t raw_size = (size_t)h->uncompressed_size;
    if (raw_size > 0) {
        SF_CHECK_ARG(raw != NULL);
        sf_result_t r = sf_binary_writer_pad(bdt_bw, 0x10);
        if (r != SF_OK) return r;
    }

    uint64_t data_offset = (uint64_t)sf_binary_writer_position(bdt_bw);
    uint64_t compressed  = 0;

    if ((h->flags & SF_BINDER_FILE_FLAG_COMPRESSED) != 0) {
        const sf_allocator_t *a = sfi_ostream_allocator(sf_binary_writer_stream(bdt_bw));
        uint8_t *cbuf = NULL;
        size_t   cn   = 0;
        sf_result_t r = sf_dcx_compress_to_buffer(raw, raw_size,
                                                  &h->compression_info,
                                                  &cbuf, &cn, a);
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_bytes(bdt_bw, cbuf, cn);
        sf_free(a, cbuf);
        if (r != SF_OK) return r;
        compressed = (uint64_t)cn;
    } else {
        if (raw_size > 0) {
            sf_result_t r = sf_binary_writer_write_bytes(bdt_bw, raw, raw_size);
            if (r != SF_OK) return r;
        }
        compressed = (uint64_t)raw_size;
    }

    char name[64];
    sf_result_t r;
    bxf3_format_index_name(name, sizeof name, "FileCompressedSize", entry_index);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bhd_bw, name, (int32_t)compressed), return r);

    if (sf_binder_format_has_compression(f)) {
        bxf3_format_index_name(name, sizeof name, "FileUncompressedSize", entry_index);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bhd_bw, name, (int32_t)h->uncompressed_size), return r);
    }

    bxf3_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
    if (sf_binder_format_has_long_offsets(f)) {
        r = sf_binary_writer_fill_i64(bhd_bw, name, (int64_t)data_offset);
    } else {
        r = sf_binary_writer_fill_u32(bhd_bw, name, (uint32_t)data_offset);
    }
    return r;
}

static sf_result_t bxf3_write_bdf_header(sf_binary_writer_t *bw, const sf_bxf3_t *b) {
    sf_result_t r;
    r = sf_binary_writer_write_ascii(bw, "BDF3", false); if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, b->version ? b->version : "", 8, 0);
    if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t bxf3_write_bhf_header(sf_binary_writer_t              *bw,
                                         const sf_bxf3_t                 *b,
                                         sfi_binder_file_header_t        *hdrs) {
    sf_result_t r;
    bool host_be = b->big_endian || sf_binder_format_force_big_endian(b->format);
    sf_binary_writer_set_big_endian(bw, host_be);

    r = sf_binary_writer_write_ascii(bw, "BHF3", false); if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, b->version ? b->version : "", 8, 0);
    if (r != SF_OK) return r;

    sfi_binder_write_format(bw, b->format, b->bit_big_endian);
    r = sf_binary_writer_write_bool(bw, b->big_endian);     if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, b->bit_big_endian); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8  (bw, 0);                 if (r != SF_OK) return r;

    r = sf_binary_writer_write_i32(bw, (int32_t)b->file_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                      if (r != SF_OK) return r;

    for (size_t i = 0; i < b->file_count; i++) {
        r = sfi_binder3_write_file_header(bw, b->format, b->bit_big_endian,
                                          &hdrs[i], i);
        if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < b->file_count; i++) {
        r = sfi_binder3_write_file_name(bw, b->format, &hdrs[i], i);
        if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t bxf3_write_to_writers(const sf_bxf3_t    *b,
                                         sf_binary_writer_t *bhd_bw,
                                         sf_binary_writer_t *bdt_bw) {
    sf_result_t r;

    sfi_binder_file_header_t *hdrs = NULL;
    if (b->file_count > 0) {
        hdrs = (sfi_binder_file_header_t *)sf_xalloc(b->alloc,
                                                     b->file_count * sizeof(*hdrs));
        if (!hdrs) return SF_ERR_OOM;
        memset(hdrs, 0, b->file_count * sizeof(*hdrs));
    }

    for (size_t i = 0; i < b->file_count; i++) {
        const sf_binder_file_t *f = &b->files[i];
        hdrs[i].flags             = f->flags;
        hdrs[i].id                = f->id;
        hdrs[i].name_utf8         = (char *)f->name_utf8;
        hdrs[i].uncompressed_size = (uint64_t)f->size;
        hdrs[i].compression_info  = f->compression_info;
    }

    r = bxf3_write_bdf_header(bdt_bw, b);
    if (r != SF_OK) goto cleanup;

    r = bxf3_write_bhf_header(bhd_bw, b, hdrs);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < b->file_count; i++) {
        const sf_binder_file_t *f = &b->files[i];
        r = bxf3_write_file_data(bhd_bw, bdt_bw, b->format, &hdrs[i], f->data, i);
        if (r != SF_OK) goto cleanup;
    }

cleanup:
    sf_xfree(b->alloc, hdrs);
    return r;
}

sf_result_t sf_bxf3_write_to_memory(const sf_bxf3_t *b,
                                    uint8_t **out_bhd, size_t *out_bhd_size,
                                    uint8_t **out_bdt, size_t *out_bdt_size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(out_bhd != NULL);
    SF_CHECK_ARG(out_bhd_size != NULL);
    SF_CHECK_ARG(out_bdt != NULL);
    SF_CHECK_ARG(out_bdt_size != NULL);
    a = sf_alloc_or_default(a);

    sf_ostream_t *bhd_os = NULL;
    sf_result_t r = sf_ostream_open_memory(&bhd_os, a);
    if (r != SF_OK) return r;

    sf_ostream_t *bdt_os = NULL;
    r = sf_ostream_open_memory(&bdt_os, a);
    if (r != SF_OK) { sf_ostream_close(bhd_os); return r; }

    sf_binary_writer_t *bhd_bw = NULL;
    r = sf_binary_writer_create(&bhd_bw, bhd_os, false, a);
    if (r != SF_OK) { sf_ostream_close(bdt_os); sf_ostream_close(bhd_os); return r; }

    sf_binary_writer_t *bdt_bw = NULL;
    r = sf_binary_writer_create(&bdt_bw, bdt_os, false, a);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bhd_bw);
        sf_ostream_close(bdt_os); sf_ostream_close(bhd_os);
        return r;
    }

    r = bxf3_write_to_writers(b, bhd_bw, bdt_bw);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bdt_bw);
        sf_binary_writer_destroy(bhd_bw);
        sf_ostream_close(bdt_os);
        sf_ostream_close(bhd_os);
        return r;
    }

    r = sf_binary_writer_finish_bytes(bhd_bw, out_bhd, out_bhd_size);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bdt_bw);
        sf_ostream_close(bdt_os);
        sf_ostream_close(bhd_os);
        return r;
    }

    r = sf_binary_writer_finish_bytes(bdt_bw, out_bdt, out_bdt_size);
    if (r != SF_OK) {
        sf_free(a, *out_bhd);
        *out_bhd = NULL;
        *out_bhd_size = 0;
    }

    sf_ostream_close(bdt_os);
    sf_ostream_close(bhd_os);
    return r;
}

sf_result_t sf_bxf3_write_to_paths(const sf_bxf3_t *b,
                                   const wchar_t *bhd_path,
                                   const wchar_t *bdt_path) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(bhd_path != NULL);
    SF_CHECK_ARG(bdt_path != NULL);

    sf_ostream_t *bhd_os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&bhd_os, bhd_path, b->alloc);
    if (r != SF_OK) return r;

    sf_ostream_t *bdt_os = NULL;
    r = sf_ostream_open_wfile(&bdt_os, bdt_path, b->alloc);
    if (r != SF_OK) { sf_ostream_close(bhd_os); return r; }

    sf_binary_writer_t *bhd_bw = NULL;
    r = sf_binary_writer_create(&bhd_bw, bhd_os, false, b->alloc);
    if (r != SF_OK) { sf_ostream_close(bdt_os); sf_ostream_close(bhd_os); return r; }

    sf_binary_writer_t *bdt_bw = NULL;
    r = sf_binary_writer_create(&bdt_bw, bdt_os, false, b->alloc);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bhd_bw);
        sf_ostream_close(bdt_os); sf_ostream_close(bhd_os);
        return r;
    }

    r = bxf3_write_to_writers(b, bhd_bw, bdt_bw);
    if (r == SF_OK) {
        r = sf_binary_writer_finish(bhd_bw);
    } else {
        sf_binary_writer_destroy(bhd_bw);
    }
    if (r == SF_OK) {
        r = sf_binary_writer_finish(bdt_bw);
    } else {
        sf_binary_writer_destroy(bdt_bw);
    }
    sf_ostream_close(bdt_os);
    sf_ostream_close(bhd_os);
    return r;
}

/*===========================================================================
 * Property accessors / mutators
 *===========================================================================*/

size_t sf_bxf3_file_count(const sf_bxf3_t *b) { return b ? b->file_count : 0; }

const sf_binder_file_t *sf_bxf3_get_file(const sf_bxf3_t *b, size_t index) {
    if (!b || index >= b->file_count) return NULL;
    return &b->files[index];
}

const char        *sf_bxf3_get_version       (const sf_bxf3_t *b) { return b ? b->version : NULL; }
sf_binder_format_t sf_bxf3_get_format        (const sf_bxf3_t *b) { return b ? b->format : SF_BINDER_FORMAT_NONE; }
bool               sf_bxf3_get_big_endian    (const sf_bxf3_t *b) { return b ? b->big_endian : false; }
bool               sf_bxf3_get_bit_big_endian(const sf_bxf3_t *b) { return b ? b->bit_big_endian : false; }

void sf_bxf3_set_version(sf_bxf3_t *b, const char *v) {
    if (!b) return;
    sf_xfree(b->alloc, b->version);
    b->version = v ? sf_strdup(b->alloc, v) : NULL;
}
void sf_bxf3_set_format        (sf_bxf3_t *b, sf_binder_format_t f) { if (b) b->format = f; }
void sf_bxf3_set_big_endian    (sf_bxf3_t *b, bool v)               { if (b) b->big_endian = v; }
void sf_bxf3_set_bit_big_endian(sf_bxf3_t *b, bool v)               { if (b) b->bit_big_endian = v; }

sf_result_t sf_bxf3_add_file(sf_bxf3_t *b, const sf_binder_file_t *file) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(file != NULL);

    if (b->file_count == b->file_capacity) {
        size_t new_cap = b->file_capacity ? b->file_capacity * 2 : 8;
        size_t old_bytes = b->file_capacity * sizeof(sf_binder_file_t);
        size_t new_bytes = new_cap * sizeof(sf_binder_file_t);
        void *new_buf = sf_xrealloc(b->alloc, b->files, old_bytes, new_bytes);
        if (!new_buf) return SF_ERR_OOM;
        b->files = (sf_binder_file_t *)new_buf;
        memset(&b->files[b->file_capacity], 0,
               (new_cap - b->file_capacity) * sizeof(sf_binder_file_t));
        b->file_capacity = new_cap;
    }

    sf_result_t r = bxf3_file_dup(&b->files[b->file_count], file, b->alloc);
    if (r != SF_OK) return r;
    b->file_count++;
    return SF_OK;
}

sf_result_t sf_bxf3_remove_file(sf_bxf3_t *b, size_t index) {
    SF_CHECK_ARG(b != NULL);
    if (index >= b->file_count) return SF_ERR_OUT_OF_RANGE;
    bxf3_file_free_with_name_pool(&b->files[index], b->alloc, b->name_pool,
                                  b->name_pool_size);
    for (size_t i = index + 1; i < b->file_count; i++) {
        b->files[i - 1] = b->files[i];
    }
    b->file_count--;
    memset(&b->files[b->file_count], 0, sizeof(sf_binder_file_t));
    return SF_OK;
}

/*===========================================================================
 * sf_bxf3_reader_t — streaming variant
 *===========================================================================*/

static void bxf3_reader_free(sf_bxf3_reader_t *r) {
    if (!r) return;
    const sf_allocator_t *a = r->alloc;
    if (r->headers) {
        for (size_t i = 0; i < r->file_count; i++) {
            sfi_binder_file_header_destroy(&r->headers[i], a);
        }
        sf_xfree(a, r->headers);
    }
    if (r->files) {
        for (size_t i = 0; i < r->file_count; i++) {
            bxf3_file_free_with_name_pool(&r->files[i], a, r->name_pool, r->name_pool_size);
        }
        sf_xfree(a, r->files);
    }
    sf_xfree(a, r->name_pool);
    sf_xfree(a, r->version);
    if (r->bhd_br) sf_binary_reader_destroy(r->bhd_br);
    if (r->bdt_br) sf_binary_reader_destroy(r->bdt_br);
    sf_xfree(a, r);
}

void sf_bxf3_reader_close(sf_bxf3_reader_t *r) { bxf3_reader_free(r); }

/* Load whole file into a heap buffer, optionally unwrap DCX, return an
 * owning binary reader. */
static sf_result_t bxf3_reader_load_file(const wchar_t            *path,
                                         const sf_allocator_t     *a,
                                         sf_binary_reader_t      **out_br,
                                         sf_dcx_compression_info_t *out_info) {
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;

    int64_t total = sf_istream_length(is);
    if (total < 0) { sf_istream_close(is); return SF_ERR_IO; }
    uint8_t *raw_buf = (total > 0) ? (uint8_t *)sf_xalloc(a, (size_t)total) : NULL;
    if (total > 0 && !raw_buf) { sf_istream_close(is); return SF_ERR_OOM; }
    if (total > 0) {
        r = sf_istream_read(is, raw_buf, (size_t)total);
        if (r != SF_OK) { sf_xfree(a, raw_buf); sf_istream_close(is); return r; }
    }
    sf_istream_close(is);

    sf_binary_reader_t *raw_br = NULL;
    r = sf_binary_reader_create_from_memory(&raw_br, false, raw_buf, (size_t)total, a);
    if (r != SF_OK) { sf_xfree(a, raw_buf); return r; }

    sf_binary_reader_t *use_br = NULL;
    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof info);
    r = sf_get_decompressed_reader(raw_br, &use_br, &info, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw_br); return r; }

    if (use_br != raw_br) {
        sf_binary_reader_destroy(raw_br);
        *out_br = use_br;
    } else {
        *out_br = raw_br;
    }
    if (out_info) *out_info = info;
    return SF_OK;
}

sf_result_t sf_bxf3_reader_open(sf_bxf3_reader_t **out,
                                const wchar_t *bhd_path,
                                const wchar_t *bdt_path,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bhd_path != NULL);
    SF_CHECK_ARG(bdt_path != NULL);
    a = sf_alloc_or_default(a);

    sf_binary_reader_t *bhd_br = NULL;
    sf_binary_reader_t *bdt_br = NULL;
    sf_dcx_compression_info_t bhd_info, bdt_info;
    memset(&bhd_info, 0, sizeof bhd_info);
    memset(&bdt_info, 0, sizeof bdt_info);

    sf_result_t r = bxf3_reader_load_file(bhd_path, a, &bhd_br, &bhd_info);
    if (r != SF_OK) return r;
    r = bxf3_reader_load_file(bdt_path, a, &bdt_br, &bdt_info);
    if (r != SF_OK) { sf_binary_reader_destroy(bhd_br); return r; }

    sf_bxf3_reader_t *rd = (sf_bxf3_reader_t *)sf_xalloc(a, sizeof(*rd));
    if (!rd) {
        sf_binary_reader_destroy(bdt_br);
        sf_binary_reader_destroy(bhd_br);
        return SF_ERR_OOM;
    }
    memset(rd, 0, sizeof(*rd));
    rd->alloc           = a;
    rd->bhd_compression = bhd_info;
    rd->bdt_compression = bdt_info;

    r = bxf3_read_bdf_header(bdt_br, NULL, a);
    if (r != SF_OK) {
        sf_xfree(a, rd);
        sf_binary_reader_destroy(bdt_br);
        sf_binary_reader_destroy(bhd_br);
        return r;
    }

    bxf3_props_t props;
    sfi_binder_file_header_t *headers = NULL;
    size_t                    n       = 0;
    r = bxf3_read_bhf_header(bhd_br, &props, &headers, &n, a);
    if (r != SF_OK) {
        sf_xfree(a, rd);
        sf_binary_reader_destroy(bdt_br);
        sf_binary_reader_destroy(bhd_br);
        return r;
    }

    rd->version        = props.version;
    rd->format         = props.format;
    rd->big_endian     = props.big_endian;
    rd->bit_big_endian = props.bit_big_endian;
    rd->headers        = headers;
    rd->file_count     = n;

    if (n > 0) {
        rd->files = (sf_binder_file_t *)sf_xalloc(a, n * sizeof(*rd->files));
        if (!rd->files) {
            r = SF_ERR_OOM;
            goto cleanup;
        }
        memset(rd->files, 0, n * sizeof(*rd->files));
        for (size_t i = 0; i < n; i++) {
            rd->files[i].id               = headers[i].id;
            rd->files[i].flags            = headers[i].flags;
            rd->files[i].size             = (size_t)headers[i].uncompressed_size;
            rd->files[i].compression_info = headers[i].compression_info;
        }
        r = bxf3_bulk_copy_names(rd->files, headers, n, &rd->name_pool,
                                 &rd->name_pool_size, a);
        if (r != SF_OK) goto cleanup;
    }

    rd->bhd_br = bhd_br;
    rd->bdt_br = bdt_br;
    *out = rd;
    return SF_OK;

cleanup:
    bxf3_reader_free(rd);
    sf_binary_reader_destroy(bdt_br);
    sf_binary_reader_destroy(bhd_br);
    return r;
}

size_t sf_bxf3_reader_file_count(const sf_bxf3_reader_t *r) {
    return r ? r->file_count : 0;
}

const sf_binder_file_t *sf_bxf3_reader_get_file(const sf_bxf3_reader_t *r, size_t idx) {
    if (!r || idx >= r->file_count) return NULL;
    return &r->files[idx];
}

sf_result_t sf_bxf3_reader_read_file_by_index(sf_bxf3_reader_t *r, size_t idx,
                                              uint8_t **out, size_t *out_size,
                                              const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    if (idx >= r->file_count) return SF_ERR_OUT_OF_RANGE;
    a = sf_alloc_or_default(a);
    return bxf3_extract_file_data(r->bdt_br, &r->headers[idx], out, out_size, NULL, a);
}

sf_result_t sf_bxf3_reader_read_file_by_id(sf_bxf3_reader_t *r, int32_t id,
                                           uint8_t **out, size_t *out_size,
                                           const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    for (size_t i = 0; i < r->file_count; i++) {
        if (r->headers[i].id == id) {
            a = sf_alloc_or_default(a);
            return bxf3_extract_file_data(r->bdt_br, &r->headers[i], out, out_size, NULL, a);
        }
    }
    return SF_ERR_NOT_FOUND;
}
