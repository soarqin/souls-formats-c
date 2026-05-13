/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Upstream reference: SoulsFormats/Formats/LUAGNL.cs
 */

#include "souls_formats/sf_luagnl.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

struct sf_luagnl {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    bool                  long_format;
    char                **globals;
    size_t                global_count;
    size_t                global_capacity;
};

sf_result_t sf_luagnl_create(sf_luagnl_t **out, bool big_endian,
                             bool long_format, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_luagnl_t *gnl = (sf_luagnl_t *)sf_xalloc(alloc, sizeof(*gnl));
    if (!gnl) return SF_ERR_OOM;
    memset(gnl, 0, sizeof(*gnl));
    gnl->alloc       = alloc;
    gnl->big_endian  = big_endian;
    gnl->long_format = long_format;
    *out = gnl;
    return SF_OK;
}

void sf_luagnl_destroy(sf_luagnl_t *gnl) {
    if (!gnl) return;
    if (gnl->globals) {
        for (size_t i = 0; i < gnl->global_count; i++) {
            sf_xfree(gnl->alloc, gnl->globals[i]);
        }
        sf_xfree(gnl->alloc, gnl->globals);
    }
    sf_xfree(gnl->alloc, gnl);
}

bool sf_luagnl_is(const void *bytes, size_t size) {
    (void)bytes;
    return size >= 4;
}

bool sf_luagnl_big_endian (const sf_luagnl_t *gnl) {
    return gnl ? gnl->big_endian : false;
}

bool sf_luagnl_long_format(const sf_luagnl_t *gnl) {
    return gnl ? gnl->long_format : false;
}

size_t sf_luagnl_global_count(const sf_luagnl_t *gnl) {
    return gnl ? gnl->global_count : 0u;
}

sf_result_t sf_luagnl_get_global(const sf_luagnl_t *gnl, size_t index,
                                 const char **out_utf8) {
    SF_CHECK_ARG(gnl != NULL && out_utf8 != NULL);
    if (index >= gnl->global_count) return SF_ERR_OUT_OF_RANGE;
    *out_utf8 = gnl->globals[index];
    return SF_OK;
}

static sf_result_t luagnl_reserve_one(sf_luagnl_t *gnl) {
    if (gnl->global_count < gnl->global_capacity) return SF_OK;
    size_t new_cap = gnl->global_capacity ? gnl->global_capacity * 2u : 8u;
    if (new_cap > SIZE_MAX / sizeof(char *)) return SF_ERR_OUT_OF_RANGE;
    char **new_buf = (char **)sf_xrealloc(gnl->alloc, gnl->globals,
                                          gnl->global_capacity * sizeof(char *),
                                          new_cap * sizeof(char *));
    if (!new_buf) return SF_ERR_OOM;
    gnl->globals         = new_buf;
    gnl->global_capacity = new_cap;
    return SF_OK;
}

sf_result_t sf_luagnl_add_global(sf_luagnl_t *gnl, const char *utf8) {
    SF_CHECK_ARG(gnl != NULL && utf8 != NULL);
    TRY(luagnl_reserve_one(gnl));

    size_t len = strlen(utf8);
    if (len > SIZE_MAX - 1u) return SF_ERR_OUT_OF_RANGE;
    char *dup = (char *)sf_xalloc(gnl->alloc, len + 1u);
    if (!dup) return SF_ERR_OOM;
    memcpy(dup, utf8, len + 1u);

    gnl->globals[gnl->global_count++] = dup;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

sf_result_t sf_luagnl_read_from_memory(sf_luagnl_t **out, const void *bytes,
                                       size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < 4) return SF_ERR_TRUNCATED;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t       *s   = NULL;
    sf_binary_reader_t *r   = NULL;
    sf_luagnl_t        *gnl = NULL;
    sf_result_t         err = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    int16_t probe16 = 0;
    err = sf_binary_reader_get_i16(r, 0, &probe16);
    if (err != SF_OK) goto done;
    bool big_endian = (probe16 == 0);
    sf_binary_reader_set_big_endian(r, big_endian);

    int32_t probe32 = 0;
    err = sf_binary_reader_get_i32(r, big_endian ? 0 : 4, &probe32);
    if (err != SF_OK) goto done;
    bool long_format = (probe32 == 0);

    err = sf_luagnl_create(&gnl, big_endian, long_format, alloc);
    if (err != SF_OK) goto done;

    int64_t offset = 0;
    do {
        if (long_format) {
            int64_t off64 = 0;
            err = sf_binary_reader_read_i64(r, &off64);
            if (err != SF_OK) goto done;
            offset = off64;
        } else {
            uint32_t off32 = 0;
            err = sf_binary_reader_read_u32(r, &off32);
            if (err != SF_OK) goto done;
            offset = (int64_t)off32;
        }
        if (offset != 0) {
            char *str = NULL;
            if (long_format) {
                err = sf_binary_reader_get_utf16(r, offset, &str, NULL);
            } else {
                err = sf_binary_reader_get_shift_jis(r, offset, &str, NULL);
            }
            if (err != SF_OK) goto done;

            err = luagnl_reserve_one(gnl);
            if (err != SF_OK) { sf_xfree(alloc, str); goto done; }
            gnl->globals[gnl->global_count++] = str;
        }
    } while (offset != 0);

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_luagnl_destroy(gnl); return err; }
    *out = gnl;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t luagnl_write_body(sf_binary_writer_t *bw, const sf_luagnl_t *gnl) {
    char namebuf[32];
    for (size_t i = 0; i < gnl->global_count; i++) {
        snprintf(namebuf, sizeof(namebuf), "Offset%zu", i);
        if (gnl->long_format) {
            TRY(sf_binary_writer_reserve_i64(bw, namebuf));
        } else {
            TRY(sf_binary_writer_reserve_u32(bw, namebuf));
        }
    }

    if (gnl->long_format) {
        TRY(sf_binary_writer_write_i64(bw, 0));
    } else {
        TRY(sf_binary_writer_write_u32(bw, 0u));
    }

    for (size_t i = 0; i < gnl->global_count; i++) {
        snprintf(namebuf, sizeof(namebuf), "Offset%zu", i);
        int64_t pos = sf_binary_writer_position(bw);
        if (gnl->long_format) {
            TRY(sf_binary_writer_fill_i64(bw, namebuf, pos));
            TRY(sf_binary_writer_write_utf16(bw, gnl->globals[i], true));
        } else {
            if ((uint64_t)pos > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
            TRY(sf_binary_writer_fill_u32(bw, namebuf, (uint32_t)pos));
            TRY(sf_binary_writer_write_shift_jis(bw, gnl->globals[i], true));
        }
    }

    TRY(sf_binary_writer_pad(bw, 0x10));
    return SF_OK;
}

sf_result_t sf_luagnl_write_to_memory(const sf_luagnl_t *gnl, uint8_t **out_data,
                                      size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(gnl != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t       *os = NULL;
    sf_binary_writer_t *bw = NULL;
    sf_result_t         err = SF_OK;

    err = sf_ostream_open_memory(&os, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_writer_create(&bw, os, gnl->big_endian, alloc);
    if (err != SF_OK) { sf_ostream_close(os); return err; }

    err = luagnl_write_body(bw, gnl);
    if (err != SF_OK) {
        sf_binary_writer_destroy(bw);
        sf_ostream_close(os);
        return err;
    }

    err = sf_binary_writer_finish_bytes(bw, out_data, out_size);
    sf_ostream_close(os);
    return err;
}
