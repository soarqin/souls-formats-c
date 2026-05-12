/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BND4 archive container.
 *
 * Mirrors:
 *   SoulsFormats/Formats/Binder/BND4/BND4.cs
 *   SoulsFormats/Formats/Binder/BND4/BND4Reader.cs
 */

#include "souls_formats/sf_bnd4.h"

#include "archive/binder_common.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct sf_bnd4 {
    const sf_allocator_t *alloc;

    sf_binder_file_t *files;
    size_t            file_count;
    size_t            file_capacity;

    char              *version;
    sf_binder_format_t format;
    bool               unk04;
    bool               unk05;
    bool               big_endian;
    bool               bit_big_endian;
    bool               unicode;
    uint8_t            extended;
};

struct sf_bnd4_reader {
    const sf_allocator_t *alloc;

    sf_binder_file_t         *files;
    sfi_binder_file_header_t *headers;
    size_t                    file_count;

    sf_binary_reader_t       *br;
    sf_dcx_compression_info_t compression;

    char              *version;
    sf_binder_format_t format;
    bool               unk04;
    bool               unk05;
    bool               big_endian;
    bool               bit_big_endian;
    bool               unicode;
    uint8_t            extended;
};

static void bnd4_default_version(char out[9]) {
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

static sf_result_t bnd4_file_dup(sf_binder_file_t *dst, const sf_binder_file_t *src,
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

static void bnd4_file_free(sf_binder_file_t *f, const sf_allocator_t *a) {
    if (!f) return;
    sf_xfree(a, (void *)f->name_utf8);
    sf_xfree(a, (void *)f->data);
    f->name_utf8 = NULL;
    f->data = NULL;
    f->size = 0;
}

sf_result_t sf_bnd4_create(sf_bnd4_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);

    sf_bnd4_t *b = (sf_bnd4_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    b->alloc = a;

    char ver[9];
    bnd4_default_version(ver);
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
    b->unicode        = true;
    b->extended       = 4;

    *out = b;
    return SF_OK;
}

void sf_bnd4_destroy(sf_bnd4_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    for (size_t i = 0; i < b->file_count; i++) bnd4_file_free(&b->files[i], a);
    sf_xfree(a, b->files);
    sf_xfree(a, b->version);
    sf_xfree(a, b);
}

typedef struct bnd4_props {
    char              *version;
    sf_binder_format_t format;
    bool               unk04;
    bool               unk05;
    bool               big_endian;
    bool               bit_big_endian;
    bool               unicode;
    uint8_t            extended;
} bnd4_props_t;

static sf_result_t bnd4_read_header(sf_binary_reader_t        *br,
                                    bnd4_props_t              *props,
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
    r = sf_binary_reader_assert_ascii(br, "BND4"); if (r != SF_OK) return r;

    r = sf_binary_reader_read_bool(br, &props->unk04); if (r != SF_OK) return r;
    r = sf_binary_reader_read_bool(br, &props->unk05); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);         if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);         if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);         if (r != SF_OK) return r;

    r = sf_binary_reader_read_bool(br, &props->big_endian); if (r != SF_OK) return r;
    bool not_bit_big_endian = false;
    r = sf_binary_reader_read_bool(br, &not_bit_big_endian); if (r != SF_OK) return r;
    props->bit_big_endian = !not_bit_big_endian;
    r = sf_binary_reader_assert_u8_one(br, 0);              if (r != SF_OK) return r;

    sf_binary_reader_set_big_endian(br, props->big_endian);

    int32_t file_count = 0;
    r = sf_binary_reader_read_i32(br, &file_count);       if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i64_one(br, 0x40);        if (r != SF_OK) return r;

    char *version = NULL;
    r = sf_binary_reader_read_fix_str(br, 8, &version, NULL);
    if (r != SF_OK) return r;
    props->version = version;

    int64_t file_header_size = 0;
    r = sf_binary_reader_read_i64(br, &file_header_size); if (r != SF_OK) goto fail;
    int64_t headers_end = 0;
    r = sf_binary_reader_read_i64(br, &headers_end);      if (r != SF_OK) goto fail;
    (void)headers_end;

    r = sf_binary_reader_read_bool(br, &props->unicode);  if (r != SF_OK) goto fail;
    props->format = sfi_binder_read_format(br, props->bit_big_endian);
    static const uint8_t extended_options[] = { 0, 1, 4, 0x80 };
    r = sf_binary_reader_assert_u8(br, 4, extended_options, &props->extended);
    if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_u8_one(br, 0);            if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_i32_one(br, 0);           if (r != SF_OK) goto fail;

    int64_t hash_table_offset = 0;
    r = sf_binary_reader_read_i64(br, &hash_table_offset); if (r != SF_OK) goto fail;
    if (props->extended == 4) {
        r = sf_binary_reader_step_in(br, hash_table_offset); if (r != SF_OK) goto fail;
        r = sfi_binder_hash_table_assert(br, (size_t)file_count);
        sf_result_t step_r = sf_binary_reader_step_out(br);
        if (r != SF_OK) goto fail;
        if (step_r != SF_OK) { r = step_r; goto fail; }
    } else if (hash_table_offset != 0) {
        r = SF_ERR_BAD_MAGIC;
        goto fail;
    }

    if (file_header_size != (int64_t)sfi_binder_get_bnd4_file_header_size(props->format)) {
        r = SF_ERR_BAD_MAGIC;
        goto fail;
    }
    if (file_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

    sfi_binder_file_header_t *headers = NULL;
    if (file_count > 0) {
        headers = (sfi_binder_file_header_t *)sf_xalloc(a, (size_t)file_count * sizeof(*headers));
        if (!headers) { r = SF_ERR_OOM; goto fail; }
        memset(headers, 0, (size_t)file_count * sizeof(*headers));
    }

    for (int32_t i = 0; i < file_count; i++) {
        r = sfi_binder4_read_file_header(br, props->format, props->bit_big_endian,
                                         props->unicode, &headers[i], a);
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

static sf_result_t bnd4_extract_file_data(sf_binary_reader_t              *br,
                                          const sfi_binder_file_header_t  *h,
                                          uint8_t                        **out_data,
                                          size_t                          *out_size,
                                          sf_dcx_compression_info_t       *out_info,
                                          const sf_allocator_t            *a) {
    SF_CHECK_ARG(br != NULL);
    SF_CHECK_ARG(h != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);

    *out_data = NULL;
    *out_size = 0;

    size_t compressed_size = (size_t)h->compressed_size;
    uint8_t *raw = (compressed_size > 0) ? (uint8_t *)sf_xalloc(a, compressed_size) : NULL;
    if (compressed_size > 0 && !raw) return SF_ERR_OOM;

    if (compressed_size > 0) {
        sf_result_t r = sf_binary_reader_get_bytes(br, (int64_t)h->data_offset,
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

static sf_result_t bnd4_populate_from_reader(sf_bnd4_t *b, sf_binary_reader_t *br) {
    bnd4_props_t              props;
    sfi_binder_file_header_t *headers = NULL;
    size_t                    n       = 0;

    sf_result_t r = bnd4_read_header(br, &props, &headers, &n, b->alloc);
    if (r != SF_OK) return r;

    sf_xfree(b->alloc, b->version);
    b->version        = props.version;
    b->format         = props.format;
    b->unk04          = props.unk04;
    b->unk05          = props.unk05;
    b->big_endian     = props.big_endian;
    b->bit_big_endian = props.bit_big_endian;
    b->unicode        = props.unicode;
    b->extended       = props.extended;

    if (n > 0) {
        b->files = (sf_binder_file_t *)sf_xalloc(b->alloc, n * sizeof(*b->files));
        if (!b->files) { r = SF_ERR_OOM; goto cleanup_headers; }
        memset(b->files, 0, n * sizeof(*b->files));
        b->file_capacity = n;
    }

    for (size_t i = 0; i < n; i++) {
        uint8_t *data = NULL;
        size_t   dsize = 0;
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof info);

        r = bnd4_extract_file_data(br, &headers[i], &data, &dsize, &info, b->alloc);
        if (r != SF_OK) goto cleanup_files;

        sf_binder_file_t *out_f = &b->files[i];
        out_f->id               = headers[i].id;
        out_f->flags            = headers[i].flags;
        out_f->compression_info = info;
        out_f->size             = dsize;
        out_f->data             = data;

        if (headers[i].name_utf8) {
            char *name_copy = sf_strdup(b->alloc, headers[i].name_utf8);
            if (!name_copy) {
                sf_xfree(b->alloc, data);
                out_f->data = NULL;
                out_f->size = 0;
                r = SF_ERR_OOM;
                goto cleanup_files;
            }
            out_f->name_utf8 = name_copy;
        }
        b->file_count++;
    }

    for (size_t i = 0; i < n; i++) sfi_binder_file_header_destroy(&headers[i], b->alloc);
    sf_xfree(b->alloc, headers);
    return SF_OK;

cleanup_files:
    for (size_t i = 0; i < b->file_count; i++) bnd4_file_free(&b->files[i], b->alloc);
    sf_xfree(b->alloc, b->files);
    b->files = NULL;
    b->file_count = 0;
    b->file_capacity = 0;
cleanup_headers:
    for (size_t i = 0; i < n; i++) sfi_binder_file_header_destroy(&headers[i], b->alloc);
    sf_xfree(b->alloc, headers);
    return r;
}

static sf_result_t bnd4_open_decompressed(sf_binary_reader_t        *raw,
                                          sf_binary_reader_t       **out_reader,
                                          bool                      *out_owns,
                                          sf_dcx_compression_info_t *out_info,
                                          const sf_allocator_t      *a) {
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

sf_result_t sf_bnd4_read_from_memory(sf_bnd4_t **out, const uint8_t *data, size_t size,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL || size == 0);
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, data, size, a);
    if (r != SF_OK) return r;

    sf_binary_reader_t *raw_br = NULL;
    r = sf_binary_reader_create(&raw_br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    sf_binary_reader_t *use_br = NULL;
    bool owns = false;
    r = bnd4_open_decompressed(raw_br, &use_br, &owns, NULL, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw_br); sf_istream_close(is); return r; }

    sf_bnd4_t *b = NULL;
    r = sf_bnd4_create(&b, a);
    if (r != SF_OK) goto done;

    r = bnd4_populate_from_reader(b, use_br);
    if (r != SF_OK) { sf_bnd4_destroy(b); b = NULL; goto done; }

    *out = b;

done:
    if (owns) sf_binary_reader_destroy(use_br);
    sf_binary_reader_destroy(raw_br);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_bnd4_read_from_path(sf_bnd4_t **out, const wchar_t *path,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;

    sf_binary_reader_t *raw_br = NULL;
    r = sf_binary_reader_create(&raw_br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    sf_binary_reader_t *use_br = NULL;
    bool owns = false;
    r = bnd4_open_decompressed(raw_br, &use_br, &owns, NULL, a);
    if (r != SF_OK) { sf_binary_reader_destroy(raw_br); sf_istream_close(is); return r; }

    sf_bnd4_t *b = NULL;
    r = sf_bnd4_create(&b, a);
    if (r != SF_OK) goto done;

    r = bnd4_populate_from_reader(b, use_br);
    if (r != SF_OK) { sf_bnd4_destroy(b); b = NULL; goto done; }

    *out = b;

done:
    if (owns) sf_binary_reader_destroy(use_br);
    sf_binary_reader_destroy(raw_br);
    sf_istream_close(is);
    return r;
}

static sf_result_t bnd4_write_to_writer(const sf_bnd4_t *b, sf_binary_writer_t *bw) {
    sf_result_t r;

    sf_binary_writer_set_big_endian(bw, b->big_endian);

    r = sf_binary_writer_write_ascii(bw, "BND4", false); if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, b->unk04);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, b->unk05);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, b->big_endian);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, !b->bit_big_endian); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                 if (r != SF_OK) return r;

    r = sf_binary_writer_write_i32(bw, (int32_t)b->file_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i64(bw, 0x40);                   if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, b->version ? b->version : "", 8, 0);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i64(bw, (int64_t)sfi_binder_get_bnd4_file_header_size(b->format));
    if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, "HeadersEnd"), return r);

    r = sf_binary_writer_write_bool(bw, b->unicode); if (r != SF_OK) return r;
    sfi_binder_write_format(bw, b->format, b->bit_big_endian);
    r = sf_binary_writer_write_u8(bw, b->extended); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);           if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);          if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, "HashTableOffset"), return r);

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
        r = sfi_binder4_write_file_header(bw, b->format, b->bit_big_endian,
                                          &hdrs[i], i);
        if (r != SF_OK) goto cleanup;
    }

    for (size_t i = 0; i < b->file_count; i++) {
        r = sfi_binder4_write_file_name(bw, b->format, b->unicode, &hdrs[i], i);
        if (r != SF_OK) goto cleanup;
    }

    if (b->extended == 4) {
        r = sf_binary_writer_pad(bw, 0x8); if (r != SF_OK) goto cleanup;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, "HashTableOffset", sf_binary_writer_position(bw)), goto cleanup);
        r = sfi_binder_hash_table_write(bw, b->files, b->file_count, b->alloc);
        if (r != SF_OK) goto cleanup;
    } else {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, "HashTableOffset", 0), goto cleanup);
    }

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, "HeadersEnd", sf_binary_writer_position(bw)), goto cleanup);

    for (size_t i = 0; i < b->file_count; i++) {
        const sf_binder_file_t *f = &b->files[i];
        r = sfi_binder4_write_file_data(bw, b->format, &hdrs[i], f->data, i);
        if (r != SF_OK) goto cleanup;
    }

cleanup:
    sf_xfree(b->alloc, hdrs);
    return r;
}

sf_result_t sf_bnd4_write_to_memory(const sf_bnd4_t *b, uint8_t **out, size_t *out_size,
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

    r = bnd4_write_to_writer(b, bw);
    if (r != SF_OK) { sf_binary_writer_destroy(bw); sf_ostream_close(os); return r; }

    r = sf_binary_writer_finish_bytes(bw, out, out_size);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_bnd4_write_to_path(const sf_bnd4_t *b, const wchar_t *path) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(path != NULL);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, b->alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, b->alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = bnd4_write_to_writer(b, bw);
    if (r == SF_OK) {
        r = sf_binary_writer_finish(bw);
    } else {
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(os);
    return r;
}

size_t sf_bnd4_file_count(const sf_bnd4_t *b) { return b ? b->file_count : 0; }

const sf_binder_file_t *sf_bnd4_get_file(const sf_bnd4_t *b, size_t index) {
    if (!b || index >= b->file_count) return NULL;
    return &b->files[index];
}

const char        *sf_bnd4_get_version       (const sf_bnd4_t *b) { return b ? b->version : NULL; }
sf_binder_format_t sf_bnd4_get_format        (const sf_bnd4_t *b) { return b ? b->format : SF_BINDER_FORMAT_NONE; }
bool               sf_bnd4_get_big_endian    (const sf_bnd4_t *b) { return b ? b->big_endian : false; }
bool               sf_bnd4_get_bit_big_endian(const sf_bnd4_t *b) { return b ? b->bit_big_endian : false; }
bool               sf_bnd4_get_unicode       (const sf_bnd4_t *b) { return b ? b->unicode : false; }
uint8_t            sf_bnd4_get_extended      (const sf_bnd4_t *b) { return b ? b->extended : 0; }
bool               sf_bnd4_get_unk04         (const sf_bnd4_t *b) { return b ? b->unk04 : false; }
bool               sf_bnd4_get_unk05         (const sf_bnd4_t *b) { return b ? b->unk05 : false; }

void sf_bnd4_set_version(sf_bnd4_t *b, const char *v) {
    if (!b) return;
    char *copy = v ? sf_strdup(b->alloc, v) : NULL;
    if (v && !copy) return;
    sf_xfree(b->alloc, b->version);
    b->version = copy;
}
void sf_bnd4_set_format        (sf_bnd4_t *b, sf_binder_format_t f) { if (b) b->format = f; }
void sf_bnd4_set_big_endian    (sf_bnd4_t *b, bool v)               { if (b) b->big_endian = v; }
void sf_bnd4_set_bit_big_endian(sf_bnd4_t *b, bool v)               { if (b) b->bit_big_endian = v; }
void sf_bnd4_set_unicode       (sf_bnd4_t *b, bool v)               { if (b) b->unicode = v; }
void sf_bnd4_set_extended      (sf_bnd4_t *b, uint8_t v)            { if (b) b->extended = v; }
void sf_bnd4_set_unk04         (sf_bnd4_t *b, bool v)               { if (b) b->unk04 = v; }
void sf_bnd4_set_unk05         (sf_bnd4_t *b, bool v)               { if (b) b->unk05 = v; }

sf_result_t sf_bnd4_add_file(sf_bnd4_t *b, const sf_binder_file_t *file) {
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

    sf_result_t r = bnd4_file_dup(&b->files[b->file_count], file, b->alloc);
    if (r != SF_OK) return r;
    b->file_count++;
    return SF_OK;
}

sf_result_t sf_bnd4_remove_file(sf_bnd4_t *b, size_t index) {
    SF_CHECK_ARG(b != NULL);
    if (index >= b->file_count) return SF_ERR_OUT_OF_RANGE;
    bnd4_file_free(&b->files[index], b->alloc);
    for (size_t i = index + 1; i < b->file_count; i++) b->files[i - 1] = b->files[i];
    b->file_count--;
    memset(&b->files[b->file_count], 0, sizeof(sf_binder_file_t));
    return SF_OK;
}

sf_result_t sf_bnd4_find_by_path_hash(const sf_bnd4_t *b, uint32_t hash, size_t *index_out) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(index_out != NULL);
    for (size_t i = 0; i < b->file_count; i++) {
        if (sf_path_hash(b->files[i].name_utf8 ? b->files[i].name_utf8 : "") == hash) {
            *index_out = i;
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

static void bnd4_reader_free(sf_bnd4_reader_t *r) {
    if (!r) return;
    const sf_allocator_t *a = r->alloc;
    if (r->headers) {
        for (size_t i = 0; i < r->file_count; i++) sfi_binder_file_header_destroy(&r->headers[i], a);
        sf_xfree(a, r->headers);
    }
    if (r->files) {
        for (size_t i = 0; i < r->file_count; i++) sf_xfree(a, (void *)r->files[i].name_utf8);
        sf_xfree(a, r->files);
    }
    sf_xfree(a, r->version);
    if (r->br) sf_binary_reader_destroy(r->br);
    sf_xfree(a, r);
}

void sf_bnd4_reader_close(sf_bnd4_reader_t *r) { bnd4_reader_free(r); }

sf_result_t sf_bnd4_reader_open(sf_bnd4_reader_t **out, const wchar_t *path,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);

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

    bool owns_use_br = (use_br != raw_br);
    if (owns_use_br) {
        sf_binary_reader_destroy(raw_br);
        raw_br = NULL;
    }

    sf_bnd4_reader_t *rd = (sf_bnd4_reader_t *)sf_xalloc(a, sizeof(*rd));
    if (!rd) { r = SF_ERR_OOM; goto cleanup_brs; }
    memset(rd, 0, sizeof(*rd));
    rd->alloc       = a;
    rd->compression = info;

    bnd4_props_t props;
    sfi_binder_file_header_t *headers = NULL;
    size_t n = 0;
    r = bnd4_read_header(use_br, &props, &headers, &n, a);
    if (r != SF_OK) { sf_xfree(a, rd); goto cleanup_brs; }

    rd->version        = props.version;
    rd->format         = props.format;
    rd->unk04          = props.unk04;
    rd->unk05          = props.unk05;
    rd->big_endian     = props.big_endian;
    rd->bit_big_endian = props.bit_big_endian;
    rd->unicode        = props.unicode;
    rd->extended       = props.extended;
    rd->headers        = headers;
    rd->file_count     = n;

    if (n > 0) {
        rd->files = (sf_binder_file_t *)sf_xalloc(a, n * sizeof(*rd->files));
        if (!rd->files) { r = SF_ERR_OOM; goto cleanup_rd; }
        memset(rd->files, 0, n * sizeof(*rd->files));
        for (size_t i = 0; i < n; i++) {
            rd->files[i].id               = headers[i].id;
            rd->files[i].flags            = headers[i].flags;
            rd->files[i].size             = (size_t)headers[i].uncompressed_size;
            rd->files[i].compression_info = headers[i].compression_info;
            if (headers[i].name_utf8) {
                char *nc = sf_strdup(a, headers[i].name_utf8);
                if (!nc) { r = SF_ERR_OOM; goto cleanup_rd; }
                rd->files[i].name_utf8 = nc;
            }
        }
    }

    rd->br = use_br;
    *out = rd;
    return SF_OK;

cleanup_rd:
    bnd4_reader_free(rd);
    return r;

cleanup_brs:
    if (owns_use_br) sf_binary_reader_destroy(use_br);
    else sf_binary_reader_destroy(raw_br);
    return r;
}

size_t sf_bnd4_reader_file_count(const sf_bnd4_reader_t *r) { return r ? r->file_count : 0; }

const sf_binder_file_t *sf_bnd4_reader_get_file(const sf_bnd4_reader_t *r, size_t idx) {
    if (!r || idx >= r->file_count) return NULL;
    return &r->files[idx];
}

sf_dcx_compression_info_t sf_bnd4_reader_get_outer_compression(const sf_bnd4_reader_t *r) {
    sf_dcx_compression_info_t info;
    memset(&info, 0, sizeof info);
    info.type = SF_DCX_TYPE_UNKNOWN;
    if (r) info = r->compression;
    return info;
}

sf_result_t sf_bnd4_reader_read_file_by_index(sf_bnd4_reader_t *r, size_t idx,
                                              uint8_t **out, size_t *out_size,
                                              const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    if (idx >= r->file_count) return SF_ERR_OUT_OF_RANGE;
    a = sf_alloc_or_default(a);
    return bnd4_extract_file_data(r->br, &r->headers[idx], out, out_size, NULL, a);
}

sf_result_t sf_bnd4_reader_read_file_by_id(sf_bnd4_reader_t *r, int32_t id,
                                           uint8_t **out, size_t *out_size,
                                           const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    for (size_t i = 0; i < r->file_count; i++) {
        if (r->headers[i].id == id) {
            a = sf_alloc_or_default(a);
            return bnd4_extract_file_data(r->br, &r->headers[i], out, out_size, NULL, a);
        }
    }
    return SF_ERR_NOT_FOUND;
}

sf_result_t sf_bnd4_reader_read_file_by_path_hash(sf_bnd4_reader_t *r, uint32_t path_hash,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a) {
    SF_CHECK_ARG(r != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    for (size_t i = 0; i < r->file_count; i++) {
        const char *name = r->headers[i].name_utf8 ? r->headers[i].name_utf8 : "";
        if (sf_path_hash(name) == path_hash) {
            a = sf_alloc_or_default(a);
            return bnd4_extract_file_data(r->br, &r->headers[i], out, out_size, NULL, a);
        }
    }
    return SF_ERR_NOT_FOUND;
}
