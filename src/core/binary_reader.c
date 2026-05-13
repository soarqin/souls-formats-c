/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_binary_reader_t — equivalent of upstream BinaryReaderEx.
 *
 * Borrows an sf_istream_t (does not close it). Maintains:
 *   - mutable big-endian flag
 *   - mutable varint-long flag (4 vs 8 byte)
 *   - LIFO offset stack for StepIn/StepOut
 *   - per-allocator-managed scratch heap
 */

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_flver.h"

#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct sf_binary_reader {
    sf_istream_t        *stream;       /* borrowed unless owns_stream is set */
    const sf_allocator_t *alloc;
    bool                 big_endian;
    bool                 varint_long;

    int64_t             *steps;
    size_t               steps_size;
    size_t               steps_cap;

    bool                 is_flexible;

    bool                 owns_stream;
    void                *owned_buffer;
};

static bool g_binary_reader_flexible_default = false;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

sf_result_t sf_binary_reader_create(sf_binary_reader_t **out, sf_istream_t *s,
                                    bool big_endian, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(s   != NULL);
    *out = NULL;

    a = sf_alloc_or_default(a);
    sf_binary_reader_t *r = (sf_binary_reader_t *)sf_xalloc(a, sizeof(*r));
    if (!r) return SF_ERR_OOM;
    memset(r, 0, sizeof(*r));
    r->stream     = s;
    r->alloc      = a;
    r->big_endian = big_endian;
    r->is_flexible = g_binary_reader_flexible_default;
    *out = r;
    return SF_OK;
}

sf_result_t sf_binary_reader_create_from_memory(sf_binary_reader_t **out,
                                                bool big_endian, void *data,
                                                size_t size,
                                                const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (size == 0u || data != NULL));
    *out = NULL;

    a = sf_alloc_or_default(a);

    sf_istream_t *stream = NULL;
    sf_result_t e = sf_istream_open_memory(&stream, data, size, a);
    if (e != SF_OK) return e;

    sf_binary_reader_t *r = NULL;
    e = sf_binary_reader_create(&r, stream, big_endian, a);
    if (e != SF_OK) {
        sf_istream_close(stream);
        return e;
    }
    r->owns_stream  = true;
    r->owned_buffer = data;
    *out = r;
    return SF_OK;
}

void sf_binary_reader_destroy(sf_binary_reader_t *r) {
    if (!r) return;
    sf_xfree(r->alloc, r->steps);
    if (r->owns_stream) {
        sf_istream_close(r->stream);
    }
    if (r->owned_buffer) {
        sf_xfree(r->alloc, r->owned_buffer);
    }
    sf_xfree(r->alloc, r);
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

bool sf_binary_reader_big_endian(const sf_binary_reader_t *r) {
    return r ? r->big_endian : false;
}

bool sf_binary_reader_flexible_default(void) {
    return g_binary_reader_flexible_default;
}

void sf_binary_reader_set_flexible_default(bool flexible) {
    g_binary_reader_flexible_default = flexible;
}

bool sf_binary_reader_flexible(const sf_binary_reader_t *r) {
    return r ? r->is_flexible : false;
}

void sf_binary_reader_set_flexible(sf_binary_reader_t *r, bool flexible) {
    if (r) r->is_flexible = flexible;
}

sf_istream_t *sf_binary_reader_stream(sf_binary_reader_t *r) {
    return r ? r->stream : NULL;
}

void sf_binary_reader_set_big_endian(sf_binary_reader_t *r, bool be) {
    if (r) r->big_endian = be;
}

bool sf_binary_reader_varint_long(const sf_binary_reader_t *r) {
    return r ? r->varint_long : false;
}

void sf_binary_reader_set_varint_long(sf_binary_reader_t *r, bool long64) {
    if (r) r->varint_long = long64;
}

int64_t sf_binary_reader_position (const sf_binary_reader_t *r) {
    return r ? sf_istream_position(r->stream) : 0;
}
int64_t sf_binary_reader_length   (const sf_binary_reader_t *r) {
    return r ? sf_istream_length(r->stream) : 0;
}
int64_t sf_binary_reader_remaining(const sf_binary_reader_t *r) {
    return r ? sf_istream_remaining(r->stream) : 0;
}

/*===========================================================================
 * Step / pad / skip
 *===========================================================================*/

static sf_result_t steps_push(sf_binary_reader_t *r, int64_t pos) {
    if (r->steps_size == r->steps_cap) {
        size_t new_cap = r->steps_cap ? r->steps_cap * 2 : 8;
        int64_t *p = (int64_t *)sf_xrealloc(r->alloc, r->steps,
                                            r->steps_cap * sizeof(int64_t),
                                            new_cap * sizeof(int64_t));
        if (!p) return SF_ERR_OOM;
        r->steps = p;
        r->steps_cap = new_cap;
    }
    r->steps[r->steps_size++] = pos;
    return SF_OK;
}

sf_result_t sf_binary_reader_step_in(sf_binary_reader_t *r, int64_t pos) {
    SF_CHECK_ARG(r != NULL);
    int64_t cur = sf_istream_position(r->stream);
    sf_result_t e = steps_push(r, cur);
    if (e != SF_OK) return e;
    return sf_istream_seek(r->stream, pos);
}

sf_result_t sf_binary_reader_step_out(sf_binary_reader_t *r) {
    SF_CHECK_ARG(r != NULL);
    if (r->steps_size == 0) return SF_ERR_INTERNAL;
    int64_t pos = r->steps[--r->steps_size];
    return sf_istream_seek(r->stream, pos);
}

sf_result_t sf_binary_reader_pad(sf_binary_reader_t *r, int align) {
    SF_CHECK_ARG(r != NULL && align > 0);
    int64_t pos = sf_istream_position(r->stream);
    int64_t rem = pos % align;
    if (rem == 0) return SF_OK;
    return sf_istream_seek(r->stream, pos + (align - rem));
}

sf_result_t sf_binary_reader_pad_relative(sf_binary_reader_t *r, int64_t start, int align) {
    SF_CHECK_ARG(r != NULL && align > 0);
    int64_t pos = sf_istream_position(r->stream);
    SF_CHECK_ARG(start <= pos);
    int64_t rel = pos - start;
    int64_t rem = rel % align;
    if (rem == 0) return SF_OK;
    return sf_istream_seek(r->stream, pos + (align - rem));
}

sf_result_t sf_binary_reader_skip(sf_binary_reader_t *r, int64_t n) {
    SF_CHECK_ARG(r != NULL);
    int64_t pos = sf_istream_position(r->stream);
    return sf_istream_seek(r->stream, pos + n);
}

/*===========================================================================
 * Primitive reads
 *
 * The "_le" form reads the host-LE bytes; for big-endian we byte-swap.
 *===========================================================================*/

sf_result_t sf_binary_reader_read_bytes(sf_binary_reader_t *r, void *buf, size_t n) {
    SF_CHECK_ARG(r != NULL);
    return sf_istream_read(r->stream, buf, n);
}

sf_result_t sf_binary_reader_read_bool(sf_binary_reader_t *r, bool *out) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    uint8_t b = 0;
    sf_result_t e = sf_istream_read(r->stream, &b, 1);
    if (e != SF_OK) return e;
    if (b > 1) return SF_ERR_BAD_MAGIC;
    *out = (b != 0);
    return SF_OK;
}

sf_result_t sf_binary_reader_read_i8(sf_binary_reader_t *r, int8_t *out) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    uint8_t b = 0;
    sf_result_t e = sf_istream_read(r->stream, &b, 1);
    if (e != SF_OK) return e;
    *out = (int8_t)b;
    return SF_OK;
}

sf_result_t sf_binary_reader_read_u8(sf_binary_reader_t *r, uint8_t *out) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    return sf_istream_read(r->stream, out, 1);
}

#define DEFINE_RW_INT(suffix, ctype, swap_fn)                                \
    sf_result_t sf_binary_reader_read_##suffix(sf_binary_reader_t *r,        \
                                               ctype *out) {                 \
        SF_CHECK_ARG(r != NULL && out != NULL);                              \
        ctype v = 0;                                                          \
        sf_result_t e = sf_istream_read(r->stream, &v, sizeof(v));           \
        if (e != SF_OK) return e;                                            \
        if (r->big_endian) {                                                 \
            v = (ctype)swap_fn((ctype)v);                                    \
        }                                                                    \
        *out = v;                                                            \
        return SF_OK;                                                        \
    }

DEFINE_RW_INT(u16, uint16_t, sf_bswap16)
DEFINE_RW_INT(u32, uint32_t, sf_bswap32)
DEFINE_RW_INT(u64, uint64_t, sf_bswap64)

sf_result_t sf_binary_reader_read_i16(sf_binary_reader_t *r, int16_t *out) {
    uint16_t u = 0;
    sf_result_t e = sf_binary_reader_read_u16(r, &u);
    if (e != SF_OK) return e;
    *out = (int16_t)u;
    return SF_OK;
}
sf_result_t sf_binary_reader_read_i32(sf_binary_reader_t *r, int32_t *out) {
    uint32_t u = 0;
    sf_result_t e = sf_binary_reader_read_u32(r, &u);
    if (e != SF_OK) return e;
    *out = (int32_t)u;
    return SF_OK;
}
sf_result_t sf_binary_reader_read_i64(sf_binary_reader_t *r, int64_t *out) {
    uint64_t u = 0;
    sf_result_t e = sf_binary_reader_read_u64(r, &u);
    if (e != SF_OK) return e;
    *out = (int64_t)u;
    return SF_OK;
}

sf_result_t sf_binary_reader_read_f32(sf_binary_reader_t *r, float *out) {
    SF_CHECK_ARG(out != NULL);
    uint32_t u = 0;
    sf_result_t e = sf_binary_reader_read_u32(r, &u);
    if (e != SF_OK) return e;
    memcpy(out, &u, sizeof(*out));
    return SF_OK;
}

sf_result_t sf_binary_reader_read_f64(sf_binary_reader_t *r, double *out) {
    SF_CHECK_ARG(out != NULL);
    uint64_t u = 0;
    sf_result_t e = sf_binary_reader_read_u64(r, &u);
    if (e != SF_OK) return e;
    memcpy(out, &u, sizeof(*out));
    return SF_OK;
}

sf_result_t sf_binary_reader_read_varint(sf_binary_reader_t *r, int64_t *out) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    if (r->varint_long) return sf_binary_reader_read_i64(r, out);
    int32_t v = 0;
    sf_result_t e = sf_binary_reader_read_i32(r, &v);
    if (e != SF_OK) return e;
    *out = (int64_t)v;
    return SF_OK;
}

static void bswap_array16(void *values, size_t n) {
    uint8_t *p = (uint8_t *)values;
    for (size_t i = 0; i < n; i++) {
        uint16_t v = 0;
        memcpy(&v, p + i * sizeof(v), sizeof(v));
        v = sf_bswap16(v);
        memcpy(p + i * sizeof(v), &v, sizeof(v));
    }
}

static void bswap_array32(void *values, size_t n) {
    uint8_t *p = (uint8_t *)values;
    for (size_t i = 0; i < n; i++) {
        uint32_t v = 0;
        memcpy(&v, p + i * sizeof(v), sizeof(v));
        v = sf_bswap32(v);
        memcpy(p + i * sizeof(v), &v, sizeof(v));
    }
}

static void bswap_array64(void *values, size_t n) {
    uint8_t *p = (uint8_t *)values;
    for (size_t i = 0; i < n; i++) {
        uint64_t v = 0;
        memcpy(&v, p + i * sizeof(v), sizeof(v));
        v = sf_bswap64(v);
        memcpy(p + i * sizeof(v), &v, sizeof(v));
    }
}

#define DEFINE_READ_ARRAY_RAW(suffix, ctype, swap_fn)                           \
    sf_result_t sf_binary_reader_read_##suffix##s(sf_binary_reader_t *r,        \
                                                  size_t n, ctype *out_array) { \
        SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));               \
        if (n == 0) return SF_OK;                                               \
        if (n > SIZE_MAX / sizeof(*out_array)) return SF_ERR_OUT_OF_RANGE;      \
        sf_result_t e = sf_istream_read(r->stream, out_array, n * sizeof(*out_array)); \
        if (e != SF_OK) return e;                                               \
        if (r->big_endian) swap_fn(out_array, n);                               \
        return SF_OK;                                                           \
    }

sf_result_t sf_binary_reader_read_bools(sf_binary_reader_t *r, size_t n,
                                        bool *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    for (size_t i = 0; i < n; i++) {
        sf_result_t e = sf_binary_reader_read_bool(r, &out_array[i]);
        if (e != SF_OK) return e;
    }
    return SF_OK;
}

sf_result_t sf_binary_reader_read_i8s(sf_binary_reader_t *r, size_t n, int8_t *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    if (n == 0) return SF_OK;
    return sf_istream_read(r->stream, out_array, n);
}

sf_result_t sf_binary_reader_read_u8s(sf_binary_reader_t *r, size_t n,
                                      uint8_t *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    if (n == 0) return SF_OK;
    return sf_istream_read(r->stream, out_array, n);
}

DEFINE_READ_ARRAY_RAW(i16, int16_t, bswap_array16)
DEFINE_READ_ARRAY_RAW(u16, uint16_t, bswap_array16)
DEFINE_READ_ARRAY_RAW(i32, int32_t, bswap_array32)
DEFINE_READ_ARRAY_RAW(u32, uint32_t, bswap_array32)
DEFINE_READ_ARRAY_RAW(i64, int64_t, bswap_array64)
DEFINE_READ_ARRAY_RAW(u64, uint64_t, bswap_array64)
DEFINE_READ_ARRAY_RAW(f32, float, bswap_array32)
DEFINE_READ_ARRAY_RAW(f64, double, bswap_array64)

sf_result_t sf_binary_reader_read_varints(sf_binary_reader_t *r, size_t n,
                                          int64_t *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    for (size_t i = 0; i < n; i++) {
        sf_result_t e = sf_binary_reader_read_varint(r, &out_array[i]);
        if (e != SF_OK) return e;
    }
    return SF_OK;
}

/*===========================================================================
 * Get* (read at absolute offset)
 *===========================================================================*/

#define DEFINE_GET(suffix, ctype)                                            \
    sf_result_t sf_binary_reader_get_##suffix(sf_binary_reader_t *r,         \
                                              int64_t off, ctype *out) {     \
        SF_CHECK_ARG(r != NULL && out != NULL);                              \
        sf_result_t e = sf_binary_reader_step_in(r, off);                    \
        if (e != SF_OK) return e;                                            \
        e = sf_binary_reader_read_##suffix(r, out);                          \
        sf_result_t e2 = sf_binary_reader_step_out(r);                       \
        return (e != SF_OK) ? e : e2;                                        \
    }

DEFINE_GET(u32, uint32_t)
DEFINE_GET(i32, int32_t)
DEFINE_GET(u64, uint64_t)
DEFINE_GET(i64, int64_t)
DEFINE_GET(bool, bool)
DEFINE_GET(i8, int8_t)
DEFINE_GET(u8, uint8_t)
DEFINE_GET(i16, int16_t)
DEFINE_GET(u16, uint16_t)
DEFINE_GET(f32, float)
DEFINE_GET(f64, double)

sf_result_t sf_binary_reader_get_varint(sf_binary_reader_t *r, int64_t off,
                                        int64_t *out) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_varint(r, out);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

sf_result_t sf_binary_reader_get_bytes(sf_binary_reader_t *r, int64_t off,
                                       void *buf, size_t n) {
    SF_CHECK_ARG(r != NULL);
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_istream_read(r->stream, buf, n);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

#define DEFINE_GET_ARRAY(suffix, ctype)                                      \
    sf_result_t sf_binary_reader_get_##suffix##s(sf_binary_reader_t *r,       \
                                                 int64_t off, size_t n,       \
                                                 ctype *out_array) {          \
        SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));             \
        sf_result_t e = sf_binary_reader_step_in(r, off);                     \
        if (e != SF_OK) return e;                                             \
        e = sf_binary_reader_read_##suffix##s(r, n, out_array);               \
        sf_result_t e2 = sf_binary_reader_step_out(r);                        \
        return (e != SF_OK) ? e : e2;                                         \
    }

sf_result_t sf_binary_reader_get_bools(sf_binary_reader_t *r, int64_t off, size_t n,
                                       bool *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_bools(r, n, out_array);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

DEFINE_GET_ARRAY(i8, int8_t)
DEFINE_GET_ARRAY(u8, uint8_t)
DEFINE_GET_ARRAY(i16, int16_t)
DEFINE_GET_ARRAY(u16, uint16_t)
DEFINE_GET_ARRAY(i32, int32_t)
DEFINE_GET_ARRAY(u32, uint32_t)
DEFINE_GET_ARRAY(i64, int64_t)
DEFINE_GET_ARRAY(u64, uint64_t)
DEFINE_GET_ARRAY(f32, float)
DEFINE_GET_ARRAY(f64, double)

sf_result_t sf_binary_reader_get_varints(sf_binary_reader_t *r, int64_t off, size_t n,
                                         int64_t *out_array) {
    SF_CHECK_ARG(r != NULL && (n == 0 || out_array != NULL));
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_varints(r, n, out_array);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

/*===========================================================================
 * Assert*
 *===========================================================================*/

#define DEFINE_ASSERT_MULTI(suffix, ctype)                                    \
    sf_result_t sf_binary_reader_assert_##suffix(sf_binary_reader_t *r,       \
                                                 size_t n_options,            \
                                                 const ctype *options,        \
                                                 ctype *out_value) {          \
        SF_CHECK_ARG(r != NULL && n_options > 0 && options != NULL);          \
        ctype v;                                                              \
        sf_result_t e = sf_binary_reader_read_##suffix(r, &v);                \
        if (e != SF_OK) return e;                                             \
        if (out_value) *out_value = v;                                        \
        if (r->is_flexible) return SF_OK;                                     \
        for (size_t i = 0; i < n_options; i++) {                              \
            if (v == options[i]) return SF_OK;                                \
        }                                                                     \
        return SF_ERR_BAD_MAGIC;                                              \
    }                                                                         \
    sf_result_t sf_binary_reader_assert_##suffix##_one(sf_binary_reader_t *r, \
                                                       ctype expect) {         \
        return sf_binary_reader_assert_##suffix(r, 1, &expect, NULL);         \
    }

DEFINE_ASSERT_MULTI(bool, bool)
DEFINE_ASSERT_MULTI(i8, int8_t)
DEFINE_ASSERT_MULTI(u8, uint8_t)
DEFINE_ASSERT_MULTI(i16, int16_t)
DEFINE_ASSERT_MULTI(u16, uint16_t)
DEFINE_ASSERT_MULTI(i32, int32_t)
DEFINE_ASSERT_MULTI(u32, uint32_t)
DEFINE_ASSERT_MULTI(i64, int64_t)
DEFINE_ASSERT_MULTI(u64, uint64_t)
DEFINE_ASSERT_MULTI(f32, float)
DEFINE_ASSERT_MULTI(f64, double)
DEFINE_ASSERT_MULTI(varint, int64_t)

#define DEFINE_ENUM_READ(width, ctype, read_suffix)                           \
    sf_result_t sf_binary_reader_read_enum_##width(sf_binary_reader_t *r,     \
                                                   size_t n_options,          \
                                                   const ctype *options,      \
                                                   ctype *out_value) {        \
        SF_CHECK_ARG(r != NULL && n_options > 0 && options != NULL &&         \
                     out_value != NULL);                                      \
        ctype v;                                                              \
        sf_result_t e = sf_binary_reader_read_##read_suffix(r, &v);           \
        if (e != SF_OK) return e;                                             \
        *out_value = v;                                                       \
        for (size_t i = 0; i < n_options; i++) {                              \
            if (v == options[i]) return SF_OK;                                \
        }                                                                     \
        return SF_ERR_BAD_MAGIC;                                              \
    }                                                                         \
    sf_result_t sf_binary_reader_get_enum_##width(sf_binary_reader_t *r,      \
                                                  int64_t off,                \
                                                  size_t n_options,           \
                                                  const ctype *options,       \
                                                  ctype *out_value) {         \
        SF_CHECK_ARG(r != NULL && n_options > 0 && options != NULL &&         \
                     out_value != NULL);                                      \
        sf_result_t e = sf_binary_reader_step_in(r, off);                     \
        if (e != SF_OK) return e;                                             \
        e = sf_binary_reader_read_enum_##width(r, n_options, options,         \
                                               out_value);                    \
        sf_result_t e2 = sf_binary_reader_step_out(r);                        \
        return (e != SF_OK) ? e : e2;                                         \
    }

DEFINE_ENUM_READ(8, uint8_t, u8)
DEFINE_ENUM_READ(16, uint16_t, u16)
DEFINE_ENUM_READ(32, uint32_t, u32)
DEFINE_ENUM_READ(64, uint64_t, u64)

sf_result_t sf_binary_reader_assert_pattern(sf_binary_reader_t *r, size_t length,
                                            uint8_t pattern) {
    SF_CHECK_ARG(r != NULL);
    /*  Read in 256-byte chunks to avoid heap alloc and bound stack use. */
    uint8_t buf[256];
    size_t left = length;
    while (left > 0) {
        size_t chunk = (left > sizeof(buf)) ? sizeof(buf) : left;
        sf_result_t e = sf_istream_read(r->stream, buf, chunk);
        if (e != SF_OK) return e;
        for (size_t i = 0; i < chunk; i++) {
            if (buf[i] != pattern) return SF_ERR_BAD_MAGIC;
        }
        left -= chunk;
    }
    return SF_OK;
}

/*===========================================================================
 * Vector / quat / color
 *===========================================================================*/

sf_result_t sf_binary_reader_read_vec2(sf_binary_reader_t *r, sf_vec2_t *out) {
    SF_CHECK_ARG(out != NULL);
    sf_result_t e;
    if ((e = sf_binary_reader_read_f32(r, &out->x)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->y)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_reader_read_vec3(sf_binary_reader_t *r, sf_vec3_t *out) {
    SF_CHECK_ARG(out != NULL);
    sf_result_t e;
    if ((e = sf_binary_reader_read_f32(r, &out->x)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->y)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->z)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_reader_read_vec4(sf_binary_reader_t *r, sf_vec4_t *out) {
    SF_CHECK_ARG(out != NULL);
    sf_result_t e;
    if ((e = sf_binary_reader_read_f32(r, &out->x)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->y)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->z)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->w)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_reader_read_quat(sf_binary_reader_t *r, sf_quat_t *out) {
    SF_CHECK_ARG(out != NULL);
    sf_result_t e;
    if ((e = sf_binary_reader_read_f32(r, &out->x)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->y)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->z)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_f32(r, &out->w)) != SF_OK) return e;
    return SF_OK;
}

sf_result_t sf_binary_reader_read_11_11_10_vec3(sf_binary_reader_t *r, sf_vec3_t *out) {
    SF_CHECK_ARG(out != NULL);
    uint32_t packed;
    sf_result_t e = sf_binary_reader_read_u32(r, &packed);
    if (e != SF_OK) return e;
    sf_unpack_11_11_10(packed, &out->x, &out->y, &out->z);
    return SF_OK;
}

static sf_result_t read_color_4(sf_binary_reader_t *r, uint8_t *a, uint8_t *b,
                                uint8_t *c, uint8_t *d) {
    sf_result_t e;
    if ((e = sf_binary_reader_read_u8(r, a)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_u8(r, b)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_u8(r, c)) != SF_OK) return e;
    if ((e = sf_binary_reader_read_u8(r, d)) != SF_OK) return e;
    return SF_OK;
}

sf_result_t sf_binary_reader_read_argb(sf_binary_reader_t *r, sf_color_t *out) {
    SF_CHECK_ARG(out != NULL);
    return read_color_4(r, &out->a, &out->r, &out->g, &out->b);
}
sf_result_t sf_binary_reader_read_abgr(sf_binary_reader_t *r, sf_color_t *out) {
    SF_CHECK_ARG(out != NULL);
    return read_color_4(r, &out->a, &out->b, &out->g, &out->r);
}
sf_result_t sf_binary_reader_read_rgba(sf_binary_reader_t *r, sf_color_t *out) {
    SF_CHECK_ARG(out != NULL);
    return read_color_4(r, &out->r, &out->g, &out->b, &out->a);
}
sf_result_t sf_binary_reader_read_bgra(sf_binary_reader_t *r, sf_color_t *out) {
    SF_CHECK_ARG(out != NULL);
    return read_color_4(r, &out->b, &out->g, &out->r, &out->a);
}

/*===========================================================================
 * Strings
 *
 * Internal `read_terminated_1byte` reads bytes until a single 0x00, then
 * passes the (non-terminator) bytes to the supplied decoder. Length is
 * returned in `bytes_consumed_excl_term`.
 *===========================================================================*/

typedef sf_result_t (*decode_fn)(const void *, size_t, char **, size_t *,
                                 const sf_allocator_t *);

static sf_result_t read_str_terminated_1byte(sf_binary_reader_t *r,
                                             decode_fn decode,
                                             char **out_utf8, size_t *out_len) {
    /*  Buffer grows geometrically. Worst case: very long ASCII names. */
    size_t cap = 64, len = 0;
    uint8_t *buf = (uint8_t *)sf_xalloc(r->alloc, cap);
    if (!buf) return SF_ERR_OOM;

    for (;;) {
        uint8_t b;
        sf_result_t e = sf_istream_read(r->stream, &b, 1);
        if (e != SF_OK) {
            sf_xfree(r->alloc, buf);
            return e;
        }
        if (b == 0) break;
        if (len + 1 > cap) {
            size_t new_cap = cap * 2;
            uint8_t *p = (uint8_t *)sf_xrealloc(r->alloc, buf, cap, new_cap);
            if (!p) { sf_xfree(r->alloc, buf); return SF_ERR_OOM; }
            buf = p; cap = new_cap;
        }
        buf[len++] = b;
    }

    sf_result_t e = decode(buf, len, out_utf8, out_len, r->alloc);
    sf_xfree(r->alloc, buf);
    return e;
}

static sf_result_t read_str_n(sf_binary_reader_t *r, size_t n,
                              decode_fn decode,
                              char **out_utf8, size_t *out_len) {
    if (n == 0) return decode(NULL, 0, out_utf8, out_len, r->alloc);
    uint8_t *buf = (uint8_t *)sf_xalloc(r->alloc, n);
    if (!buf) return SF_ERR_OOM;
    sf_result_t e = sf_istream_read(r->stream, buf, n);
    if (e != SF_OK) { sf_xfree(r->alloc, buf); return e; }
    e = decode(buf, n, out_utf8, out_len, r->alloc);
    sf_xfree(r->alloc, buf);
    return e;
}

sf_result_t sf_binary_reader_read_ascii(sf_binary_reader_t *r,
                                        char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    return read_str_terminated_1byte(r, sf_ascii_to_utf8, out, out_len);
}

sf_result_t sf_binary_reader_read_ascii_n(sf_binary_reader_t *r, size_t n,
                                          char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    return read_str_n(r, n, sf_ascii_to_utf8, out, out_len);
}

sf_result_t sf_binary_reader_read_shift_jis(sf_binary_reader_t *r,
                                            char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    return read_str_terminated_1byte(r, sf_shift_jis_to_utf8, out, out_len);
}

sf_result_t sf_binary_reader_read_shift_jis_n(sf_binary_reader_t *r, size_t n,
                                              char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    return read_str_n(r, n, sf_shift_jis_to_utf8, out, out_len);
}

sf_result_t sf_binary_reader_read_utf16(sf_binary_reader_t *r,
                                        char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);

    /*  Read 2 bytes at a time until 0x0000. */
    size_t cap = 64, len = 0;
    uint8_t *buf = (uint8_t *)sf_xalloc(r->alloc, cap);
    if (!buf) return SF_ERR_OOM;
    for (;;) {
        uint8_t pair[2];
        sf_result_t e = sf_istream_read(r->stream, pair, 2);
        if (e != SF_OK) { sf_xfree(r->alloc, buf); return e; }
        if (pair[0] == 0 && pair[1] == 0) break;
        if (len + 2 > cap) {
            size_t new_cap = cap * 2;
            uint8_t *p = (uint8_t *)sf_xrealloc(r->alloc, buf, cap, new_cap);
            if (!p) { sf_xfree(r->alloc, buf); return SF_ERR_OOM; }
            buf = p; cap = new_cap;
        }
        buf[len++] = pair[0];
        buf[len++] = pair[1];
    }

    sf_result_t e = r->big_endian
        ? sf_utf16be_to_utf8(buf, len, out, out_len, r->alloc)
        : sf_utf16le_to_utf8(buf, len, out, out_len, r->alloc);
    sf_xfree(r->alloc, buf);
    return e;
}

sf_result_t sf_binary_reader_read_fix_str(sf_binary_reader_t *r, size_t size,
                                          char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    if (size == 0) return sf_shift_jis_to_utf8(NULL, 0, out, out_len, r->alloc);
    uint8_t *buf = (uint8_t *)sf_xalloc(r->alloc, size);
    if (!buf) return SF_ERR_OOM;
    sf_result_t e = sf_istream_read(r->stream, buf, size);
    if (e != SF_OK) { sf_xfree(r->alloc, buf); return e; }
    /*  Truncate at first NUL within field. */
    size_t term;
    for (term = 0; term < size; term++) if (buf[term] == 0) break;
    e = sf_shift_jis_to_utf8(buf, term, out, out_len, r->alloc);
    sf_xfree(r->alloc, buf);
    return e;
}

sf_result_t sf_binary_reader_read_fix_str_w(sf_binary_reader_t *r, size_t size,
                                            char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    if (size == 0) return sf_utf16le_to_utf8(NULL, 0, out, out_len, r->alloc);
    uint8_t *buf = (uint8_t *)sf_xalloc(r->alloc, size);
    if (!buf) return SF_ERR_OOM;
    sf_result_t e = sf_istream_read(r->stream, buf, size);
    if (e != SF_OK) { sf_xfree(r->alloc, buf); return e; }
    /*  Truncate at first 0x0000 pair within field, aligned to 2. */
    size_t term;
    for (term = 0; term + 1 < size; term += 2) {
        if (buf[term] == 0 && buf[term + 1] == 0) break;
    }
    if (term > size) term = size;
    e = r->big_endian
        ? sf_utf16be_to_utf8(buf, term, out, out_len, r->alloc)
        : sf_utf16le_to_utf8(buf, term, out, out_len, r->alloc);
    sf_xfree(r->alloc, buf);
    return e;
}

sf_result_t sf_binary_reader_assert_ascii(sf_binary_reader_t *r, const char *expected) {
    SF_CHECK_ARG(r != NULL && expected != NULL);
    size_t len = strlen(expected);
    char buf[64];
    if (len > sizeof(buf)) {
        char *heap = (char *)sf_xalloc(r->alloc, len);
        if (!heap) return SF_ERR_OOM;
        sf_result_t e = sf_istream_read(r->stream, heap, len);
        if (e != SF_OK) { sf_xfree(r->alloc, heap); return e; }
        int diff = memcmp(heap, expected, len);
        sf_xfree(r->alloc, heap);
        return diff == 0 ? SF_OK : SF_ERR_BAD_MAGIC;
    }
    sf_result_t e = sf_istream_read(r->stream, buf, len);
    if (e != SF_OK) return e;
    return memcmp(buf, expected, len) == 0 ? SF_OK : SF_ERR_BAD_MAGIC;
}

#define DEFINE_GET_STRING(name, read_call)                                    \
    sf_result_t sf_binary_reader_get_##name(sf_binary_reader_t *r, int64_t off, \
                                            char **out, size_t *out_len) {     \
        SF_CHECK_ARG(r != NULL && out != NULL);                               \
        sf_result_t e = sf_binary_reader_step_in(r, off);                     \
        if (e != SF_OK) return e;                                             \
        e = read_call;                                                        \
        sf_result_t e2 = sf_binary_reader_step_out(r);                        \
        return (e != SF_OK) ? e : e2;                                         \
    }

DEFINE_GET_STRING(ascii, sf_binary_reader_read_ascii(r, out, out_len))
DEFINE_GET_STRING(shift_jis, sf_binary_reader_read_shift_jis(r, out, out_len))
DEFINE_GET_STRING(utf16, sf_binary_reader_read_utf16(r, out, out_len))

sf_result_t sf_binary_reader_get_ascii_n(sf_binary_reader_t *r, int64_t off,
                                         size_t n, char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_ascii_n(r, n, out, out_len);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

sf_result_t sf_binary_reader_get_shift_jis_n(sf_binary_reader_t *r, int64_t off,
                                             size_t n, char **out, size_t *out_len) {
    SF_CHECK_ARG(r != NULL && out != NULL);
    sf_result_t e = sf_binary_reader_step_in(r, off);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_shift_jis_n(r, n, out, out_len);
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

/*===========================================================================
 * Bit utilities — mirrors upstream SoulsFormats.Utilities.EndianHelper
 *===========================================================================*/

SF_API uint8_t sf_reverse_bits_u8(uint8_t b) {
    return (uint8_t)(
        ((b & 0x01u) << 7) |
        ((b & 0x02u) << 5) |
        ((b & 0x04u) << 3) |
        ((b & 0x08u) << 1) |
        ((b & 0x10u) >> 1) |
        ((b & 0x20u) >> 3) |
        ((b & 0x40u) >> 5) |
        ((b & 0x80u) >> 7)
    );
}
