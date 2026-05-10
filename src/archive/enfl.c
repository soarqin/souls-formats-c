/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — ENFL (entryfilelist) format.
 *
 * Mirrors:
 *   SoulsFormats/Formats/ENFL.cs
 *
 * The on-disk layout is documented in include/souls_formats/sf_enfl.h.
 * The outer envelope is plain little-endian; the inner payload is
 * zlib-deflated using the same level (0xDA flags byte) upstream uses.
 */

#include "souls_formats/sf_enfl.h"

#include "compression/compression_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct sf_enfl {
    const sf_allocator_t *alloc;

    sf_enfl_struct1_t *struct1s;
    size_t             struct1_count;
    size_t             struct1_capacity;

    sf_enfl_struct2_t *struct2s;
    size_t             struct2_count;
    size_t             struct2_capacity;

    char             **strings;
    size_t             string_count;
    size_t             string_capacity;
};

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

sf_result_t sf_enfl_create(sf_enfl_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);
    sf_enfl_t *e = (sf_enfl_t *)sf_xalloc(a, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    e->alloc = a;
    *out = e;
    return SF_OK;
}

void sf_enfl_destroy(sf_enfl_t *e) {
    if (!e) return;
    const sf_allocator_t *a = e->alloc;
    sf_xfree(a, e->struct1s);
    sf_xfree(a, e->struct2s);
    if (e->strings) {
        for (size_t i = 0; i < e->string_count; i++) sf_xfree(a, e->strings[i]);
        sf_xfree(a, e->strings);
    }
    sf_xfree(a, e);
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t enfl_read_inner(sf_enfl_t *e, sf_binary_reader_t *br) {
    sf_result_t r;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;

    int32_t s1c = 0, s2c = 0;
    r = sf_binary_reader_read_i32(br, &s1c); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &s2c); if (r != SF_OK) return r;
    if (s1c < 0 || s2c < 0) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;

    if (s1c > 0) {
        e->struct1s = (sf_enfl_struct1_t *)sf_xalloc(e->alloc,
                                                     (size_t)s1c * sizeof(*e->struct1s));
        if (!e->struct1s) return SF_ERR_OOM;
        e->struct1_capacity = (size_t)s1c;
        for (int32_t i = 0; i < s1c; i++) {
            int16_t step = 0, idx = 0;
            r = sf_binary_reader_read_i16(br, &step); if (r != SF_OK) return r;
            r = sf_binary_reader_read_i16(br, &idx);  if (r != SF_OK) return r;
            e->struct1s[i].step  = step;
            e->struct1s[i].index = idx;
        }
        e->struct1_count = (size_t)s1c;
    }
    r = sf_binary_reader_pad(br, 0x10); if (r != SF_OK) return r;

    if (s2c > 0) {
        e->struct2s = (sf_enfl_struct2_t *)sf_xalloc(e->alloc,
                                                     (size_t)s2c * sizeof(*e->struct2s));
        if (!e->struct2s) return SF_ERR_OOM;
        e->struct2_capacity = (size_t)s2c;
        for (int32_t i = 0; i < s2c; i++) {
            int64_t v = 0;
            r = sf_binary_reader_read_i64(br, &v); if (r != SF_OK) return r;
            e->struct2s[i].unk1 = v;
        }
        e->struct2_count = (size_t)s2c;
    }
    r = sf_binary_reader_pad(br, 0x10); if (r != SF_OK) return r;

    r = sf_binary_reader_assert_i16_one(br, 0); if (r != SF_OK) return r;
    if (s2c > 0) {
        e->strings = (char **)sf_xalloc(e->alloc, (size_t)s2c * sizeof(*e->strings));
        if (!e->strings) return SF_ERR_OOM;
        memset(e->strings, 0, (size_t)s2c * sizeof(*e->strings));
        e->string_capacity = (size_t)s2c;
        for (int32_t i = 0; i < s2c; i++) {
            char *s = NULL;
            r = sf_binary_reader_read_utf16(br, &s, NULL);
            if (r != SF_OK) return r;
            if (e->alloc && e->alloc != sf_default_allocator()) {
                char *copy = sf_strdup(e->alloc, s);
                sf_free(NULL, s);
                if (!copy) return SF_ERR_OOM;
                e->strings[i] = copy;
            } else {
                e->strings[i] = s;
            }
            e->string_count++;
        }
    }
    return SF_OK;
}

static sf_result_t enfl_read_from_reader(sf_enfl_t *e, sf_binary_reader_t *br) {
    sf_binary_reader_set_big_endian(br, false);

    sf_result_t r;
    r = sf_binary_reader_assert_ascii(br, "ENFL");      if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0x10415);    if (r != SF_OK) return r;

    int32_t compressed_size = 0, uncompressed_size = 0;
    r = sf_binary_reader_read_i32(br, &compressed_size);    if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &uncompressed_size);  if (r != SF_OK) return r;
    if (compressed_size < 0 || uncompressed_size < 0) return SF_ERR_OUT_OF_RANGE;

    uint8_t *compressed = (uint8_t *)sf_xalloc(e->alloc, (size_t)compressed_size);
    if ((size_t)compressed_size > 0 && !compressed) return SF_ERR_OOM;

    if (compressed_size > 0) {
        r = sf_binary_reader_read_bytes(br, compressed, (size_t)compressed_size);
        if (r != SF_OK) { sf_xfree(e->alloc, compressed); return r; }
    }

    void *raw = NULL;
    r = sfi_zlib_decompress(compressed, (size_t)compressed_size,
                            &raw, (size_t)uncompressed_size, e->alloc);
    sf_xfree(e->alloc, compressed);
    if (r != SF_OK) return r;

    sf_binary_reader_t *inner = NULL;
    r = sf_binary_reader_create_from_memory(&inner, false, raw,
                                            (size_t)uncompressed_size, e->alloc);
    if (r != SF_OK) { sf_xfree(e->alloc, raw); return r; }

    r = enfl_read_inner(e, inner);
    sf_binary_reader_destroy(inner);
    return r;
}

sf_result_t sf_enfl_read_from_memory(sf_enfl_t **out, const uint8_t *data, size_t size,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL || size == 0);
    a = sf_alloc_or_default(a);

    sf_enfl_t *e = NULL;
    sf_result_t r = sf_enfl_create(&e, a);
    if (r != SF_OK) return r;

    sf_istream_t *is = NULL;
    r = sf_istream_open_memory(&is, data, size, a);
    if (r != SF_OK) { sf_enfl_destroy(e); return r; }

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); sf_enfl_destroy(e); return r; }

    r = enfl_read_from_reader(e, br);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);

    if (r != SF_OK) { sf_enfl_destroy(e); return r; }
    *out = e;
    return SF_OK;
}

sf_result_t sf_enfl_read_from_path(sf_enfl_t **out, const wchar_t *path,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);

    sf_enfl_t *e = NULL;
    sf_result_t r = sf_enfl_create(&e, a);
    if (r != SF_OK) return r;

    sf_istream_t *is = NULL;
    r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) { sf_enfl_destroy(e); return r; }

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); sf_enfl_destroy(e); return r; }

    r = enfl_read_from_reader(e, br);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);

    if (r != SF_OK) { sf_enfl_destroy(e); return r; }
    *out = e;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t enfl_write_inner(const sf_enfl_t *e, sf_binary_writer_t *bw) {
    sf_result_t r;
    r = sf_binary_writer_write_i32(bw, 0);                      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)e->struct1_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)e->struct2_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                      if (r != SF_OK) return r;

    for (size_t i = 0; i < e->struct1_count; i++) {
        r = sf_binary_writer_write_i16(bw, e->struct1s[i].step);  if (r != SF_OK) return r;
        r = sf_binary_writer_write_i16(bw, e->struct1s[i].index); if (r != SF_OK) return r;
    }
    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;

    for (size_t i = 0; i < e->struct2_count; i++) {
        r = sf_binary_writer_write_i64(bw, e->struct2s[i].unk1); if (r != SF_OK) return r;
    }
    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;

    r = sf_binary_writer_write_i16(bw, 0); if (r != SF_OK) return r;
    for (size_t i = 0; i < e->string_count; i++) {
        const char *s = e->strings[i] ? e->strings[i] : "";
        r = sf_binary_writer_write_utf16(bw, s, true); if (r != SF_OK) return r;
    }
    return sf_binary_writer_pad(bw, 0x10);
}

static sf_result_t enfl_write_to_writer(const sf_enfl_t *e, sf_binary_writer_t *bw) {
    sf_binary_writer_set_big_endian(bw, false);

    sf_result_t r;

    sf_ostream_t *inner_os = NULL;
    r = sf_ostream_open_memory(&inner_os, e->alloc); if (r != SF_OK) return r;

    sf_binary_writer_t *inner_bw = NULL;
    r = sf_binary_writer_create(&inner_bw, inner_os, false, e->alloc);
    if (r != SF_OK) { sf_ostream_close(inner_os); return r; }

    r = enfl_write_inner(e, inner_bw);
    if (r != SF_OK) {
        sf_binary_writer_destroy(inner_bw);
        sf_ostream_close(inner_os);
        return r;
    }

    uint8_t *raw = NULL;
    size_t   raw_size = 0;
    r = sf_binary_writer_finish_bytes(inner_bw, &raw, &raw_size);
    sf_ostream_close(inner_os);
    if (r != SF_OK) return r;

    void   *compressed     = NULL;
    size_t  compressed_size = 0;
    r = sfi_zlib_compress(raw, raw_size, &compressed, &compressed_size, e->alloc);
    sf_xfree(e->alloc, raw);
    if (r != SF_OK) return r;

    r = sf_binary_writer_write_ascii(bw, "ENFL", false); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, 0x10415);          if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, (int32_t)compressed_size); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, (int32_t)raw_size);        if (r != SF_OK) goto done;
    r = sf_binary_writer_write_bytes(bw, compressed, compressed_size);

done:
    sf_xfree(e->alloc, compressed);
    return r;
}

sf_result_t sf_enfl_write_to_memory(const sf_enfl_t *e, uint8_t **out, size_t *out_size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(e != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    a = sf_alloc_or_default(a);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, a);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, a);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = enfl_write_to_writer(e, bw);
    if (r != SF_OK) {
        sf_binary_writer_destroy(bw);
        sf_ostream_close(os);
        return r;
    }

    r = sf_binary_writer_finish_bytes(bw, out, out_size);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_enfl_write_to_path(const sf_enfl_t *e, const wchar_t *path) {
    SF_CHECK_ARG(e != NULL);
    SF_CHECK_ARG(path != NULL);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, e->alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, e->alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = enfl_write_to_writer(e, bw);
    if (r == SF_OK) {
        r = sf_binary_writer_finish(bw);
    } else {
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(os);
    return r;
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

size_t sf_enfl_struct1_count(const sf_enfl_t *e) { return e ? e->struct1_count : 0; }
size_t sf_enfl_struct2_count(const sf_enfl_t *e) { return e ? e->struct2_count : 0; }
size_t sf_enfl_string_count (const sf_enfl_t *e) { return e ? e->string_count : 0; }

const sf_enfl_struct1_t *sf_enfl_get_struct1(const sf_enfl_t *e, size_t idx) {
    if (!e || idx >= e->struct1_count) return NULL;
    return &e->struct1s[idx];
}

const sf_enfl_struct2_t *sf_enfl_get_struct2(const sf_enfl_t *e, size_t idx) {
    if (!e || idx >= e->struct2_count) return NULL;
    return &e->struct2s[idx];
}

const char *sf_enfl_get_string(const sf_enfl_t *e, size_t idx) {
    if (!e || idx >= e->string_count) return NULL;
    return e->strings[idx];
}

sf_result_t sf_enfl_add_struct1(sf_enfl_t *e, sf_enfl_struct1_t s) {
    SF_CHECK_ARG(e != NULL);
    if (e->struct1_count == e->struct1_capacity) {
        size_t cap = e->struct1_capacity ? e->struct1_capacity * 2 : 8;
        size_t old_b = e->struct1_capacity * sizeof(sf_enfl_struct1_t);
        size_t new_b = cap * sizeof(sf_enfl_struct1_t);
        void *p = sf_xrealloc(e->alloc, e->struct1s, old_b, new_b);
        if (!p) return SF_ERR_OOM;
        e->struct1s = (sf_enfl_struct1_t *)p;
        e->struct1_capacity = cap;
    }
    e->struct1s[e->struct1_count++] = s;
    return SF_OK;
}

sf_result_t sf_enfl_add_struct2(sf_enfl_t *e, sf_enfl_struct2_t s) {
    SF_CHECK_ARG(e != NULL);
    if (e->struct2_count == e->struct2_capacity) {
        size_t cap = e->struct2_capacity ? e->struct2_capacity * 2 : 8;
        size_t old_b = e->struct2_capacity * sizeof(sf_enfl_struct2_t);
        size_t new_b = cap * sizeof(sf_enfl_struct2_t);
        void *p = sf_xrealloc(e->alloc, e->struct2s, old_b, new_b);
        if (!p) return SF_ERR_OOM;
        e->struct2s = (sf_enfl_struct2_t *)p;
        e->struct2_capacity = cap;
    }
    e->struct2s[e->struct2_count++] = s;
    return SF_OK;
}

sf_result_t sf_enfl_add_string(sf_enfl_t *e, const char *utf8) {
    SF_CHECK_ARG(e != NULL);
    if (e->string_count == e->string_capacity) {
        size_t cap = e->string_capacity ? e->string_capacity * 2 : 8;
        size_t old_b = e->string_capacity * sizeof(char *);
        size_t new_b = cap * sizeof(char *);
        void *p = sf_xrealloc(e->alloc, e->strings, old_b, new_b);
        if (!p) return SF_ERR_OOM;
        e->strings = (char **)p;
        e->string_capacity = cap;
    }
    char *copy = sf_strdup(e->alloc, utf8 ? utf8 : "");
    if (!copy) return SF_ERR_OOM;
    e->strings[e->string_count++] = copy;
    return SF_OK;
}
