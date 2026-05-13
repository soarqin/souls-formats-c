/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_binary_writer_t — equivalent of upstream BinaryWriterEx.
 *
 * Borrows an sf_ostream_t (does not close it). Maintains:
 *   - mutable big-endian / varint-long flags
 *   - LIFO offset stack (StepIn / StepOut)
 *   - a flat list of pending Reservations (small N, linear scan is fine)
 */

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_encoding.h"

#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SFRES_BOOL = 1,
    SFRES_I8,
    SFRES_U8,
    SFRES_I16,
    SFRES_U16,
    SFRES_U32,
    SFRES_I32,
    SFRES_U64,
    SFRES_I64,
    SFRES_VARINT_4,
    SFRES_VARINT_8,
    SFRES_F32,
    SFRES_F64,
} sfres_kind_t;

typedef struct sfres {
    char        *name;     /* heap-owned via writer's allocator */
    int64_t      pos;
    sfres_kind_t kind;
} sfres_t;

struct sf_binary_writer {
    sf_ostream_t        *stream;       /* borrowed */
    const sf_allocator_t *alloc;
    bool                 big_endian;
    bool                 varint_long;

    int64_t             *steps;
    size_t               steps_size;
    size_t               steps_cap;

    sfres_t             *res;
    size_t               res_size;
    size_t               res_cap;
    bool                 closed;
};

static sf_result_t writer_open(const sf_binary_writer_t *w) {
    SF_CHECK_ARG(w != NULL);
    return w->closed ? SF_ERR_INVALID_ARG : SF_OK;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

sf_result_t sf_binary_writer_create(sf_binary_writer_t **out, sf_ostream_t *s,
                                    bool big_endian, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(s   != NULL);
    *out = NULL;
    a = sf_alloc_or_default(a);
    sf_binary_writer_t *w = (sf_binary_writer_t *)sf_xalloc(a, sizeof(*w));
    if (!w) return SF_ERR_OOM;
    memset(w, 0, sizeof(*w));
    w->stream     = s;
    w->alloc      = a;
    w->big_endian = big_endian;
    *out = w;
    return SF_OK;
}

void sf_binary_writer_destroy(sf_binary_writer_t *w) {
    if (!w) return;
    sf_xfree(w->alloc, w->steps);
    for (size_t i = 0; i < w->res_size; i++) sf_xfree(w->alloc, w->res[i].name);
    sf_xfree(w->alloc, w->res);
    sf_xfree(w->alloc, w);
}

sf_result_t sf_binary_writer_finish(sf_binary_writer_t *w) {
    SF_CHECK_ARG(w != NULL);
    if (w->res_size > 0) return SF_ERR_INTERNAL;
    w->closed = true;
    return SF_OK;
}

sf_result_t sf_binary_writer_to_array(sf_binary_writer_t *w, uint8_t **out, size_t *out_size) {
    SF_CHECK_ARG(w != NULL);
    return sfi_ostream_to_array(w->stream, w->alloc, out, out_size);
}

sf_result_t sf_binary_writer_finish_bytes(sf_binary_writer_t *w, uint8_t **out, size_t *out_size) {
    SF_CHECK_ARG(w != NULL);
    SF_CHECK_ARG(out != NULL && out_size != NULL);
    if (w->res_size > 0) return SF_ERR_INTERNAL;
    sf_result_t e = sfi_ostream_to_array(w->stream, w->alloc, out, out_size);
    if (e != SF_OK) return e;
    w->closed = true;
    return SF_OK;
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

bool sf_binary_writer_big_endian(const sf_binary_writer_t *w) { return w ? w->big_endian : false; }
void sf_binary_writer_set_big_endian(sf_binary_writer_t *w, bool be) { if (w) w->big_endian = be; }
bool sf_binary_writer_varint_long(const sf_binary_writer_t *w) { return w ? w->varint_long : false; }
void sf_binary_writer_set_varint_long(sf_binary_writer_t *w, bool l) { if (w) w->varint_long = l; }
sf_ostream_t *sf_binary_writer_stream(sf_binary_writer_t *w) { return w ? w->stream : NULL; }

int64_t sf_binary_writer_position(const sf_binary_writer_t *w) {
    return w ? sf_ostream_position(w->stream) : 0;
}
int64_t sf_binary_writer_length(const sf_binary_writer_t *w) {
    return w ? sf_ostream_length(w->stream) : 0;
}

/*===========================================================================
 * Step / pad
 *===========================================================================*/

static sf_result_t steps_push(sf_binary_writer_t *w, int64_t pos) {
    if (w->steps_size == w->steps_cap) {
        size_t new_cap = w->steps_cap ? w->steps_cap * 2 : 8;
        int64_t *p = (int64_t *)sf_xrealloc(w->alloc, w->steps,
                                            w->steps_cap * sizeof(int64_t),
                                            new_cap * sizeof(int64_t));
        if (!p) return SF_ERR_OOM;
        w->steps = p; w->steps_cap = new_cap;
    }
    w->steps[w->steps_size++] = pos;
    return SF_OK;
}

sf_result_t sf_binary_writer_step_in(sf_binary_writer_t *w, int64_t pos) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    int64_t cur = sf_ostream_position(w->stream);
    e = steps_push(w, cur);
    if (e != SF_OK) return e;
    return sf_ostream_seek(w->stream, pos);
}

sf_result_t sf_binary_writer_step_out(sf_binary_writer_t *w) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    if (w->steps_size == 0) return SF_ERR_INTERNAL;
    int64_t pos = w->steps[--w->steps_size];
    return sf_ostream_seek(w->stream, pos);
}

sf_result_t sf_binary_writer_pad_byte(sf_binary_writer_t *w, int align, uint8_t fill) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    SF_CHECK_ARG(align > 0);
    int64_t pos = sf_ostream_position(w->stream);
    int64_t rem = pos % align;
    if (rem == 0) return SF_OK;
    int64_t pad = align - rem;
    uint8_t buf[64];
    memset(buf, fill, sizeof(buf));
    while (pad > 0) {
        size_t chunk = (pad > (int64_t)sizeof(buf)) ? sizeof(buf) : (size_t)pad;
        e = sf_ostream_write(w->stream, buf, chunk);
        if (e != SF_OK) return e;
        pad -= (int64_t)chunk;
    }
    return SF_OK;
}

sf_result_t sf_binary_writer_pad(sf_binary_writer_t *w, int align) {
    return sf_binary_writer_pad_byte(w, align, 0);
}

sf_result_t sf_binary_writer_pad_ff(sf_binary_writer_t *w, int align) {
    return sf_binary_writer_pad_byte(w, align, 0xFF);
}

sf_result_t sf_binary_writer_pad_relative(sf_binary_writer_t *w, int64_t start, int align) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    int64_t pos = sf_ostream_position(w->stream);
    SF_CHECK_ARG(align > 0 && start <= pos);
    int64_t rel = pos - start;
    int64_t rem = rel % align;
    if (rem == 0) return SF_OK;
    int64_t pad = align - rem;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    while (pad > 0) {
        size_t chunk = (pad > (int64_t)sizeof(buf)) ? sizeof(buf) : (size_t)pad;
        e = sf_ostream_write(w->stream, buf, chunk);
        if (e != SF_OK) return e;
        pad -= (int64_t)chunk;
    }
    return SF_OK;
}

sf_result_t sf_binary_writer_write_pattern(sf_binary_writer_t *w, size_t length, uint8_t v) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    uint8_t buf[256];
    memset(buf, v, sizeof(buf));
    while (length > 0) {
        size_t chunk = (length > sizeof(buf)) ? sizeof(buf) : length;
        e = sf_ostream_write(w->stream, buf, chunk);
        if (e != SF_OK) return e;
        length -= chunk;
    }
    return SF_OK;
}

/*===========================================================================
 * Primitive writes
 *===========================================================================*/

sf_result_t sf_binary_writer_write_bytes(sf_binary_writer_t *w, const void *buf, size_t n) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    return sf_ostream_write(w->stream, buf, n);
}

sf_result_t sf_binary_writer_write_bool(sf_binary_writer_t *w, bool v) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    uint8_t b = v ? 1u : 0u;
    return sf_ostream_write(w->stream, &b, 1);
}

sf_result_t sf_binary_writer_write_i8(sf_binary_writer_t *w, int8_t v) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    return sf_ostream_write(w->stream, &v, 1);
}

sf_result_t sf_binary_writer_write_u8(sf_binary_writer_t *w, uint8_t v) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    return sf_ostream_write(w->stream, &v, 1);
}

#define DEFINE_WRITE_INT(suffix, ctype, swap_fn)                             \
    sf_result_t sf_binary_writer_write_##suffix(sf_binary_writer_t *w,       \
                                                ctype v) {                    \
        sf_result_t _open = writer_open(w);                                   \
        if (_open != SF_OK) return _open;                                      \
        if (w->big_endian) v = (ctype)swap_fn((ctype)v);                      \
        return sf_ostream_write(w->stream, &v, sizeof(v));                    \
    }

DEFINE_WRITE_INT(u16, uint16_t, sf_bswap16)
DEFINE_WRITE_INT(u32, uint32_t, sf_bswap32)
DEFINE_WRITE_INT(u64, uint64_t, sf_bswap64)

sf_result_t sf_binary_writer_write_i16(sf_binary_writer_t *w, int16_t v) {
    return sf_binary_writer_write_u16(w, (uint16_t)v);
}
sf_result_t sf_binary_writer_write_i32(sf_binary_writer_t *w, int32_t v) {
    return sf_binary_writer_write_u32(w, (uint32_t)v);
}
sf_result_t sf_binary_writer_write_i64(sf_binary_writer_t *w, int64_t v) {
    return sf_binary_writer_write_u64(w, (uint64_t)v);
}

sf_result_t sf_binary_writer_write_f32(sf_binary_writer_t *w, float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    return sf_binary_writer_write_u32(w, u);
}
sf_result_t sf_binary_writer_write_f64(sf_binary_writer_t *w, double v) {
    uint64_t u;
    memcpy(&u, &v, sizeof(u));
    return sf_binary_writer_write_u64(w, u);
}
sf_result_t sf_binary_writer_write_varint(sf_binary_writer_t *w, int64_t v) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    return w->varint_long ? sf_binary_writer_write_i64(w, v)
                          : sf_binary_writer_write_i32(w, (int32_t)v);
}

#define DEFINE_WRITE_ARRAY(suffix, ctype, write_fn)                           \
    sf_result_t sf_binary_writer_write_##suffix##s(sf_binary_writer_t *w,     \
                                                   size_t count,              \
                                                   const ctype *values) {     \
        sf_result_t e = writer_open(w);                                        \
        if (e != SF_OK) return e;                                              \
        if (count == 0) return SF_OK;                                          \
        SF_CHECK_ARG(values != NULL);                                          \
        for (size_t i = 0; i < count; i++) {                                   \
            e = write_fn(w, values[i]);                                        \
            if (e != SF_OK) return e;                                          \
        }                                                                      \
        return SF_OK;                                                          \
    }

DEFINE_WRITE_ARRAY(bool, bool, sf_binary_writer_write_bool)
DEFINE_WRITE_ARRAY(i8, int8_t, sf_binary_writer_write_i8)
DEFINE_WRITE_ARRAY(u8, uint8_t, sf_binary_writer_write_u8)
DEFINE_WRITE_ARRAY(i16, int16_t, sf_binary_writer_write_i16)
DEFINE_WRITE_ARRAY(u16, uint16_t, sf_binary_writer_write_u16)
DEFINE_WRITE_ARRAY(i32, int32_t, sf_binary_writer_write_i32)
DEFINE_WRITE_ARRAY(u32, uint32_t, sf_binary_writer_write_u32)
DEFINE_WRITE_ARRAY(i64, int64_t, sf_binary_writer_write_i64)
DEFINE_WRITE_ARRAY(u64, uint64_t, sf_binary_writer_write_u64)
DEFINE_WRITE_ARRAY(f32, float, sf_binary_writer_write_f32)
DEFINE_WRITE_ARRAY(f64, double, sf_binary_writer_write_f64)

sf_result_t sf_binary_writer_write_varints(sf_binary_writer_t *w, size_t count,
                                           const int64_t *values) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    if (count == 0) return SF_OK;
    SF_CHECK_ARG(values != NULL);
    for (size_t i = 0; i < count; i++) {
        e = sf_binary_writer_write_varint(w, values[i]);
        if (e != SF_OK) return e;
    }
    return SF_OK;
}

/*===========================================================================
 * Reservations
 *===========================================================================*/

static int kind_width(sfres_kind_t k) {
    switch (k) {
        case SFRES_BOOL:      case SFRES_I8:  case SFRES_U8:  return 1;
        case SFRES_I16:       case SFRES_U16: return 2;
        case SFRES_U32:       case SFRES_I32: return 4;
        case SFRES_U64:       case SFRES_I64: return 8;
        case SFRES_VARINT_4:  return 4;
        case SFRES_VARINT_8:  return 8;
        case SFRES_F32:       return 4;
        case SFRES_F64:       return 8;
    }
    return 0;
}

static sf_result_t reservations_push(sf_binary_writer_t *w, const char *name,
                                     int64_t pos, sfres_kind_t kind) {
    /*  Duplicate detection. */
    for (size_t i = 0; i < w->res_size; i++) {
        if (strcmp(w->res[i].name, name) == 0 && w->res[i].kind == kind) {
            return SF_ERR_ALREADY_EXISTS;
        }
    }
    if (w->res_size == w->res_cap) {
        size_t new_cap = w->res_cap ? w->res_cap * 2 : 8;
        sfres_t *p = (sfres_t *)sf_xrealloc(w->alloc, w->res,
                                            w->res_cap * sizeof(sfres_t),
                                            new_cap * sizeof(sfres_t));
        if (!p) return SF_ERR_OOM;
        w->res = p; w->res_cap = new_cap;
    }
    char *dup = sf_strdup(w->alloc, name);
    if (!dup) return SF_ERR_OOM;
    w->res[w->res_size].name = dup;
    w->res[w->res_size].pos  = pos;
    w->res[w->res_size].kind = kind;
    w->res_size++;
    return SF_OK;
}

static sf_result_t reservations_pop(sf_binary_writer_t *w, const char *name,
                                    sfres_kind_t kind, int64_t *out_pos) {
    for (size_t i = 0; i < w->res_size; i++) {
        if (strcmp(w->res[i].name, name) == 0 && w->res[i].kind == kind) {
            *out_pos = w->res[i].pos;
            sf_xfree(w->alloc, w->res[i].name);
            /*  Compact: move last item into this slot. */
            w->res[i] = w->res[w->res_size - 1];
            w->res_size--;
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

static sf_result_t reservations_peek(const sf_binary_writer_t *w, const char *name,
                                     sfres_kind_t kind, int64_t *out_pos) {
    for (size_t i = 0; i < w->res_size; i++) {
        if (strcmp(w->res[i].name, name) == 0 && w->res[i].kind == kind) {
            *out_pos = w->res[i].pos;
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

static sf_result_t reserve_kind(sf_binary_writer_t *w, const char *name,
                                sfres_kind_t kind) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    SF_CHECK_ARG(name != NULL);
    int64_t pos = sf_ostream_position(w->stream);
    int width = kind_width(kind);
    e = reservations_push(w, name, pos, kind);
    if (e != SF_OK) return e;
    e = sf_binary_writer_write_pattern(w, (size_t)width, 0xFE);
    if (e != SF_OK) {
        int64_t unused = 0;
        (void)reservations_pop(w, name, kind, &unused);
    }
    return e;
}

sf_result_t sf_binary_writer_reserve_bool(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_BOOL);
}
sf_result_t sf_binary_writer_reserve_i8(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_I8);
}
sf_result_t sf_binary_writer_reserve_u8(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_U8);
}
sf_result_t sf_binary_writer_reserve_i16(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_I16);
}
sf_result_t sf_binary_writer_reserve_u16(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_U16);
}
sf_result_t sf_binary_writer_reserve_u32(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_U32);
}
sf_result_t sf_binary_writer_reserve_i32(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_I32);
}
sf_result_t sf_binary_writer_reserve_u64(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_U64);
}
sf_result_t sf_binary_writer_reserve_i64(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_I64);
}
sf_result_t sf_binary_writer_reserve_varint(sf_binary_writer_t *w, const char *n) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    return reserve_kind(w, n, w->varint_long ? SFRES_VARINT_8 : SFRES_VARINT_4);
}
sf_result_t sf_binary_writer_reserve_f32(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_F32);
}
sf_result_t sf_binary_writer_reserve_f64(sf_binary_writer_t *w, const char *n) {
    return reserve_kind(w, n, SFRES_F64);
}

#define DEFINE_FILL(suffix, kind_const, ctype, write_fn)                     \
    sf_result_t sf_binary_writer_fill_##suffix(sf_binary_writer_t *w,        \
                                               const char *n, ctype v) {    \
        sf_result_t _open = writer_open(w);                                   \
        if (_open != SF_OK) return _open;                                      \
        SF_CHECK_ARG(n != NULL);                                               \
        int64_t pos;                                                          \
        sf_result_t e = reservations_peek(w, n, kind_const, &pos);           \
        if (e != SF_OK) return e;                                            \
        e = sf_binary_writer_step_in(w, pos);                                 \
        if (e != SF_OK) return e;                                            \
        e = write_fn(w, v);                                                   \
        sf_result_t e2 = sf_binary_writer_step_out(w);                       \
        if (e == SF_OK && e2 == SF_OK) {                                      \
            int64_t unused = 0;                                               \
            (void)reservations_pop(w, n, kind_const, &unused);                \
        }                                                                     \
        return (e != SF_OK) ? e : e2;                                        \
    }

DEFINE_FILL(bool, SFRES_BOOL, bool, sf_binary_writer_write_bool)
DEFINE_FILL(i8, SFRES_I8, int8_t, sf_binary_writer_write_i8)
DEFINE_FILL(u8, SFRES_U8, uint8_t, sf_binary_writer_write_u8)
DEFINE_FILL(i16, SFRES_I16, int16_t, sf_binary_writer_write_i16)
DEFINE_FILL(u16, SFRES_U16, uint16_t, sf_binary_writer_write_u16)
DEFINE_FILL(u32, SFRES_U32, uint32_t, sf_binary_writer_write_u32)
DEFINE_FILL(i32, SFRES_I32, int32_t,  sf_binary_writer_write_i32)
DEFINE_FILL(u64, SFRES_U64, uint64_t, sf_binary_writer_write_u64)
DEFINE_FILL(i64, SFRES_I64, int64_t,  sf_binary_writer_write_i64)
DEFINE_FILL(f32, SFRES_F32, float, sf_binary_writer_write_f32)
DEFINE_FILL(f64, SFRES_F64, double, sf_binary_writer_write_f64)

sf_result_t sf_binary_writer_fill_varint(sf_binary_writer_t *w, const char *n, int64_t v) {
    sf_result_t open = writer_open(w);
    if (open != SF_OK) return open;
    SF_CHECK_ARG(n != NULL);
    sfres_kind_t kind = w->varint_long ? SFRES_VARINT_8 : SFRES_VARINT_4;
    int64_t pos;
    sf_result_t e = reservations_peek(w, n, kind, &pos);
    if (e != SF_OK) return e;
    e = sf_binary_writer_step_in(w, pos);
    if (e != SF_OK) return e;
    e = w->varint_long ? sf_binary_writer_write_i64(w, v)
                       : sf_binary_writer_write_i32(w, (int32_t)v);
    sf_result_t e2 = sf_binary_writer_step_out(w);
    if (e == SF_OK && e2 == SF_OK) {
        int64_t unused = 0;
        (void)reservations_pop(w, n, kind, &unused);
    }
    return (e != SF_OK) ? e : e2;
}

/*===========================================================================
 * Strings
 *===========================================================================*/

sf_result_t sf_binary_writer_write_ascii(sf_binary_writer_t *w, const char *utf8, bool term) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    void *bytes = NULL;
    size_t n = 0;
    e = sf_utf8_to_ascii(utf8, term, &bytes, &n, w->alloc);
    if (e != SF_OK) return e;
    e = sf_ostream_write(w->stream, bytes, n);
    sf_xfree(w->alloc, bytes);
    return e;
}

sf_result_t sf_binary_writer_write_shift_jis(sf_binary_writer_t *w, const char *utf8, bool term) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    void *bytes = NULL;
    size_t n = 0;
    e = sf_utf8_to_shift_jis(utf8, term, &bytes, &n, w->alloc);
    if (e != SF_OK) return e;
    e = sf_ostream_write(w->stream, bytes, n);
    sf_xfree(w->alloc, bytes);
    return e;
}

sf_result_t sf_binary_writer_write_utf16(sf_binary_writer_t *w, const char *utf8, bool term) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    void *bytes = NULL;
    size_t n = 0;
    e = w->big_endian
        ? sf_utf8_to_utf16be(utf8, term, &bytes, &n, w->alloc)
        : sf_utf8_to_utf16le(utf8, term, &bytes, &n, w->alloc);
    if (e != SF_OK) return e;
    e = sf_ostream_write(w->stream, bytes, n);
    sf_xfree(w->alloc, bytes);
    return e;
}

static sf_result_t write_fix_str_impl(sf_binary_writer_t *w, const char *utf8,
                                       size_t size, uint8_t pad,
                                       sf_result_t (*conv)(const char*, bool, void**, size_t*, const sf_allocator_t*)) {
    sf_result_t e = writer_open(w);
    if (e != SF_OK) return e;
    if (size == 0) return SF_OK;
    void *raw = NULL;
    size_t raw_n = 0;
    e = conv(utf8, /* terminate */ true, &raw, &raw_n, w->alloc);
    if (e != SF_OK) return e;

    /*  Allocate fixed-size scratch, fill with `pad`, copy converted bytes
     *  truncated to size. */
    uint8_t *fix = (uint8_t *)sf_xalloc(w->alloc, size);
    if (!fix) { sf_xfree(w->alloc, raw); return SF_ERR_OOM; }
    memset(fix, pad, size);
    size_t to_copy = (raw_n < size) ? raw_n : size;
    if (to_copy > 0) memcpy(fix, raw, to_copy);
    sf_xfree(w->alloc, raw);

    e = sf_ostream_write(w->stream, fix, size);
    sf_xfree(w->alloc, fix);
    return e;
}

sf_result_t sf_binary_writer_write_fix_str(sf_binary_writer_t *w, const char *utf8,
                                           size_t size, uint8_t pad) {
    return write_fix_str_impl(w, utf8, size, pad, sf_utf8_to_shift_jis);
}

sf_result_t sf_binary_writer_write_fix_str_w(sf_binary_writer_t *w, const char *utf8,
                                              size_t size, uint8_t pad) {
    SF_CHECK_ARG(w != NULL);
    return write_fix_str_impl(w, utf8, size, pad,
                              w->big_endian ? sf_utf8_to_utf16be
                                            : sf_utf8_to_utf16le);
}

/*===========================================================================
 * Vector / quat / color
 *===========================================================================*/

sf_result_t sf_binary_writer_write_vec2(sf_binary_writer_t *w, sf_vec2_t v) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_f32(w, v.x)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.y)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_writer_write_vec3(sf_binary_writer_t *w, sf_vec3_t v) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_f32(w, v.x)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.y)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.z)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_writer_write_vec4(sf_binary_writer_t *w, sf_vec4_t v) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_f32(w, v.x)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.y)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.z)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, v.w)) != SF_OK) return e;
    return SF_OK;
}
sf_result_t sf_binary_writer_write_quat(sf_binary_writer_t *w, sf_quat_t q) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_f32(w, q.x)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, q.y)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, q.z)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_f32(w, q.w)) != SF_OK) return e;
    return SF_OK;
}

#define WC4(c1, c2, c3, c4) do {                                             \
    sf_result_t _e;                                                           \
    if ((_e = sf_binary_writer_write_u8(w, c1)) != SF_OK) return _e;          \
    if ((_e = sf_binary_writer_write_u8(w, c2)) != SF_OK) return _e;          \
    if ((_e = sf_binary_writer_write_u8(w, c3)) != SF_OK) return _e;          \
    if ((_e = sf_binary_writer_write_u8(w, c4)) != SF_OK) return _e;          \
    return SF_OK;                                                             \
} while (0)

sf_result_t sf_binary_writer_write_argb(sf_binary_writer_t *w, sf_color_t c) { WC4(c.a, c.r, c.g, c.b); }
sf_result_t sf_binary_writer_write_abgr(sf_binary_writer_t *w, sf_color_t c) { WC4(c.a, c.b, c.g, c.r); }
sf_result_t sf_binary_writer_write_rgba(sf_binary_writer_t *w, sf_color_t c) { WC4(c.r, c.g, c.b, c.a); }
sf_result_t sf_binary_writer_write_bgra(sf_binary_writer_t *w, sf_color_t c) { WC4(c.b, c.g, c.r, c.a); }
