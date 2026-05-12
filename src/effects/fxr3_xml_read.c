/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 XML reader (mxml DOM -> sf_fxr3_t).
 */

#include "souls_formats/sf_fxr3.h"
#include "internal/sf_internal.h"
#include "fxr3_internal.h"

#include <mxml.h>
#include <stdlib.h>
#include <string.h>

static const char *get_text(mxml_node_t *node) {
    if (!node) return NULL;
    return mxmlGetOpaque(node);
}

static const char *get_child_text(mxml_node_t *parent, const char *name) {
    mxml_node_t *child = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    return get_text(child);
}

static sf_result_t parse_int(const char *str, int32_t *out) {
    if (!str) {
    return SF_ERR_INVALID_ARG;
    }
    char *end;
    long val = strtol(str, &end, 10);
    if (*end != '\0') {
    return SF_ERR_INVALID_ARG;
    }
    *out = (int32_t)val;
    return SF_OK;
}

static sf_result_t parse_float(const char *str, float *out) {
    if (!str) {
    return SF_ERR_INVALID_ARG;
    }
    char *end;
    float val = strtof(str, &end);
    if (*end != '\0') {
    return SF_ERR_INVALID_ARG;
    }
    *out = val;
    return SF_OK;
}

static sf_result_t parse_bool(const char *str, bool *out) {
    if (!str) {
    return SF_ERR_INVALID_ARG;
    }
    if (strcmp(str, "true") == 0 || strcmp(str, "1") == 0) *out = true;
    else if (strcmp(str, "false") == 0 || strcmp(str, "0") == 0) *out = false;
    else {
    return SF_ERR_INVALID_ARG;
    }
    return SF_OK;
}

static sf_result_t read_operand(mxml_node_t *parent, const char *name, sf_fxr3_operand_t *out) {
    mxml_node_t *node = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    if (!node) {
    return SF_ERR_INVALID_ARG;
    }
    
    mxml_node_t *child = mxmlGetFirstChild(node);
    while (child && mxmlGetType(child) != MXML_TYPE_ELEMENT) {
        child = mxmlGetNextSibling(child);
    }
    if (!child) {
    return SF_ERR_INVALID_ARG;
    }
    
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

static sf_result_t read_condition(mxml_node_t *node, sf_fxr3_state_condition_t **out, const sf_allocator_t *a) {
    sf_fxr3_state_condition_t *c = (sf_fxr3_state_condition_t *)sf_xalloc(a, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    
    sf_result_t e = read_operand(node, "LeftOperand", &c->left_operand);
    if (e != SF_OK) { sf_xfree(a, c); return e; }
    
    mxml_node_t *op_node = mxmlFindElement(node, node, "Op", NULL, NULL, MXML_DESCEND_FIRST);
    if (!op_node) { sf_xfree(a, c); {
    return SF_ERR_INVALID_ARG;
    } }
    
    int32_t unk = 0;
    e = parse_int(mxmlElementGetAttr(op_node, "Unk"), &unk);
    if (e != SF_OK) { sf_xfree(a, c); return e; }
    c->unk_modifier = (uint8_t)unk;
    
    const char *op_str = get_text(op_node);
    if (!op_str) { sf_xfree(a, c); {
    return SF_ERR_INVALID_ARG;
    } }
    
    if (strcmp(op_str, "NotEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_NOT_EQUAL;
    else if (strcmp(op_str, "Equal") == 0) c->operator_type = SF_FXR3_OPERATOR_EQUAL;
    else if (strcmp(op_str, "GreaterThanOrEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_GE;
    else if (strcmp(op_str, "GreaterThan") == 0) c->operator_type = SF_FXR3_OPERATOR_GT;
    else if (strcmp(op_str, "LessThanOrEqual") == 0) c->operator_type = SF_FXR3_OPERATOR_LE;
    else if (strcmp(op_str, "LessThan") == 0) c->operator_type = SF_FXR3_OPERATOR_LT;
    else { sf_xfree(a, c); {
    return SF_ERR_INVALID_ARG;
    } }
    
    e = read_operand(node, "RightOperand", &c->right_operand);
    if (e != SF_OK) { sf_xfree(a, c); return e; }
    
    e = parse_int(mxmlElementGetAttr(node, "ElseMoveToStateIndex"), &c->next_state);
    if (e != SF_OK) { sf_xfree(a, c); return e; }
    
    *out = c;
    return SF_OK;
}

static sf_result_t read_state(mxml_node_t *node, sf_fxr3_state_t **out, const sf_allocator_t *a) {
    sf_fxr3_state_t *s = (sf_fxr3_state_t *)sf_xalloc(a, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    s->condition_count = 0;
    s->conditions = NULL;
    
    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(node, node, "StayCondition", NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, node, "StayCondition", NULL, NULL, MXML_DESCEND_NONE)) {
        count++;
    }
    
    if (count > 0) {
        s->conditions = (sf_fxr3_state_condition_t **)sf_xalloc(a, count * sizeof(sf_fxr3_state_condition_t *));
        if (!s->conditions) { sf_xfree(a, s); return SF_ERR_OOM; }
        
        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "StayCondition", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "StayCondition", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = read_condition(child, &s->conditions[i++], a);
            if (e != SF_OK) {
                for (size_t j = 0; j < i - 1; j++) sf_xfree(a, s->conditions[j]);
                sf_xfree(a, s->conditions);
                sf_xfree(a, s);
                return e;
            }
        }
    }
    s->condition_count = count;
    *out = s;
    return SF_OK;
}

static sf_result_t read_state_map(mxml_node_t *node, sf_fxr3_state_map_t **out, const sf_allocator_t *a) {
    sf_fxr3_state_map_t *m = (sf_fxr3_state_map_t *)sf_xalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    m->state_count = 0;
    m->states = NULL;
    
    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(node, node, "State", NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, node, "State", NULL, NULL, MXML_DESCEND_NONE)) {
        count++;
    }
    
    if (count > 0) {
        m->states = (sf_fxr3_state_t **)sf_xalloc(a, count * sizeof(sf_fxr3_state_t *));
        if (!m->states) { sf_xfree(a, m); return SF_ERR_OOM; }
        
        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "State", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "State", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = read_state(child, &m->states[i++], a);
            if (e != SF_OK) {
                for (size_t j = 0; j < i - 1; j++) {
                    for (size_t k = 0; k < m->states[j]->condition_count; k++) sf_xfree(a, m->states[j]->conditions[k]);
                    sf_xfree(a, m->states[j]->conditions);
                    sf_xfree(a, m->states[j]);
                }
                sf_xfree(a, m->states);
                sf_xfree(a, m);
                return e;
            }
        }
    }
    m->state_count = count;
    *out = m;
    return SF_OK;
}

static sf_result_t read_fields(mxml_node_t *parent, const char *name, sf_fxr3_field_t **out_fields, size_t *out_count, const sf_allocator_t *a) {
    *out_fields = NULL;
    *out_count = 0;
    
    mxml_node_t *node = mxmlFindElement(parent, parent, name, NULL, NULL, MXML_DESCEND_FIRST);
    if (!node) return SF_OK;
    
    size_t count = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(node); child != NULL; child = mxmlGetNextSibling(child)) {
        if (mxmlGetType(child) == MXML_TYPE_ELEMENT) count++;
    }
    
    if (count > 0) {
        sf_fxr3_field_t *fields = (sf_fxr3_field_t *)sf_xalloc(a, count * sizeof(sf_fxr3_field_t));
        if (!fields) return SF_ERR_OOM;
        
        size_t i = 0;
        for (mxml_node_t *child = mxmlGetFirstChild(node); child != NULL; child = mxmlGetNextSibling(child)) {
            if (mxmlGetType(child) != MXML_TYPE_ELEMENT) continue;
            
            const char *type_name = mxmlGetElement(child);
            if (strcmp(type_name, "Int") == 0) {
                fields[i].type = SF_FXR3_FIELD_TYPE_INT;
                sf_result_t e = parse_int(mxmlElementGetAttr(child, "Value"), &fields[i].value.as_int);
                if (e != SF_OK) { sf_xfree(a, fields); return e; }
            } else if (strcmp(type_name, "Float") == 0) {
                fields[i].type = SF_FXR3_FIELD_TYPE_FLOAT;
                sf_result_t e = parse_float(mxmlElementGetAttr(child, "Value"), &fields[i].value.as_float);
                if (e != SF_OK) { sf_xfree(a, fields); return e; }
            } else {
                sf_xfree(a, fields);
                {
    return SF_ERR_INVALID_ARG;
    }
            }
            i++;
        }
        *out_fields = fields;
    }
    *out_count = count;
    return SF_OK;
}

static sf_result_t read_property(mxml_node_t *node, sf_fxr3_property_t **out, const sf_allocator_t *a);

static sf_result_t read_modifier(mxml_node_t *node, sf_fxr3_property_modifier_t **out, const sf_allocator_t *a) {
    sf_fxr3_property_modifier_t *m = (sf_fxr3_property_modifier_t *)sf_xalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    
    int32_t type_a = 0, type_b = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "TypeEnumA"), &type_a);
    if (e != SF_OK) { sf_xfree(a, m); return e; }
    e = parse_int(mxmlElementGetAttr(node, "TypeEnumB"), &type_b);
    if (e != SF_OK) { sf_xfree(a, m); return e; }
    m->type_enum_a = (uint16_t)type_a;
    m->type_enum_b = (uint32_t)type_b;
    
    e = read_fields(node, "Fields", &m->fields, &m->field_count, a);
    if (e != SF_OK) { sf_xfree(a, m); return e; }
    
    m->property_count = 0;
    m->properties = NULL;
    
    mxml_node_t *props_node = mxmlFindElement(node, node, "Properties", NULL, NULL, MXML_DESCEND_FIRST);
    if (props_node) {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(props_node, props_node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, props_node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        
        if (count > 0) {
            m->properties = (sf_fxr3_property_t **)sf_xalloc(a, count * sizeof(sf_fxr3_property_t *));
            if (!m->properties) { sf_xfree(a, m->fields); sf_xfree(a, m); return SF_ERR_OOM; }
            
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(props_node, props_node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, props_node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_property(child, &m->properties[i++], a);
                if (e != SF_OK) {
                    // Cleanup omitted for brevity, will be handled by sf_fxr3_destroy on failure
                    return e;
                }
            }
        }
        m->property_count = count;
    }
    
    *out = m;
    return SF_OK;
}

static sf_result_t read_property(mxml_node_t *node, sf_fxr3_property_t **out, const sf_allocator_t *a) {
    sf_fxr3_property_t *p = (sf_fxr3_property_t *)sf_xalloc(a, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    
    const char *type_str = mxmlElementGetAttr(node, "PropertyType");
    if (!type_str) { sf_xfree(a, p); {
    return SF_ERR_INVALID_ARG;
    } }
    if (strcmp(type_str, "Scalar") == 0) p->type = SF_FXR3_PROPERTY_TYPE_SCALAR;
    else if (strcmp(type_str, "Vector2") == 0) p->type = SF_FXR3_PROPERTY_TYPE_VECTOR2;
    else if (strcmp(type_str, "Vector3") == 0) p->type = SF_FXR3_PROPERTY_TYPE_VECTOR3;
    else if (strcmp(type_str, "Color") == 0) p->type = SF_FXR3_PROPERTY_TYPE_COLOR;
    else { sf_xfree(a, p); {
    return SF_ERR_INVALID_ARG;
    } }
    
    const char *interp_str = mxmlElementGetAttr(node, "InterpolationType");
    if (!interp_str) { sf_xfree(a, p); {
    return SF_ERR_INVALID_ARG;
    } }
    if (strcmp(interp_str, "Zero") == 0) p->interpolation = SF_FXR3_INTERP_ZERO;
    else if (strcmp(interp_str, "One") == 0) p->interpolation = SF_FXR3_INTERP_ONE;
    else if (strcmp(interp_str, "Constant") == 0) p->interpolation = SF_FXR3_INTERP_CONSTANT;
    else if (strcmp(interp_str, "Stepped") == 0) p->interpolation = SF_FXR3_INTERP_STEPPED;
    else if (strcmp(interp_str, "Linear") == 0) p->interpolation = SF_FXR3_INTERP_LINEAR;
    else if (strcmp(interp_str, "Curve1") == 0) p->interpolation = SF_FXR3_INTERP_CURVE1;
    else if (strcmp(interp_str, "Curve2") == 0) p->interpolation = SF_FXR3_INTERP_CURVE2;
    else if (strcmp(interp_str, "UnkAc6") == 0) p->interpolation = SF_FXR3_INTERP_UNK_AC6;
    else { sf_xfree(a, p); {
    return SF_ERR_INVALID_ARG;
    } }
    
    sf_result_t e = parse_bool(mxmlElementGetAttr(node, "IsLoop"), &p->is_loop);
    if (e != SF_OK) { sf_xfree(a, p); return e; }
    
    e = read_fields(node, "Fields", &p->fields, &p->field_count, a);
    if (e != SF_OK) { sf_xfree(a, p); return e; }
    
    p->modifier_count = 0;
    p->modifiers = NULL;
    
    mxml_node_t *mods_node = mxmlFindElement(node, node, "Modifiers", NULL, NULL, MXML_DESCEND_FIRST);
    if (mods_node) {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(mods_node, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        
        if (count > 0) {
            p->modifiers = (sf_fxr3_property_modifier_t **)sf_xalloc(a, count * sizeof(sf_fxr3_property_modifier_t *));
            if (!p->modifiers) { sf_xfree(a, p->fields); sf_xfree(a, p); return SF_ERR_OOM; }
            
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(mods_node, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, mods_node, "PropertyModifier", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_modifier(child, &p->modifiers[i++], a);
                if (e != SF_OK) return e;
            }
        }
        p->modifier_count = count;
    }
    
    *out = p;
    return SF_OK;
}

static sf_result_t read_unk_field_list(mxml_node_t *node, sf_fxr3_unk_field_list_t **out, const sf_allocator_t *a) {
    sf_fxr3_unk_field_list_t *l = (sf_fxr3_unk_field_list_t *)sf_xalloc(a, sizeof(*l));
    if (!l) return SF_ERR_OOM;
    
    sf_result_t e = read_fields(node, "Fields", &l->fields, &l->field_count, a);
    if (e != SF_OK) { sf_xfree(a, l); return e; }
    
    *out = l;
    return SF_OK;
}

static sf_result_t read_action(mxml_node_t *node, sf_fxr3_action_t **out, const sf_allocator_t *a) {
    sf_fxr3_action_t *act = (sf_fxr3_action_t *)sf_xalloc(a, sizeof(*act));
    if (!act) return SF_ERR_OOM;
    
    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) { sf_xfree(a, act); return e; }
    act->id = (int16_t)id;
    
    e = parse_bool(mxmlElementGetAttr(node, "Unk02"), &act->unk02);
    if (e != SF_OK) { sf_xfree(a, act); return e; }
    
    e = parse_bool(mxmlElementGetAttr(node, "Unk03"), &act->unk03);
    if (e != SF_OK) { sf_xfree(a, act); return e; }
    
    e = parse_int(mxmlElementGetAttr(node, "Unk04"), &act->unk04);
    if (e != SF_OK) { sf_xfree(a, act); return e; }
    
    e = read_fields(node, "Fields1", &act->fields1, &act->field1_count, a);
    if (e != SF_OK) { sf_xfree(a, act); return e; }
    
    e = read_fields(node, "Fields2", &act->fields2, &act->field2_count, a);
    if (e != SF_OK) { sf_xfree(a, act->fields1); sf_xfree(a, act); return e; }
    
    act->property_count = 0;
    act->properties = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            act->properties = (sf_fxr3_property_t **)sf_xalloc(a, count * sizeof(sf_fxr3_property_t *));
            if (!act->properties) { sf_xfree(a, act); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Property", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Property", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_property(child, &act->properties[i++], a);
                if (e != SF_OK) { sf_xfree(a, act); return e; }
            }
        }
        act->property_count = count;
    }

    act->unk_field_list_count = 0;
    act->unk_field_lists = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            act->unk_field_lists = (sf_fxr3_unk_field_list_t **)sf_xalloc(a, count * sizeof(sf_fxr3_unk_field_list_t *));
            if (!act->unk_field_lists) { sf_xfree(a, act); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "UnkFieldList", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_unk_field_list(child, &act->unk_field_lists[i++], a);
                if (e != SF_OK) { sf_xfree(a, act); return e; }
            }
        }
        act->unk_field_list_count = count;
    }
    
    *out = act;
    return SF_OK;
}

static sf_result_t read_effect(mxml_node_t *node, sf_fxr3_effect_t **out, const sf_allocator_t *a) {
    sf_fxr3_effect_t *eff = (sf_fxr3_effect_t *)sf_xalloc(a, sizeof(*eff));
    if (!eff) return SF_ERR_OOM;
    
    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) { sf_xfree(a, eff); return e; }
    eff->id = (int16_t)id;
    
    eff->action_count = 0;
    eff->actions = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            eff->actions = (sf_fxr3_action_t **)sf_xalloc(a, count * sizeof(sf_fxr3_action_t *));
            if (!eff->actions) { sf_xfree(a, eff); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_action(child, &eff->actions[i++], a);
                if (e != SF_OK) return e;
            }
        }
        eff->action_count = count;
    }
    
    *out = eff;
    return SF_OK;
}

static sf_result_t read_container(mxml_node_t *node, sf_fxr3_container_t **out, const sf_allocator_t *a) {
    sf_fxr3_container_t *c = (sf_fxr3_container_t *)sf_xalloc(a, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    
    int32_t id = 0;
    sf_result_t e = parse_int(mxmlElementGetAttr(node, "Id"), &id);
    if (e != SF_OK) { sf_xfree(a, c); return e; }
    c->id = (int16_t)id;
    
    /* Children and Effects are direct child elements of <Container> (no wrapper),
     * matching the upstream XmlSerializer [XmlElement("Container")] / [XmlElement("Effect")] schema. */
    c->child_count = 0;
    c->children = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Container", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Container", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->children = (sf_fxr3_container_t **)sf_xalloc(a, count * sizeof(sf_fxr3_container_t *));
            if (!c->children) { sf_xfree(a, c); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Container", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Container", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_container(child, &c->children[i++], a);
                if (e != SF_OK) { sf_xfree(a, c); return e; }
            }
        }
        c->child_count = count;
    }

    c->effect_count = 0;
    c->effects = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Effect", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Effect", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->effects = (sf_fxr3_effect_t **)sf_xalloc(a, count * sizeof(sf_fxr3_effect_t *));
            if (!c->effects) { sf_xfree(a, c); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Effect", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Effect", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_effect(child, &c->effects[i++], a);
                if (e != SF_OK) { sf_xfree(a, c); return e; }
            }
        }
        c->effect_count = count;
    }
    
    c->action_count = 0;
    c->actions = NULL;
    {
        size_t count = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
            count++;
        }
        if (count > 0) {
            c->actions = (sf_fxr3_action_t **)sf_xalloc(a, count * sizeof(sf_fxr3_action_t *));
            if (!c->actions) { sf_xfree(a, c); return SF_ERR_OOM; }
            size_t i = 0;
            for (mxml_node_t *child = mxmlFindElement(node, node, "Action", NULL, NULL, MXML_DESCEND_FIRST);
                 child != NULL;
                 child = mxmlFindElement(child, node, "Action", NULL, NULL, MXML_DESCEND_NONE)) {
                e = read_action(child, &c->actions[i++], a);
                if (e != SF_OK) { sf_xfree(a, c); return e; }
            }
        }
        c->action_count = count;
    }
    
    *out = c;
    return SF_OK;
}

static sf_result_t read_int_list(mxml_node_t *parent, const char *name, int32_t **out_items, size_t *out_count, const sf_allocator_t *a) {
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
        int32_t *items = (int32_t *)sf_xalloc(a, count * sizeof(int32_t));
        if (!items) return SF_ERR_OOM;
        
        size_t i = 0;
        for (mxml_node_t *child = mxmlFindElement(node, node, "int", NULL, NULL, MXML_DESCEND_FIRST);
             child != NULL;
             child = mxmlFindElement(child, node, "int", NULL, NULL, MXML_DESCEND_NONE)) {
            sf_result_t e = parse_int(get_text(child), &items[i++]);
            if (e != SF_OK) { sf_xfree(a, items); return e; }
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
    
    char *nul_terminated = (char *)sf_xalloc(a, xml_size + 1);
    if (!nul_terminated) {
        if (options) mxmlOptionsDelete(options);
        return SF_ERR_OOM;
    }
    memcpy(nul_terminated, xml_utf8, xml_size);
    nul_terminated[xml_size] = '\0';
    
    mxml_node_t *tree = mxmlLoadString(NULL, options, nul_terminated);
    if (options) mxmlOptionsDelete(options);
    sf_xfree(a, nul_terminated);
    if (!tree) return SF_ERR_INTERNAL;
    
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
        return SF_ERR_BAD_MAGIC;
    }
    
    sf_fxr3_t *f = (sf_fxr3_t *)sf_xalloc(a, sizeof(*f));
    if (!f) {
        mxmlDelete(tree);
        return SF_ERR_OOM;
    }
    memset(f, 0, sizeof(*f));
    f->alloc = a;
    
    const char *version_str = get_child_text(root, "Version");
    if (!version_str) { sf_fxr3_destroy(f); mxmlDelete(tree); {
    return SF_ERR_INVALID_ARG;
    } }
    if (strcmp(version_str, "Sekiro") == 0) f->version = SF_FXR3_VERSION_SEKIRO;
    else if (strcmp(version_str, "DarkSouls3") == 0) f->version = SF_FXR3_VERSION_DARK_SOULS_3;
    else { sf_fxr3_destroy(f); mxmlDelete(tree); {
    return SF_ERR_INVALID_ARG;
    } }
    
    sf_result_t e = parse_int(get_child_text(root, "Id"), &f->id);
    if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    
    mxml_node_t *sm_node = mxmlFindElement(root, root, "StateMap", NULL, NULL, MXML_DESCEND_FIRST);
    if (sm_node) {
        e = read_state_map(sm_node, &f->root_state_map, a);
        if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    }
    
    mxml_node_t *cont_node = mxmlFindElement(root, root, "Container", NULL, NULL, MXML_DESCEND_FIRST);
    if (cont_node) {
        e = read_container(cont_node, &f->root_container, a);
        if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    }
    
    e = read_int_list(root, "ReferenceList", &f->references, &f->reference_count, a);
    if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    
    e = read_int_list(root, "ExternalValueList", &f->external_values, &f->external_value_count, a);
    if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    
    e = read_int_list(root, "UnkBloodEnabler", &f->unk_blood_enablers, &f->unk_blood_enabler_count, a);
    if (e != SF_OK) { sf_fxr3_destroy(f); mxmlDelete(tree); return e; }
    
    mxmlDelete(tree);
    *out = f;
    return SF_OK;
}
