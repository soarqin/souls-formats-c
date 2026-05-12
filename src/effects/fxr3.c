/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 Rainbow Stone particle effects format implementation.
 *
 * Mirrors:
 *   SoulsFormats/Formats/FXR3.cs
 *
 * Wave 3 (T16-T20) implements binary read; Wave 4 (T21-T22) implements XML
 * round-trip. This stub provides the opaque struct definitions and accessor
 * scaffolding only.
 */

#include "souls_formats/sf_fxr3.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sf_fxr3_property_modifier {
    int _placeholder; /* Wave 3 T18 replaces this with the real layout. */
};

struct sf_fxr3_unk_field_list {
    int _placeholder; /* Wave 3 T18 replaces this with the real layout. */
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
    sf_fxr3_property_t **properties;
    size_t property_count;
    sf_fxr3_unk_field_list_t **unk_field_lists;
    size_t unk_field_list_count;
};

struct sf_fxr3_effect {
    sf_fxr3_action_t **actions;
    size_t action_count;
};

struct sf_fxr3_container {
    size_t id;
    sf_fxr3_container_t **children;
    size_t child_count;
    sf_fxr3_effect_t **effects;
    size_t effect_count;
    sf_fxr3_action_t **actions;
    size_t action_count;
};

struct sf_fxr3_state_condition {
    sf_fxr3_operator_type_t operator_type;
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
};

SF_API sf_result_t sf_fxr3_read_from_memory(sf_fxr3_t **out, const void *bytes, size_t size,
                                            const sf_allocator_t *a) {
    (void)bytes;
    (void)size;
    (void)a;
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    return SF_ERR_INTERNAL; /* Wave 3 T20 implements this */
}

SF_API sf_result_t sf_fxr3_write_to_memory(const sf_fxr3_t *f, void **out_bytes, size_t *out_size,
                                           const sf_allocator_t *a) {
    (void)f;
    (void)out_bytes;
    (void)out_size;
    (void)a;
    return SF_ERR_INTERNAL; /* Wave 3 T20 implements this */
}

SF_API void sf_fxr3_destroy(sf_fxr3_t *f) {
    if (!f)
        return;
    const sf_allocator_t *alloc = f->alloc;
    /* Nested state map / container / actions / properties are freed in Wave 3
     * once the reader populates them. */
    sf_xfree(alloc, f->references);
    sf_xfree(alloc, f->external_values);
    sf_xfree(alloc, f->unk_blood_enablers);
    sf_xfree(alloc, f);
}

SF_API sf_fxr3_version_t sf_fxr3_version(const sf_fxr3_t *f) {
    return f ? f->version : SF_FXR3_VERSION_SEKIRO;
}

SF_API int32_t sf_fxr3_id(const sf_fxr3_t *f) {
    return f ? f->id : 0;
}

SF_API const sf_fxr3_state_map_t *sf_fxr3_root_state_map(const sf_fxr3_t *f) {
    return f ? f->root_state_map : NULL;
}

SF_API const sf_fxr3_container_t *sf_fxr3_root_container(const sf_fxr3_t *f) {
    return f ? f->root_container : NULL;
}

SF_API size_t sf_fxr3_reference_count(const sf_fxr3_t *f) {
    return f ? f->reference_count : 0;
}

SF_API int32_t sf_fxr3_reference(const sf_fxr3_t *f, size_t i) {
    return (f && i < f->reference_count) ? f->references[i] : 0;
}

SF_API size_t sf_fxr3_external_value_count(const sf_fxr3_t *f) {
    return f ? f->external_value_count : 0;
}

SF_API int32_t sf_fxr3_external_value(const sf_fxr3_t *f, size_t i) {
    return (f && i < f->external_value_count) ? f->external_values[i] : 0;
}

SF_API size_t sf_fxr3_unk_blood_enabler_count(const sf_fxr3_t *f) {
    return f ? f->unk_blood_enabler_count : 0;
}

SF_API int32_t sf_fxr3_unk_blood_enabler(const sf_fxr3_t *f, size_t i) {
    return (f && i < f->unk_blood_enabler_count) ? f->unk_blood_enablers[i] : 0;
}

SF_API size_t sf_fxr3_state_map_state_count(const sf_fxr3_state_map_t *m) {
    return m ? m->state_count : 0;
}

SF_API const sf_fxr3_state_t *sf_fxr3_state_map_state(const sf_fxr3_state_map_t *m, size_t i) {
    return (m && i < m->state_count) ? m->states[i] : NULL;
}

SF_API size_t sf_fxr3_state_condition_count(const sf_fxr3_state_t *s) {
    return s ? s->condition_count : 0;
}

SF_API const sf_fxr3_state_condition_t *sf_fxr3_state_condition(const sf_fxr3_state_t *s,
                                                                size_t i) {
    return (s && i < s->condition_count) ? s->conditions[i] : NULL;
}

SF_API sf_fxr3_operator_type_t sf_fxr3_condition_operator(const sf_fxr3_state_condition_t *c) {
    return c ? c->operator_type : SF_FXR3_OPERATOR_NOT_EQUAL;
}

SF_API sf_fxr3_operand_t sf_fxr3_condition_left_operand(const sf_fxr3_state_condition_t *c) {
    sf_fxr3_operand_t empty = {SF_FXR3_OPERAND_LITERAL, {0.0f}};
    return c ? c->left_operand : empty;
}

SF_API sf_fxr3_operand_t sf_fxr3_condition_right_operand(const sf_fxr3_state_condition_t *c) {
    sf_fxr3_operand_t empty = {SF_FXR3_OPERAND_LITERAL, {0.0f}};
    return c ? c->right_operand : empty;
}

SF_API int32_t sf_fxr3_condition_next_state(const sf_fxr3_state_condition_t *c) {
    return c ? c->next_state : 0;
}

SF_API size_t sf_fxr3_container_id(const sf_fxr3_container_t *c) {
    return c ? c->id : 0;
}

SF_API size_t sf_fxr3_container_child_count(const sf_fxr3_container_t *c) {
    return c ? c->child_count : 0;
}

SF_API const sf_fxr3_container_t *sf_fxr3_container_child(const sf_fxr3_container_t *c, size_t i) {
    return (c && i < c->child_count) ? c->children[i] : NULL;
}

SF_API size_t sf_fxr3_container_effect_count(const sf_fxr3_container_t *c) {
    return c ? c->effect_count : 0;
}

SF_API const sf_fxr3_effect_t *sf_fxr3_container_effect(const sf_fxr3_container_t *c, size_t i) {
    return (c && i < c->effect_count) ? c->effects[i] : NULL;
}

SF_API size_t sf_fxr3_effect_action_count(const sf_fxr3_effect_t *e) {
    return e ? e->action_count : 0;
}

SF_API const sf_fxr3_action_t *sf_fxr3_effect_action(const sf_fxr3_effect_t *e, size_t i) {
    return (e && i < e->action_count) ? e->actions[i] : NULL;
}

SF_API size_t sf_fxr3_action_property_count(const sf_fxr3_action_t *a) {
    return a ? a->property_count : 0;
}

SF_API const sf_fxr3_property_t *sf_fxr3_action_property(const sf_fxr3_action_t *a, size_t i) {
    return (a && i < a->property_count) ? a->properties[i] : NULL;
}

SF_API sf_fxr3_property_type_t sf_fxr3_property_type(const sf_fxr3_property_t *p) {
    return p ? p->type : SF_FXR3_PROPERTY_TYPE_SCALAR;
}

SF_API sf_fxr3_property_interpolation_type_t
sf_fxr3_property_interpolation(const sf_fxr3_property_t *p) {
    return p ? p->interpolation : SF_FXR3_INTERP_ZERO;
}

SF_API bool sf_fxr3_property_is_loop(const sf_fxr3_property_t *p) {
    return p ? p->is_loop : false;
}

SF_API size_t sf_fxr3_property_field_count(const sf_fxr3_property_t *p) {
    return p ? p->field_count : 0;
}

SF_API sf_fxr3_field_t sf_fxr3_property_field(const sf_fxr3_property_t *p, size_t i) {
    sf_fxr3_field_t empty = {SF_FXR3_FIELD_TYPE_INT, {0}};
    return (p && i < p->field_count) ? p->fields[i] : empty;
}

SF_API size_t sf_fxr3_property_modifier_count(const sf_fxr3_property_t *p) {
    return p ? p->modifier_count : 0;
}

SF_API const sf_fxr3_property_modifier_t *sf_fxr3_property_modifier(const sf_fxr3_property_t *p,
                                                                    size_t i) {
    return (p && i < p->modifier_count) ? p->modifiers[i] : NULL;
}
