/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FXR3 XML writer (sf_fxr3_t -> mxml DOM -> string).
 */

#include "souls_formats/sf_fxr3.h"
#include "internal/sf_internal.h"
#include "fxr3_internal.h"

#include <mxml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_int_element(mxml_node_t *parent, const char *name, int32_t value) {
    mxml_node_t *node = mxmlNewElement(parent, name);
    mxmlNewOpaquef(node, "%d", value);
}

static void add_string_element(mxml_node_t *parent, const char *name, const char *value) {
    mxml_node_t *node = mxmlNewElement(parent, name);
    mxmlNewOpaque(node, value);
}

static void write_operand(mxml_node_t *parent, const char *name, sf_fxr3_operand_t op) {
    mxml_node_t *node = mxmlNewElement(parent, name);
    mxml_node_t *child = NULL;
    switch (op.type) {
        case SF_FXR3_OPERAND_LITERAL:
            child = mxmlNewElement(node, "Literal");
            mxmlElementSetAttrf(child, "Value", "%g", op.value.as_literal);
            break;
        case SF_FXR3_OPERAND_EXTERNAL:
            child = mxmlNewElement(node, "External");
            mxmlElementSetAttrf(child, "Value", "%d", op.value.as_external);
            break;
        case SF_FXR3_OPERAND_TIME_OF_DAY:
            mxmlNewElement(node, "UnkMinus2");
            break;
        case SF_FXR3_OPERAND_STATE_TIME:
            mxmlNewElement(node, "StateTime");
            break;
    }
}

static void write_condition(mxml_node_t *parent, const sf_fxr3_state_condition_t *c) {
    mxml_node_t *node = mxmlNewElement(parent, "StayCondition");
    
    write_operand(node, "LeftOperand", c->left_operand);
    
    mxml_node_t *op_node = mxmlNewElement(node, "Op");
    mxmlElementSetAttrf(op_node, "Unk", "%u", c->unk_modifier);
    const char *op_str = "NotEqual";
    switch (c->operator_type) {
        case SF_FXR3_OPERATOR_NOT_EQUAL: op_str = "NotEqual"; break;
        case SF_FXR3_OPERATOR_EQUAL: op_str = "Equal"; break;
        case SF_FXR3_OPERATOR_GE: op_str = "GreaterThanOrEqual"; break;
        case SF_FXR3_OPERATOR_GT: op_str = "GreaterThan"; break;
        case SF_FXR3_OPERATOR_LE: op_str = "LessThanOrEqual"; break;
        case SF_FXR3_OPERATOR_LT: op_str = "LessThan"; break;
    }
    mxmlNewOpaque(op_node, op_str);
    
    write_operand(node, "RightOperand", c->right_operand);
    
    mxmlElementSetAttrf(node, "ElseMoveToStateIndex", "%d", c->next_state);
}

static void write_state(mxml_node_t *parent, const sf_fxr3_state_t *s) {
    mxml_node_t *node = mxmlNewElement(parent, "State");
    for (size_t i = 0; i < s->condition_count; i++) {
        write_condition(node, s->conditions[i]);
    }
}

static void write_state_map(mxml_node_t *parent, const sf_fxr3_state_map_t *m) {
    mxml_node_t *node = mxmlNewElement(parent, "StateMap");
    for (size_t i = 0; i < m->state_count; i++) {
        write_state(node, m->states[i]);
    }
}

static void write_fields(mxml_node_t *parent, const char *name, const sf_fxr3_field_t *fields, size_t count) {
    if (count == 0) return;
    mxml_node_t *node = mxmlNewElement(parent, name);
    for (size_t i = 0; i < count; i++) {
        if (fields[i].type == SF_FXR3_FIELD_TYPE_INT) {
            mxml_node_t *f = mxmlNewElement(node, "Int");
            mxmlElementSetAttrf(f, "Value", "%d", fields[i].value.as_int);
        } else {
            mxml_node_t *f = mxmlNewElement(node, "Float");
            mxmlElementSetAttrf(f, "Value", "%g", fields[i].value.as_float);
        }
    }
}

static void write_property(mxml_node_t *parent, const sf_fxr3_property_t *p);

static void write_modifier(mxml_node_t *parent, const sf_fxr3_property_modifier_t *m) {
    mxml_node_t *node = mxmlNewElement(parent, "PropertyModifier");
    mxmlElementSetAttrf(node, "TypeEnumA", "%u", m->type_enum_a);
    mxmlElementSetAttrf(node, "TypeEnumB", "%u", m->type_enum_b);
    write_fields(node, "Fields", m->fields, m->field_count);
    
    for (size_t i = 0; i < m->property_count; i++) {
        write_property(node, m->properties[i]);
    }
}

static void write_property(mxml_node_t *parent, const sf_fxr3_property_t *p) {
    mxml_node_t *node = mxmlNewElement(parent, "Property");
    
    const char *type_str = "Scalar";
    switch (p->type) {
        case SF_FXR3_PROPERTY_TYPE_SCALAR: type_str = "Scalar"; break;
        case SF_FXR3_PROPERTY_TYPE_VECTOR2: type_str = "Vector2"; break;
        case SF_FXR3_PROPERTY_TYPE_VECTOR3: type_str = "Vector3"; break;
        case SF_FXR3_PROPERTY_TYPE_COLOR: type_str = "Color"; break;
    }
    mxmlElementSetAttr(node, "PropertyType", type_str);
    
    const char *interp_str = "Zero";
    switch (p->interpolation) {
        case SF_FXR3_INTERP_ZERO: interp_str = "Zero"; break;
        case SF_FXR3_INTERP_ONE: interp_str = "One"; break;
        case SF_FXR3_INTERP_CONSTANT: interp_str = "Constant"; break;
        case SF_FXR3_INTERP_STEPPED: interp_str = "Stepped"; break;
        case SF_FXR3_INTERP_LINEAR: interp_str = "Linear"; break;
        case SF_FXR3_INTERP_CURVE1: interp_str = "Curve1"; break;
        case SF_FXR3_INTERP_CURVE2: interp_str = "Curve2"; break;
        case SF_FXR3_INTERP_UNK_AC6: interp_str = "UnkAc6"; break;
    }
    mxmlElementSetAttr(node, "InterpolationType", interp_str);
    
    mxmlElementSetAttr(node, "IsLoop", p->is_loop ? "true" : "false");
    
    write_fields(node, "Fields", p->fields, p->field_count);
    
    for (size_t i = 0; i < p->modifier_count; i++) {
        write_modifier(node, p->modifiers[i]);
    }
}

static void write_unk_field_list(mxml_node_t *parent, const sf_fxr3_unk_field_list_t *l) {
    mxml_node_t *node = mxmlNewElement(parent, "UnkFieldList");
    write_fields(node, "Fields", l->fields, l->field_count);
}

static void write_action(mxml_node_t *parent, const sf_fxr3_action_t *a) {
    mxml_node_t *node = mxmlNewElement(parent, "Action");
    mxmlElementSetAttrf(node, "Id", "%d", a->id);
    mxmlElementSetAttr(node, "Unk02", a->unk02 ? "true" : "false");
    mxmlElementSetAttr(node, "Unk03", a->unk03 ? "true" : "false");
    mxmlElementSetAttrf(node, "Unk04", "%d", a->unk04);
    
    write_fields(node, "Fields1", a->fields1, a->field1_count);
    write_fields(node, "Fields2", a->fields2, a->field2_count);
    
    for (size_t i = 0; i < a->property_count; i++) {
        write_property(node, a->properties[i]);
    }
    
    for (size_t i = 0; i < a->unk_field_list_count; i++) {
        write_unk_field_list(node, a->unk_field_lists[i]);
    }
}

static void write_effect(mxml_node_t *parent, const sf_fxr3_effect_t *e) {
    mxml_node_t *node = mxmlNewElement(parent, "Effect");
    mxmlElementSetAttrf(node, "Id", "%d", e->id);
    
    for (size_t i = 0; i < e->action_count; i++) {
        write_action(node, e->actions[i]);
    }
}

static void write_container(mxml_node_t *parent, const sf_fxr3_container_t *c) {
    mxml_node_t *node = mxmlNewElement(parent, "Container");
    mxmlElementSetAttrf(node, "Id", "%d", c->id);
    
    for (size_t i = 0; i < c->child_count; i++) {
        write_container(node, c->children[i]);
    }
    
    for (size_t i = 0; i < c->effect_count; i++) {
        write_effect(node, c->effects[i]);
    }
    
    for (size_t i = 0; i < c->action_count; i++) {
        write_action(node, c->actions[i]);
    }
}

static void write_int_list(mxml_node_t *parent, const char *name, const int32_t *items, size_t count) {
    if (count == 0) return;
    mxml_node_t *node = mxmlNewElement(parent, name);
    for (size_t i = 0; i < count; i++) {
        add_int_element(node, "int", items[i]);
    }
}

SF_API sf_result_t sf_fxr3_to_xml(const sf_fxr3_t *f, char **out_xml_utf8, size_t *out_size,
                                  const sf_allocator_t *a) {
    SF_CHECK_ARG(f != NULL && out_xml_utf8 != NULL && out_size != NULL);
    *out_xml_utf8 = NULL;
    *out_size = 0;
    
    mxml_node_t *root = mxmlNewElement(NULL, "FXR3");
    if (!root) return SF_ERR_INTERNAL;
    
    add_string_element(root, "Version", f->version == SF_FXR3_VERSION_SEKIRO ? "Sekiro" : "DarkSouls3");
    add_int_element(root, "Id", f->id);
    
    if (f->root_state_map) write_state_map(root, f->root_state_map);
    if (f->root_container) write_container(root, f->root_container);
    
    write_int_list(root, "ReferenceList", f->references, f->reference_count);
    write_int_list(root, "ExternalValueList", f->external_values, f->external_value_count);
    write_int_list(root, "UnkBloodEnabler", f->unk_blood_enablers, f->unk_blood_enabler_count);
    
    mxml_options_t *options = mxmlOptionsNew();
    if (!options) {
        mxmlDelete(root);
        return SF_ERR_INTERNAL;
    }
    mxmlOptionsSetWrapMargin(options, 0);
    
    char *xml = mxmlSaveAllocString(root, options);
    mxmlOptionsDelete(options);
    mxmlDelete(root);
    
    if (!xml) return SF_ERR_INTERNAL;
    
    *out_size = strlen(xml);
    *out_xml_utf8 = xml; // Caller frees with free()
    
    return SF_OK;
}
