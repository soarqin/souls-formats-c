/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 XML reader (mxml DOM -> sf_fxr3_t).
 *
 * Per-parse bump-allocator arena: every struct, pointer array, and field
 * array produced by this reader lives inside a single contiguous buffer
 * stored as `sf_fxr3_t::xml_arena`. sf_fxr3_destroy frees that buffer in
 * one call instead of walking the tree. See docs/api-mapping/extensions.md
 * row "FXR3 XML scratch arena".
 */

#include "souls_formats/sf_fxr3.h"
#include "internal/sf_internal.h"
#include "fxr3_internal.h"

#include <mxml.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char  *buf;
    size_t used;
    size_t cap;
} fxr3_arena_t;

#define FXR3_ARENA_ALIGN 16u

static void *arena_alloc(fxr3_arena_t *a, size_t size) {
    size_t pad = (FXR3_ARENA_ALIGN - (a->used & (FXR3_ARENA_ALIGN - 1u))) & (FXR3_ARENA_ALIGN - 1u);
    size_t off = a->used + pad;
    if (size == 0 || off > a->cap || size > a->cap - off) return NULL;
    void *p = a->buf + off;
    a->used = off + size;
    memset(p, 0, size);
    return p;
}

static const char *get_text(mxml_node_t *node) {
    if (!node) return NULL;
    return mxmlGetOpaque(node);
}

static const char *get_child_text(mxml_node_t *parent, const char *name) {
    mxml_node_t *child = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    return get_text(child);
}

static sf_result_t parse_int(const char *str, int32_t *out) {
    if (!str) return SF_ERR_INVALID_ARG;
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') return SF_ERR_INVALID_ARG;
    *out = (int32_t)val;
    return SF_OK;
}

static sf_result_t parse_float(const char *str, float *out) {
    if (!str) return SF_ERR_INVALID_ARG;
    char *end;
    float val = strtof(str, &end);
    if (*end != '\0') return SF_ERR_INVALID_ARG;
    *out = val;
    return SF_OK;
}

static sf_result_t parse_bool(const char *str, bool *out) {
    if (!str) return SF_ERR_INVALID_ARG;
    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0) *out = true;
    else if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0) *out = false;
    else return SF_ERR_INVALID_ARG;
    return SF_OK;
}

static sf_result_t read_operand(mxml_node_t *parent, const char *name, sf_fxr3_operand_t *out) {
    mxml_node_t *node = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    if (!node) return SF_ERR_INVALID_ARG;

    mxml_node_t *child = mxmlGetFirstChild(node);
    while (child && mxmlGetType(child) != MXML_TYPE_ELEMENT) {
        child = mxmlGetNextSibling(child);
    }
    if (!child) return SF_ERR_INVALID_ARG;

    const char *type_name = mxmlGetElement(child);
    if (strcmp(type_name, "Literal") == 0) {
        out->type = SF_FXR3_OPERAND_LITERAL;
        return parse_float(mxmlElementGetAttr(child, "Value"), &out->value.as_literal);
    } else if (strcmp(type_name, "External") == 0) {
        out->type = SF_FXR3_OPERAND_EXTERNAL;
        return parse_int(mxmlElementGetAttr(child, "Value"), &out->value.as_external);
    } else if (strcmp(type_name, "UnkMinus2") == 0) {
        out->type = SF_FXR3_OPERAND_TIME_OF_DAY;
        return SF_OK;
    } else if (strcmp(type_name, "StateTime") == 0) {
        out->type = SF_FXR3_OPERAND_STATE_TIME;
        return SF_OK;
    }
    return SF_ERR_INVALID_ARG;
}

static sf_result_t read_condition(mxml_node_t *node, sf_fxr3_state_condition_t **out, fxr3_arena_t *ar) {
    sf_fxr3_state_condition_t *c = (sf_fxr3_state_condition_t *)arena_alloc(ar, sizeof(*c));
    if (!c) return SF_ERR_OOM;

    sf_result_t e = read_operand(node, "LeftOperand", &c->left_operand);
    if (e != SF_OK) return e;

    mxml_node_t *op_node = mxmlFindElement(node, node, "Op", NULL, NULL, MXML_DESCEND_FIRST);
    if (!op_node) return SF_ERR_INVALID_ARG;

    int32_t unk = 0;
    e = parse_int(mxmlElementGetAttr(op_node, "Unk"), &unk);
    if (e != SF_OK) return e;
    c->unk_modifier = (uint8_t)unk;

    const char *op_str = get_text(op_node);
    if (!op_str) return SF_ERR_INVALID_ARG;

    if (strcmp(op_str, "NotEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_NOT_EQUAL;
    else if (strcmp(op_str, "Equal") == 0) c->operator_type = SF_FXR3_OPERATOR_EQUAL;
    else if (strcmp(op_str, "GreaterThanOrEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_GE;
    else if (strcmp(op_str, "GreaterThan") == 0) c->operator_type = SF_FXR3_OPERATOR_GT;
    else if (strcmp(op_str, "LessThanOrEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_LE;
    else if (strcmp(op_str, "LessThan") == 0) c->operator_type = SF_FXR3_OPERATOR_LT;
    else return SF_ERR_INVALID_ARG;

    e = read_operand(node, "RightOperand", &c->right_operand);
    if (e != SF_OK) return e;

    e = parse_int(mxmlElementGetAttr(node, "ElseMoveToStateIndex"), &c->next_state);
    if (e != SF_OK) return e;

    *out = c;
    return SF_OK;
}

static sf_result_t read_state(mxml_node_t *node, sf_fxr3_state_t **out, fxr3_arena_t *ar) {
    sf_fxr3_state_t *s = (sf_fxr3_state_t *)arena_alloc(ar, sizeof(*s));
    if (!s) return SF_ERR_OOM;

    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(node, node, "StayCondition", NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, node, "StayCondition", NULL, NULL, MXML_DESCEND_NONE)) {
        count++;
    }

    if (count > 0) {
        s->conditions = (sf_fxr3_state_condition_t **)arena_alloc(ar, count * sizeof(*s->conditions));
        if (!s->conditions) return SF_ERR_OOM;

        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "StayCondition", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "StayCondition", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = read_condition(child, &s->conditions[i++], ar);
            if (e != SF_OK) return e;
        }
    }
    s->condition_count = count;
    *out = s;
    return SF_OK;
}

static sf_result_t read_state_map(mxml_node_t *node, sf_fxr3_state_map_t **out, fxr3_arena_t *ar) {
    sf_fxr3_state_map_t *m = (sf_fxr3_state_map_t *)arena_alloc(ar, sizeof(*m));
    if (!m) return SF_ERR_OOM;

    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(node, node, "State", NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, node, "State", NULL, NULL, MXML_DESCEND_NONE)) {
        count++;
    }

    if (count > 0) {
        m->states = (sf_fxr3_state_t **)arena_alloc(ar, count * sizeof(*m->states));
        if (!m->states) return SF_ERR_OOM;

        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "State", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "State", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = read_state(child, &m->states[i++], ar);
            if (e != SF_OK) return e;
        }
    }
    m->state_count = count;
    *out = m;
    return SF_OK;
}

static sf_result_t read_fields(mxml_node_t *parent, const char *name, sf_fxr3_field_t **out_fields, size_t *out_count, fxr3_arena_t *ar) {
    *out_fields = NULL;
    *out_count = 0;

    mxml_node_t *node = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    if (!node) return SF_OK;

    size_t count = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(node); child != NULL; child = mxmlGetNextSibling(child)) {
        if (mxmlGetType(child) == MXML_TYPE_ELEMENT) count++;
    }

    if (count > 0) {
        sf_fxr3_field_t *fields = (sf_fxr3_field_t *)arena_alloc(ar, count * sizeof(*fields));
        if (!fields) return SF_ERR_OOM;

        size_t i = 0;
        for (mxml_node_t *child = mxmlGetFirstChild(node); child != NULL; child = mxmlGetNextSibling(child)) {
            if (mxmlGetType(child) != MXML_TYPE_ELEMENT) continue;

            const char *type_name = mxmlGetElement(child);
            if (strcmp(type_name, "Int") == 0) {
                fields[i].type = SF_FXR3_FIELD_TYPE_INT;
                sf_result_t e = parse_int(mxmlElementGetAttr(child, "Value"), &fields[i].value.as_int);
                if (e != SF_OK) return e;
            } else if (strcmp(type_name, "Float") == 0) {
                fields[i].type = SF_FXR3_FIELD_TYPE_FLOAT;
                sf_result_t e = parse_float(mxmlElementGetAttr(child, "Value"), &fields[i].value.as_float);
                if (e != SF_OK) return e;
            } else {
                return SF_ERR_INVALID_ARG;
            }
            i++;
        }
        *out_fields = fields;
    }
    *out_count = count;
    return SF_OK;
}

static sf_result_t read_property(mxml_node_t *node, sf_fxr3_property_t **out, fxr3_arena_t *ar);

static sf_result_t read_modifier(mxml_node_t *node, sf_fxr3_property_modifier_t **out, fxr3_arena_t *ar) {
    sf_fxr3_property_modifier_t *m = (sf_fxr3_property_modifier_t *)arena_alloc(ar, sizeof(*m));
    if (!m) return SF_ERR_OOM;

    int32_t type_a = 0, type_b = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "TypeEnumA"), &type_a);
    if (e != SF_OK) return e;
    e = parse_int(mxmlElementGetAttr(node, "TypeEnumB"), &type_b);
    if (e != SF_OK) return e;
    m->type_enum_a = (uint16_t)type_a;
    m->type_enum_b = (uint32_t)type_b;

    e = read_fields(node, "Fields", &m->fields, &m->field_count, ar);
    if (e != SF_OK) return e;

    mxml_node_t *props_node = mxmlFindElement(node, node, "Properties", NULL, NULL, MXML_DESCEND_FIRST);
    if (props_node) {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(props_node, props_node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, props_node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }

        if (count > 0) {
            m->properties = (sf_fxr3_property_t **)arena_alloc(ar, count * sizeof(*m->properties));
            if (!m->properties) return SF_ERR_OOM;

            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(props_node, props_node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, props_node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_property(child, &m->properties[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        m->property_count = count;
    }

    *out = m;
    return SF_OK;
}

static sf_result_t read_property(mxml_node_t *node, sf_fxr3_property_t **out, fxr3_arena_t *ar) {
    sf_fxr3_property_t *p = (sf_fxr3_property_t *)arena_alloc(ar, sizeof(*p));
    if (!p) return SF_ERR_OOM;

    const char *type_str = mxmlElementGetAttr(node, "PropertyType");
    if (!type_str) return SF_ERR_INVALID_ARG;
    if (strcmp(type_str, "Scalar") == 0) p->type = SF_FXR3_PROPERTY_TYPE_SCALAR;
    else if (strcmp(type_str, "Vector2") == 0) p->type = SF_FXR3_PROPERTY_TYPE_VECTOR2;
    else if (strcmp(type_str, "Vector3") == 0) p->type = SF_FXR3_PROPERTY_TYPE_VECTOR3;
    else if (strcmp(type_str, "Color") == 0) p->type = SF_FXR3_PROPERTY_TYPE_COLOR;
    else return SF_ERR_INVALID_ARG;

    const char *interp_str = mxmlElementGetAttr(node, "InterpolationType");
    if (!interp_str) return SF_ERR_INVALID_ARG;
    if (strcmp(interp_str, "Zero") == 0) p->interpolation = SF_FXR3_INTERP_ZERO;
    else if (strcmp(interp_str, "One") == 0) p->interpolation = SF_FXR3_INTERP_ONE;
    else if (strcmp(interp_str, "Constant") == 0) p->interpolation = SF_FXR3_INTERP_CONSTANT;
    else if (strcmp(interp_str, "Stepped") == 0) p->interpolation = SF_FXR3_INTERP_STEPPED;
    else if (strcmp(interp_str, "Linear") == 0) p->interpolation = SF_FXR3_INTERP_LINEAR;
    else if (strcmp(interp_str, "Curve1") == 0) p->interpolation = SF_FXR3_INTERP_CURVE1;
    else if (strcmp(interp_str, "Curve2") == 0) p->interpolation = SF_FXR3_INTERP_CURVE2;
    else if (strcmp(interp_str, "UnkAc6") == 0) p->interpolation = SF_FXR3_INTERP_UNK_AC6;
    else return SF_ERR_INVALID_ARG;

    sf_result_t e = parse_bool(mxmlElementGetAttr(node, "IsLoop"), &p->is_loop);
    if (e != SF_OK) return e;

    e = read_fields(node, "Fields", &p->fields, &p->field_count, ar);
    if (e != SF_OK) return e;

    mxml_node_t *mods_node = mxmlFindElement(node, node, "Modifiers", NULL, NULL, MXML_DESCEND_FIRST);
    if (mods_node) {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(mods_node, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }

        if (count > 0) {
            p->modifiers = (sf_fxr3_property_modifier_t **)arena_alloc(ar, count * sizeof(*p->modifiers));
            if (!p->modifiers) return SF_ERR_OOM;

            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(mods_node, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_modifier(child, &p->modifiers[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        p->modifier_count = count;
    }

    *out = p;
    return SF_OK;
}

static sf_result_t read_unk_field_list(mxml_node_t *node, sf_fxr3_unk_field_list_t **out, fxr3_arena_t *ar) {
    sf_fxr3_unk_field_list_t *l = (sf_fxr3_unk_field_list_t *)arena_alloc(ar, sizeof(*l));
    if (!l) return SF_ERR_OOM;

    sf_result_t e = read_fields(node, "Fields", &l->fields, &l->field_count, ar);
    if (e != SF_OK) return e;

    *out = l;
    return SF_OK;
}

static sf_result_t read_action(mxml_node_t *node, sf_fxr3_action_t **out, fxr3_arena_t *ar) {
    sf_fxr3_action_t *act = (sf_fxr3_action_t *)arena_alloc(ar, sizeof(*act));
    if (!act) return SF_ERR_OOM;

    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) return e;
    act->id = (int16_t)id;

    e = parse_bool(mxmlElementGetAttr(node, "Unk02"), &act->unk02);
    if (e != SF_OK) return e;

    e = parse_bool(mxmlElementGetAttr(node, "Unk03"), &act->unk03);
    if (e != SF_OK) return e;

    e = parse_int(mxmlElementGetAttr(node, "Unk04"), &act->unk04);
    if (e != SF_OK) return e;

    e = read_fields(node, "Fields1", &act->fields1, &act->field1_count, ar);
    if (e != SF_OK) return e;

    e = read_fields(node, "Fields2", &act->fields2, &act->field2_count, ar);
    if (e != SF_OK) return e;

    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            act->properties = (sf_fxr3_property_t **)arena_alloc(ar, count * sizeof(*act->properties));
            if (!act->properties) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_property(child, &act->properties[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        act->property_count = count;
    }

    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            act->unk_field_lists = (sf_fxr3_unk_field_list_t **)arena_alloc(ar, count * sizeof(*act->unk_field_lists));
            if (!act->unk_field_lists) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_unk_field_list(child, &act->unk_field_lists[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        act->unk_field_list_count = count;
    }

    *out = act;
    return SF_OK;
}

static sf_result_t read_effect(mxml_node_t *node, sf_fxr3_effect_t **out, fxr3_arena_t *ar) {
    sf_fxr3_effect_t *eff = (sf_fxr3_effect_t *)arena_alloc(ar, sizeof(*eff));
    if (!eff) return SF_ERR_OOM;

    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) return e;
    eff->id = (int16_t)id;

    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            eff->actions = (sf_fxr3_action_t **)arena_alloc(ar, count * sizeof(*eff->actions));
            if (!eff->actions) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_action(child, &eff->actions[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        eff->action_count = count;
    }

    *out = eff;
    return SF_OK;
}

static sf_result_t read_container(mxml_node_t *node, sf_fxr3_container_t **out, fxr3_arena_t *ar) {
    sf_fxr3_container_t *c = (sf_fxr3_container_t *)arena_alloc(ar, sizeof(*c));
    if (!c) return SF_ERR_OOM;

    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) return e;
    c->id = (int16_t)id;

    /* Children and Effects are direct child elements of <Container> (no wrapper),
     * matching the upstream XmlSerializer [XmlElement("Container")] / [XmlElement("Effect")] schema. */
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Container", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Container", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->children = (sf_fxr3_container_t **)arena_alloc(ar, count * sizeof(*c->children));
            if (!c->children) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Container", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Container", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_container(child, &c->children[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        c->child_count = count;
    }

    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Effect", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Effect", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->effects = (sf_fxr3_effect_t **)arena_alloc(ar, count * sizeof(*c->effects));
            if (!c->effects) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Effect", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Effect", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_effect(child, &c->effects[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        c->effect_count = count;
    }

    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->actions = (sf_fxr3_action_t **)arena_alloc(ar, count * sizeof(*c->actions));
            if (!c->actions) return SF_ERR_OOM;
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_action(child, &c->actions[i++], ar);
                if (e != SF_OK) return e;
            }
        }
        c->action_count = count;
    }

    *out = c;
    return SF_OK;
}

static sf_result_t read_int_list(mxml_node_t *parent, const char *name, int32_t **out_items, size_t *out_count, fxr3_arena_t *ar) {
    *out_items = NULL;
    *out_count = 0;

    mxml_node_t *node = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    if (!node) return SF_OK;

    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(node, node, "int", NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, node, "int", NULL, NULL, MXML_DESCEND_NONE)) {
        count++;
    }

    if (count > 0) {
        int32_t *items = (int32_t *)arena_alloc(ar, count * sizeof(int32_t));
        if (!items) return SF_ERR_OOM;

        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "int", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "int", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = parse_int(get_text(child), &items[i++]);
            if (e != SF_OK) return e;
        }
        *out_items = items;
    }
    *out_count = count;
    return SF_OK;
}

SF_API sf_result_t sf_fxr3_from_xml(sf_fxr3_t **out, const char *xml_utf8, size_t xml_size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && xml_utf8 != NULL);
    *out = NULL;
    a = sf_alloc_or_default(a);

    mxml_options_t *options = mxmlOptionsNew();
    if (options) {
        mxmlOptionsSetTypeValue(options, MXML_TYPE_OPAQUE);
    }

    /* Single arena big enough for the full tree + NUL-terminated XML copy.
     * Heuristic: 6x XML size + 8 KiB overhead is comfortable for the
     * struct/pointer-array overhead of every node observed in real ER fixtures
     * (one mxml element produces at most a ~96-byte struct plus a pointer
     * slot in its parent's array; XML text already encodes the same shape
     * at ~32 bytes/element minimum). */
    size_t arena_cap = (xml_size * 6u) + 8192u;
    if (arena_cap < xml_size + 1u) arena_cap = xml_size + 1u + 8192u;
    fxr3_arena_t arena = {0};
    arena.buf = (char *)sf_xalloc(a, arena_cap);
    if (!arena.buf) {
        if (options) mxmlOptionsDelete(options);
        return SF_ERR_OOM;
    }
    arena.cap = arena_cap;

    char *nul_terminated = (char *)arena_alloc(&arena, xml_size + 1);
    if (!nul_terminated) {
        sf_xfree(a, arena.buf);
        if (options) mxmlOptionsDelete(options);
        return SF_ERR_OOM;
    }
    memcpy(nul_terminated, xml_utf8, xml_size);
    nul_terminated[xml_size] = '\0';

    mxml_node_t *tree = mxmlLoadString(NULL, options, nul_terminated);
    if (options) mxmlOptionsDelete(options);
    if (!tree) {
        sf_xfree(a, arena.buf);
        return SF_ERR_INTERNAL;
    }

    /* mxmlLoadString may return the root element directly (mxml 4.x behaviour).
     * Mirror the pattern from src/param/paramdef_xml_read.c:744-748. */
    mxml_node_t *root = NULL;
    const char *tree_name = mxmlGetElement(tree);
    if (tree_name && strcmp(tree_name, "FXR3") == 0) {
        root = tree;
    } else {
        root = mxmlFindElement(tree, tree, "FXR3", NULL, NULL, MXML_DESCEND_ALL);
    }
    if (!root) {
        mxmlDelete(tree);
        sf_xfree(a, arena.buf);
        return SF_ERR_BAD_MAGIC;
    }

    sf_fxr3_t *f = (sf_fxr3_t *)sf_xalloc(a, sizeof(*f));
    if (!f) {
        mxmlDelete(tree);
        sf_xfree(a, arena.buf);
        return SF_ERR_OOM;
    }
    memset(f, 0, sizeof(*f));
    f->alloc = a;

    const char *version_str = get_child_text(root, "Version");
    sf_result_t e = SF_OK;
    if (!version_str) { e = SF_ERR_INVALID_ARG; goto fail; }
    if (strcmp(version_str, "Sekiro") == 0) f->version = SF_FXR3_VERSION_SEKIRO;
    else if (strcmp(version_str, "DarkSouls3") == 0) f->version = SF_FXR3_VERSION_DARK_SOULS_3;
    else { e = SF_ERR_INVALID_ARG; goto fail; }

    e = parse_int(get_child_text(root, "Id"), &f->id);
    if (e != SF_OK) goto fail;

    mxml_node_t *sm_node = mxmlFindElement(root, root, "StateMap", NULL, NULL, MXML_DESCEND_FIRST);
    if (sm_node) {
        e = read_state_map(sm_node, &f->root_state_map, &arena);
        if (e != SF_OK) goto fail;
    }

    mxml_node_t *cont_node = mxmlFindElement(root, root, "Container", NULL, NULL, MXML_DESCEND_FIRST);
    if (cont_node) {
        e = read_container(cont_node, &f->root_container, &arena);
        if (e != SF_OK) goto fail;
    }

    e = read_int_list(root, "ReferenceList", &f->references, &f->reference_count, &arena);
    if (e != SF_OK) goto fail;

    e = read_int_list(root, "ExternalValueList", &f->external_values, &f->external_value_count, &arena);
    if (e != SF_OK) goto fail;

    e = read_int_list(root, "UnkBloodEnabler", &f->unk_blood_enablers, &f->unk_blood_enabler_count, &arena);
    if (e != SF_OK) goto fail;

    mxmlDelete(tree);
    f->xml_arena = arena.buf;
    *out = f;
    return SF_OK;

fail:
    mxmlDelete(tree);
    sf_xfree(a, arena.buf);
    sf_xfree(a, f);
    return e;
}
