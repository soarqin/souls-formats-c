/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_binary_writer_t — equivalent of upstream BinaryWriterEx.
 *
 * Borrows an sf_ostream_t (does not close it). Maintains:
 *   - mutable big-endian / varint-long flags
 *   - LIFO offset stack (StepIn / StepOut)
 *   - an open-addressing hash table of pending Reservations keyed on
 *     (name, kind). Hash = FNV-1a-32 over name; linear probing on collision.
 *     Push / peek / pop are amortized O(1); resize on load > 70 %.
 *
 *     The flat-array predecessor was O(N²) on writers that emit thousands
 *     of distinct snprintf-built reservation names (FMG, PARAM, FLVER2, …)
 *     and dominated the total write cost on real-game-scale data.
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

/*===========================================================================
 * Reservation hash table — open addressing, linear probing.
 *===========================================================================*/

enum {
    SFRES_SLOT_EMPTY     = 0,  /* never used — zero-init friendly */
    SFRES_SLOT_OCCUPIED  = 1,  /* live reservation, `name` is heap-owned */
    SFRES_SLOT_TOMBSTONE = 2,  /* popped slot, must be skipped by probe walks */
};

typedef struct sfres_slot {
    char    *name;       /* heap-owned via writer's allocator when OCCUPIED */
    int64_t  pos;
    uint32_t name_hash;
    uint8_t  kind;       /* sfres_kind_t value — values fit in a byte */
    uint8_t  state;      /* SFRES_SLOT_* */
    uint16_t _pad;
} sfres_slot_t;

#define SFRES_INIT_CAP    16     /* must be a power of two */
#define SFRES_LOAD_NUM    7
#define SFRES_LOAD_DEN   10      /* resize when (used * 10) > (cap * 7)   */

_Static_assert((SFRES_INIT_CAP & (SFRES_INIT_CAP - 1)) == 0,
               "SFRES_INIT_CAP must be a power of two");

struct sf_binary_writer {
    sf_ostream_t        *stream;       /* borrowed */
    const sf_allocator_t *alloc;
    bool                 big_endian;
    bool                 varint_long;

    int64_t             *steps;
    size_t               steps_size;
    size_t               steps_cap;

    sfres_slot_t        *res_slots;   /* NULL until first reservation */
    size_t               res_cap;     /* power of two, or 0 when unallocated */
    size_t               res_used;    /* OCCUPIED + TOMBSTONE — for resize */
    size_t               res_live;    /* OCCUPIED only       — for finish */

    bool                 closed;
};

/*  FNV-1a-32 over a NUL-terminated string. Cheap and good enough mixing
 *  for the short, predictable reservation names callers build. */
static uint32_t sfres_hash(const char *name) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        h ^= (uint32_t)*p;
        h *= 16777619u;
    }
    return h;
}

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
    if (w->res_slots) {
        for (size_t i = 0; i < w->res_cap; i++) {
            if (w->res_slots[i].state == SFRES_SLOT_OCCUPIED) {
                sf_xfree(w->alloc, w->res_slots[i].name);
            }
        }
        sf_xfree(w->alloc, w->res_slots);
    }
    sf_xfree(w->alloc, w);
}

sf_result_t sf_binary_writer_finish(sf_binary_writer_t *w) {
    SF_CHECK_ARG(w != NULL);
    if (w->res_live > 0) return SF_ERR_INTERNAL;
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
    if (w->res_live > 0) return SF_ERR_INTERNAL;
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

/*  Locate the slot for (name_hash, kind, name) starting from `start` index.
 *  Returns the matching OCCUPIED slot if present, or the first slot that
 *  terminates the probe walk (EMPTY) otherwise. Tombstones are walked over
 *  without being treated as terminators for lookups. */
static size_t sfres_probe_lookup(const sfres_slot_t *slots, size_t cap,
                                 uint32_t hash, uint8_t kind, const char *name) {
    const size_t mask = cap - 1;
    size_t i = (size_t)hash & mask;
    for (;;) {
        const sfres_slot_t *s = &slots[i];
        if (s->state == SFRES_SLOT_EMPTY) return i;
        if (s->state == SFRES_SLOT_OCCUPIED &&
            s->name_hash == hash && s->kind == kind &&
            strcmp(s->name, name) == 0) {
            return i;
        }
        i = (i + 1) & mask;
    }
}

/*  Locate the slot to insert (name_hash, kind, name) into, or detect an
 *  existing duplicate. Sets `*out_idx` to the insertion / duplicate slot.
 *  Returns SF_ERR_ALREADY_EXISTS if a matching OCCUPIED slot is found,
 *  SF_OK otherwise (caller writes into *out_idx). */
static sf_result_t sfres_probe_insert(const sfres_slot_t *slots, size_t cap,
                                      uint32_t hash, uint8_t kind,
                                      const char *name, size_t *out_idx) {
    const size_t mask = cap - 1;
    size_t i = (size_t)hash & mask;
    size_t first_tomb = SIZE_MAX;
    for (;;) {
        const sfres_slot_t *s = &slots[i];
        if (s->state == SFRES_SLOT_EMPTY) {
            *out_idx = (first_tomb != SIZE_MAX) ? first_tomb : i;
            return SF_OK;
        }
        if (s->state == SFRES_SLOT_TOMBSTONE) {
            if (first_tomb == SIZE_MAX) first_tomb = i;
        } else if (s->name_hash == hash && s->kind == kind &&
                   strcmp(s->name, name) == 0) {
            *out_idx = i;
            return SF_ERR_ALREADY_EXISTS;
        }
        i = (i + 1) & mask;
    }
}

static sf_result_t sfres_table_grow(sf_binary_writer_t *w, size_t new_cap) {
    sfres_slot_t *new_slots =
        (sfres_slot_t *)sf_xalloc(w->alloc, new_cap * sizeof(sfres_slot_t));
    if (!new_slots) return SF_ERR_OOM;
    memset(new_slots, 0, new_cap * sizeof(sfres_slot_t));

    /*  Rehash live entries; tombstones drop out cleanly. */
    if (w->res_slots) {
        for (size_t i = 0; i < w->res_cap; i++) {
            sfres_slot_t *src = &w->res_slots[i];
            if (src->state != SFRES_SLOT_OCCUPIED) continue;
            size_t idx;
            (void)sfres_probe_insert(new_slots, new_cap, src->name_hash,
                                     src->kind, src->name, &idx);
            new_slots[idx] = *src;
        }
        sf_xfree(w->alloc, w->res_slots);
    }
    w->res_slots = new_slots;
    w->res_cap   = new_cap;
    w->res_used  = w->res_live;  /* tombstones gone post-rehash */
    return SF_OK;
}

static sf_result_t reservations_push(sf_binary_writer_t *w, const char *name,
                                     int64_t pos, sfres_kind_t kind) {
    /*  Lazy init / preemptive resize. We resize on (used+1)*DEN > cap*NUM so
     *  the post-insert load stays below the threshold. */
    if (w->res_cap == 0) {
        sf_result_t e = sfres_table_grow(w, SFRES_INIT_CAP);
        if (e != SF_OK) return e;
    } else if ((w->res_used + 1) * SFRES_LOAD_DEN > w->res_cap * SFRES_LOAD_NUM) {
        size_t new_cap = w->res_cap * 2;
        if (new_cap < w->res_cap) return SF_ERR_OUT_OF_RANGE;  /* overflow */
        sf_result_t e = sfres_table_grow(w, new_cap);
        if (e != SF_OK) return e;
    }

    const uint32_t hash = sfres_hash(name);
    size_t idx;
    sf_result_t e = sfres_probe_insert(w->res_slots, w->res_cap, hash,
                                       (uint8_t)kind, name, &idx);
    if (e == SF_ERR_ALREADY_EXISTS) return e;

    char *dup = sf_strdup(w->alloc, name);
    if (!dup) return SF_ERR_OOM;
    sfres_slot_t *slot = &w->res_slots[idx];
    bool was_tomb = (slot->state == SFRES_SLOT_TOMBSTONE);
    slot->name      = dup;
    slot->pos       = pos;
    slot->name_hash = hash;
    slot->kind      = (uint8_t)kind;
    slot->state     = SFRES_SLOT_OCCUPIED;
    if (!was_tomb) w->res_used++;
    w->res_live++;
    return SF_OK;
}

static sf_result_t reservations_pop(sf_binary_writer_t *w, const char *name,
                                    sfres_kind_t kind, int64_t *out_pos) {
    if (w->res_cap == 0) return SF_ERR_NOT_FOUND;
    const uint32_t hash = sfres_hash(name);
    size_t idx = sfres_probe_lookup(w->res_slots, w->res_cap, hash,
                                    (uint8_t)kind, name);
    sfres_slot_t *slot = &w->res_slots[idx];
    if (slot->state != SFRES_SLOT_OCCUPIED) return SF_ERR_NOT_FOUND;
    *out_pos = slot->pos;
    sf_xfree(w->alloc, slot->name);
    slot->name = NULL;
    /*  Mark as tombstone so subsequent probes still walk past this slot
     *  to find later inserts that collided here. `res_used` keeps the
     *  tombstone counted; rehash reclaims it. */
    slot->state = SFRES_SLOT_TOMBSTONE;
    w->res_live--;
    return SF_OK;
}

static sf_result_t reservations_peek(const sf_binary_writer_t *w, const char *name,
                                     sfres_kind_t kind, int64_t *out_pos) {
    if (w->res_cap == 0) return SF_ERR_NOT_FOUND;
    const uint32_t hash = sfres_hash(name);
    size_t idx = sfres_probe_lookup(w->res_slots, w->res_cap, hash,
                                    (uint8_t)kind, name);
    const sfres_slot_t *slot = &w->res_slots[idx];
    if (slot->state != SFRES_SLOT_OCCUPIED) return SF_ERR_NOT_FOUND;
    *out_pos = slot->pos;
    return SF_OK;
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
