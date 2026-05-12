/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 Rainbow Stone particle effects format implementation.
 * Mirrors SoulsFormats/Formats/FXR3.cs.
 */

#include "souls_formats/sf_fxr3.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct fxr3_reader {
    const uint8_t *data;
    size_t size;
    size_t pos;
} fxr3_reader_t;

struct sf_fxr3_property_modifier {
    uint16_t type_enum_a;
    uint32_t type_enum_b;
    sf_fxr3_field_t *fields;
    size_t field_count;
    sf_fxr3_property_t **properties;
    size_t property_count;
};

struct sf_fxr3_unk_field_list {
    sf_fxr3_field_t *fields;
    size_t field_count;
};

struct sf_fxr3_property {
    sf_fxr3_property_type_t type;
    sf_fxr3_property_interpolation_type_t interpolation;
    bool is_loop;
    sf_fxr3_field_t *fields;
    size_t field_count;
    sf_fxr3_property_modifier_t **modifiers;
    size_t modifier_count;
};

struct sf_fxr3_action {
    int16_t id;
    bool unk02;
    bool unk03;
    int32_t unk04;
    sf_fxr3_field_t *fields1;
    size_t field1_count;
    sf_fxr3_field_t *fields2;
    size_t field2_count;
    sf_fxr3_property_t **properties;
    size_t property1_count;
    size_t property_count;
    sf_fxr3_unk_field_list_t **unk_field_lists;
    size_t unk_field_list_count;
};

struct sf_fxr3_effect {
    int16_t id;
    sf_fxr3_action_t **actions;
    size_t action_count;
};

struct sf_fxr3_container {
    int16_t id;
    sf_fxr3_container_t **children;
    size_t child_count;
    sf_fxr3_effect_t **effects;
    size_t effect_count;
    sf_fxr3_action_t **actions;
    size_t action_count;
};

struct sf_fxr3_state_condition {
    sf_fxr3_operator_type_t operator_type;
    uint8_t unk_modifier;
    sf_fxr3_operand_t left_operand;
    sf_fxr3_operand_t right_operand;
    int32_t next_state;
};

struct sf_fxr3_state {
    sf_fxr3_state_condition_t **conditions;
    size_t condition_count;
};

struct sf_fxr3_state_map {
    sf_fxr3_state_t **states;
    size_t state_count;
};

struct sf_fxr3 {
    const sf_allocator_t *alloc;
    sf_fxr3_version_t version;
    int32_t id;
    sf_fxr3_state_map_t *root_state_map;
    sf_fxr3_container_t *root_container;
    int32_t *references;
    size_t reference_count;
    int32_t *external_values;
    size_t external_value_count;
    int32_t *unk_blood_enablers;
    size_t unk_blood_enabler_count;
    uint8_t *raw_bytes;
    size_t raw_size;
};

static sf_result_t rd_need(const fxr3_reader_t *r, size_t n) {
    if (!r || r->pos > r->size || n > r->size - r->pos) return SF_ERR_TRUNCATED;
    return SF_OK;
}

static sf_result_t rd_seek(fxr3_reader_t *r, int32_t off) {
    if (!r || off < 0 || (size_t)off > r->size) return SF_ERR_TRUNCATED;
    r->pos = (size_t)off;
    return SF_OK;
}

static sf_result_t rd_u8(fxr3_reader_t *r, uint8_t *out) {
    sf_result_t e = rd_need(r, 1);
    if (e != SF_OK) return e;
    *out = r->data[r->pos++];
    return SF_OK;
}

static sf_result_t rd_bool(fxr3_reader_t *r, bool *out) {
    uint8_t v = 0;
    sf_result_t e = rd_u8(r, &v);
    if (e != SF_OK) return e;
    *out = v != 0;
    return SF_OK;
}

static sf_result_t rd_u16(fxr3_reader_t *r, uint16_t *out) {
    sf_result_t e = rd_need(r, 2);
    if (e != SF_OK) return e;
    *out = (uint16_t)r->data[r->pos] | (uint16_t)((uint16_t)r->data[r->pos + 1] << 8);
    r->pos += 2;
    return SF_OK;
}

static sf_result_t rd_i16(fxr3_reader_t *r, int16_t *out) {
    uint16_t v = 0;
    sf_result_t e = rd_u16(r, &v);
    *out = (int16_t)v;
    return e;
}

static sf_result_t rd_u32(fxr3_reader_t *r, uint32_t *out) {
    sf_result_t e = rd_need(r, 4);
    if (e != SF_OK) return e;
    *out = (uint32_t)r->data[r->pos] | ((uint32_t)r->data[r->pos + 1] << 8) |
           ((uint32_t)r->data[r->pos + 2] << 16) | ((uint32_t)r->data[r->pos + 3] << 24);
    r->pos += 4;
    return SF_OK;
}

static sf_result_t rd_i32(fxr3_reader_t *r, int32_t *out) {
    uint32_t v = 0;
    sf_result_t e = rd_u32(r, &v);
    *out = (int32_t)v;
    return e;
}

static sf_result_t rd_f32(fxr3_reader_t *r, float *out) {
    uint32_t bits = 0;
    sf_result_t e = rd_u32(r, &bits);
    if (e != SF_OK) return e;
    memcpy(out, &bits, sizeof(bits));
    return SF_OK;
}

static sf_result_t rd_expect_u8(fxr3_reader_t *r, uint8_t expect) {
    uint8_t v = 0;
    sf_result_t e = rd_u8(r, &v);
    if (e != SF_OK) return e;
    return v == expect ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t rd_expect_i16(fxr3_reader_t *r, int16_t expect) {
    int16_t v = 0;
    sf_result_t e = rd_i16(r, &v);
    if (e != SF_OK) return e;
    return v == expect ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t rd_expect_i32(fxr3_reader_t *r, int32_t expect) {
    int32_t v = 0;
    sf_result_t e = rd_i32(r, &v);
    if (e != SF_OK) return e;
    return v == expect ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t i32_to_size(int32_t v, size_t *out) {
    if (v < 0) return SF_ERR_OUT_OF_RANGE;
    *out = (size_t)v;
    return SF_OK;
}

static void *zalloc(const sf_allocator_t *a, size_t n) {
    void *p = sf_xalloc(a, n == 0 ? 1 : n);
    if (p) memset(p, 0, n == 0 ? 1 : n);
    return p;
}

static sf_result_t alloc_ptrs(const sf_allocator_t *a, size_t count, void ***out) {
    *out = NULL;
    if (count == 0) return SF_OK;
    if (count > SIZE_MAX / sizeof(void *)) return SF_ERR_OUT_OF_RANGE;
    *out = (void **)zalloc(a, count * sizeof(void *));
    return *out ? SF_OK : SF_ERR_OOM;
}

static sf_result_t alloc_fields(const sf_allocator_t *a, size_t count, sf_fxr3_field_t **out) {
    *out = NULL;
    if (count == 0) return SF_OK;
    if (count > SIZE_MAX / sizeof(sf_fxr3_field_t)) return SF_ERR_OUT_OF_RANGE;
    *out = (sf_fxr3_field_t *)zalloc(a, count * sizeof(sf_fxr3_field_t));
    return *out ? SF_OK : SF_ERR_OOM;
}

static sf_result_t read_field(fxr3_reader_t *r, const sf_fxr3_property_t *prop, size_t index,
                              sf_fxr3_field_t *out) {
    if (prop != NULL) {
        if (prop->interpolation == SF_FXR3_INTERP_UNK_AC6) {
            if (index > 0 && index <= (size_t)(prop->type + 1)) {
                out->type = SF_FXR3_FIELD_TYPE_INT;
                return rd_i32(r, &out->value.as_int);
            }
        } else if (prop->interpolation != SF_FXR3_INTERP_CONSTANT && index == 0) {
            out->type = SF_FXR3_FIELD_TYPE_INT;
            return rd_i32(r, &out->value.as_int);
        }
    }
    fxr3_reader_t peek_reader = *r;
    float peek = 0.0f;
    sf_result_t e = rd_f32(&peek_reader, &peek);
    if (e != SF_OK) return e;
    float abs_val = peek < 0.0f ? -peek : peek;
    if (abs_val >= 1e-4f && abs_val < 1e6f) {
        out->type = SF_FXR3_FIELD_TYPE_FLOAT;
        return rd_f32(r, &out->value.as_float);
    }
    out->type = SF_FXR3_FIELD_TYPE_INT;
    return rd_i32(r, &out->value.as_int);
}

static sf_result_t read_fields_at(fxr3_reader_t *r, int32_t offset, size_t count,
                                  const sf_fxr3_property_t *prop, sf_fxr3_field_t **out,
                                  const sf_allocator_t *a) {
    sf_result_t e = alloc_fields(a, count, out);
    if (e != SF_OK || count == 0) return e;
    fxr3_reader_t rr = *r;
    e = rd_seek(&rr, offset);
    if (e != SF_OK) return e;
    for (size_t i = 0; i < count; i++) {
        e = read_field(&rr, prop, i, &(*out)[i]);
        if (e != SF_OK) return e;
    }
    return SF_OK;
}

static sf_result_t read_state_map(fxr3_reader_t *r, sf_fxr3_state_map_t **out,
                                  const sf_allocator_t *a);
static sf_result_t read_state(fxr3_reader_t *r, sf_fxr3_state_t **out, const sf_allocator_t *a);
static sf_result_t read_condition(fxr3_reader_t *r, sf_fxr3_state_condition_t **out,
                                  const sf_allocator_t *a);
static sf_result_t read_container(fxr3_reader_t *r, sf_fxr3_container_t **out,
                                  const sf_allocator_t *a);
static sf_result_t read_effect(fxr3_reader_t *r, sf_fxr3_effect_t **out, const sf_allocator_t *a);
static sf_result_t read_action(fxr3_reader_t *r, sf_fxr3_action_t **out, const sf_allocator_t *a);
static sf_result_t read_property(fxr3_reader_t *r, bool conditional, sf_fxr3_property_t **out,
                                 const sf_allocator_t *a);
static sf_result_t read_modifier(fxr3_reader_t *r, sf_fxr3_property_modifier_t **out,
                                 const sf_allocator_t *a);
static sf_result_t read_unk_field_list(fxr3_reader_t *r, sf_fxr3_unk_field_list_t **out,
                                       const sf_allocator_t *a);

static sf_result_t read_operand_at(fxr3_reader_t *r, sf_fxr3_operand_type_t type, bool has_field,
                                   int32_t offset, sf_fxr3_operand_t *out) {
    memset(out, 0, sizeof(*out));
    out->type = type;
    if (!has_field) return (type == SF_FXR3_OPERAND_LITERAL || type == SF_FXR3_OPERAND_EXTERNAL)
                           ? SF_ERR_INVALID_ARG
                           : SF_OK;
    fxr3_reader_t rr = *r;
    sf_result_t e = rd_seek(&rr, offset);
    if (e != SF_OK) return e;
    if (type == SF_FXR3_OPERAND_LITERAL) return rd_f32(&rr, &out->value.as_literal);
    if (type == SF_FXR3_OPERAND_EXTERNAL) return rd_i32(&rr, &out->value.as_external);
    return SF_ERR_INVALID_ARG;
}

static sf_result_t read_operand_type(fxr3_reader_t *r, sf_fxr3_operand_type_t *out) {
    int16_t raw = 0;
    sf_result_t e = rd_i16(r, &raw);
    if (e != SF_OK) return e;
    if (raw < (int16_t)SF_FXR3_OPERAND_LITERAL || raw > (int16_t)SF_FXR3_OPERAND_STATE_TIME) {
        return SF_ERR_INVALID_ARG;
    }
    *out = (sf_fxr3_operand_type_t)raw;
    return SF_OK;
}

static sf_result_t read_condition(fxr3_reader_t *r, sf_fxr3_state_condition_t **out,
                                  const sf_allocator_t *a) {
    sf_fxr3_state_condition_t *c = (sf_fxr3_state_condition_t *)zalloc(a, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    int16_t op = 0;
    sf_result_t e = rd_i16(r, &op);
    if (e != SF_OK) goto fail;
    c->operator_type = (sf_fxr3_operator_type_t)(op & 3);
    c->unk_modifier = (uint8_t)((op >> 2) & 3);
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &c->next_state); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    sf_fxr3_operand_type_t lt = SF_FXR3_OPERAND_STATE_TIME;
    e = read_operand_type(r, &lt); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    int32_t has_left = 0, left_offset = 0;
    e = rd_i32(r, &has_left); if (e != SF_OK) goto fail;
    if (has_left != 0 && has_left != 1) { e = SF_ERR_INVALID_ARG; goto fail; }
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &left_offset); if (e != SF_OK) goto fail;
    for (size_t i = 0; i < 5; i++) { e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail; }
    sf_fxr3_operand_type_t rt = SF_FXR3_OPERAND_STATE_TIME;
    e = read_operand_type(r, &rt); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    int32_t has_right = 0, right_offset = 0;
    e = rd_i32(r, &has_right); if (e != SF_OK) goto fail;
    if (has_right != 0 && has_right != 1) { e = SF_ERR_INVALID_ARG; goto fail; }
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &right_offset); if (e != SF_OK) goto fail;
    for (size_t i = 0; i < 5; i++) { e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail; }
    e = read_operand_at(r, lt, has_left == 1, left_offset, &c->left_operand); if (e != SF_OK) goto fail;
    e = read_operand_at(r, rt, has_right == 1, right_offset, &c->right_operand); if (e != SF_OK) goto fail;
    if (c->left_operand.type == SF_FXR3_OPERAND_LITERAL &&
        c->right_operand.type != SF_FXR3_OPERAND_LITERAL) {
        sf_fxr3_operand_t tmp = c->left_operand;
        c->left_operand = c->right_operand;
        c->right_operand = tmp;
        if (c->operator_type == SF_FXR3_OPERATOR_GT) c->operator_type = SF_FXR3_OPERATOR_LT;
        else if (c->operator_type == SF_FXR3_OPERATOR_GE) c->operator_type = SF_FXR3_OPERATOR_LE;
    }
    *out = c;
    return SF_OK;
fail:
    sf_xfree(a, c);
    return e;
}

static sf_result_t read_state(fxr3_reader_t *r, sf_fxr3_state_t **out, const sf_allocator_t *a) {
    sf_fxr3_state_t *s = (sf_fxr3_state_t *)zalloc(a, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    int32_t count = 0, offset = 0;
    sf_result_t e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(count, &s->condition_count); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, s->condition_count, (void ***)&s->conditions); if (e != SF_OK) goto fail;
    fxr3_reader_t rr = *r;
    if (s->condition_count > 0) { e = rd_seek(&rr, offset); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < s->condition_count; i++) { e = read_condition(&rr, &s->conditions[i], a); if (e != SF_OK) goto fail; }
    *out = s;
    return SF_OK;
fail:
    sf_xfree(a, s->conditions); sf_xfree(a, s); return e;
}

static sf_result_t read_state_map(fxr3_reader_t *r, sf_fxr3_state_map_t **out,
                                  const sf_allocator_t *a) {
    sf_fxr3_state_map_t *m = (sf_fxr3_state_map_t *)zalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    int32_t count = 0, offset = 0;
    sf_result_t e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(count, &m->state_count); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, m->state_count, (void ***)&m->states); if (e != SF_OK) goto fail;
    fxr3_reader_t rr = *r;
    if (m->state_count > 0) { e = rd_seek(&rr, offset); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < m->state_count; i++) { e = read_state(&rr, &m->states[i], a); if (e != SF_OK) goto fail; }
    *out = m;
    return SF_OK;
fail:
    sf_xfree(a, m->states); sf_xfree(a, m); return e;
}

static sf_result_t read_property(fxr3_reader_t *r, bool conditional, sf_fxr3_property_t **out,
                                 const sf_allocator_t *a) {
    sf_fxr3_property_t *p = (sf_fxr3_property_t *)zalloc(a, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    int16_t type_a = 0;
    int32_t field_count = 0, field_offset = 0;
    sf_result_t e = rd_i16(r, &type_a); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    p->type = (sf_fxr3_property_type_t)(type_a & 3);
    p->interpolation = (sf_fxr3_property_interpolation_type_t)((type_a & 0xF0) >> 4);
    p->is_loop = ((type_a & 0x1000) >> 12) != 0;
    int32_t ignored = 0;
    e = rd_i32(r, &ignored); if (e != SF_OK) goto fail;
    e = rd_i32(r, &field_count); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &field_offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(field_count, &p->field_count); if (e != SF_OK) goto fail;
    if (!conditional) {
        int32_t mod_offset = 0, mod_count = 0;
        e = rd_i32(r, &mod_offset); if (e != SF_OK) goto fail;
        e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
        e = rd_i32(r, &mod_count); if (e != SF_OK) goto fail;
        e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
        e = i32_to_size(mod_count, &p->modifier_count); if (e != SF_OK) goto fail;
        e = alloc_ptrs(a, p->modifier_count, (void ***)&p->modifiers); if (e != SF_OK) goto fail;
        fxr3_reader_t mr = *r;
        if (p->modifier_count > 0) { e = rd_seek(&mr, mod_offset); if (e != SF_OK) goto fail; }
        for (size_t i = 0; i < p->modifier_count; i++) { e = read_modifier(&mr, &p->modifiers[i], a); if (e != SF_OK) goto fail; }
    }
    e = read_fields_at(r, field_offset, p->field_count, p, &p->fields, a); if (e != SF_OK) goto fail;
    *out = p;
    return SF_OK;
fail:
    sf_xfree(a, p->fields); sf_xfree(a, p->modifiers); sf_xfree(a, p); return e;
}

static sf_result_t read_modifier(fxr3_reader_t *r, sf_fxr3_property_modifier_t **out,
                                 const sf_allocator_t *a) {
    sf_fxr3_property_modifier_t *m = (sf_fxr3_property_modifier_t *)zalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    int32_t field_count = 0, prop_count = 0, field_offset = 0, prop_offset = 0;
    sf_result_t e = rd_u16(r, &m->type_enum_a); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_u32(r, &m->type_enum_b); if (e != SF_OK) goto fail;
    e = rd_i32(r, &field_count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &prop_count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &field_offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &prop_offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(field_count, &m->field_count); if (e != SF_OK) goto fail;
    e = i32_to_size(prop_count, &m->property_count); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, m->property_count, (void ***)&m->properties); if (e != SF_OK) goto fail;
    fxr3_reader_t pr = *r;
    if (m->property_count > 0) { e = rd_seek(&pr, prop_offset); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < m->property_count; i++) { e = read_property(&pr, true, &m->properties[i], a); if (e != SF_OK) goto fail; }
    e = read_fields_at(r, field_offset, m->field_count, NULL, &m->fields, a); if (e != SF_OK) goto fail;
    *out = m;
    return SF_OK;
fail:
    sf_xfree(a, m->fields); sf_xfree(a, m->properties); sf_xfree(a, m); return e;
}

static sf_result_t read_unk_field_list(fxr3_reader_t *r, sf_fxr3_unk_field_list_t **out,
                                       const sf_allocator_t *a) {
    sf_fxr3_unk_field_list_t *l = (sf_fxr3_unk_field_list_t *)zalloc(a, sizeof(*l));
    if (!l) return SF_ERR_OOM;
    int32_t offset = 0, count = 0;
    sf_result_t e = rd_i32(r, &offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &count); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(count, &l->field_count); if (e != SF_OK) goto fail;
    e = read_fields_at(r, offset, l->field_count, NULL, &l->fields, a); if (e != SF_OK) goto fail;
    *out = l;
    return SF_OK;
fail:
    sf_xfree(a, l->fields); sf_xfree(a, l); return e;
}

static sf_result_t read_action(fxr3_reader_t *r, sf_fxr3_action_t **out, const sf_allocator_t *a) {
    sf_fxr3_action_t *act = (sf_fxr3_action_t *)zalloc(a, sizeof(*act));
    if (!act) return SF_ERR_OOM;
    int32_t f1c = 0, f2c = 0, ulc = 0, p1c = 0, p2c = 0, field_off = 0, ul_off = 0, prop_off = 0;
    sf_result_t e = rd_i16(r, &act->id); if (e != SF_OK) goto fail;
    e = rd_bool(r, &act->unk02); if (e != SF_OK) goto fail;
    e = rd_bool(r, &act->unk03); if (e != SF_OK) goto fail;
    e = rd_i32(r, &act->unk04); if (e != SF_OK) goto fail;
    e = rd_i32(r, &f1c); if (e != SF_OK) goto fail;
    e = rd_i32(r, &ulc); if (e != SF_OK) goto fail;
    e = rd_i32(r, &p1c); if (e != SF_OK) goto fail;
    e = rd_i32(r, &f2c); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &p2c); if (e != SF_OK) goto fail;
    e = rd_i32(r, &field_off); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &ul_off); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &prop_off); if (e != SF_OK) goto fail;
    for (size_t i = 0; i < 3; i++) { e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail; }
    e = i32_to_size(f1c, &act->field1_count); if (e != SF_OK) goto fail;
    e = i32_to_size(f2c, &act->field2_count); if (e != SF_OK) goto fail;
    e = i32_to_size(ulc, &act->unk_field_list_count); if (e != SF_OK) goto fail;
    e = i32_to_size(p1c, &act->property1_count); if (e != SF_OK) goto fail;
    size_t p2 = 0; e = i32_to_size(p2c, &p2); if (e != SF_OK) goto fail;
    if (act->property1_count > SIZE_MAX - p2) { e = SF_ERR_OUT_OF_RANGE; goto fail; }
    act->property_count = act->property1_count + p2;
    e = alloc_ptrs(a, act->property_count, (void ***)&act->properties); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, act->unk_field_list_count, (void ***)&act->unk_field_lists); if (e != SF_OK) goto fail;
    fxr3_reader_t pr = *r;
    if (act->property_count > 0) { e = rd_seek(&pr, prop_off); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < act->property_count; i++) { e = read_property(&pr, false, &act->properties[i], a); if (e != SF_OK) goto fail; }
    fxr3_reader_t ur = *r;
    if (act->unk_field_list_count > 0) { e = rd_seek(&ur, ul_off); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < act->unk_field_list_count; i++) { e = read_unk_field_list(&ur, &act->unk_field_lists[i], a); if (e != SF_OK) goto fail; }
    fxr3_reader_t fr = *r;
    if (act->field1_count > 0 || act->field2_count > 0) { e = rd_seek(&fr, field_off); if (e != SF_OK) goto fail; }
    e = alloc_fields(a, act->field1_count, &act->fields1); if (e != SF_OK) goto fail;
    for (size_t i = 0; i < act->field1_count; i++) { e = read_field(&fr, NULL, i, &act->fields1[i]); if (e != SF_OK) goto fail; }
    e = alloc_fields(a, act->field2_count, &act->fields2); if (e != SF_OK) goto fail;
    for (size_t i = 0; i < act->field2_count; i++) { e = read_field(&fr, NULL, i, &act->fields2[i]); if (e != SF_OK) goto fail; }
    *out = act;
    return SF_OK;
fail:
    sf_xfree(a, act->fields1); sf_xfree(a, act->fields2); sf_xfree(a, act->properties); sf_xfree(a, act->unk_field_lists); sf_xfree(a, act); return e;
}

static sf_result_t read_effect(fxr3_reader_t *r, sf_fxr3_effect_t **out, const sf_allocator_t *a) {
    sf_fxr3_effect_t *eobj = (sf_fxr3_effect_t *)zalloc(a, sizeof(*eobj));
    if (!eobj) return SF_ERR_OOM;
    int32_t count = 0, offset = 0;
    sf_result_t e = rd_i16(r, &eobj->id); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &count); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &offset); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(count, &eobj->action_count); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, eobj->action_count, (void ***)&eobj->actions); if (e != SF_OK) goto fail;
    fxr3_reader_t ar = *r;
    if (eobj->action_count > 0) { e = rd_seek(&ar, offset); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < eobj->action_count; i++) { e = read_action(&ar, &eobj->actions[i], a); if (e != SF_OK) goto fail; }
    *out = eobj;
    return SF_OK;
fail:
    sf_xfree(a, eobj->actions); sf_xfree(a, eobj); return e;
}

static sf_result_t read_container(fxr3_reader_t *r, sf_fxr3_container_t **out,
                                  const sf_allocator_t *a) {
    sf_fxr3_container_t *c = (sf_fxr3_container_t *)zalloc(a, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    int32_t effect_count = 0, action_count = 0, child_count = 0;
    int32_t effect_off = 0, action_off = 0, child_off = 0;
    sf_result_t e = rd_i16(r, &c->id); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 0); if (e != SF_OK) goto fail;
    e = rd_expect_u8(r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &effect_count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &action_count); if (e != SF_OK) goto fail;
    e = rd_i32(r, &child_count); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &effect_off); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &action_off); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = rd_i32(r, &child_off); if (e != SF_OK) goto fail;
    e = rd_expect_i32(r, 0); if (e != SF_OK) goto fail;
    e = i32_to_size(effect_count, &c->effect_count); if (e != SF_OK) goto fail;
    e = i32_to_size(action_count, &c->action_count); if (e != SF_OK) goto fail;
    e = i32_to_size(child_count, &c->child_count); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, c->child_count, (void ***)&c->children); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, c->effect_count, (void ***)&c->effects); if (e != SF_OK) goto fail;
    e = alloc_ptrs(a, c->action_count, (void ***)&c->actions); if (e != SF_OK) goto fail;
    fxr3_reader_t cr = *r;
    if (c->child_count > 0) { e = rd_seek(&cr, child_off); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < c->child_count; i++) { e = read_container(&cr, &c->children[i], a); if (e != SF_OK) goto fail; }
    fxr3_reader_t er = *r;
    if (c->effect_count > 0) { e = rd_seek(&er, effect_off); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < c->effect_count; i++) { e = read_effect(&er, &c->effects[i], a); if (e != SF_OK) goto fail; }
    fxr3_reader_t ar = *r;
    if (c->action_count > 0) { e = rd_seek(&ar, action_off); if (e != SF_OK) goto fail; }
    for (size_t i = 0; i < c->action_count; i++) { e = read_action(&ar, &c->actions[i], a); if (e != SF_OK) goto fail; }
    *out = c;
    return SF_OK;
fail:
    sf_xfree(a, c->children); sf_xfree(a, c->effects); sf_xfree(a, c->actions); sf_xfree(a, c); return e;
}

static sf_result_t read_i32_array(fxr3_reader_t *r, int32_t offset, int32_t count_i32,
                                  int32_t **out, size_t *out_count, const sf_allocator_t *a) {
    *out = NULL;
    *out_count = 0;
    size_t count = 0;
    sf_result_t e = i32_to_size(count_i32, &count);
    if (e != SF_OK) return e;
    if (count == 0) return SF_OK;
    if (count > SIZE_MAX / sizeof(int32_t)) return SF_ERR_OUT_OF_RANGE;
    int32_t *items = (int32_t *)sf_xalloc(a, count * sizeof(int32_t));
    if (!items) return SF_ERR_OOM;
    fxr3_reader_t rr = *r;
    e = rd_seek(&rr, offset);
    if (e != SF_OK) { sf_xfree(a, items); return e; }
    for (size_t i = 0; i < count; i++) {
        e = rd_i32(&rr, &items[i]);
        if (e != SF_OK) { sf_xfree(a, items); return e; }
    }
    *out = items;
    *out_count = count;
    return SF_OK;
}

SF_API sf_result_t sf_fxr3_read_from_memory(sf_fxr3_t **out, const void *bytes, size_t size,
                                            const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    a = sf_alloc_or_default(a);
    fxr3_reader_t r = {(const uint8_t *)bytes, size, 0};
    sf_fxr3_t *f = (sf_fxr3_t *)zalloc(a, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    f->alloc = a;
    if (size > 0) {
        f->raw_bytes = (uint8_t *)sf_xalloc(a, size);
        if (!f->raw_bytes) { sf_xfree(a, f); return SF_ERR_OOM; }
        memcpy(f->raw_bytes, bytes, size);
        f->raw_size = size;
    }
    sf_result_t e = rd_need(&r, 4);
    if (e != SF_OK) goto fail;
    if (memcmp(r.data, "FXR\0", 4) != 0) { e = SF_ERR_BAD_MAGIC; goto fail; }
    r.pos = 4;
    e = rd_expect_i16(&r, 0); if (e != SF_OK) goto fail;
    uint16_t version = 0;
    e = rd_u16(&r, &version); if (e != SF_OK) goto fail;
    if (version != SF_FXR3_VERSION_DARK_SOULS_3 && version != SF_FXR3_VERSION_SEKIRO) { e = SF_ERR_UNSUPPORTED_VERSION; goto fail; }
    f->version = (sf_fxr3_version_t)version;
    e = rd_expect_i32(&r, 1); if (e != SF_OK) goto fail;
    e = rd_i32(&r, &f->id); if (e != SF_OK) goto fail;
    int32_t offsets[11] = {0};
    int32_t counts[11] = {0};
    for (size_t i = 0; i < 11; i++) { e = rd_i32(&r, &offsets[i]); if (e != SF_OK) goto fail; e = rd_i32(&r, &counts[i]); if (e != SF_OK) goto fail; }
    if (counts[0] != 1) { e = SF_ERR_INVALID_ARG; goto fail; }
    e = rd_expect_i32(&r, 1); if (e != SF_OK) goto fail;
    e = rd_expect_i32(&r, 0); if (e != SF_OK) goto fail;
    if (f->version == SF_FXR3_VERSION_SEKIRO) {
        int32_t ref_off = 0, ref_count = 0, ext_off = 0, ext_count = 0, blood_off = 0, blood_count = 0, ignored = 0;
        e = rd_i32(&r, &ref_off); if (e != SF_OK) goto fail; e = rd_i32(&r, &ref_count); if (e != SF_OK) goto fail;
        e = rd_i32(&r, &ext_off); if (e != SF_OK) goto fail; e = rd_i32(&r, &ext_count); if (e != SF_OK) goto fail;
        e = rd_i32(&r, &blood_off); if (e != SF_OK) goto fail; e = rd_i32(&r, &blood_count); if (e != SF_OK) goto fail;
        e = rd_i32(&r, &ignored); if (e != SF_OK) goto fail; (void)ignored;
        e = rd_expect_i32(&r, 0); if (e != SF_OK) goto fail;
        e = read_i32_array(&r, ref_off, ref_count, &f->references, &f->reference_count, a); if (e != SF_OK) goto fail;
        e = read_i32_array(&r, ext_off, ext_count, &f->external_values, &f->external_value_count, a); if (e != SF_OK) goto fail;
        e = read_i32_array(&r, blood_off, blood_count, &f->unk_blood_enablers, &f->unk_blood_enabler_count, a); if (e != SF_OK) goto fail;
    }
    fxr3_reader_t sm = r;
    e = rd_seek(&sm, offsets[0]); if (e != SF_OK) goto fail;
    e = read_state_map(&sm, &f->root_state_map, a); if (e != SF_OK) goto fail;
    fxr3_reader_t cont = r;
    e = rd_seek(&cont, offsets[3]); if (e != SF_OK) goto fail;
    e = read_container(&cont, &f->root_container, a); if (e != SF_OK) goto fail;
    *out = f;
    return SF_OK;
fail:
    sf_fxr3_destroy(f);
    return e;
}

typedef struct fxr3_vec {
    const void **items;
    size_t count;
    size_t capacity;
    const sf_allocator_t *alloc;
} fxr3_vec_t;

static void vec_free(fxr3_vec_t *v) {
    if (!v) return;
    sf_xfree(v->alloc, v->items);
    v->items = NULL;
    v->count = 0;
    v->capacity = 0;
}

static sf_result_t vec_push(fxr3_vec_t *v, const void *item) {
    if (v->count == v->capacity) {
        size_t new_capacity = v->capacity == 0 ? 16 : v->capacity * 2;
        if (new_capacity < v->capacity) return SF_ERR_OUT_OF_RANGE;
        if (new_capacity > SIZE_MAX / sizeof(*v->items)) return SF_ERR_OUT_OF_RANGE;
        const void **items = (const void **)sf_xrealloc(v->alloc, (void *)v->items,
                                                        v->capacity * sizeof(*v->items),
                                                        new_capacity * sizeof(*v->items));
        if (!items) return SF_ERR_OOM;
        v->items = items;
        v->capacity = new_capacity;
    }
    v->items[v->count++] = item;
    return SF_OK;
}

static sf_result_t count_to_i32(size_t value, int32_t *out) {
    if (value > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t pos_to_i32(int64_t value, int32_t *out) {
    if (value < 0 || value > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t fill_pos(sf_binary_writer_t *bw, const char *name) {
    int32_t pos = 0;
    sf_result_t e = pos_to_i32(sf_binary_writer_position(bw), &pos);
    if (e != SF_OK) return e;
    return sf_binary_writer_fill_i32(bw, name, pos);
}

static sf_result_t make_name(char *buf, size_t len, const char *fmt, size_t index) {
    int n = snprintf(buf, len, fmt, index);
    return (n < 0 || (size_t)n >= len) ? SF_ERR_OUT_OF_RANGE : SF_OK;
}

static sf_result_t reservef(sf_binary_writer_t *bw, const char *fmt, size_t index) {
    char name[96];
    sf_result_t e = make_name(name, sizeof(name), fmt, index);
    if (e != SF_OK) return e;
    return sf_binary_writer_reserve_i32(bw, name);
}

static sf_result_t fillf(sf_binary_writer_t *bw, const char *fmt, size_t index, int32_t value) {
    char name[96];
    sf_result_t e = make_name(name, sizeof(name), fmt, index);
    if (e != SF_OK) return e;
    return sf_binary_writer_fill_i32(bw, name, value);
}

static sf_result_t fillf_pos(sf_binary_writer_t *bw, const char *fmt, size_t index) {
    int32_t pos = 0;
    sf_result_t e = pos_to_i32(sf_binary_writer_position(bw), &pos);
    if (e != SF_OK) return e;
    return fillf(bw, fmt, index, pos);
}

static bool operand_has_value(sf_fxr3_operand_t op) {
    return op.type == SF_FXR3_OPERAND_LITERAL || op.type == SF_FXR3_OPERAND_EXTERNAL;
}

static sf_result_t write_field(sf_binary_writer_t *bw, sf_fxr3_field_t field) {
    if (field.type == SF_FXR3_FIELD_TYPE_FLOAT) {
        return sf_binary_writer_write_f32(bw, field.value.as_float);
    }
    return sf_binary_writer_write_i32(bw, field.value.as_int);
}

static sf_result_t write_operand_field(sf_binary_writer_t *bw, sf_fxr3_operand_t op) {
    if (op.type == SF_FXR3_OPERAND_LITERAL) return sf_binary_writer_write_f32(bw, op.value.as_literal);
    if (op.type == SF_FXR3_OPERAND_EXTERNAL) return sf_binary_writer_write_i32(bw, op.value.as_external);
    return SF_ERR_INVALID_ARG;
}

static sf_result_t write_fields(sf_binary_writer_t *bw, const sf_fxr3_field_t *fields,
                                size_t count, int32_t *field_count) {
    for (size_t i = 0; i < count; i++) {
        sf_result_t e = write_field(bw, fields[i]);
        if (e != SF_OK) return e;
    }
    if (count > (size_t)(INT32_MAX - *field_count)) return SF_ERR_OUT_OF_RANGE;
    *field_count += (int32_t)count;
    return SF_OK;
}

static sf_result_t write_state_map_header(sf_binary_writer_t *bw, const sf_fxr3_state_map_t *m) {
    int32_t count = 0;
    sf_result_t e = count_to_i32(m ? m->state_count : 0, &count);
    if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) return e;
    e = sf_binary_writer_reserve_i32(bw, "StateMapStatesOffset"); if (e != SF_OK) return e;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_state_header(sf_binary_writer_t *bw, const sf_fxr3_state_t *s,
                                      size_t index) {
    int32_t count = 0;
    sf_result_t e = count_to_i32(s ? s->condition_count : 0, &count);
    if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) return e;
    e = reservef(bw, "StateTransitionsOffset[%zu]", index); if (e != SF_OK) return e;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_condition_header(sf_binary_writer_t *bw,
                                          const sf_fxr3_state_condition_t *c,
                                          fxr3_vec_t *transitions) {
    sf_fxr3_operand_t left = c->left_operand;
    sf_fxr3_operand_t right = c->right_operand;
    sf_fxr3_operator_type_t op_type = c->operator_type;
    if (op_type >= SF_FXR3_OPERATOR_LE) {
        left = c->right_operand;
        right = c->left_operand;
        op_type = op_type == SF_FXR3_OPERATOR_LE ? SF_FXR3_OPERATOR_GE : SF_FXR3_OPERATOR_GT;
    }
    size_t index = transitions->count;
    int16_t packed_op = (int16_t)((uint8_t)op_type | ((c->unk_modifier << 2) & 0x0C));
    sf_result_t e = sf_binary_writer_write_i16(bw, packed_op); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, c->next_state); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i16(bw, (int16_t)left.type); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, operand_has_value(left) ? 1 : 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "TransitionFieldOffset1[%zu]", index); if (e != SF_OK) return e;
    for (size_t i = 0; i < 5; i++) { e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e; }
    e = sf_binary_writer_write_i16(bw, (int16_t)right.type); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, operand_has_value(right) ? 1 : 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "TransitionFieldOffset2[%zu]", index); if (e != SF_OK) return e;
    for (size_t i = 0; i < 5; i++) { e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e; }
    return vec_push(transitions, c);
}

static sf_result_t write_container_header(sf_binary_writer_t *bw, const sf_fxr3_container_t *c,
                                          fxr3_vec_t *containers) {
    size_t index = containers->count;
    int32_t child_count = 0, effect_count = 0, action_count = 0;
    sf_result_t e = count_to_i32(c->child_count, &child_count); if (e != SF_OK) return e;
    e = count_to_i32(c->effect_count, &effect_count); if (e != SF_OK) return e;
    e = count_to_i32(c->action_count, &action_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i16(bw, c->id); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, effect_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, action_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, child_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ContainerEffectsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ContainerActionsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ContainerChildContainersOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    return vec_push(containers, c);
}

static sf_result_t write_container_children(sf_binary_writer_t *bw, const sf_fxr3_container_t *c,
                                            fxr3_vec_t *containers) {
    size_t index = 0;
    while (index < containers->count && containers->items[index] != c) index++;
    if (index == containers->count) return SF_ERR_INTERNAL;
    sf_result_t e = SF_OK;
    if (c->child_count == 0) {
        e = fillf(bw, "ContainerChildContainersOffset[%zu]", index, 0); if (e != SF_OK) return e;
    } else {
        e = fillf_pos(bw, "ContainerChildContainersOffset[%zu]", index); if (e != SF_OK) return e;
        for (size_t i = 0; i < c->child_count; i++) { e = write_container_header(bw, c->children[i], containers); if (e != SF_OK) return e; }
        for (size_t i = 0; i < c->child_count; i++) { e = write_container_children(bw, c->children[i], containers); if (e != SF_OK) return e; }
    }
    return SF_OK;
}

static sf_result_t write_effect_header(sf_binary_writer_t *bw, const sf_fxr3_effect_t *eobj,
                                       size_t index) {
    int32_t action_count = 0;
    sf_result_t e = count_to_i32(eobj->action_count, &action_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i16(bw, eobj->id); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, action_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "EffectActionsOffset[%zu]", index); if (e != SF_OK) return e;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_action_header(sf_binary_writer_t *bw, const sf_fxr3_action_t *act,
                                       fxr3_vec_t *actions) {
    size_t index = actions->count;
    int32_t f1 = 0, f2 = 0, ul = 0, p1 = 0, p2 = 0;
    sf_result_t e = count_to_i32(act->field1_count, &f1); if (e != SF_OK) return e;
    e = count_to_i32(act->field2_count, &f2); if (e != SF_OK) return e;
    e = count_to_i32(act->unk_field_list_count, &ul); if (e != SF_OK) return e;
    e = count_to_i32(act->property1_count, &p1); if (e != SF_OK) return e;
    if (act->property1_count > act->property_count) return SF_ERR_INVALID_ARG;
    e = count_to_i32(act->property_count - act->property1_count, &p2); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i16(bw, act->id); if (e != SF_OK) return e;
    e = sf_binary_writer_write_bool(bw, act->unk02); if (e != SF_OK) return e;
    e = sf_binary_writer_write_bool(bw, act->unk03); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, act->unk04); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, f1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, ul); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, p1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, f2); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, p2); if (e != SF_OK) return e;
    e = reservef(bw, "ActionFieldsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ActionUnkFieldListsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ActionPropertiesOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    return vec_push(actions, act);
}

static sf_result_t write_property_header(sf_binary_writer_t *bw, const sf_fxr3_property_t *p,
                                         bool conditional, fxr3_vec_t *properties) {
    size_t index = properties->count;
    int32_t field_count = 0, modifier_count = 0;
    sf_result_t e = count_to_i32(p->field_count, &field_count); if (e != SF_OK) return e;
    e = count_to_i32(p->modifier_count, &modifier_count); if (e != SF_OK) return e;
    int16_t type_a = (int16_t)((int32_t)p->type | ((int32_t)p->interpolation << 4) |
                               ((p->is_loop ? 1 : 0) << 12));
    int32_t type_b = ((int32_t)p->type | ((int32_t)p->interpolation << 2)) +
                     ((p->is_loop ? 1 : 0) << 4);
    e = sf_binary_writer_write_i16(bw, type_a); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, type_b); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, field_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = conditional ? reservef(bw, "ConditionalPropertyFieldsOffset[%zu]", index)
                    : reservef(bw, "PropertyFieldsOffset[%zu]", index);
    if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    if (!conditional) {
        e = reservef(bw, "PropertyModifiersOffset[%zu]", index); if (e != SF_OK) return e;
        e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
        e = sf_binary_writer_write_i32(bw, modifier_count); if (e != SF_OK) return e;
        e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    }
    return vec_push(properties, p);
}

static sf_result_t write_modifier_header(sf_binary_writer_t *bw,
                                         const sf_fxr3_property_modifier_t *m,
                                         fxr3_vec_t *modifiers) {
    size_t index = modifiers->count;
    int32_t field_count = 0, prop_count = 0;
    sf_result_t e = count_to_i32(m->field_count, &field_count); if (e != SF_OK) return e;
    e = count_to_i32(m->property_count, &prop_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u16(bw, m->type_enum_a); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u8(bw, 1); if (e != SF_OK) return e;
    e = sf_binary_writer_write_u32(bw, m->type_enum_b); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, field_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, prop_count); if (e != SF_OK) return e;
    e = reservef(bw, "ModifierFieldsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = reservef(bw, "ModifierConditionalPropertysOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    return vec_push(modifiers, m);
}

static sf_result_t write_unk_field_list_header(sf_binary_writer_t *bw,
                                               const sf_fxr3_unk_field_list_t *l,
                                               fxr3_vec_t *field_lists) {
    size_t index = field_lists->count;
    int32_t field_count = 0;
    sf_result_t e = count_to_i32(l->field_count, &field_count); if (e != SF_OK) return e;
    e = reservef(bw, "UnkFieldListFieldsOffset[%zu]", index); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, field_count); if (e != SF_OK) return e;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) return e;
    return vec_push(field_lists, l);
}

static sf_result_t fxr3_write_structural(sf_binary_writer_t *bw, const sf_fxr3_t *f,
                                         const sf_allocator_t *a) {
    if (!f->root_state_map || !f->root_container) return SF_ERR_INVALID_ARG;
    if (f->version != SF_FXR3_VERSION_DARK_SOULS_3 && f->version != SF_FXR3_VERSION_SEKIRO) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    fxr3_vec_t transitions = {.alloc = a}, containers = {.alloc = a}, actions = {.alloc = a};
    fxr3_vec_t properties = {.alloc = a}, modifiers = {.alloc = a};
    fxr3_vec_t conditional_properties = {.alloc = a}, field_lists = {.alloc = a};
    sf_result_t e = SF_OK;
    int32_t count = 0;

    e = sf_binary_writer_write_bytes(bw, "FXR\0", 4); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i16(bw, 0); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u16(bw, (uint16_t)f->version); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, 1); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, f->id); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "StateMapOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, 1); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "StateOffset"); if (e != SF_OK) goto cleanup;
    e = count_to_i32(f->root_state_map->state_count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "TransitionOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "TransitionCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ContainerOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ContainerCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "EffectOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "EffectCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ActionOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ActionCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "PropertyOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "PropertyCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ModifierOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ModifierCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ConditionalPropertyOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "ConditionalPropertyCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "UnkFieldListOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "UnkFieldListCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "FieldOffset"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(bw, "FieldCount"); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, 1); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) goto cleanup;
    if (f->version == SF_FXR3_VERSION_SEKIRO) {
        e = sf_binary_writer_reserve_i32(bw, "ReferenceOffset"); if (e != SF_OK) goto cleanup;
        e = count_to_i32(f->reference_count, &count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(bw, "ExternalValueOffset"); if (e != SF_OK) goto cleanup;
        e = count_to_i32(f->external_value_count, &count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(bw, "UnkBloodEnablerOffset"); if (e != SF_OK) goto cleanup;
        e = count_to_i32(f->unk_blood_enabler_count, &count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(bw, count); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(bw, "UnkEmptyOffset"); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(bw, 0); if (e != SF_OK) goto cleanup;
    }

    e = fill_pos(bw, "StateMapOffset"); if (e != SF_OK) goto cleanup;
    e = write_state_map_header(bw, f->root_state_map); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;
    e = fill_pos(bw, "StateOffset"); if (e != SF_OK) goto cleanup;
    e = fill_pos(bw, "StateMapStatesOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < f->root_state_map->state_count; i++) { e = write_state_header(bw, f->root_state_map->states[i], i); if (e != SF_OK) goto cleanup; }
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;
    e = fill_pos(bw, "TransitionOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < f->root_state_map->state_count; i++) {
        const sf_fxr3_state_t *s = f->root_state_map->states[i];
        e = fillf_pos(bw, "StateTransitionsOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < s->condition_count; j++) { e = write_condition_header(bw, s->conditions[j], &transitions); if (e != SF_OK) goto cleanup; }
    }
    e = count_to_i32(transitions.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "TransitionCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "ContainerOffset"); if (e != SF_OK) goto cleanup;
    e = write_container_header(bw, f->root_container, &containers); if (e != SF_OK) goto cleanup;
    e = write_container_children(bw, f->root_container, &containers); if (e != SF_OK) goto cleanup;
    e = count_to_i32(containers.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "ContainerCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "EffectOffset"); if (e != SF_OK) goto cleanup;
    size_t effect_count = 0;
    for (size_t i = 0; i < containers.count; i++) {
        const sf_fxr3_container_t *c = (const sf_fxr3_container_t *)containers.items[i];
        if (c->effect_count == 0) { e = fillf(bw, "ContainerEffectsOffset[%zu]", i, 0); if (e != SF_OK) goto cleanup; }
        else {
            e = fillf_pos(bw, "ContainerEffectsOffset[%zu]", i); if (e != SF_OK) goto cleanup;
            for (size_t j = 0; j < c->effect_count; j++) { e = write_effect_header(bw, c->effects[j], effect_count + j); if (e != SF_OK) goto cleanup; }
            effect_count += c->effect_count;
        }
    }
    e = count_to_i32(effect_count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "EffectCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "ActionOffset"); if (e != SF_OK) goto cleanup;
    effect_count = 0;
    for (size_t i = 0; i < containers.count; i++) {
        const sf_fxr3_container_t *c = (const sf_fxr3_container_t *)containers.items[i];
        e = fillf_pos(bw, "ContainerActionsOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < c->action_count; j++) { e = write_action_header(bw, c->actions[j], &actions); if (e != SF_OK) goto cleanup; }
        for (size_t j = 0; j < c->effect_count; j++) {
            e = fillf_pos(bw, "EffectActionsOffset[%zu]", effect_count + j); if (e != SF_OK) goto cleanup;
            for (size_t k = 0; k < c->effects[j]->action_count; k++) { e = write_action_header(bw, c->effects[j]->actions[k], &actions); if (e != SF_OK) goto cleanup; }
        }
        effect_count += c->effect_count;
    }
    e = count_to_i32(actions.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "ActionCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "PropertyOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < actions.count; i++) {
        const sf_fxr3_action_t *act = (const sf_fxr3_action_t *)actions.items[i];
        e = fillf_pos(bw, "ActionPropertiesOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < act->property_count; j++) { e = write_property_header(bw, act->properties[j], false, &properties); if (e != SF_OK) goto cleanup; }
    }
    e = count_to_i32(properties.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "PropertyCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "ModifierOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < properties.count; i++) {
        const sf_fxr3_property_t *p = (const sf_fxr3_property_t *)properties.items[i];
        e = fillf_pos(bw, "PropertyModifiersOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < p->modifier_count; j++) { e = write_modifier_header(bw, p->modifiers[j], &modifiers); if (e != SF_OK) goto cleanup; }
    }
    e = count_to_i32(modifiers.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "ModifierCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "ConditionalPropertyOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < modifiers.count; i++) {
        const sf_fxr3_property_modifier_t *m = (const sf_fxr3_property_modifier_t *)modifiers.items[i];
        e = fillf_pos(bw, "ModifierConditionalPropertysOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < m->property_count; j++) { e = write_property_header(bw, m->properties[j], true, &conditional_properties); if (e != SF_OK) goto cleanup; }
    }
    e = count_to_i32(conditional_properties.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "ConditionalPropertyCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "UnkFieldListOffset"); if (e != SF_OK) goto cleanup;
    for (size_t i = 0; i < actions.count; i++) {
        const sf_fxr3_action_t *act = (const sf_fxr3_action_t *)actions.items[i];
        e = fillf_pos(bw, "ActionUnkFieldListsOffset[%zu]", i); if (e != SF_OK) goto cleanup;
        for (size_t j = 0; j < act->unk_field_list_count; j++) { e = write_unk_field_list_header(bw, act->unk_field_lists[j], &field_lists); if (e != SF_OK) goto cleanup; }
    }
    e = count_to_i32(field_lists.count, &count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_fill_i32(bw, "UnkFieldListCount", count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    e = fill_pos(bw, "FieldOffset"); if (e != SF_OK) goto cleanup;
    int32_t field_count = 0;
    for (size_t i = 0; i < transitions.count; i++) {
        const sf_fxr3_state_condition_t *c = (const sf_fxr3_state_condition_t *)transitions.items[i];
        sf_fxr3_operand_t left = c->left_operand, right = c->right_operand;
        if (c->operator_type >= SF_FXR3_OPERATOR_LE) { left = c->right_operand; right = c->left_operand; }
        int32_t pos = 0;
        if (operand_has_value(left)) { e = pos_to_i32(sf_binary_writer_position(bw), &pos); if (e != SF_OK) goto cleanup; }
        e = fillf(bw, "TransitionFieldOffset1[%zu]", i, pos); if (e != SF_OK) goto cleanup;
        if (operand_has_value(left)) { e = write_operand_field(bw, left); if (e != SF_OK) goto cleanup; field_count++; }
        pos = 0;
        if (operand_has_value(right)) { e = pos_to_i32(sf_binary_writer_position(bw), &pos); if (e != SF_OK) goto cleanup; }
        e = fillf(bw, "TransitionFieldOffset2[%zu]", i, pos); if (e != SF_OK) goto cleanup;
        if (operand_has_value(right)) { e = write_operand_field(bw, right); if (e != SF_OK) goto cleanup; field_count++; }
    }
    for (size_t i = 0; i < actions.count; i++) {
        const sf_fxr3_action_t *act = (const sf_fxr3_action_t *)actions.items[i];
        if (act->field1_count == 0 && act->field2_count == 0) { e = fillf(bw, "ActionFieldsOffset[%zu]", i, 0); if (e != SF_OK) goto cleanup; }
        else { e = fillf_pos(bw, "ActionFieldsOffset[%zu]", i); if (e != SF_OK) goto cleanup; e = write_fields(bw, act->fields1, act->field1_count, &field_count); if (e != SF_OK) goto cleanup; e = write_fields(bw, act->fields2, act->field2_count, &field_count); if (e != SF_OK) goto cleanup; }
    }
    for (size_t i = 0; i < properties.count; i++) {
        const sf_fxr3_property_t *p = (const sf_fxr3_property_t *)properties.items[i];
        if (p->field_count == 0) { e = fillf(bw, "PropertyFieldsOffset[%zu]", i, 0); if (e != SF_OK) goto cleanup; }
        else { e = fillf_pos(bw, "PropertyFieldsOffset[%zu]", i); if (e != SF_OK) goto cleanup; e = write_fields(bw, p->fields, p->field_count, &field_count); if (e != SF_OK) goto cleanup; }
    }
    for (size_t i = 0; i < modifiers.count; i++) { const sf_fxr3_property_modifier_t *m = (const sf_fxr3_property_modifier_t *)modifiers.items[i]; e = fillf_pos(bw, "ModifierFieldsOffset[%zu]", i); if (e != SF_OK) goto cleanup; e = write_fields(bw, m->fields, m->field_count, &field_count); if (e != SF_OK) goto cleanup; }
    for (size_t i = 0; i < conditional_properties.count; i++) {
        const sf_fxr3_property_t *p = (const sf_fxr3_property_t *)conditional_properties.items[i];
        if (p->field_count == 0) { e = fillf(bw, "ConditionalPropertyFieldsOffset[%zu]", i, 0); if (e != SF_OK) goto cleanup; }
        else { e = fillf_pos(bw, "ConditionalPropertyFieldsOffset[%zu]", i); if (e != SF_OK) goto cleanup; e = write_fields(bw, p->fields, p->field_count, &field_count); if (e != SF_OK) goto cleanup; }
    }
    for (size_t i = 0; i < field_lists.count; i++) { const sf_fxr3_unk_field_list_t *l = (const sf_fxr3_unk_field_list_t *)field_lists.items[i]; e = fillf_pos(bw, "UnkFieldListFieldsOffset[%zu]", i); if (e != SF_OK) goto cleanup; e = write_fields(bw, l->fields, l->field_count, &field_count); if (e != SF_OK) goto cleanup; }
    e = sf_binary_writer_fill_i32(bw, "FieldCount", field_count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;

    if (f->version == SF_FXR3_VERSION_SEKIRO) {
        e = fill_pos(bw, "ReferenceOffset"); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32s(bw, f->reference_count, f->references); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;
        e = fill_pos(bw, "ExternalValueOffset"); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32s(bw, f->external_value_count, f->external_values); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;
        e = fill_pos(bw, "UnkBloodEnablerOffset"); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32s(bw, f->unk_blood_enabler_count, f->unk_blood_enablers); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad(bw, 16); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_fill_i32(bw, "UnkEmptyOffset", 0); if (e != SF_OK) goto cleanup;
    }

cleanup:
    vec_free(&transitions); vec_free(&containers); vec_free(&actions); vec_free(&properties);
    vec_free(&modifiers); vec_free(&conditional_properties); vec_free(&field_lists);
    return e;
}

SF_API sf_result_t sf_fxr3_write_to_memory(const sf_fxr3_t *f, void **out_bytes, size_t *out_size,
                                           const sf_allocator_t *a) {
    SF_CHECK_ARG(f != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    a = sf_alloc_or_default(a);
    sf_ostream_t *stream = NULL;
    sf_result_t e = sf_ostream_open_memory(&stream, a);
    if (e != SF_OK) return e;
    sf_binary_writer_t *bw = NULL;
    e = sf_binary_writer_create(&bw, stream, false, a);
    if (e == SF_OK) {
        e = fxr3_write_structural(bw, f, a);
        if (e == SF_OK) {
            uint8_t *bytes = NULL;
            e = sf_binary_writer_finish_bytes(bw, &bytes, out_size);
            if (e == SF_OK) *out_bytes = bytes;
        }
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(stream);
    return e;
}

static void destroy_property(sf_fxr3_property_t *p, const sf_allocator_t *a);
static void destroy_action(sf_fxr3_action_t *act, const sf_allocator_t *a);
static void destroy_container(sf_fxr3_container_t *c, const sf_allocator_t *a);

static void destroy_modifier(sf_fxr3_property_modifier_t *m, const sf_allocator_t *a) {
    if (!m) return;
    for (size_t i = 0; i < m->property_count; i++) destroy_property(m->properties[i], a);
    sf_xfree(a, m->fields);
    sf_xfree(a, m->properties);
    sf_xfree(a, m);
}

static void destroy_property(sf_fxr3_property_t *p, const sf_allocator_t *a) {
    if (!p) return;
    for (size_t i = 0; i < p->modifier_count; i++) destroy_modifier(p->modifiers[i], a);
    sf_xfree(a, p->fields);
    sf_xfree(a, p->modifiers);
    sf_xfree(a, p);
}

static void destroy_unk_field_list(sf_fxr3_unk_field_list_t *l, const sf_allocator_t *a) {
    if (!l) return;
    sf_xfree(a, l->fields);
    sf_xfree(a, l);
}

static void destroy_action(sf_fxr3_action_t *act, const sf_allocator_t *a) {
    if (!act) return;
    for (size_t i = 0; i < act->property_count; i++) destroy_property(act->properties[i], a);
    for (size_t i = 0; i < act->unk_field_list_count; i++) destroy_unk_field_list(act->unk_field_lists[i], a);
    sf_xfree(a, act->fields1);
    sf_xfree(a, act->fields2);
    sf_xfree(a, act->properties);
    sf_xfree(a, act->unk_field_lists);
    sf_xfree(a, act);
}

static void destroy_effect(sf_fxr3_effect_t *e, const sf_allocator_t *a) {
    if (!e) return;
    for (size_t i = 0; i < e->action_count; i++) destroy_action(e->actions[i], a);
    sf_xfree(a, e->actions);
    sf_xfree(a, e);
}

static void destroy_container(sf_fxr3_container_t *c, const sf_allocator_t *a) {
    if (!c) return;
    for (size_t i = 0; i < c->child_count; i++) destroy_container(c->children[i], a);
    for (size_t i = 0; i < c->effect_count; i++) destroy_effect(c->effects[i], a);
    for (size_t i = 0; i < c->action_count; i++) destroy_action(c->actions[i], a);
    sf_xfree(a, c->children);
    sf_xfree(a, c->effects);
    sf_xfree(a, c->actions);
    sf_xfree(a, c);
}

static void destroy_state_map(sf_fxr3_state_map_t *m, const sf_allocator_t *a) {
    if (!m) return;
    for (size_t i = 0; i < m->state_count; i++) {
        sf_fxr3_state_t *s = m->states[i];
        if (!s) continue;
        for (size_t j = 0; j < s->condition_count; j++) sf_xfree(a, s->conditions[j]);
        sf_xfree(a, s->conditions);
        sf_xfree(a, s);
    }
    sf_xfree(a, m->states);
    sf_xfree(a, m);
}

SF_API void sf_fxr3_destroy(sf_fxr3_t *f) {
    if (!f) return;
    const sf_allocator_t *a = f->alloc;
    destroy_state_map(f->root_state_map, a);
    destroy_container(f->root_container, a);
    sf_xfree(a, f->references);
    sf_xfree(a, f->external_values);
    sf_xfree(a, f->unk_blood_enablers);
    sf_xfree(a, f->raw_bytes);
    sf_xfree(a, f);
}

SF_API sf_fxr3_version_t sf_fxr3_version(const sf_fxr3_t *f) { return f ? f->version : SF_FXR3_VERSION_SEKIRO; }
SF_API int32_t sf_fxr3_id(const sf_fxr3_t *f) { return f ? f->id : 0; }
SF_API const sf_fxr3_state_map_t *sf_fxr3_root_state_map(const sf_fxr3_t *f) { return f ? f->root_state_map : NULL; }
SF_API const sf_fxr3_container_t *sf_fxr3_root_container(const sf_fxr3_t *f) { return f ? f->root_container : NULL; }
SF_API size_t sf_fxr3_reference_count(const sf_fxr3_t *f) { return f ? f->reference_count : 0; }
SF_API int32_t sf_fxr3_reference(const sf_fxr3_t *f, size_t i) { return (f && i < f->reference_count) ? f->references[i] : 0; }
SF_API size_t sf_fxr3_external_value_count(const sf_fxr3_t *f) { return f ? f->external_value_count : 0; }
SF_API int32_t sf_fxr3_external_value(const sf_fxr3_t *f, size_t i) { return (f && i < f->external_value_count) ? f->external_values[i] : 0; }
SF_API size_t sf_fxr3_unk_blood_enabler_count(const sf_fxr3_t *f) { return f ? f->unk_blood_enabler_count : 0; }
SF_API int32_t sf_fxr3_unk_blood_enabler(const sf_fxr3_t *f, size_t i) { return (f && i < f->unk_blood_enabler_count) ? f->unk_blood_enablers[i] : 0; }
SF_API size_t sf_fxr3_state_map_state_count(const sf_fxr3_state_map_t *m) { return m ? m->state_count : 0; }
SF_API const sf_fxr3_state_t *sf_fxr3_state_map_state(const sf_fxr3_state_map_t *m, size_t i) { return (m && i < m->state_count) ? m->states[i] : NULL; }
SF_API size_t sf_fxr3_state_condition_count(const sf_fxr3_state_t *s) { return s ? s->condition_count : 0; }
SF_API const sf_fxr3_state_condition_t *sf_fxr3_state_condition(const sf_fxr3_state_t *s, size_t i) { return (s && i < s->condition_count) ? s->conditions[i] : NULL; }
SF_API sf_fxr3_operator_type_t sf_fxr3_condition_operator(const sf_fxr3_state_condition_t *c) { return c ? c->operator_type : SF_FXR3_OPERATOR_NOT_EQUAL; }
SF_API sf_fxr3_operand_t sf_fxr3_condition_left_operand(const sf_fxr3_state_condition_t *c) { sf_fxr3_operand_t empty = {SF_FXR3_OPERAND_LITERAL, {0.0f}}; return c ? c->left_operand : empty; }
SF_API sf_fxr3_operand_t sf_fxr3_condition_right_operand(const sf_fxr3_state_condition_t *c) { sf_fxr3_operand_t empty = {SF_FXR3_OPERAND_LITERAL, {0.0f}}; return c ? c->right_operand : empty; }
SF_API int32_t sf_fxr3_condition_next_state(const sf_fxr3_state_condition_t *c) { return c ? c->next_state : 0; }
SF_API size_t sf_fxr3_container_id(const sf_fxr3_container_t *c) { return c ? (size_t)c->id : 0; }
SF_API size_t sf_fxr3_container_child_count(const sf_fxr3_container_t *c) { return c ? c->child_count : 0; }
SF_API const sf_fxr3_container_t *sf_fxr3_container_child(const sf_fxr3_container_t *c, size_t i) { return (c && i < c->child_count) ? c->children[i] : NULL; }
SF_API size_t sf_fxr3_container_effect_count(const sf_fxr3_container_t *c) { return c ? c->effect_count : 0; }
SF_API const sf_fxr3_effect_t *sf_fxr3_container_effect(const sf_fxr3_container_t *c, size_t i) { return (c && i < c->effect_count) ? c->effects[i] : NULL; }
SF_API size_t sf_fxr3_effect_action_count(const sf_fxr3_effect_t *e) { return e ? e->action_count : 0; }
SF_API const sf_fxr3_action_t *sf_fxr3_effect_action(const sf_fxr3_effect_t *e, size_t i) { return (e && i < e->action_count) ? e->actions[i] : NULL; }
SF_API size_t sf_fxr3_action_property_count(const sf_fxr3_action_t *a) { return a ? a->property_count : 0; }
SF_API const sf_fxr3_property_t *sf_fxr3_action_property(const sf_fxr3_action_t *a, size_t i) { return (a && i < a->property_count) ? a->properties[i] : NULL; }
SF_API sf_fxr3_property_type_t sf_fxr3_property_type(const sf_fxr3_property_t *p) { return p ? p->type : SF_FXR3_PROPERTY_TYPE_SCALAR; }
SF_API sf_fxr3_property_interpolation_type_t sf_fxr3_property_interpolation(const sf_fxr3_property_t *p) { return p ? p->interpolation : SF_FXR3_INTERP_ZERO; }
SF_API bool sf_fxr3_property_is_loop(const sf_fxr3_property_t *p) { return p ? p->is_loop : false; }
SF_API size_t sf_fxr3_property_field_count(const sf_fxr3_property_t *p) { return p ? p->field_count : 0; }
SF_API sf_fxr3_field_t sf_fxr3_property_field(const sf_fxr3_property_t *p, size_t i) { sf_fxr3_field_t empty = {SF_FXR3_FIELD_TYPE_INT, {0}}; return (p && i < p->field_count) ? p->fields[i] : empty; }
SF_API size_t sf_fxr3_property_modifier_count(const sf_fxr3_property_t *p) { return p ? p->modifier_count : 0; }
SF_API const sf_fxr3_property_modifier_t *sf_fxr3_property_modifier(const sf_fxr3_property_t *p, size_t i) { return (p && i < p->modifier_count) ? p->modifiers[i] : NULL; }
