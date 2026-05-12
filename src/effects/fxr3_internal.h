#ifndef SF_FXR3_INTERNAL_H
#define SF_FXR3_INTERNAL_H

#include "souls_formats/sf_fxr3.h"

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

#endif
