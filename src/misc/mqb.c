/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mqb.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _r = (expr); if (_r != SF_OK) return _r; } while (0)

typedef union mqb_value {
    bool b;
    int8_t s8;
    uint8_t u8;
    int16_t s16;
    int32_t s32;
    uint32_t u32;
    float f32;
    char *str;
    struct { uint8_t *bytes; size_t size; } custom;
    sf_color_t color;
    int32_t int_color[4];
    sf_vec4_t vec;
} mqb_value_t;

struct sf_mqb_point {
    const sf_allocator_t *alloc;
    sf_mqb_param_type_t type;
    mqb_value_t value;
    int32_t unk08;
    float unk10;
    float unk14;
};

struct sf_mqb_sequence {
    const sf_allocator_t *alloc;
    sf_mqb_param_type_t value_type;
    int32_t point_type;
    int32_t value_index;
    sf_mqb_point_t **points;
    size_t point_count;
    size_t point_cap;
};

struct sf_mqb_parameter {
    const sf_allocator_t *alloc;
    char *name;
    sf_mqb_param_type_t type;
    int32_t member_count;
    mqb_value_t value;
    sf_mqb_sequence_t **sequences;
    size_t sequence_count;
    size_t sequence_cap;
};

struct sf_mqb_event {
    const sf_allocator_t *alloc;
    int32_t id;
    int32_t resource_index;
    int32_t unk08;
    int32_t start_frame;
    int32_t duration;
    int32_t unk14;
    int32_t unk18;
    int32_t unk1c;
    int32_t unk20;
    int32_t unk28;
    sf_mqb_parameter_t **parameters;
    size_t parameter_count;
    size_t parameter_cap;
    sf_mqb_transform_t *transforms;
    size_t transform_count;
    size_t transform_cap;
};

struct sf_mqb_timeline {
    const sf_allocator_t *alloc;
    int32_t unk10;
    sf_mqb_event_t **events;
    size_t event_count;
    size_t event_cap;
    sf_mqb_parameter_t **parameters;
    size_t parameter_count;
    size_t parameter_cap;
};

struct sf_mqb_cut {
    const sf_allocator_t *alloc;
    char *name;
    int32_t unk44;
    int32_t duration;
    sf_mqb_timeline_t **timelines;
    size_t timeline_count;
    size_t timeline_cap;
};

struct sf_mqb_resource {
    const sf_allocator_t *alloc;
    char *name;
    int32_t parent_index;
    int32_t unk48;
    char *path;
    sf_mqb_parameter_t **parameters;
    size_t parameter_count;
    size_t parameter_cap;
};

struct sf_mqb {
    const sf_allocator_t *alloc;
    sf_mqb_version_t version;
    bool big_endian;
    char *name;
    float framerate;
    char *resource_directory;
    sf_mqb_resource_t **resources;
    size_t resource_count;
    size_t resource_cap;
    sf_mqb_cut_t **cuts;
    size_t cut_count;
    size_t cut_cap;
};

typedef struct rdr {
    const uint8_t *data;
    size_t size;
    size_t pos;
    bool be;
    bool var64;
} rdr_t;

typedef struct wr {
    const sf_allocator_t *alloc;
    uint8_t *data;
    size_t size;
    size_t cap;
    bool be;
    bool var64;
} wr_t;

typedef struct event_offset { const sf_mqb_event_t *event; size_t offset; } event_offset_t;
typedef struct timeline_patch {
    const sf_mqb_timeline_t *timeline;
    size_t events_pos;
    size_t params_pos;
} timeline_patch_t;
typedef struct param_patch {
    const sf_mqb_parameter_t *param;
    size_t sequences_pos;
    size_t value_offset;
} param_patch_t;
typedef struct seq_patch { const sf_mqb_sequence_t *seq; size_t points_pos; } seq_patch_t;

typedef struct write_ctx {
    event_offset_t *events;
    size_t event_count;
    size_t event_cap;
    timeline_patch_t *timelines;
    size_t timeline_count;
    size_t timeline_cap;
    param_patch_t *params;
    size_t param_count;
    size_t param_cap;
    seq_patch_t *seqs;
    size_t seq_count;
    size_t seq_cap;
} write_ctx_t;

static sf_result_t mqb_strdup(const sf_allocator_t *alloc, const char *s, char **out) {
    if (!out) return SF_ERR_INVALID_ARG;
    if (!s) s = "";
    size_t n = strlen(s) + 1u;
    char *p = (char *)sf_xalloc(alloc, n);
    if (!p) return SF_ERR_OOM;
    memcpy(p, s, n);
    *out = p;
    return SF_OK;
}

static sf_result_t mqb_set_string(const sf_allocator_t *alloc, char **dst, const char *s) {
    char *dup = NULL;
    TRY(mqb_strdup(alloc, s, &dup));
    sf_xfree(alloc, *dst);
    *dst = dup;
    return SF_OK;
}

static sf_result_t mqb_grow_ptrs(const sf_allocator_t *alloc, void ***arr, size_t count, size_t *cap) {
    if (count < *cap) return SF_OK;
    size_t new_cap = *cap == 0 ? 4u : *cap * 2u;
    void **na = (void **)sf_xalloc(alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    if (*arr) {
        memcpy(na, *arr, count * sizeof(*na));
        sf_xfree(alloc, *arr);
    }
    *arr = na;
    *cap = new_cap;
    return SF_OK;
}

static sf_result_t mqb_grow_bytes(const sf_allocator_t *alloc, void **arr, size_t elem,
                                  size_t count, size_t *cap) {
    if (count < *cap) return SF_OK;
    size_t new_cap = *cap == 0 ? 8u : *cap * 2u;
    if (new_cap > SIZE_MAX / elem) return SF_ERR_OUT_OF_RANGE;
    void *na = sf_xalloc(alloc, new_cap * elem);
    if (!na) return SF_ERR_OOM;
    if (*arr) {
        memcpy(na, *arr, count * elem);
        sf_xfree(alloc, *arr);
    }
    *arr = na;
    *cap = new_cap;
    return SF_OK;
}

static sf_result_t read_exact(rdr_t *r, void *out, size_t n) {
    if (n > r->size || r->pos > r->size - n) return SF_ERR_TRUNCATED;
    memcpy(out, r->data + r->pos, n);
    r->pos += n;
    return SF_OK;
}

static sf_result_t read_u8(rdr_t *r, uint8_t *out) { return read_exact(r, out, 1); }

static sf_result_t read_u16(rdr_t *r, uint16_t *out) {
    uint16_t v;
    TRY(read_exact(r, &v, 2));
    *out = r->be ? sf_bswap16(v) : v;
    return SF_OK;
}

static sf_result_t read_u32(rdr_t *r, uint32_t *out) {
    uint32_t v;
    TRY(read_exact(r, &v, 4));
    *out = r->be ? sf_bswap32(v) : v;
    return SF_OK;
}

static sf_result_t read_i32(rdr_t *r, int32_t *out) {
    uint32_t v;
    TRY(read_u32(r, &v));
    *out = (int32_t)v;
    return SF_OK;
}

static sf_result_t read_u64(rdr_t *r, uint64_t *out) {
    uint64_t v;
    TRY(read_exact(r, &v, 8));
    *out = r->be ? sf_bswap64(v) : v;
    return SF_OK;
}

static sf_result_t read_i64(rdr_t *r, int64_t *out) {
    uint64_t v;
    TRY(read_u64(r, &v));
    *out = (int64_t)v;
    return SF_OK;
}

static sf_result_t read_f32(rdr_t *r, float *out) {
    uint32_t v;
    TRY(read_u32(r, &v));
    memcpy(out, &v, sizeof(*out));
    return SF_OK;
}

static sf_result_t read_var(rdr_t *r, size_t *out) {
    if (r->var64) {
        uint64_t v;
        TRY(read_u64(r, &v));
        if (v > SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
        *out = (size_t)v;
    } else {
        uint32_t v;
        TRY(read_u32(r, &v));
        *out = (size_t)v;
    }
    return SF_OK;
}

static sf_result_t assert_i32(rdr_t *r, int32_t expected) {
    int32_t v;
    TRY(read_i32(r, &v));
    return v == expected ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t assert_i64(rdr_t *r, int64_t expected) {
    int64_t v;
    TRY(read_i64(r, &v));
    return v == expected ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t read_utf16_units(rdr_t *r, size_t units, const sf_allocator_t *alloc,
                                    char **out) {
    size_t start = r->pos;
    size_t chars = 0;
    for (size_t i = 0; i < units; i++) {
        uint16_t ch;
        TRY(read_u16(r, &ch));
        if (ch == 0) break;
        chars++;
    }
    r->pos = start + units * 2u;
    char *s = (char *)sf_xalloc(alloc, chars * 3u + 1u);
    if (!s) return SF_ERR_OOM;
    size_t w = 0;
    for (size_t i = 0; i < chars; i++) {
        uint16_t ch;
        size_t off = start + i * 2u;
        memcpy(&ch, r->data + off, 2);
        if (r->be) ch = sf_bswap16(ch);
        if (ch < 0x80u) s[w++] = (char)ch;
        else if (ch < 0x800u) {
            s[w++] = (char)(0xC0u | (ch >> 6));
            s[w++] = (char)(0x80u | (ch & 0x3Fu));
        } else {
            s[w++] = (char)(0xE0u | (ch >> 12));
            s[w++] = (char)(0x80u | ((ch >> 6) & 0x3Fu));
            s[w++] = (char)(0x80u | (ch & 0x3Fu));
        }
    }
    s[w] = '\0';
    *out = s;
    return SF_OK;
}

static sf_result_t read_fixstr_w(rdr_t *r, size_t bytes, const sf_allocator_t *alloc,
                                 char **out) {
    if ((bytes & 1u) != 0) return SF_ERR_BAD_MAGIC;
    if (bytes > r->size || r->pos > r->size - bytes) return SF_ERR_TRUNCATED;
    return read_utf16_units(r, bytes / 2u, alloc, out);
}

static sf_result_t read_utf16_z_at(rdr_t *r, size_t offset, const sf_allocator_t *alloc,
                                   char **out) {
    if (offset >= r->size) return SF_ERR_TRUNCATED;
    size_t units = 0;
    for (size_t p = offset; p + 1u < r->size; p += 2u) {
        uint16_t ch;
        memcpy(&ch, r->data + p, 2);
        if (r->be) ch = sf_bswap16(ch);
        units++;
        if (ch == 0) break;
    }
    if (offset + units * 2u > r->size || units == 0) return SF_ERR_TRUNCATED;
    size_t old = r->pos;
    r->pos = offset;
    sf_result_t res = read_utf16_units(r, units, alloc, out);
    r->pos = old;
    return res;
}

static sf_result_t wr_reserve(wr_t *w, size_t n) {
    if (n > SIZE_MAX - w->size) return SF_ERR_OUT_OF_RANGE;
    size_t need = w->size + n;
    if (need <= w->cap) return SF_OK;
    size_t nc = w->cap == 0 ? 256u : w->cap;
    while (nc < need) {
        if (nc > SIZE_MAX / 2u) return SF_ERR_OUT_OF_RANGE;
        nc *= 2u;
    }
    uint8_t *nd = (uint8_t *)sf_xalloc(w->alloc, nc);
    if (!nd) return SF_ERR_OOM;
    if (w->data) {
        memcpy(nd, w->data, w->size);
        sf_xfree(w->alloc, w->data);
    }
    w->data = nd;
    w->cap = nc;
    return SF_OK;
}

static sf_result_t write_raw(wr_t *w, const void *p, size_t n) {
    TRY(wr_reserve(w, n));
    memcpy(w->data + w->size, p, n);
    w->size += n;
    return SF_OK;
}

static sf_result_t write_u8(wr_t *w, uint8_t v) { return write_raw(w, &v, 1); }
static sf_result_t write_u16(wr_t *w, uint16_t v) { if (w->be) v = sf_bswap16(v); return write_raw(w, &v, 2); }
static sf_result_t write_u32(wr_t *w, uint32_t v) { if (w->be) v = sf_bswap32(v); return write_raw(w, &v, 4); }
static sf_result_t write_i32(wr_t *w, int32_t v) { return write_u32(w, (uint32_t)v); }
static sf_result_t write_u64(wr_t *w, uint64_t v) { if (w->be) v = sf_bswap64(v); return write_raw(w, &v, 8); }
static sf_result_t write_i64(wr_t *w, int64_t v) { return write_u64(w, (uint64_t)v); }
static sf_result_t write_f32(wr_t *w, float v) { uint32_t u; memcpy(&u, &v, 4); return write_u32(w, u); }

static sf_result_t write_var(wr_t *w, size_t v) {
    if (w->var64) return write_u64(w, (uint64_t)v);
    if (v > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return write_u32(w, (uint32_t)v);
}

static sf_result_t patch_u32(wr_t *w, size_t pos, uint32_t v) {
    if (pos > w->size || w->size - pos < 4u) return SF_ERR_INTERNAL;
    if (w->be) v = sf_bswap32(v);
    memcpy(w->data + pos, &v, 4);
    return SF_OK;
}

static sf_result_t patch_var(wr_t *w, size_t pos, size_t v) {
    if (w->var64) {
        if (pos > w->size || w->size - pos < 8u) return SF_ERR_INTERNAL;
        uint64_t u = (uint64_t)v;
        if (w->be) u = sf_bswap64(u);
        memcpy(w->data + pos, &u, 8);
        return SF_OK;
    }
    if (v > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return patch_u32(w, pos, (uint32_t)v);
}

static uint32_t utf8_next(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    if (*p < 0x80u) { *s = (const char *)(p + 1); return *p; }
    if ((*p & 0xE0u) == 0xC0u && p[1]) {
        *s = (const char *)(p + 2);
        return ((*p & 0x1Fu) << 6) | (p[1] & 0x3Fu);
    }
    if ((*p & 0xF0u) == 0xE0u && p[1] && p[2]) {
        *s = (const char *)(p + 3);
        return ((*p & 0x0Fu) << 12) | ((p[1] & 0x3Fu) << 6) | (p[2] & 0x3Fu);
    }
    *s = (const char *)(p + 1);
    return '?';
}

static size_t utf16_bytes_z(const char *s) {
    size_t units = 1u;
    if (!s) return 2u;
    while (*s) { (void)utf8_next(&s); units++; }
    return units * 2u;
}

static sf_result_t write_utf16_z_padded(wr_t *w, const char *s, size_t bytes) {
    size_t written = 0;
    if (!s) s = "";
    while (*s && written + 2u <= bytes) {
        uint32_t ch = utf8_next(&s);
        if (ch > 0xFFFFu) ch = '?';
        TRY(write_u16(w, (uint16_t)ch));
        written += 2u;
    }
    while (written < bytes) { TRY(write_u16(w, 0)); written += 2u; }
    return SF_OK;
}

static sf_result_t write_fixstr_w(wr_t *w, const char *s, size_t bytes) {
    if ((bytes & 1u) != 0) return SF_ERR_INVALID_ARG;
    return write_utf16_z_padded(w, s, bytes);
}

static sf_result_t write_utf16_z(wr_t *w, const char *s) {
    if (!s) s = "";
    while (*s) {
        uint32_t ch = utf8_next(&s);
        if (ch > 0xFFFFu) ch = '?';
        TRY(write_u16(w, (uint16_t)ch));
    }
    return write_u16(w, 0);
}

static sf_result_t pad4(wr_t *w) {
    while ((w->size & 3u) != 0) TRY(write_u8(w, 0));
    return SF_OK;
}

static int32_t mqb_header_size(sf_mqb_version_t version) {
    switch (version) {
    case SF_MQB_VERSION_DARK_SOULS_2: return 0x14;
    case SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR: return 0x28;
    case SF_MQB_VERSION_BLOODBORNE: return 0x20;
    case SF_MQB_VERSION_DARK_SOULS_3: return 0x24;
    default: return 0;
    }
}

static void free_parameter(sf_mqb_parameter_t *p);

static void free_point(sf_mqb_point_t *p) { if (p) sf_xfree(p->alloc, p); }

static void free_sequence(sf_mqb_sequence_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->point_count; i++) free_point(s->points[i]);
    sf_xfree(s->alloc, s->points);
    sf_xfree(s->alloc, s);
}

static void parameter_clear_value(sf_mqb_parameter_t *p) {
    if (!p) return;
    if (p->type == SF_MQB_PARAM_TYPE_STRING) sf_xfree(p->alloc, p->value.str);
    if (p->type == SF_MQB_PARAM_TYPE_CUSTOM) sf_xfree(p->alloc, p->value.custom.bytes);
    memset(&p->value, 0, sizeof(p->value));
}

static void free_parameter(sf_mqb_parameter_t *p) {
    if (!p) return;
    sf_xfree(p->alloc, p->name);
    parameter_clear_value(p);
    for (size_t i = 0; i < p->sequence_count; i++) free_sequence(p->sequences[i]);
    sf_xfree(p->alloc, p->sequences);
    sf_xfree(p->alloc, p);
}

static void free_event(sf_mqb_event_t *e) {
    if (!e) return;
    for (size_t i = 0; i < e->parameter_count; i++) free_parameter(e->parameters[i]);
    sf_xfree(e->alloc, e->parameters);
    sf_xfree(e->alloc, e->transforms);
    sf_xfree(e->alloc, e);
}

static void free_timeline(sf_mqb_timeline_t *t) {
    if (!t) return;
    for (size_t i = 0; i < t->event_count; i++) free_event(t->events[i]);
    for (size_t i = 0; i < t->parameter_count; i++) free_parameter(t->parameters[i]);
    sf_xfree(t->alloc, t->events);
    sf_xfree(t->alloc, t->parameters);
    sf_xfree(t->alloc, t);
}

static void free_cut(sf_mqb_cut_t *c) {
    if (!c) return;
    sf_xfree(c->alloc, c->name);
    for (size_t i = 0; i < c->timeline_count; i++) free_timeline(c->timelines[i]);
    sf_xfree(c->alloc, c->timelines);
    sf_xfree(c->alloc, c);
}

static void free_resource(sf_mqb_resource_t *r) {
    if (!r) return;
    sf_xfree(r->alloc, r->name);
    sf_xfree(r->alloc, r->path);
    for (size_t i = 0; i < r->parameter_count; i++) free_parameter(r->parameters[i]);
    sf_xfree(r->alloc, r->parameters);
    sf_xfree(r->alloc, r);
}

void sf_mqb_destroy(sf_mqb_t *m) {
    if (!m) return;
    sf_xfree(m->alloc, m->name);
    sf_xfree(m->alloc, m->resource_directory);
    for (size_t i = 0; i < m->resource_count; i++) free_resource(m->resources[i]);
    for (size_t i = 0; i < m->cut_count; i++) free_cut(m->cuts[i]);
    sf_xfree(m->alloc, m->resources);
    sf_xfree(m->alloc, m->cuts);
    sf_xfree(m->alloc, m);
}

sf_result_t sf_mqb_create(sf_mqb_t **out, sf_mqb_version_t version, bool big_endian,
                          const sf_allocator_t *alloc) {
    if (!out || mqb_header_size(version) == 0) return SF_ERR_INVALID_ARG;
    const sf_allocator_t *a = sf_alloc_or_default(alloc);
    sf_mqb_t *m = (sf_mqb_t *)sf_xalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    memset(m, 0, sizeof(*m));
    m->alloc = a;
    m->version = version;
    m->big_endian = big_endian;
    m->framerate = 30.0f;
    if (mqb_strdup(a, "", &m->name) != SF_OK || mqb_strdup(a, "", &m->resource_directory) != SF_OK) {
        sf_mqb_destroy(m);
        return SF_ERR_OOM;
    }
    *out = m;
    return SF_OK;
}

static sf_result_t new_resource(sf_mqb_t *m, sf_mqb_resource_t **out) {
    TRY(mqb_grow_ptrs(m->alloc, (void ***)&m->resources, m->resource_count, &m->resource_cap));
    sf_mqb_resource_t *r = (sf_mqb_resource_t *)sf_xalloc(m->alloc, sizeof(*r));
    if (!r) return SF_ERR_OOM;
    memset(r, 0, sizeof(*r));
    r->alloc = m->alloc;
    if (mqb_strdup(r->alloc, "", &r->name) != SF_OK) { sf_xfree(r->alloc, r); return SF_ERR_OOM; }
    m->resources[m->resource_count++] = r;
    if (out) *out = r;
    return SF_OK;
}

static sf_result_t new_cut(sf_mqb_t *m, sf_mqb_cut_t **out) {
    TRY(mqb_grow_ptrs(m->alloc, (void ***)&m->cuts, m->cut_count, &m->cut_cap));
    sf_mqb_cut_t *c = (sf_mqb_cut_t *)sf_xalloc(m->alloc, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    memset(c, 0, sizeof(*c));
    c->alloc = m->alloc;
    if (mqb_strdup(c->alloc, "", &c->name) != SF_OK) { sf_xfree(c->alloc, c); return SF_ERR_OOM; }
    m->cuts[m->cut_count++] = c;
    if (out) *out = c;
    return SF_OK;
}

static sf_result_t new_timeline(sf_mqb_cut_t *c, sf_mqb_timeline_t **out) {
    TRY(mqb_grow_ptrs(c->alloc, (void ***)&c->timelines, c->timeline_count, &c->timeline_cap));
    sf_mqb_timeline_t *t = (sf_mqb_timeline_t *)sf_xalloc(c->alloc, sizeof(*t));
    if (!t) return SF_ERR_OOM;
    memset(t, 0, sizeof(*t));
    t->alloc = c->alloc;
    c->timelines[c->timeline_count++] = t;
    if (out) *out = t;
    return SF_OK;
}

static sf_result_t new_event(sf_mqb_timeline_t *t, sf_mqb_event_t **out) {
    TRY(mqb_grow_ptrs(t->alloc, (void ***)&t->events, t->event_count, &t->event_cap));
    sf_mqb_event_t *e = (sf_mqb_event_t *)sf_xalloc(t->alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    e->alloc = t->alloc;
    t->events[t->event_count++] = e;
    if (out) *out = e;
    return SF_OK;
}

static sf_result_t init_parameter(sf_mqb_parameter_t *p) {
    p->type = SF_MQB_PARAM_TYPE_INT;
    p->value.s32 = 0;
    return mqb_strdup(p->alloc, "", &p->name);
}

static sf_result_t new_parameter(const sf_allocator_t *alloc, sf_mqb_parameter_t ***arr,
                                 size_t *count, size_t *cap, sf_mqb_parameter_t **out) {
    TRY(mqb_grow_ptrs(alloc, (void ***)arr, *count, cap));
    sf_mqb_parameter_t *p = (sf_mqb_parameter_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->alloc = alloc;
    if (init_parameter(p) != SF_OK) { sf_xfree(alloc, p); return SF_ERR_OOM; }
    (*arr)[(*count)++] = p;
    if (out) *out = p;
    return SF_OK;
}

static sf_result_t new_sequence(sf_mqb_parameter_t *p, sf_mqb_sequence_t **out) {
    TRY(mqb_grow_ptrs(p->alloc, (void ***)&p->sequences, p->sequence_count, &p->sequence_cap));
    sf_mqb_sequence_t *s = (sf_mqb_sequence_t *)sf_xalloc(p->alloc, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    memset(s, 0, sizeof(*s));
    s->alloc = p->alloc;
    s->value_type = SF_MQB_PARAM_TYPE_BYTE;
    s->point_type = 1;
    p->sequences[p->sequence_count++] = s;
    if (out) *out = s;
    return SF_OK;
}

static sf_result_t new_point(sf_mqb_sequence_t *s, sf_mqb_point_t **out) {
    TRY(mqb_grow_ptrs(s->alloc, (void ***)&s->points, s->point_count, &s->point_cap));
    sf_mqb_point_t *p = (sf_mqb_point_t *)sf_xalloc(s->alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->alloc = s->alloc;
    p->type = s->value_type;
    s->points[s->point_count++] = p;
    if (out) *out = p;
    return SF_OK;
}

static sf_result_t read_parameter(rdr_t *r, const sf_allocator_t *alloc, sf_mqb_parameter_t **out);

static sf_result_t read_transform(rdr_t *r, sf_mqb_transform_t *t) {
    TRY(read_f32(r, &t->frame));
    TRY(read_f32(r, &t->translation.x)); TRY(read_f32(r, &t->translation.y)); TRY(read_f32(r, &t->translation.z));
    TRY(read_f32(r, &t->unk10.x)); TRY(read_f32(r, &t->unk10.y)); TRY(read_f32(r, &t->unk10.z));
    TRY(read_f32(r, &t->unk1c.x)); TRY(read_f32(r, &t->unk1c.y)); TRY(read_f32(r, &t->unk1c.z));
    TRY(read_f32(r, &t->rotation.x)); TRY(read_f32(r, &t->rotation.y)); TRY(read_f32(r, &t->rotation.z));
    TRY(read_f32(r, &t->unk34.x)); TRY(read_f32(r, &t->unk34.y)); TRY(read_f32(r, &t->unk34.z));
    TRY(read_f32(r, &t->unk40.x)); TRY(read_f32(r, &t->unk40.y)); TRY(read_f32(r, &t->unk40.z));
    TRY(read_f32(r, &t->scale.x)); TRY(read_f32(r, &t->scale.y)); TRY(read_f32(r, &t->scale.z));
    TRY(read_f32(r, &t->unk58.x)); TRY(read_f32(r, &t->unk58.y)); TRY(read_f32(r, &t->unk58.z));
    TRY(read_f32(r, &t->unk64.x)); TRY(read_f32(r, &t->unk64.y)); TRY(read_f32(r, &t->unk64.z));
    return SF_OK;
}

static sf_result_t read_event_at(rdr_t *r, const sf_allocator_t *alloc, sf_mqb_event_t **out) {
    sf_mqb_event_t *e = (sf_mqb_event_t *)sf_xalloc(alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    e->alloc = alloc;
    int32_t param_count, transform_count;
    sf_result_t res = read_i32(r, &e->id);
    if (res == SF_OK) res = read_i32(r, &e->resource_index);
    if (res == SF_OK) res = read_i32(r, &e->unk08);
    if (res == SF_OK) res = read_i32(r, &e->start_frame);
    if (res == SF_OK) res = read_i32(r, &e->duration);
    if (res == SF_OK) res = read_i32(r, &e->unk14);
    if (res == SF_OK) res = read_i32(r, &e->unk18);
    if (res == SF_OK) res = read_i32(r, &e->unk1c);
    if (res == SF_OK) res = read_i32(r, &e->unk20);
    if (res == SF_OK && e->unk20 != 0 && e->unk20 != 1) res = SF_ERR_BAD_MAGIC;
    if (res == SF_OK) res = read_i32(r, &param_count);
    if (res == SF_OK) res = read_i32(r, &e->unk28);
    if (res == SF_OK) res = assert_i32(r, 0);
    for (int32_t i = 0; res == SF_OK && i < param_count; i++) {
        sf_mqb_parameter_t *p = NULL;
        res = read_parameter(r, alloc, &p);
        if (res == SF_OK) {
            res = mqb_grow_ptrs(alloc, (void ***)&e->parameters, e->parameter_count, &e->parameter_cap);
            if (res == SF_OK) e->parameters[e->parameter_count++] = p;
            else free_parameter(p);
        }
    }
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK) res = read_i32(r, &transform_count);
    for (int i = 0; res == SF_OK && i < 6; i++) res = assert_i32(r, 0);
    for (int32_t i = 0; res == SF_OK && i < transform_count; i++) {
        res = mqb_grow_bytes(alloc, (void **)&e->transforms, sizeof(*e->transforms),
                             e->transform_count, &e->transform_cap);
        if (res == SF_OK) res = read_transform(r, &e->transforms[e->transform_count++]);
    }
    if (res != SF_OK) { free_event(e); return res; }
    *out = e;
    return SF_OK;
}

static sf_result_t find_event(event_offset_t *events, size_t count, size_t off, sf_mqb_event_t **out) {
    for (size_t i = 0; i < count; i++) {
        if (events[i].offset == off) { *out = (sf_mqb_event_t *)events[i].event; return SF_OK; }
    }
    return SF_ERR_BAD_MAGIC;
}

static sf_result_t read_timeline_at(rdr_t *r, const sf_allocator_t *alloc, sf_mqb_version_t version,
                                    event_offset_t *events, size_t event_total,
                                    sf_mqb_timeline_t **out) {
    sf_mqb_timeline_t *t = (sf_mqb_timeline_t *)sf_xalloc(alloc, sizeof(*t));
    if (!t) return SF_ERR_OOM;
    memset(t, 0, sizeof(*t));
    t->alloc = alloc;
    size_t event_offsets_offset, parameter_offset;
    int32_t event_count, parameter_count;
    sf_result_t res = read_var(r, &event_offsets_offset);
    if (res == SF_OK) res = read_i32(r, &event_count);
    if (res == SF_OK && version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) res = assert_i32(r, 0);
    if (res == SF_OK) res = read_var(r, &parameter_offset);
    if (res == SF_OK) res = read_i32(r, &parameter_count);
    if (res == SF_OK) res = read_i32(r, &t->unk10);
    if (res == SF_OK) {
        size_t old = r->pos;
        r->pos = event_offsets_offset;
        for (int32_t i = 0; res == SF_OK && i < event_count; i++) {
            size_t off;
            res = read_var(r, &off);
            if (res == SF_OK) {
                sf_mqb_event_t *e = NULL;
                res = find_event(events, event_total, off, &e);
                if (res == SF_OK) {
                    res = mqb_grow_ptrs(alloc, (void ***)&t->events, t->event_count, &t->event_cap);
                    if (res == SF_OK) t->events[t->event_count++] = e;
                }
            }
        }
        r->pos = old;
    }
    if (res == SF_OK) {
        size_t old = r->pos;
        r->pos = parameter_offset;
        for (int32_t i = 0; res == SF_OK && i < parameter_count; i++) {
            sf_mqb_parameter_t *p = NULL;
            res = read_parameter(r, alloc, &p);
            if (res == SF_OK) {
                res = mqb_grow_ptrs(alloc, (void ***)&t->parameters, t->parameter_count, &t->parameter_cap);
                if (res == SF_OK) t->parameters[t->parameter_count++] = p;
                else free_parameter(p);
            }
        }
        r->pos = old;
    }
    if (res != SF_OK) { sf_xfree(alloc, t->events); sf_xfree(alloc, t->parameters); sf_xfree(alloc, t); return res; }
    *out = t;
    return SF_OK;
}

static sf_result_t read_resource(rdr_t *r, sf_mqb_t *m, int32_t index) {
    sf_mqb_resource_t *res = NULL;
    TRY(new_resource(m, &res));
    sf_xfree(res->alloc, res->name);
    TRY(read_fixstr_w(r, 0x40, res->alloc, &res->name));
    int32_t idx, parameter_count;
    TRY(read_i32(r, &res->parent_index));
    TRY(read_i32(r, &idx));
    if (idx != index) return SF_ERR_BAD_MAGIC;
    TRY(read_i32(r, &res->unk48));
    TRY(read_i32(r, &parameter_count));
    for (int32_t i = 0; i < parameter_count; i++) {
        sf_mqb_parameter_t *p = NULL;
        TRY(read_parameter(r, res->alloc, &p));
        TRY(mqb_grow_ptrs(res->alloc, (void ***)&res->parameters, res->parameter_count, &res->parameter_cap));
        res->parameters[res->parameter_count++] = p;
    }
    return SF_OK;
}

static sf_result_t read_cut(rdr_t *r, sf_mqb_t *m) {
    sf_mqb_cut_t *c = NULL;
    TRY(new_cut(m, &c));
    sf_xfree(c->alloc, c->name);
    TRY(read_fixstr_w(r, 0x40, c->alloc, &c->name));
    int32_t event_count, timeline_count;
    TRY(read_i32(r, &event_count));
    TRY(read_i32(r, &c->unk44));
    TRY(read_i32(r, &c->duration));
    TRY(assert_i32(r, 0));
    TRY(read_i32(r, &timeline_count));
    if (m->version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) TRY(assert_i32(r, 0));
    size_t timelines_offset;
    TRY(read_var(r, &timelines_offset));
    if (m->version != SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) TRY(assert_i64(r, 0));
    event_offset_t *events = NULL;
    if (event_count > 0) {
        events = (event_offset_t *)sf_xalloc(m->alloc, (size_t)event_count * sizeof(*events));
        if (!events) return SF_ERR_OOM;
    }
    sf_result_t res = SF_OK;
    for (int32_t i = 0; res == SF_OK && i < event_count; i++) {
        events[i].offset = r->pos;
        sf_mqb_event_t *e = NULL;
        res = read_event_at(r, m->alloc, &e);
        events[i].event = e;
    }
    if (res == SF_OK) {
        size_t old = r->pos;
        r->pos = timelines_offset;
        for (int32_t i = 0; res == SF_OK && i < timeline_count; i++) {
            sf_mqb_timeline_t *t = NULL;
            res = read_timeline_at(r, m->alloc, m->version, events, (size_t)event_count, &t);
            if (res == SF_OK) {
                res = mqb_grow_ptrs(c->alloc, (void ***)&c->timelines, c->timeline_count, &c->timeline_cap);
                if (res == SF_OK) c->timelines[c->timeline_count++] = t;
                else { sf_xfree(t->alloc, t->events); sf_xfree(t->alloc, t->parameters); sf_xfree(t->alloc, t); }
            }
        }
        r->pos = old;
    }
    sf_xfree(m->alloc, events);
    return res;
}

static sf_result_t read_point(rdr_t *r, const sf_allocator_t *alloc, sf_mqb_param_type_t type,
                              int32_t point_type, sf_mqb_point_t **out) {
    sf_mqb_point_t *p = (sf_mqb_point_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->alloc = alloc;
    p->type = type;
    sf_result_t res = SF_OK;
    if (type == SF_MQB_PARAM_TYPE_BYTE) {
        res = read_u8(r, &p->value.u8);
        if (res == SF_OK) { uint16_t pad; res = read_u16(r, &pad); if (res == SF_OK && pad != 0) res = SF_ERR_BAD_MAGIC; }
        if (res == SF_OK) { uint8_t pad; res = read_u8(r, &pad); if (res == SF_OK && pad != 0) res = SF_ERR_BAD_MAGIC; }
    } else if (type == SF_MQB_PARAM_TYPE_FLOAT) res = read_f32(r, &p->value.f32);
    else if (type == SF_MQB_PARAM_TYPE_UINT) res = read_u32(r, &p->value.u32);
    else res = SF_ERR_UNSUPPORTED_VERSION;
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK) res = read_i32(r, &p->unk08);
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK && point_type == 2) { res = read_f32(r, &p->unk10); if (res == SF_OK) res = read_f32(r, &p->unk14); }
    if (res != SF_OK) { sf_xfree(alloc, p); return res; }
    *out = p;
    return SF_OK;
}

static sf_result_t read_sequence(rdr_t *r, const sf_allocator_t *alloc, size_t parent_value_offset,
                                 size_t parent_value_end_offset, sf_mqb_sequence_t **out) {
    sf_mqb_sequence_t *s = (sf_mqb_sequence_t *)sf_xalloc(alloc, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    memset(s, 0, sizeof(*s));
    s->alloc = alloc;
    int32_t point_count, point_size, value_offset;
    uint32_t vt = 0;
    sf_result_t res = assert_i32(r, 0x1C);
    if (res == SF_OK) res = read_i32(r, &point_count);
    if (res == SF_OK) res = read_u32(r, &vt);
    s->value_type = (sf_mqb_param_type_t)vt;
    if (res == SF_OK) res = read_i32(r, &s->point_type);
    if (res == SF_OK && s->point_type != 0 && s->point_type != 1 && s->point_type != 2) res = SF_ERR_BAD_MAGIC;
    if (res == SF_OK) res = read_i32(r, &point_size);
    if (res == SF_OK && point_size != ((s->point_type == 0 || s->point_type == 1) ? 0x10 : 0x18)) res = SF_ERR_BAD_MAGIC;
    int32_t points_offset = 0;
    if (res == SF_OK) res = read_i32(r, &points_offset);
    if (res == SF_OK) res = read_i32(r, &value_offset);
    if (res == SF_OK) {
        if ((size_t)value_offset < parent_value_offset || (size_t)value_offset >= parent_value_end_offset) res = SF_ERR_BAD_MAGIC;
        else s->value_index = value_offset - (int32_t)parent_value_offset;
    }
    if (res == SF_OK) {
        size_t old = r->pos;
        r->pos = (size_t)points_offset;
        for (int32_t i = 0; res == SF_OK && i < point_count; i++) {
            sf_mqb_point_t *p = NULL;
            res = read_point(r, alloc, s->value_type, s->point_type, &p);
            if (res == SF_OK) {
                res = mqb_grow_ptrs(alloc, (void ***)&s->points, s->point_count, &s->point_cap);
                if (res == SF_OK) s->points[s->point_count++] = p;
                else free_point(p);
            }
        }
        r->pos = old;
    }
    if (res != SF_OK) { free_sequence(s); return res; }
    *out = s;
    return SF_OK;
}

static sf_result_t read_parameter(rdr_t *r, const sf_allocator_t *alloc, sf_mqb_parameter_t **out) {
    sf_mqb_parameter_t *p = (sf_mqb_parameter_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->alloc = alloc;
    sf_result_t res = read_fixstr_w(r, 0x40, alloc, &p->name);
    uint32_t type = 0;
    int32_t sequences_offset, sequence_count, len;
    size_t value_offset = r->pos;
    size_t value_end_offset = value_offset;
    if (res == SF_OK) res = read_u32(r, &type);
    p->type = (sf_mqb_param_type_t)type;
    if (res == SF_OK) res = read_i32(r, &p->member_count);
    value_offset = r->pos;
    if (res == SF_OK) {
        switch (p->type) {
        case SF_MQB_PARAM_TYPE_BOOL: { uint8_t v; res = read_u8(r, &v); p->value.b = v != 0; break; }
        case SF_MQB_PARAM_TYPE_SBYTE: { uint8_t v; res = read_u8(r, &v); p->value.s8 = (int8_t)v; break; }
        case SF_MQB_PARAM_TYPE_BYTE: res = read_u8(r, &p->value.u8); break;
        case SF_MQB_PARAM_TYPE_SHORT: { uint16_t v; res = read_u16(r, &v); p->value.s16 = (int16_t)v; break; }
        case SF_MQB_PARAM_TYPE_INT: res = read_i32(r, &p->value.s32); break;
        case SF_MQB_PARAM_TYPE_UINT: res = read_u32(r, &p->value.u32); break;
        case SF_MQB_PARAM_TYPE_FLOAT: res = read_f32(r, &p->value.f32); break;
        case SF_MQB_PARAM_TYPE_STRING:
        case SF_MQB_PARAM_TYPE_CUSTOM:
        case SF_MQB_PARAM_TYPE_COLOR:
        case SF_MQB_PARAM_TYPE_INT_COLOR:
        case SF_MQB_PARAM_TYPE_VECTOR: res = read_i32(r, &len); break;
        default: res = SF_ERR_UNSUPPORTED_VERSION; break;
        }
    }
    value_end_offset = r->pos;
    if (res == SF_OK && (p->type == SF_MQB_PARAM_TYPE_BOOL || p->type == SF_MQB_PARAM_TYPE_SBYTE || p->type == SF_MQB_PARAM_TYPE_BYTE)) {
        uint8_t z8; uint16_t z16;
        res = read_u8(r, &z8); if (res == SF_OK && z8 != 0) res = SF_ERR_BAD_MAGIC;
        if (res == SF_OK) { res = read_u16(r, &z16); if (res == SF_OK && z16 != 0) res = SF_ERR_BAD_MAGIC; }
    } else if (res == SF_OK && p->type == SF_MQB_PARAM_TYPE_SHORT) {
        uint16_t z16; res = read_u16(r, &z16); if (res == SF_OK && z16 != 0) res = SF_ERR_BAD_MAGIC;
    }
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK) res = read_i32(r, &sequences_offset);
    if (res == SF_OK) res = read_i32(r, &sequence_count);
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK) res = assert_i32(r, 0);
    if (res == SF_OK) {
        switch (p->type) {
        case SF_MQB_PARAM_TYPE_STRING:
            if (len <= 0 || (len & 0xF) != 0) res = SF_ERR_BAD_MAGIC;
            else res = read_fixstr_w(r, (size_t)len, alloc, &p->value.str);
            break;
        case SF_MQB_PARAM_TYPE_CUSTOM:
            if (len < 0 || (len & 3) != 0) res = SF_ERR_BAD_MAGIC;
            else {
                p->value.custom.size = (size_t)len;
                if (len > 0) {
                    p->value.custom.bytes = (uint8_t *)sf_xalloc(alloc, (size_t)len);
                    if (!p->value.custom.bytes) res = SF_ERR_OOM;
                    else res = read_exact(r, p->value.custom.bytes, (size_t)len);
                }
            }
            break;
        case SF_MQB_PARAM_TYPE_COLOR: {
            uint8_t z;
            if (p->member_count != 3 || len != 4) { res = SF_ERR_BAD_MAGIC; break; }
            value_offset = r->pos;
            p->value.color.a = 255;
            res = read_u8(r, &p->value.color.r);
            if (res == SF_OK) res = read_u8(r, &p->value.color.g);
            if (res == SF_OK) res = read_u8(r, &p->value.color.b);
            if (res == SF_OK) { res = read_u8(r, &z); if (res == SF_OK && z != 0) res = SF_ERR_BAD_MAGIC; }
            value_end_offset = r->pos - 1u;
            break;
        }
        case SF_MQB_PARAM_TYPE_INT_COLOR:
            if (p->member_count != 4 || len != 20) { res = SF_ERR_BAD_MAGIC; break; }
            value_offset = r->pos;
            res = read_i32(r, &p->value.int_color[0]);
            if (res == SF_OK) res = read_i32(r, &p->value.int_color[1]);
            if (res == SF_OK) res = read_i32(r, &p->value.int_color[2]);
            if (res == SF_OK) res = read_i32(r, &p->value.int_color[3]);
            value_end_offset = r->pos;
            if (res == SF_OK) res = assert_i32(r, 0);
            break;
        case SF_MQB_PARAM_TYPE_VECTOR:
            if (len != p->member_count * 4 + 4) { res = SF_ERR_BAD_MAGIC; break; }
            value_offset = r->pos;
            res = read_f32(r, &p->value.vec.x);
            if (res == SF_OK && p->member_count > 1) res = read_f32(r, &p->value.vec.y);
            if (res == SF_OK && p->member_count > 2) res = read_f32(r, &p->value.vec.z);
            if (res == SF_OK && p->member_count > 3) res = read_f32(r, &p->value.vec.w);
            value_end_offset = r->pos;
            if (res == SF_OK) res = assert_i32(r, 0);
            break;
        default: break;
        }
    }
    if (res == SF_OK && sequence_count > 0) {
        size_t old = r->pos;
        r->pos = (size_t)sequences_offset;
        for (int32_t i = 0; res == SF_OK && i < sequence_count; i++) {
            sf_mqb_sequence_t *s = NULL;
            res = read_sequence(r, alloc, value_offset, value_end_offset, &s);
            if (res == SF_OK) {
                res = mqb_grow_ptrs(alloc, (void ***)&p->sequences, p->sequence_count, &p->sequence_cap);
                if (res == SF_OK) p->sequences[p->sequence_count++] = s;
                else free_sequence(s);
            }
        }
        r->pos = old;
    }
    if (res != SF_OK) { free_parameter(p); return res; }
    *out = p;
    return SF_OK;
}

sf_result_t sf_mqb_read_from_memory(sf_mqb_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    if (!out || !bytes) return SF_ERR_INVALID_ARG;
    if (!sf_mqb_is(bytes, size)) return SF_ERR_BAD_MAGIC;
    rdr_t r = { (const uint8_t *)bytes, size, 4u, false, false };
    uint8_t endian, z, long_format;
    TRY(read_u8(&r, &endian));
    if (endian != 0 && endian != 0xFFu) return SF_ERR_BAD_MAGIC;
    r.be = endian == 0xFFu;
    TRY(read_u8(&r, &z)); if (z != 0) return SF_ERR_BAD_MAGIC;
    TRY(read_u8(&r, &long_format)); if (long_format != 0 && long_format != 0xFFu) return SF_ERR_BAD_MAGIC;
    TRY(read_u8(&r, &z)); if (z != 0) return SF_ERR_BAD_MAGIC;
    uint32_t version_u;
    TRY(read_u32(&r, &version_u));
    sf_mqb_version_t version = (sf_mqb_version_t)version_u;
    int32_t header_size;
    TRY(read_i32(&r, &header_size));
    if (mqb_header_size(version) == 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (header_size != mqb_header_size(version)) return SF_ERR_BAD_MAGIC;
    if ((version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) != (long_format == 0xFFu)) return SF_ERR_BAD_MAGIC;
    r.var64 = version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR;
    sf_mqb_t *m = NULL;
    TRY(sf_mqb_create(&m, version, r.be, alloc));
    size_t resource_paths_offset;
    sf_result_t res = read_var(&r, &resource_paths_offset);
    if (res == SF_OK && version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) {
        for (int i = 0; res == SF_OK && i < 4; i++) res = assert_i32(&r, 0);
    } else if (res == SF_OK && version >= SF_MQB_VERSION_BLOODBORNE) {
        res = assert_i32(&r, 1);
        if (res == SF_OK) res = assert_i32(&r, 0);
        if (res == SF_OK) res = assert_i32(&r, 0);
        if (res == SF_OK && version >= SF_MQB_VERSION_DARK_SOULS_3) res = assert_i32(&r, 0);
    }
    if (res == SF_OK) { sf_xfree(m->alloc, m->name); res = read_fixstr_w(&r, 0x40, m->alloc, &m->name); }
    int32_t resource_count, cut_count, ignored;
    if (res == SF_OK) res = read_f32(&r, &m->framerate);
    if (res == SF_OK) res = read_i32(&r, &resource_count);
    if (res == SF_OK) res = read_i32(&r, &cut_count);
    if (res == SF_OK) res = read_i32(&r, &ignored);
    for (int i = 0; res == SF_OK && i < 4; i++) res = assert_i32(&r, 0);
    for (int32_t i = 0; res == SF_OK && i < resource_count; i++) res = read_resource(&r, m, i);
    for (int32_t i = 0; res == SF_OK && i < cut_count; i++) res = read_cut(&r, m);
    if (res == SF_OK) {
        r.pos = resource_paths_offset;
        size_t *path_offsets = NULL;
        if (resource_count > 0) {
            path_offsets = (size_t *)sf_xalloc(m->alloc, (size_t)resource_count * sizeof(*path_offsets));
            if (!path_offsets) res = SF_ERR_OOM;
        }
        for (int32_t i = 0; res == SF_OK && i < resource_count; i++) res = read_var(&r, &path_offsets[i]);
        if (res == SF_OK) { sf_xfree(m->alloc, m->resource_directory); res = read_utf16_z_at(&r, r.pos, m->alloc, &m->resource_directory); }
        for (int32_t i = 0; res == SF_OK && i < resource_count; i++) {
            if (path_offsets[i] != 0) res = read_utf16_z_at(&r, path_offsets[i], m->alloc, &m->resources[i]->path);
        }
        sf_xfree(m->alloc, path_offsets);
    }
    if (res != SF_OK) { sf_mqb_destroy(m); return res; }
    *out = m;
    return SF_OK;
}

bool sf_mqb_is(const void *bytes, size_t size) {
    const uint8_t *b = (const uint8_t *)bytes;
    return b && size >= 4u && b[0] == 'M' && b[1] == 'Q' && b[2] == 'B' && b[3] == ' ';
}

static sf_result_t ctx_add_event(write_ctx_t *ctx, const sf_allocator_t *alloc,
                                 const sf_mqb_event_t *event, size_t offset) {
    TRY(mqb_grow_bytes(alloc, (void **)&ctx->events, sizeof(*ctx->events), ctx->event_count, &ctx->event_cap));
    ctx->events[ctx->event_count++] = (event_offset_t){ event, offset };
    return SF_OK;
}

static sf_result_t ctx_add_timeline(write_ctx_t *ctx, const sf_allocator_t *alloc,
                                    const sf_mqb_timeline_t *timeline, size_t events_pos,
                                    size_t params_pos) {
    TRY(mqb_grow_bytes(alloc, (void **)&ctx->timelines, sizeof(*ctx->timelines), ctx->timeline_count, &ctx->timeline_cap));
    ctx->timelines[ctx->timeline_count++] = (timeline_patch_t){ timeline, events_pos, params_pos };
    return SF_OK;
}

static sf_result_t ctx_add_param(write_ctx_t *ctx, const sf_allocator_t *alloc,
                                 const sf_mqb_parameter_t *param, size_t sequences_pos,
                                 size_t value_offset) {
    TRY(mqb_grow_bytes(alloc, (void **)&ctx->params, sizeof(*ctx->params), ctx->param_count, &ctx->param_cap));
    ctx->params[ctx->param_count++] = (param_patch_t){ param, sequences_pos, value_offset };
    return SF_OK;
}

static sf_result_t ctx_add_seq(write_ctx_t *ctx, const sf_allocator_t *alloc,
                               const sf_mqb_sequence_t *seq, size_t points_pos) {
    TRY(mqb_grow_bytes(alloc, (void **)&ctx->seqs, sizeof(*ctx->seqs), ctx->seq_count, &ctx->seq_cap));
    ctx->seqs[ctx->seq_count++] = (seq_patch_t){ seq, points_pos };
    return SF_OK;
}

static sf_result_t lookup_event(write_ctx_t *ctx, const sf_mqb_event_t *e, size_t *out) {
    for (size_t i = 0; i < ctx->event_count; i++) if (ctx->events[i].event == e) { *out = ctx->events[i].offset; return SF_OK; }
    return SF_ERR_INTERNAL;
}

static sf_result_t write_parameter(wr_t *w, write_ctx_t *ctx, const sf_mqb_parameter_t *p);

static sf_result_t write_transform(wr_t *w, sf_mqb_transform_t t) {
    TRY(write_f32(w, t.frame));
    const sf_vec3_t *v[] = { &t.translation, &t.unk10, &t.unk1c, &t.rotation, &t.unk34, &t.unk40, &t.scale, &t.unk58, &t.unk64 };
    for (size_t i = 0; i < 9; i++) { TRY(write_f32(w, v[i]->x)); TRY(write_f32(w, v[i]->y)); TRY(write_f32(w, v[i]->z)); }
    return SF_OK;
}

static sf_result_t write_event(wr_t *w, write_ctx_t *ctx, const sf_mqb_event_t *e) {
    TRY(write_i32(w, e->id)); TRY(write_i32(w, e->resource_index)); TRY(write_i32(w, e->unk08));
    TRY(write_i32(w, e->start_frame)); TRY(write_i32(w, e->duration)); TRY(write_i32(w, e->unk14));
    TRY(write_i32(w, e->unk18)); TRY(write_i32(w, e->unk1c)); TRY(write_i32(w, e->unk20));
    if (e->parameter_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(write_i32(w, (int32_t)e->parameter_count)); TRY(write_i32(w, e->unk28)); TRY(write_i32(w, 0));
    for (size_t i = 0; i < e->parameter_count; i++) TRY(write_parameter(w, ctx, e->parameters[i]));
    if (e->transform_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(write_i32(w, 0)); TRY(write_i32(w, (int32_t)e->transform_count));
    for (int i = 0; i < 6; i++) TRY(write_i32(w, 0));
    for (size_t i = 0; i < e->transform_count; i++) TRY(write_transform(w, e->transforms[i]));
    return SF_OK;
}

static sf_result_t write_resource(wr_t *w, write_ctx_t *ctx, const sf_mqb_resource_t *r, int32_t index) {
    if (r->parameter_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(write_fixstr_w(w, r->name, 0x40)); TRY(write_i32(w, r->parent_index));
    TRY(write_i32(w, index)); TRY(write_i32(w, r->unk48)); TRY(write_i32(w, (int32_t)r->parameter_count));
    for (size_t i = 0; i < r->parameter_count; i++) TRY(write_parameter(w, ctx, r->parameters[i]));
    return SF_OK;
}

static size_t cut_event_count(const sf_mqb_cut_t *c) {
    size_t n = 0;
    for (size_t i = 0; i < c->timeline_count; i++) n += c->timelines[i]->event_count;
    return n;
}

static sf_result_t write_cut_events(wr_t *w, write_ctx_t *ctx, const sf_mqb_cut_t *c) {
    for (size_t i = 0; i < c->timeline_count; i++) {
        const sf_mqb_timeline_t *t = c->timelines[i];
        for (size_t j = 0; j < t->event_count; j++) {
            TRY(ctx_add_event(ctx, w->alloc, t->events[j], w->size));
            TRY(write_event(w, ctx, t->events[j]));
        }
    }
    return SF_OK;
}

static sf_result_t write_cut_header(wr_t *w, const sf_mqb_cut_t *c, sf_mqb_version_t version,
                                    size_t *timelines_pos) {
    size_t ec = cut_event_count(c);
    if (ec > INT32_MAX || c->timeline_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(write_fixstr_w(w, c->name, 0x40)); TRY(write_i32(w, (int32_t)ec)); TRY(write_i32(w, c->unk44));
    TRY(write_i32(w, c->duration)); TRY(write_i32(w, 0)); TRY(write_i32(w, (int32_t)c->timeline_count));
    if (version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) TRY(write_i32(w, 0));
    *timelines_pos = w->size; TRY(write_var(w, 0));
    if (version != SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) TRY(write_i64(w, 0));
    return SF_OK;
}

static sf_result_t write_timeline_header(wr_t *w, write_ctx_t *ctx, const sf_mqb_timeline_t *t,
                                         sf_mqb_version_t version) {
    if (t->event_count > INT32_MAX || t->parameter_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    size_t events_pos = w->size; TRY(write_var(w, 0)); TRY(write_i32(w, (int32_t)t->event_count));
    if (version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) TRY(write_i32(w, 0));
    size_t params_pos = w->size; TRY(write_var(w, 0)); TRY(write_i32(w, (int32_t)t->parameter_count)); TRY(write_i32(w, t->unk10));
    return ctx_add_timeline(ctx, w->alloc, t, events_pos, params_pos);
}

static sf_result_t write_point(wr_t *w, const sf_mqb_point_t *p, sf_mqb_param_type_t type, int32_t point_type) {
    if (type == SF_MQB_PARAM_TYPE_BYTE) { TRY(write_u8(w, p->value.u8)); TRY(write_u16(w, 0)); TRY(write_u8(w, 0)); }
    else if (type == SF_MQB_PARAM_TYPE_FLOAT) TRY(write_f32(w, p->value.f32));
    else if (type == SF_MQB_PARAM_TYPE_UINT) TRY(write_u32(w, p->value.u32));
    else return SF_ERR_UNSUPPORTED_VERSION;
    TRY(write_i32(w, 0)); TRY(write_i32(w, p->unk08)); TRY(write_i32(w, 0));
    if (point_type == 2) { TRY(write_f32(w, p->unk10)); TRY(write_f32(w, p->unk14)); }
    return SF_OK;
}

static sf_result_t write_sequences(wr_t *w, write_ctx_t *ctx) {
    for (size_t i = 0; i < ctx->param_count; i++) {
        const sf_mqb_parameter_t *p = ctx->params[i].param;
        if (p->sequence_count == 0) { TRY(patch_u32(w, ctx->params[i].sequences_pos, 0)); continue; }
        if (w->size > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        TRY(patch_u32(w, ctx->params[i].sequences_pos, (uint32_t)w->size));
        for (size_t j = 0; j < p->sequence_count; j++) {
            const sf_mqb_sequence_t *s = p->sequences[j];
            if (s->point_count > INT32_MAX || ctx->params[i].value_offset > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
            TRY(write_i32(w, 0x1C)); TRY(write_i32(w, (int32_t)s->point_count)); TRY(write_u32(w, (uint32_t)s->value_type));
            TRY(write_i32(w, s->point_type)); TRY(write_i32(w, (s->point_type == 0 || s->point_type == 1) ? 0x10 : 0x18));
            size_t points_pos = w->size; TRY(write_i32(w, 0));
            TRY(write_i32(w, (int32_t)ctx->params[i].value_offset + s->value_index));
            TRY(ctx_add_seq(ctx, w->alloc, s, points_pos));
        }
    }
    for (size_t i = 0; i < ctx->seq_count; i++) {
        if (w->size > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        TRY(patch_u32(w, ctx->seqs[i].points_pos, (uint32_t)w->size));
        const sf_mqb_sequence_t *s = ctx->seqs[i].seq;
        for (size_t j = 0; j < s->point_count; j++) TRY(write_point(w, s->points[j], s->value_type, s->point_type));
    }
    return SF_OK;
}

static sf_result_t write_parameter(wr_t *w, write_ctx_t *ctx, const sf_mqb_parameter_t *p) {
    TRY(write_fixstr_w(w, p->name, 0x40)); TRY(write_u32(w, (uint32_t)p->type)); TRY(write_i32(w, p->member_count));
    int32_t length = -1;
    if (p->type == SF_MQB_PARAM_TYPE_STRING) { size_t n = utf16_bytes_z(p->value.str); n = (n + 0xFu) & ~0xFu; if (n > INT32_MAX) return SF_ERR_OUT_OF_RANGE; length = (int32_t)n; }
    else if (p->type == SF_MQB_PARAM_TYPE_CUSTOM) { if (p->value.custom.size > INT32_MAX || (p->value.custom.size & 3u) != 0) return SF_ERR_INVALID_ARG; length = (int32_t)p->value.custom.size; }
    else if (p->type == SF_MQB_PARAM_TYPE_COLOR) length = p->member_count + 1;
    else if (p->type == SF_MQB_PARAM_TYPE_INT_COLOR || p->type == SF_MQB_PARAM_TYPE_VECTOR) length = p->member_count * 4 + 4;
    size_t value_offset = w->size;
    switch (p->type) {
    case SF_MQB_PARAM_TYPE_BOOL: TRY(write_u8(w, p->value.b ? 1u : 0u)); break;
    case SF_MQB_PARAM_TYPE_SBYTE: TRY(write_u8(w, (uint8_t)p->value.s8)); break;
    case SF_MQB_PARAM_TYPE_BYTE: TRY(write_u8(w, p->value.u8)); break;
    case SF_MQB_PARAM_TYPE_SHORT: TRY(write_u16(w, (uint16_t)p->value.s16)); break;
    case SF_MQB_PARAM_TYPE_INT: TRY(write_i32(w, p->value.s32)); break;
    case SF_MQB_PARAM_TYPE_UINT: TRY(write_u32(w, p->value.u32)); break;
    case SF_MQB_PARAM_TYPE_FLOAT: TRY(write_f32(w, p->value.f32)); break;
    case SF_MQB_PARAM_TYPE_STRING:
    case SF_MQB_PARAM_TYPE_CUSTOM:
    case SF_MQB_PARAM_TYPE_COLOR:
    case SF_MQB_PARAM_TYPE_INT_COLOR:
    case SF_MQB_PARAM_TYPE_VECTOR: TRY(write_i32(w, length)); break;
    default: return SF_ERR_UNSUPPORTED_VERSION;
    }
    if (p->type == SF_MQB_PARAM_TYPE_BOOL || p->type == SF_MQB_PARAM_TYPE_SBYTE || p->type == SF_MQB_PARAM_TYPE_BYTE) { TRY(write_u8(w, 0)); TRY(write_u16(w, 0)); }
    else if (p->type == SF_MQB_PARAM_TYPE_SHORT) TRY(write_u16(w, 0));
    TRY(write_i32(w, 0));
    size_t sequences_pos = w->size; TRY(write_i32(w, 0));
    if (p->sequence_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(write_i32(w, (int32_t)p->sequence_count)); TRY(write_i32(w, 0)); TRY(write_i32(w, 0));
    if (p->type == SF_MQB_PARAM_TYPE_STRING) TRY(write_utf16_z_padded(w, p->value.str, (size_t)length));
    else if (p->type == SF_MQB_PARAM_TYPE_CUSTOM) TRY(write_raw(w, p->value.custom.bytes, p->value.custom.size));
    else if (p->type == SF_MQB_PARAM_TYPE_COLOR) { value_offset = w->size; TRY(write_u8(w, p->value.color.r)); TRY(write_u8(w, p->value.color.g)); TRY(write_u8(w, p->value.color.b)); TRY(write_u8(w, 0)); }
    else if (p->type == SF_MQB_PARAM_TYPE_INT_COLOR) { value_offset = w->size; TRY(write_i32(w, p->value.int_color[0])); TRY(write_i32(w, p->value.int_color[1])); TRY(write_i32(w, p->value.int_color[2])); TRY(write_i32(w, p->value.int_color[3])); TRY(write_i32(w, 0)); }
    else if (p->type == SF_MQB_PARAM_TYPE_VECTOR) { value_offset = w->size; TRY(write_f32(w, p->value.vec.x)); if (p->member_count > 1) TRY(write_f32(w, p->value.vec.y)); if (p->member_count > 2) TRY(write_f32(w, p->value.vec.z)); if (p->member_count > 3) TRY(write_f32(w, p->value.vec.w)); TRY(write_i32(w, 0)); }
    return ctx_add_param(ctx, w->alloc, p, sequences_pos, value_offset);
}

sf_result_t sf_mqb_write_to_memory(const sf_mqb_t *m, void **out_bytes, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    if (!m || !out_bytes || !out_size || mqb_header_size(m->version) == 0) return SF_ERR_INVALID_ARG;
    wr_t w = { sf_alloc_or_default(alloc), NULL, 0, 0, m->big_endian, m->version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR };
    write_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    sf_result_t res = write_raw(&w, "MQB ", 4);
    if (res == SF_OK) res = write_u8(&w, m->big_endian ? 0xFFu : 0u);
    if (res == SF_OK) res = write_u8(&w, 0);
    if (res == SF_OK) res = write_u8(&w, m->version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR ? 0xFFu : 0u);
    if (res == SF_OK) res = write_u8(&w, 0);
    if (res == SF_OK) res = write_u32(&w, (uint32_t)m->version);
    if (res == SF_OK) res = write_i32(&w, mqb_header_size(m->version));
    size_t resource_paths_pos = w.size;
    if (res == SF_OK) res = write_var(&w, 0);
    if (res == SF_OK && m->version == SF_MQB_VERSION_DARK_SOULS_2_SCHOLAR) for (int i = 0; res == SF_OK && i < 4; i++) res = write_i32(&w, 0);
    else if (res == SF_OK && m->version >= SF_MQB_VERSION_BLOODBORNE) { res = write_i32(&w, 1); if (res == SF_OK) res = write_i32(&w, 0); if (res == SF_OK) res = write_i32(&w, 0); if (res == SF_OK && m->version >= SF_MQB_VERSION_DARK_SOULS_3) res = write_i32(&w, 0); }
    if (res == SF_OK) res = write_fixstr_w(&w, m->name, 0x40);
    if (res == SF_OK) res = write_f32(&w, m->framerate);
    if (res == SF_OK && (m->resource_count > INT32_MAX || m->cut_count > INT32_MAX)) res = SF_ERR_OUT_OF_RANGE;
    if (res == SF_OK) res = write_i32(&w, (int32_t)m->resource_count);
    if (res == SF_OK) res = write_i32(&w, (int32_t)m->cut_count);
    for (int i = 0; res == SF_OK && i < 5; i++) res = write_i32(&w, 0);
    for (size_t i = 0; res == SF_OK && i < m->resource_count; i++) res = write_resource(&w, &ctx, m->resources[i], (int32_t)i);
    size_t *timeline_offsets = NULL;
    if (res == SF_OK && m->cut_count > 0) { timeline_offsets = (size_t *)sf_xalloc(w.alloc, m->cut_count * sizeof(*timeline_offsets)); if (!timeline_offsets) res = SF_ERR_OOM; }
    for (size_t i = 0; res == SF_OK && i < m->cut_count; i++) { res = write_cut_header(&w, m->cuts[i], m->version, &timeline_offsets[i]); if (res == SF_OK) res = write_cut_events(&w, &ctx, m->cuts[i]); }
    for (size_t i = 0; res == SF_OK && i < m->cut_count; i++) { res = patch_var(&w, timeline_offsets[i], w.size); for (size_t j = 0; res == SF_OK && j < m->cuts[i]->timeline_count; j++) res = write_timeline_header(&w, &ctx, m->cuts[i]->timelines[j], m->version); }
    for (size_t i = 0; res == SF_OK && i < ctx.timeline_count; i++) { const sf_mqb_timeline_t *t = ctx.timelines[i].timeline; res = patch_var(&w, ctx.timelines[i].params_pos, w.size); for (size_t j = 0; res == SF_OK && j < t->parameter_count; j++) res = write_parameter(&w, &ctx, t->parameters[j]); }
    for (size_t i = 0; res == SF_OK && i < ctx.timeline_count; i++) { const sf_mqb_timeline_t *t = ctx.timelines[i].timeline; res = patch_var(&w, ctx.timelines[i].events_pos, w.size); for (size_t j = 0; res == SF_OK && j < t->event_count; j++) { size_t off; res = lookup_event(&ctx, t->events[j], &off); if (res == SF_OK) res = write_var(&w, off); } }
    if (res == SF_OK) res = patch_var(&w, resource_paths_pos, w.size);
    size_t *resource_path_positions = NULL;
    if (res == SF_OK && m->resource_count > 0) { resource_path_positions = (size_t *)sf_xalloc(w.alloc, m->resource_count * sizeof(*resource_path_positions)); if (!resource_path_positions) res = SF_ERR_OOM; }
    for (size_t i = 0; res == SF_OK && i < m->resource_count; i++) { resource_path_positions[i] = w.size; res = write_var(&w, 0); }
    if (res == SF_OK) res = write_utf16_z(&w, m->resource_directory);
    for (size_t i = 0; res == SF_OK && i < m->resource_count; i++) { if (m->resources[i]->path) { res = patch_var(&w, resource_path_positions[i], w.size); if (res == SF_OK) res = write_utf16_z(&w, m->resources[i]->path); } }
    if (res == SF_OK && m->version >= SF_MQB_VERSION_BLOODBORNE) { res = write_u16(&w, 0); if (res == SF_OK) res = pad4(&w); }
    if (res == SF_OK) res = write_sequences(&w, &ctx);
    sf_xfree(w.alloc, timeline_offsets);
    sf_xfree(w.alloc, resource_path_positions);
    sf_xfree(w.alloc, ctx.events); sf_xfree(w.alloc, ctx.timelines); sf_xfree(w.alloc, ctx.params); sf_xfree(w.alloc, ctx.seqs);
    if (res != SF_OK) { sf_xfree(w.alloc, w.data); return res; }
    *out_bytes = w.data;
    *out_size = w.size;
    return SF_OK;
}

sf_mqb_version_t sf_mqb_version(const sf_mqb_t *m) { return m ? m->version : 0; }
void sf_mqb_set_version(sf_mqb_t *m, sf_mqb_version_t v) { if (m) m->version = v; }
bool sf_mqb_big_endian(const sf_mqb_t *m) { return m && m->big_endian; }
void sf_mqb_set_big_endian(sf_mqb_t *m, bool be) { if (m) m->big_endian = be; }
const char *sf_mqb_name(const sf_mqb_t *m) { return (m && m->name) ? m->name : ""; }
sf_result_t sf_mqb_set_name(sf_mqb_t *m, const char *utf8) { return m ? mqb_set_string(m->alloc, &m->name, utf8) : SF_ERR_INVALID_ARG; }
float sf_mqb_framerate(const sf_mqb_t *m) { return m ? m->framerate : 0.0f; }
void sf_mqb_set_framerate(sf_mqb_t *m, float v) { if (m) m->framerate = v; }
const char *sf_mqb_resource_directory(const sf_mqb_t *m) { return (m && m->resource_directory) ? m->resource_directory : ""; }
sf_result_t sf_mqb_set_resource_directory(sf_mqb_t *m, const char *utf8) { return m ? mqb_set_string(m->alloc, &m->resource_directory, utf8) : SF_ERR_INVALID_ARG; }

size_t sf_mqb_resource_count(const sf_mqb_t *m) { return m ? m->resource_count : 0; }
sf_mqb_resource_t *sf_mqb_resource_at(const sf_mqb_t *m, size_t i) { return (m && i < m->resource_count) ? m->resources[i] : NULL; }
sf_result_t sf_mqb_add_resource(sf_mqb_t *m, sf_mqb_resource_t **out) { return m ? new_resource(m, out) : SF_ERR_INVALID_ARG; }
const char *sf_mqb_resource_name(const sf_mqb_resource_t *r) { return (r && r->name) ? r->name : ""; }
sf_result_t sf_mqb_resource_set_name(sf_mqb_resource_t *r, const char *utf8) { return r ? mqb_set_string(r->alloc, &r->name, utf8) : SF_ERR_INVALID_ARG; }
int32_t sf_mqb_resource_parent_index(const sf_mqb_resource_t *r) { return r ? r->parent_index : 0; }
void sf_mqb_resource_set_parent_index(sf_mqb_resource_t *r, int32_t v) { if (r) r->parent_index = v; }
int32_t sf_mqb_resource_unk48(const sf_mqb_resource_t *r) { return r ? r->unk48 : 0; }
void sf_mqb_resource_set_unk48(sf_mqb_resource_t *r, int32_t v) { if (r) r->unk48 = v; }
const char *sf_mqb_resource_path(const sf_mqb_resource_t *r) { return r ? r->path : NULL; }
sf_result_t sf_mqb_resource_set_path(sf_mqb_resource_t *r, const char *utf8_or_null) { if (!r) return SF_ERR_INVALID_ARG; sf_xfree(r->alloc, r->path); r->path = NULL; return utf8_or_null ? mqb_strdup(r->alloc, utf8_or_null, &r->path) : SF_OK; }
size_t sf_mqb_resource_parameter_count(const sf_mqb_resource_t *r) { return r ? r->parameter_count : 0; }
sf_mqb_parameter_t *sf_mqb_resource_parameter_at(const sf_mqb_resource_t *r, size_t i) { return (r && i < r->parameter_count) ? r->parameters[i] : NULL; }
sf_result_t sf_mqb_resource_add_parameter(sf_mqb_resource_t *r, sf_mqb_parameter_t **out) { return r ? new_parameter(r->alloc, &r->parameters, &r->parameter_count, &r->parameter_cap, out) : SF_ERR_INVALID_ARG; }

size_t sf_mqb_cut_count(const sf_mqb_t *m) { return m ? m->cut_count : 0; }
sf_mqb_cut_t *sf_mqb_cut_at(const sf_mqb_t *m, size_t i) { return (m && i < m->cut_count) ? m->cuts[i] : NULL; }
sf_result_t sf_mqb_add_cut(sf_mqb_t *m, sf_mqb_cut_t **out) { return m ? new_cut(m, out) : SF_ERR_INVALID_ARG; }
const char *sf_mqb_cut_name(const sf_mqb_cut_t *c) { return (c && c->name) ? c->name : ""; }
sf_result_t sf_mqb_cut_set_name(sf_mqb_cut_t *c, const char *utf8) { return c ? mqb_set_string(c->alloc, &c->name, utf8) : SF_ERR_INVALID_ARG; }
int32_t sf_mqb_cut_unk44(const sf_mqb_cut_t *c) { return c ? c->unk44 : 0; }
void sf_mqb_cut_set_unk44(sf_mqb_cut_t *c, int32_t v) { if (c) c->unk44 = v; }
int32_t sf_mqb_cut_duration(const sf_mqb_cut_t *c) { return c ? c->duration : 0; }
void sf_mqb_cut_set_duration(sf_mqb_cut_t *c, int32_t v) { if (c) c->duration = v; }
size_t sf_mqb_cut_timeline_count(const sf_mqb_cut_t *c) { return c ? c->timeline_count : 0; }
sf_mqb_timeline_t *sf_mqb_cut_timeline_at(const sf_mqb_cut_t *c, size_t i) { return (c && i < c->timeline_count) ? c->timelines[i] : NULL; }
sf_result_t sf_mqb_cut_add_timeline(sf_mqb_cut_t *c, sf_mqb_timeline_t **out) { return c ? new_timeline(c, out) : SF_ERR_INVALID_ARG; }

int32_t sf_mqb_timeline_unk10(const sf_mqb_timeline_t *t) { return t ? t->unk10 : 0; }
void sf_mqb_timeline_set_unk10(sf_mqb_timeline_t *t, int32_t v) { if (t) t->unk10 = v; }
size_t sf_mqb_timeline_event_count(const sf_mqb_timeline_t *t) { return t ? t->event_count : 0; }
sf_mqb_event_t *sf_mqb_timeline_event_at(const sf_mqb_timeline_t *t, size_t i) { return (t && i < t->event_count) ? t->events[i] : NULL; }
sf_result_t sf_mqb_timeline_add_event(sf_mqb_timeline_t *t, sf_mqb_event_t **out) { return t ? new_event(t, out) : SF_ERR_INVALID_ARG; }
size_t sf_mqb_timeline_parameter_count(const sf_mqb_timeline_t *t) { return t ? t->parameter_count : 0; }
sf_mqb_parameter_t *sf_mqb_timeline_parameter_at(const sf_mqb_timeline_t *t, size_t i) { return (t && i < t->parameter_count) ? t->parameters[i] : NULL; }
sf_result_t sf_mqb_timeline_add_parameter(sf_mqb_timeline_t *t, sf_mqb_parameter_t **out) { return t ? new_parameter(t->alloc, &t->parameters, &t->parameter_count, &t->parameter_cap, out) : SF_ERR_INVALID_ARG; }

int32_t sf_mqb_event_id(const sf_mqb_event_t *e) { return e ? e->id : 0; }
void sf_mqb_event_set_id(sf_mqb_event_t *e, int32_t v) { if (e) e->id = v; }
int32_t sf_mqb_event_resource_index(const sf_mqb_event_t *e) { return e ? e->resource_index : 0; }
void sf_mqb_event_set_resource_index(sf_mqb_event_t *e, int32_t v) { if (e) e->resource_index = v; }
int32_t sf_mqb_event_unk08(const sf_mqb_event_t *e) { return e ? e->unk08 : 0; }
void sf_mqb_event_set_unk08(sf_mqb_event_t *e, int32_t v) { if (e) e->unk08 = v; }
int32_t sf_mqb_event_start_frame(const sf_mqb_event_t *e) { return e ? e->start_frame : 0; }
void sf_mqb_event_set_start_frame(sf_mqb_event_t *e, int32_t v) { if (e) e->start_frame = v; }
int32_t sf_mqb_event_duration(const sf_mqb_event_t *e) { return e ? e->duration : 0; }
void sf_mqb_event_set_duration(sf_mqb_event_t *e, int32_t v) { if (e) e->duration = v; }
int32_t sf_mqb_event_unk14(const sf_mqb_event_t *e) { return e ? e->unk14 : 0; }
void sf_mqb_event_set_unk14(sf_mqb_event_t *e, int32_t v) { if (e) e->unk14 = v; }
int32_t sf_mqb_event_unk18(const sf_mqb_event_t *e) { return e ? e->unk18 : 0; }
void sf_mqb_event_set_unk18(sf_mqb_event_t *e, int32_t v) { if (e) e->unk18 = v; }
int32_t sf_mqb_event_unk1c(const sf_mqb_event_t *e) { return e ? e->unk1c : 0; }
void sf_mqb_event_set_unk1c(sf_mqb_event_t *e, int32_t v) { if (e) e->unk1c = v; }
int32_t sf_mqb_event_unk20(const sf_mqb_event_t *e) { return e ? e->unk20 : 0; }
void sf_mqb_event_set_unk20(sf_mqb_event_t *e, int32_t v) { if (e) e->unk20 = v; }
int32_t sf_mqb_event_unk28(const sf_mqb_event_t *e) { return e ? e->unk28 : 0; }
void sf_mqb_event_set_unk28(sf_mqb_event_t *e, int32_t v) { if (e) e->unk28 = v; }
size_t sf_mqb_event_parameter_count(const sf_mqb_event_t *e) { return e ? e->parameter_count : 0; }
sf_mqb_parameter_t *sf_mqb_event_parameter_at(const sf_mqb_event_t *e, size_t i) { return (e && i < e->parameter_count) ? e->parameters[i] : NULL; }
sf_result_t sf_mqb_event_add_parameter(sf_mqb_event_t *e, sf_mqb_parameter_t **out) { return e ? new_parameter(e->alloc, &e->parameters, &e->parameter_count, &e->parameter_cap, out) : SF_ERR_INVALID_ARG; }
size_t sf_mqb_event_transform_count(const sf_mqb_event_t *e) { return e ? e->transform_count : 0; }
sf_result_t sf_mqb_event_get_transform(const sf_mqb_event_t *e, size_t i, sf_mqb_transform_t *out) { if (!e || !out || i >= e->transform_count) return SF_ERR_INVALID_ARG; *out = e->transforms[i]; return SF_OK; }
sf_result_t sf_mqb_event_add_transform(sf_mqb_event_t *e, sf_mqb_transform_t t) { if (!e) return SF_ERR_INVALID_ARG; TRY(mqb_grow_bytes(e->alloc, (void **)&e->transforms, sizeof(*e->transforms), e->transform_count, &e->transform_cap)); e->transforms[e->transform_count++] = t; return SF_OK; }

const char *sf_mqb_parameter_name(const sf_mqb_parameter_t *p) { return (p && p->name) ? p->name : ""; }
sf_result_t sf_mqb_parameter_set_name(sf_mqb_parameter_t *p, const char *utf8) { return p ? mqb_set_string(p->alloc, &p->name, utf8) : SF_ERR_INVALID_ARG; }
sf_mqb_data_type_t sf_mqb_parameter_type(const sf_mqb_parameter_t *p) { return p ? p->type : 0; }
void sf_mqb_parameter_set_type(sf_mqb_parameter_t *p, sf_mqb_data_type_t t) { if (p) { parameter_clear_value(p); p->type = t; } }
int32_t sf_mqb_parameter_member_count(const sf_mqb_parameter_t *p) { return p ? p->member_count : 0; }
void sf_mqb_parameter_set_member_count(sf_mqb_parameter_t *p, int32_t v) { if (p) p->member_count = v; }

#define CHECK_PARAM(p, typ) do { if (!(p) || (p)->type != (typ)) return SF_ERR_INVALID_ARG; } while (0)
sf_result_t sf_mqb_parameter_set_bool(sf_mqb_parameter_t *p, bool v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_BOOL); p->value.b = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_sbyte(sf_mqb_parameter_t *p, int8_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_SBYTE); p->value.s8 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_byte(sf_mqb_parameter_t *p, uint8_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_BYTE); p->value.u8 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_short(sf_mqb_parameter_t *p, int16_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_SHORT); p->value.s16 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_int(sf_mqb_parameter_t *p, int32_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_INT); p->value.s32 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_uint(sf_mqb_parameter_t *p, uint32_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_UINT); p->value.u32 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_float(sf_mqb_parameter_t *p, float v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_FLOAT); p->value.f32 = v; return SF_OK; }
sf_result_t sf_mqb_parameter_set_string(sf_mqb_parameter_t *p, const char *utf8) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_STRING); return mqb_set_string(p->alloc, &p->value.str, utf8); }
sf_result_t sf_mqb_parameter_set_custom(sf_mqb_parameter_t *p, const void *bytes, size_t size) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_CUSTOM); if (size && !bytes) return SF_ERR_INVALID_ARG; uint8_t *b = NULL; if (size) { b = (uint8_t *)sf_xalloc(p->alloc, size); if (!b) return SF_ERR_OOM; memcpy(b, bytes, size); } sf_xfree(p->alloc, p->value.custom.bytes); p->value.custom.bytes = b; p->value.custom.size = size; return SF_OK; }
sf_result_t sf_mqb_parameter_set_color(sf_mqb_parameter_t *p, sf_color_t c) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_COLOR); p->value.color = c; return SF_OK; }
sf_result_t sf_mqb_parameter_set_int_color(sf_mqb_parameter_t *p, int32_t r, int32_t g, int32_t b, int32_t a) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_INT_COLOR); p->value.int_color[0] = r; p->value.int_color[1] = g; p->value.int_color[2] = b; p->value.int_color[3] = a; return SF_OK; }
sf_result_t sf_mqb_parameter_set_vec2(sf_mqb_parameter_t *p, sf_vec2_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); p->member_count = 2; p->value.vec.x = v.x; p->value.vec.y = v.y; return SF_OK; }
sf_result_t sf_mqb_parameter_set_vec3(sf_mqb_parameter_t *p, sf_vec3_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); p->member_count = 3; p->value.vec.x = v.x; p->value.vec.y = v.y; p->value.vec.z = v.z; return SF_OK; }
sf_result_t sf_mqb_parameter_set_vec4(sf_mqb_parameter_t *p, sf_vec4_t v) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); p->member_count = 4; p->value.vec = v; return SF_OK; }
sf_result_t sf_mqb_parameter_get_bool(const sf_mqb_parameter_t *p, bool *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_BOOL); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.b; return SF_OK; }
sf_result_t sf_mqb_parameter_get_sbyte(const sf_mqb_parameter_t *p, int8_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_SBYTE); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.s8; return SF_OK; }
sf_result_t sf_mqb_parameter_get_byte(const sf_mqb_parameter_t *p, uint8_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_BYTE); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.u8; return SF_OK; }
sf_result_t sf_mqb_parameter_get_short(const sf_mqb_parameter_t *p, int16_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_SHORT); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.s16; return SF_OK; }
sf_result_t sf_mqb_parameter_get_int(const sf_mqb_parameter_t *p, int32_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_INT); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.s32; return SF_OK; }
sf_result_t sf_mqb_parameter_get_uint(const sf_mqb_parameter_t *p, uint32_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_UINT); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.u32; return SF_OK; }
sf_result_t sf_mqb_parameter_get_float(const sf_mqb_parameter_t *p, float *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_FLOAT); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.f32; return SF_OK; }
sf_result_t sf_mqb_parameter_get_string(const sf_mqb_parameter_t *p, const char **out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_STRING); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.str ? p->value.str : ""; return SF_OK; }
sf_result_t sf_mqb_parameter_get_custom(const sf_mqb_parameter_t *p, const void **out_bytes, size_t *out_size) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_CUSTOM); if (!out_bytes || !out_size) return SF_ERR_INVALID_ARG; *out_bytes = p->value.custom.bytes; *out_size = p->value.custom.size; return SF_OK; }
sf_result_t sf_mqb_parameter_get_color(const sf_mqb_parameter_t *p, sf_color_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_COLOR); if (!out) return SF_ERR_INVALID_ARG; *out = p->value.color; return SF_OK; }
sf_result_t sf_mqb_parameter_get_int_color(const sf_mqb_parameter_t *p, int32_t *r, int32_t *g, int32_t *b, int32_t *a) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_INT_COLOR); if (!r || !g || !b || !a) return SF_ERR_INVALID_ARG; *r = p->value.int_color[0]; *g = p->value.int_color[1]; *b = p->value.int_color[2]; *a = p->value.int_color[3]; return SF_OK; }
sf_result_t sf_mqb_parameter_get_vec2(const sf_mqb_parameter_t *p, sf_vec2_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); if (!out || p->member_count != 2) return SF_ERR_INVALID_ARG; out->x = p->value.vec.x; out->y = p->value.vec.y; return SF_OK; }
sf_result_t sf_mqb_parameter_get_vec3(const sf_mqb_parameter_t *p, sf_vec3_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); if (!out || p->member_count != 3) return SF_ERR_INVALID_ARG; out->x = p->value.vec.x; out->y = p->value.vec.y; out->z = p->value.vec.z; return SF_OK; }
sf_result_t sf_mqb_parameter_get_vec4(const sf_mqb_parameter_t *p, sf_vec4_t *out) { CHECK_PARAM(p, SF_MQB_PARAM_TYPE_VECTOR); if (!out || p->member_count != 4) return SF_ERR_INVALID_ARG; *out = p->value.vec; return SF_OK; }

size_t sf_mqb_parameter_sequence_count(const sf_mqb_parameter_t *p) { return p ? p->sequence_count : 0; }
sf_mqb_sequence_t *sf_mqb_parameter_sequence_at(const sf_mqb_parameter_t *p, size_t i) { return (p && i < p->sequence_count) ? p->sequences[i] : NULL; }
sf_result_t sf_mqb_parameter_add_sequence(sf_mqb_parameter_t *p, sf_mqb_sequence_t **out) { return p ? new_sequence(p, out) : SF_ERR_INVALID_ARG; }
sf_mqb_data_type_t sf_mqb_sequence_value_type(const sf_mqb_sequence_t *s) { return s ? s->value_type : 0; }
void sf_mqb_sequence_set_value_type(sf_mqb_sequence_t *s, sf_mqb_data_type_t t) { if (s) s->value_type = t; }
int32_t sf_mqb_sequence_point_type(const sf_mqb_sequence_t *s) { return s ? s->point_type : 0; }
void sf_mqb_sequence_set_point_type(sf_mqb_sequence_t *s, int32_t v) { if (s) s->point_type = v; }
int32_t sf_mqb_sequence_value_index(const sf_mqb_sequence_t *s) { return s ? s->value_index : 0; }
void sf_mqb_sequence_set_value_index(sf_mqb_sequence_t *s, int32_t v) { if (s) s->value_index = v; }
size_t sf_mqb_sequence_point_count(const sf_mqb_sequence_t *s) { return s ? s->point_count : 0; }
sf_mqb_point_t *sf_mqb_sequence_point_at(const sf_mqb_sequence_t *s, size_t i) { return (s && i < s->point_count) ? s->points[i] : NULL; }
sf_result_t sf_mqb_sequence_add_point(sf_mqb_sequence_t *s, sf_mqb_point_t **out) { return s ? new_point(s, out) : SF_ERR_INVALID_ARG; }

sf_result_t sf_mqb_point_set_byte(sf_mqb_point_t *p, uint8_t v) { if (!p) return SF_ERR_INVALID_ARG; p->type = SF_MQB_PARAM_TYPE_BYTE; p->value.u8 = v; return SF_OK; }
sf_result_t sf_mqb_point_set_float(sf_mqb_point_t *p, float v) { if (!p) return SF_ERR_INVALID_ARG; p->type = SF_MQB_PARAM_TYPE_FLOAT; p->value.f32 = v; return SF_OK; }
sf_result_t sf_mqb_point_set_uint(sf_mqb_point_t *p, uint32_t v) { if (!p) return SF_ERR_INVALID_ARG; p->type = SF_MQB_PARAM_TYPE_UINT; p->value.u32 = v; return SF_OK; }
sf_result_t sf_mqb_point_get_byte(const sf_mqb_point_t *p, uint8_t *out) { if (!p || !out || p->type != SF_MQB_PARAM_TYPE_BYTE) return SF_ERR_INVALID_ARG; *out = p->value.u8; return SF_OK; }
sf_result_t sf_mqb_point_get_float(const sf_mqb_point_t *p, float *out) { if (!p || !out || p->type != SF_MQB_PARAM_TYPE_FLOAT) return SF_ERR_INVALID_ARG; *out = p->value.f32; return SF_OK; }
sf_result_t sf_mqb_point_get_uint(const sf_mqb_point_t *p, uint32_t *out) { if (!p || !out || p->type != SF_MQB_PARAM_TYPE_UINT) return SF_ERR_INVALID_ARG; *out = p->value.u32; return SF_OK; }
int32_t sf_mqb_point_unk08(const sf_mqb_point_t *p) { return p ? p->unk08 : 0; }
void sf_mqb_point_set_unk08(sf_mqb_point_t *p, int32_t v) { if (p) p->unk08 = v; }
float sf_mqb_point_unk10(const sf_mqb_point_t *p) { return p ? p->unk10 : 0.0f; }
void sf_mqb_point_set_unk10(sf_mqb_point_t *p, float v) { if (p) p->unk10 = v; }
float sf_mqb_point_unk14(const sf_mqb_point_t *p) { return p ? p->unk14 : 0.0f; }
void sf_mqb_point_set_unk14(sf_mqb_point_t *p, float v) { if (p) p->unk14 = v; }
