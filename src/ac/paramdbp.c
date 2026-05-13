/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_paramdbp.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

struct sf_paramdbp {
    const sf_allocator_t *alloc;
    bool big_endian;
    sf_paramdbp_field_t *fields;
    size_t field_count;
    size_t field_cap;
};

struct sf_dbpparam {
    const sf_allocator_t *alloc;
    bool applied;
    uint8_t *raw_bytes;
    size_t raw_size;
    sf_dbpparam_cell_t *cells;
    size_t cell_count;
    const sf_paramdbp_t *dbp;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static size_t dbp_type_size(sf_dbp_type_t t) {
    switch (t) {
        case SF_DBP_TYPE_S8: case SF_DBP_TYPE_U8: return 1u;
        case SF_DBP_TYPE_S16: case SF_DBP_TYPE_U16: return 2u;
        case SF_DBP_TYPE_S32: case SF_DBP_TYPE_U32: case SF_DBP_TYPE_F32: return 4u;
        default: return 0u;
    }
}

sf_result_t sf_paramdbp_create(sf_paramdbp_t **out, bool big_endian,
                               const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_paramdbp_t *d = (sf_paramdbp_t *)sf_xalloc(alloc, sizeof(*d));
    if (!d) return SF_ERR_OOM;
    memset(d, 0, sizeof(*d));
    d->alloc = alloc;
    d->big_endian = big_endian;
    *out = d;
    return SF_OK;
}

void sf_paramdbp_destroy(sf_paramdbp_t *d) {
    if (!d) return;
    for (size_t i = 0; i < d->field_count; i++) {
        sf_xfree(d->alloc, d->fields[i].display_name);
        sf_xfree(d->alloc, d->fields[i].display_format);
    }
    sf_xfree(d->alloc, d->fields);
    sf_xfree(d->alloc, d);
}

bool sf_paramdbp_is_big_endian(const sf_paramdbp_t *d) { return d ? d->big_endian : true; }
size_t sf_paramdbp_field_count(const sf_paramdbp_t *d) { return d ? d->field_count : 0u; }
const sf_paramdbp_field_t *sf_paramdbp_get_field(const sf_paramdbp_t *d, size_t i) {
    if (!d || i >= d->field_count) return NULL;
    return &d->fields[i];
}

int sf_paramdbp_calculate_param_size(const sf_paramdbp_t *d) {
    if (!d) return 0;
    int sz = 0;
    for (size_t i = 0; i < d->field_count; i++)
        sz += (int)dbp_type_size(d->fields[i].type);
    return sz;
}

sf_result_t sf_paramdbp_add_field(sf_paramdbp_t *d, const sf_paramdbp_field_t *field) {
    SF_CHECK_ARG(d != NULL && field != NULL);
    if (d->field_count >= d->field_cap) {
        size_t nc = d->field_cap == 0 ? 8u : d->field_cap * 2u;
        sf_paramdbp_field_t *nf = (sf_paramdbp_field_t *)sf_xalloc(d->alloc, nc * sizeof(*nf));
        if (!nf) return SF_ERR_OOM;
        if (d->fields) { memcpy(nf, d->fields, d->field_count * sizeof(*nf)); sf_xfree(d->alloc, d->fields); }
        d->fields = nf;
        d->field_cap = nc;
    }
    sf_paramdbp_field_t *dst = &d->fields[d->field_count];
    *dst = *field;
    dst->display_name = dst->display_format = NULL;
    if (field->display_name) {
        size_t n = strlen(field->display_name) + 1u;
        dst->display_name = (char *)sf_xalloc(d->alloc, n);
        if (!dst->display_name) return SF_ERR_OOM;
        memcpy(dst->display_name, field->display_name, n);
    }
    if (field->display_format) {
        size_t n = strlen(field->display_format) + 1u;
        dst->display_format = (char *)sf_xalloc(d->alloc, n);
        if (!dst->display_format) { sf_xfree(d->alloc, dst->display_name); return SF_ERR_OOM; }
        memcpy(dst->display_format, field->display_format, n);
    }
    d->field_count++;
    return SF_OK;
}

static sf_result_t read_dbp_value(sf_binary_reader_t *r, sf_dbp_type_t t, sf_dbp_value_t *v) {
    switch (t) {
        case SF_DBP_TYPE_S8:  return sf_binary_reader_read_i8(r, &v->s8);
        case SF_DBP_TYPE_U8:  return sf_binary_reader_read_u8(r, &v->u8);
        case SF_DBP_TYPE_S16: return sf_binary_reader_read_i16(r, &v->s16);
        case SF_DBP_TYPE_U16: return sf_binary_reader_read_u16(r, &v->u16);
        case SF_DBP_TYPE_S32: return sf_binary_reader_read_i32(r, &v->s32);
        case SF_DBP_TYPE_U32: return sf_binary_reader_read_u32(r, &v->u32);
        case SF_DBP_TYPE_F32: return sf_binary_reader_read_f32(r, &v->f32);
        default: return SF_ERR_INVALID_ARG;
    }
}

static sf_result_t write_dbp_value(sf_binary_writer_t *w, sf_dbp_type_t t, sf_dbp_value_t v) {
    switch (t) {
        case SF_DBP_TYPE_S8:  return sf_binary_writer_write_i8(w, v.s8);
        case SF_DBP_TYPE_U8:  return sf_binary_writer_write_u8(w, v.u8);
        case SF_DBP_TYPE_S16: return sf_binary_writer_write_i16(w, v.s16);
        case SF_DBP_TYPE_U16: return sf_binary_writer_write_u16(w, v.u16);
        case SF_DBP_TYPE_S32: return sf_binary_writer_write_i32(w, v.s32);
        case SF_DBP_TYPE_U32: return sf_binary_writer_write_u32(w, v.u32);
        case SF_DBP_TYPE_F32: return sf_binary_writer_write_f32(w, v.f32);
        default: return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_paramdbp_read_from_memory(sf_paramdbp_t **out, const void *bytes,
                                         size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_paramdbp_t *d = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_reader_create(&r, s, true, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    int32_t field_count = 0;
    e = sf_binary_reader_read_i32(r, &field_count);
    if (e != SF_OK) goto done;

    bool big_endian = true;
    if (field_count < 0 || (size_t)field_count > size) {
        sf_binary_reader_set_big_endian(r, false);
        e = sf_binary_reader_step_in(r, 0);
        if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &field_count);
        sf_binary_reader_step_out(r);
        if (e != SF_OK) goto done;
        big_endian = false;
    }

    e = sf_paramdbp_create(&d, big_endian, alloc);
    if (e != SF_OK) goto done;

    int32_t z = 0;
    e = sf_binary_reader_read_i32(r, &z); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &z); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &z); if (e != SF_OK) goto done;

    for (int32_t i = 0; i < field_count; i++) {
        e = sf_binary_reader_read_i32(r, &z); if (e != SF_OK) goto done;
    }

    sf_paramdbp_field_t *tmp = (sf_paramdbp_field_t *)sf_xalloc(alloc,
        (size_t)field_count * sizeof(*tmp));
    if (!tmp) { e = SF_ERR_OOM; goto done; }
    memset(tmp, 0, (size_t)field_count * sizeof(*tmp));

    for (int32_t i = 0; i < field_count; i++) {
        int32_t type_val = 0;
        e = sf_binary_reader_read_i32(r, &type_val);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        tmp[i].type = (sf_dbp_type_t)type_val;
        int32_t unk1 = 0, unk2 = 0;
        e = sf_binary_reader_read_i32(r, &unk1); if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        e = sf_binary_reader_read_i32(r, &unk2); if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        e = read_dbp_value(r, tmp[i].type, &tmp[i].default_val);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        e = read_dbp_value(r, tmp[i].type, &tmp[i].increment);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        e = read_dbp_value(r, tmp[i].type, &tmp[i].minimum);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        e = read_dbp_value(r, tmp[i].type, &tmp[i].maximum);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
    }

    for (int32_t i = 0; i < field_count; i++) {
        char *name = NULL;
        size_t name_len = 0;
        e = sf_binary_reader_read_shift_jis(r, &name, &name_len);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        tmp[i].display_name = name;

        char *fmt = NULL;
        size_t fmt_len = 0;
        e = sf_binary_reader_read_shift_jis(r, &fmt, &fmt_len);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
        tmp[i].display_format = fmt;
    }

    for (int32_t i = 0; i < field_count; i++) {
        e = sf_paramdbp_add_field(d, &tmp[i]);
        sf_xfree(alloc, tmp[i].display_name);
        sf_xfree(alloc, tmp[i].display_format);
        if (e != SF_OK) { sf_xfree(alloc, tmp); goto done; }
    }
    sf_xfree(alloc, tmp);

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_paramdbp_destroy(d); return e; }
    *out = d;
    return SF_OK;
}

sf_result_t sf_paramdbp_write_to_memory(const sf_paramdbp_t *d, void **out_bytes,
                                        size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(d != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, d->big_endian, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_i32(w, (int32_t)d->field_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    for (size_t i = 0; i < d->field_count; i++) {
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    }
    for (size_t i = 0; i < d->field_count; i++) {
        const sf_paramdbp_field_t *f = &d->fields[i];
        e = sf_binary_writer_write_i32(w, (int32_t)f->type); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
        e = write_dbp_value(w, f->type, f->default_val); if (e != SF_OK) goto done;
        e = write_dbp_value(w, f->type, f->increment); if (e != SF_OK) goto done;
        e = write_dbp_value(w, f->type, f->minimum); if (e != SF_OK) goto done;
        e = write_dbp_value(w, f->type, f->maximum); if (e != SF_OK) goto done;
    }
    for (size_t i = 0; i < d->field_count; i++) {
        const sf_paramdbp_field_t *f = &d->fields[i];
        e = sf_binary_writer_write_shift_jis(w, f->display_name ? f->display_name : "", true);
        if (e != SF_OK) goto done;
        e = sf_binary_writer_write_shift_jis(w, f->display_format ? f->display_format : "", true);
        if (e != SF_OK) goto done;
    }
    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}

sf_result_t sf_dbpparam_create(sf_dbpparam_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_dbpparam_t *p = (sf_dbpparam_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->alloc = alloc;
    *out = p;
    return SF_OK;
}

void sf_dbpparam_destroy(sf_dbpparam_t *p) {
    if (!p) return;
    sf_xfree(p->alloc, p->raw_bytes);
    sf_xfree(p->alloc, p->cells);
    sf_xfree(p->alloc, p);
}

bool sf_dbpparam_is_applied(const sf_dbpparam_t *p) { return p ? p->applied : false; }
size_t sf_dbpparam_cell_count(const sf_dbpparam_t *p) { return p ? p->cell_count : 0u; }
const sf_dbpparam_cell_t *sf_dbpparam_get_cell(const sf_dbpparam_t *p, size_t i) {
    if (!p || i >= p->cell_count) return NULL;
    return &p->cells[i];
}

sf_result_t sf_dbpparam_read_from_memory(sf_dbpparam_t **out, const void *bytes,
                                         size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    sf_dbpparam_t *p = NULL;
    TRY(sf_dbpparam_create(&p, alloc));
    if (size > 0) {
        p->raw_bytes = (uint8_t *)sf_xalloc(p->alloc, size);
        if (!p->raw_bytes) { sf_dbpparam_destroy(p); return SF_ERR_OOM; }
        memcpy(p->raw_bytes, bytes, size);
        p->raw_size = size;
    }
    *out = p;
    return SF_OK;
}

sf_result_t sf_dbpparam_apply_paramdbp(sf_dbpparam_t *p, const sf_paramdbp_t *dbp) {
    SF_CHECK_ARG(p != NULL && dbp != NULL);
    int needed = sf_paramdbp_calculate_param_size(dbp);
    if ((int)p->raw_size < needed) return SF_ERR_INVALID_ARG;

    size_t n = sf_paramdbp_field_count(dbp);
    sf_dbpparam_cell_t *cells = (sf_dbpparam_cell_t *)sf_xalloc(p->alloc, n * sizeof(*cells));
    if (!cells) return SF_ERR_OOM;

    const uint8_t *ptr = p->raw_bytes;
    for (size_t i = 0; i < n; i++) {
        const sf_paramdbp_field_t *f = sf_paramdbp_get_field(dbp, i);
        cells[i].field = f;
        switch (f->type) {
            case SF_DBP_TYPE_S8:  cells[i].value.s8  = (int8_t)*ptr; ptr += 1; break;
            case SF_DBP_TYPE_U8:  cells[i].value.u8  = *ptr; ptr += 1; break;
            case SF_DBP_TYPE_S16: memcpy(&cells[i].value.s16, ptr, 2); ptr += 2; break;
            case SF_DBP_TYPE_U16: memcpy(&cells[i].value.u16, ptr, 2); ptr += 2; break;
            case SF_DBP_TYPE_S32: memcpy(&cells[i].value.s32, ptr, 4); ptr += 4; break;
            case SF_DBP_TYPE_U32: memcpy(&cells[i].value.u32, ptr, 4); ptr += 4; break;
            case SF_DBP_TYPE_F32: memcpy(&cells[i].value.f32, ptr, 4); ptr += 4; break;
            default: sf_xfree(p->alloc, cells); return SF_ERR_INVALID_ARG;
        }
    }

    sf_xfree(p->alloc, p->cells);
    p->cells = cells;
    p->cell_count = n;
    p->dbp = dbp;
    p->applied = true;
    return SF_OK;
}

sf_result_t sf_dbpparam_write_to_memory(const sf_dbpparam_t *p, void **out_bytes,
                                        size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(p != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (!p->applied) {
        if (p->raw_size == 0) { *out_size = 0; return SF_OK; }
        uint8_t *buf = (uint8_t *)sf_xalloc(alloc, p->raw_size);
        if (!buf) return SF_ERR_OOM;
        memcpy(buf, p->raw_bytes, p->raw_size);
        *out_bytes = buf;
        *out_size = p->raw_size;
        return SF_OK;
    }

    int sz = sf_paramdbp_calculate_param_size(p->dbp);
    if (sz <= 0) { *out_size = 0; return SF_OK; }
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, (size_t)sz);
    if (!buf) return SF_ERR_OOM;
    uint8_t *ptr = buf;
    for (size_t i = 0; i < p->cell_count; i++) {
        const sf_dbpparam_cell_t *c = &p->cells[i];
        switch (c->field->type) {
            case SF_DBP_TYPE_S8:  *ptr = (uint8_t)c->value.s8; ptr += 1; break;
            case SF_DBP_TYPE_U8:  *ptr = c->value.u8; ptr += 1; break;
            case SF_DBP_TYPE_S16: memcpy(ptr, &c->value.s16, 2); ptr += 2; break;
            case SF_DBP_TYPE_U16: memcpy(ptr, &c->value.u16, 2); ptr += 2; break;
            case SF_DBP_TYPE_S32: memcpy(ptr, &c->value.s32, 4); ptr += 4; break;
            case SF_DBP_TYPE_U32: memcpy(ptr, &c->value.u32, 4); ptr += 4; break;
            case SF_DBP_TYPE_F32: memcpy(ptr, &c->value.f32, 4); ptr += 4; break;
            default: sf_xfree(alloc, buf); return SF_ERR_INVALID_ARG;
        }
    }
    *out_bytes = buf;
    *out_size = (size_t)sz;
    return SF_OK;
}
