/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 7 T20 — FXR3 synthetic binary round-trip.
 *
 * Hand-builds minimal but non-trivial DS3 (version=4) and Sekiro (version=5)
 * FXR3 fixtures covering every tagged-union variant:
 *   - 4 OperandType variants: Literal, External, StateTime, TimeOfDay
 *   - 2 FieldType variants:   Int, Float
 *   - InterpolationType:      Constant + UnkAc6 (Sekiro only)
 *   - Sekiro side-tables:     ReferenceList, ExternalValueList,
 *                             UnkBloodEnabler (empty)
 *
 * Round-trip strategy: the writer normalises section padding and may emit
 * canonical zero-offsets that differ from a hand-built minimal input. The
 * test therefore verifies STABLE round-trip rather than write==input:
 *
 *   bytes_in → read → write → bytes_a → read → write → bytes_b
 *
 * It asserts (a) every key accessor on the freshly-read object matches the
 * fixture, (b) the canonical write output round-trips back to the same
 * object, and (c) the writer is deterministic (memcmp(bytes_a, bytes_b)).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FXR3.cs
 */
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_fxr3.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Buffer builder ─────────────────────────────────────────────────────── */
#define BUF_CAP 4096

typedef struct {
    uint8_t bytes[BUF_CAP];
    size_t  pos;
} buf_t;

static void put_u8(buf_t *b, uint8_t v) {
    TEST_ASSERT_TRUE(b->pos + 1 <= BUF_CAP);
    b->bytes[b->pos++] = v;
}

static void put_i16(buf_t *b, int16_t v) {
    TEST_ASSERT_TRUE(b->pos + 2 <= BUF_CAP);
    b->bytes[b->pos++] = (uint8_t)(v & 0xFF);
    b->bytes[b->pos++] = (uint8_t)((uint16_t)v >> 8);
}

static void put_u16(buf_t *b, uint16_t v) {
    TEST_ASSERT_TRUE(b->pos + 2 <= BUF_CAP);
    b->bytes[b->pos++] = (uint8_t)(v & 0xFF);
    b->bytes[b->pos++] = (uint8_t)(v >> 8);
}

static void put_i32(buf_t *b, int32_t v) {
    TEST_ASSERT_TRUE(b->pos + 4 <= BUF_CAP);
    uint32_t u = (uint32_t)v;
    b->bytes[b->pos++] = (uint8_t)(u & 0xFF);
    b->bytes[b->pos++] = (uint8_t)((u >> 8) & 0xFF);
    b->bytes[b->pos++] = (uint8_t)((u >> 16) & 0xFF);
    b->bytes[b->pos++] = (uint8_t)((u >> 24) & 0xFF);
}

static void put_f32(buf_t *b, float v) {
    uint32_t bits;
    memcpy(&bits, &v, 4);
    put_i32(b, (int32_t)bits);
}

static void put_bytes(buf_t *b, const void *src, size_t n) {
    TEST_ASSERT_TRUE(b->pos + n <= BUF_CAP);
    memcpy(b->bytes + b->pos, src, n);
    b->pos += n;
}

static void patch_i32(buf_t *b, size_t off, int32_t v) {
    TEST_ASSERT_TRUE(off + 4 <= b->pos);
    uint32_t u = (uint32_t)v;
    b->bytes[off]     = (uint8_t)(u & 0xFF);
    b->bytes[off + 1] = (uint8_t)((u >> 8) & 0xFF);
    b->bytes[off + 2] = (uint8_t)((u >> 16) & 0xFF);
    b->bytes[off + 3] = (uint8_t)((u >> 24) & 0xFF);
}

static void pad16(buf_t *b) {
    while (b->pos % 16 != 0) put_u8(b, 0);
}

/* ── Fixture writers (sub-headers) ──────────────────────────────────────── */

/* Writes the 11 section offset+count pairs plus trailing i32(1),i32(0).
 * Returns the patch positions for each offset that needs filling later. */
typedef struct {
    size_t state_map_off;
    size_t state_off;
    size_t transition_off;
    size_t container_off;
    size_t effect_off;
    size_t action_off;
    size_t property_off;
    size_t modifier_off;
    size_t cond_prop_off;
    size_t unk_field_list_off;
    size_t field_off;
    /* Sekiro-only */
    size_t reference_off;
    size_t external_off;
    size_t blood_off;
} header_patches_t;

static void put_header(buf_t *b, sf_fxr3_version_t version, int32_t id,
                       header_patches_t *out_p,
                       int32_t state_count, int32_t transition_count,
                       int32_t container_count, int32_t effect_count,
                       int32_t action_count, int32_t property_count,
                       int32_t cond_prop_count,
                       int32_t field_count,
                       int32_t reference_count, int32_t external_count,
                       int32_t blood_count) {
    put_bytes(b, "FXR\0", 4);
    put_i16(b, 0);
    put_u16(b, (uint16_t)version);
    put_i32(b, 1);
    put_i32(b, id);
    /* pair 0 */
    out_p->state_map_off = b->pos; put_i32(b, 0); put_i32(b, 1);
    /* pair 1 */
    out_p->state_off = b->pos; put_i32(b, 0); put_i32(b, state_count);
    /* pair 2 */
    out_p->transition_off = b->pos; put_i32(b, 0); put_i32(b, transition_count);
    /* pair 3 — container */
    out_p->container_off = b->pos; put_i32(b, 0); put_i32(b, container_count);
    /* pair 4 — effect */
    out_p->effect_off = b->pos; put_i32(b, 0); put_i32(b, effect_count);
    /* pair 5 — action */
    out_p->action_off = b->pos; put_i32(b, 0); put_i32(b, action_count);
    /* pair 6 — property */
    out_p->property_off = b->pos; put_i32(b, 0); put_i32(b, property_count);
    /* pair 7 — modifier */
    out_p->modifier_off = b->pos; put_i32(b, 0); put_i32(b, 0);
    /* pair 8 — conditional property */
    out_p->cond_prop_off = b->pos; put_i32(b, 0); put_i32(b, cond_prop_count);
    /* pair 9 — unk field list */
    out_p->unk_field_list_off = b->pos; put_i32(b, 0); put_i32(b, 0);
    /* pair 10 — field */
    out_p->field_off = b->pos; put_i32(b, 0); put_i32(b, field_count);
    /* trailing 1, 0 */
    put_i32(b, 1);
    put_i32(b, 0);
    if (version == SF_FXR3_VERSION_SEKIRO) {
        out_p->reference_off = b->pos; put_i32(b, 0); put_i32(b, reference_count);
        out_p->external_off  = b->pos; put_i32(b, 0); put_i32(b, external_count);
        out_p->blood_off     = b->pos; put_i32(b, 0); put_i32(b, blood_count);
        put_i32(b, 0); /* UnkEmpty offset (always 0) */
        put_i32(b, 0); /* UnkEmpty count (always 0) */
    }
}

/* StateMap header (16 bytes). states_offset patched later. */
static size_t put_state_map(buf_t *b, int32_t state_count) {
    put_i32(b, 0);
    put_i32(b, state_count);
    size_t patch = b->pos;
    put_i32(b, 0);
    put_i32(b, 0);
    return patch;
}

/* State header (16 bytes). transitions_offset patched later. */
static size_t put_state(buf_t *b, int32_t condition_count) {
    put_i32(b, 0);
    put_i32(b, condition_count);
    size_t patch = b->pos;
    put_i32(b, 0);
    put_i32(b, 0);
    return patch;
}

/* StateCondition (96 bytes). field offsets patched later. */
typedef struct {
    sf_fxr3_operator_type_t op;
    int32_t next_state;
    sf_fxr3_operand_type_t left_type;
    bool left_has_value;
    float   left_lit;
    int32_t left_ext;
    sf_fxr3_operand_type_t right_type;
    bool right_has_value;
    float   right_lit;
    int32_t right_ext;
} cond_spec_t;

typedef struct {
    size_t left_patch;
    size_t right_patch;
    cond_spec_t spec;
} cond_patch_t;

static cond_patch_t put_condition(buf_t *b, cond_spec_t spec) {
    cond_patch_t p = {0};
    p.spec = spec;
    int16_t packed_op = (int16_t)((uint8_t)spec.op);
    put_i16(b, packed_op);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, 0);
    put_i32(b, spec.next_state);
    put_i32(b, 0);
    put_i16(b, (int16_t)spec.left_type);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, 0);
    put_i32(b, spec.left_has_value ? 1 : 0);
    put_i32(b, 0);
    p.left_patch = b->pos; put_i32(b, 0);
    for (int i = 0; i < 5; i++) put_i32(b, 0);
    put_i16(b, (int16_t)spec.right_type);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, 0);
    put_i32(b, spec.right_has_value ? 1 : 0);
    put_i32(b, 0);
    p.right_patch = b->pos; put_i32(b, 0);
    for (int i = 0; i < 5; i++) put_i32(b, 0);
    return p;
}

/* Container (44 bytes). Offsets patched later. */
static void put_container(buf_t *b, int16_t id,
                          int32_t effect_count, int32_t action_count, int32_t child_count,
                          size_t *out_effects_off, size_t *out_actions_off,
                          size_t *out_children_off) {
    put_i16(b, id);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, 0);
    put_i32(b, effect_count);
    put_i32(b, action_count);
    put_i32(b, child_count);
    put_i32(b, 0);
    *out_effects_off  = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    *out_actions_off  = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    *out_children_off = b->pos; put_i32(b, 0);
    put_i32(b, 0);
}

/* Effect (32 bytes). actions_offset patched later. */
static size_t put_effect(buf_t *b, int16_t id, int32_t action_count) {
    put_i16(b, id);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, 0);
    put_i32(b, 0);
    put_i32(b, action_count);
    put_i32(b, 0);
    put_i32(b, 0);
    size_t patch = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    return patch;
}

/* Action (60 bytes). offsets patched later. */
static void put_action(buf_t *b, int16_t id, int32_t field1_count, int32_t field2_count,
                       int32_t property1_count, int32_t property2_count,
                       size_t *out_fields_off, size_t *out_unk_off, size_t *out_props_off) {
    put_i16(b, id);
    put_u8(b, 0); put_u8(b, 0);
    put_i32(b, 0); /* unk04 */
    put_i32(b, field1_count);
    put_i32(b, 0); /* ulc */
    put_i32(b, property1_count);
    put_i32(b, field2_count);
    put_i32(b, 0);
    put_i32(b, property2_count);
    *out_fields_off = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    *out_unk_off    = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    *out_props_off  = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    put_i32(b, 0);
    put_i32(b, 0);
}

/* Property (top-level, 28 bytes). fields_offset and modifiers_offset patched later. */
static size_t put_property(buf_t *b, sf_fxr3_property_type_t type,
                           sf_fxr3_property_interpolation_type_t interp,
                           bool is_loop, int32_t field_count) {
    int16_t type_a = (int16_t)((int32_t)type | ((int32_t)interp << 4) | ((is_loop ? 1 : 0) << 12));
    int32_t type_b = ((int32_t)type | ((int32_t)interp << 2)) + ((is_loop ? 1 : 0) << 4);
    put_i16(b, type_a);
    put_u8(b, 0); put_u8(b, 1);
    put_i32(b, type_b);
    put_i32(b, field_count);
    put_i32(b, 0);
    size_t patch_fields = b->pos; put_i32(b, 0);
    put_i32(b, 0);
    put_i32(b, 0); /* modifiers offset (no modifiers used) */
    put_i32(b, 0);
    put_i32(b, 0); /* modifier count */
    put_i32(b, 0);
    return patch_fields;
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

/* Build DS3 fixture. Returns total size. Populates condition operand patches
 * and the float-field offset for later reference if needed. */
static size_t build_ds3_fixture(buf_t *b) {
    header_patches_t hp;
    put_header(b, SF_FXR3_VERSION_DARK_SOULS_3, 1, &hp,
               /* states */ 1, /* transitions */ 4,
               /* containers */ 1, /* effects */ 1, /* actions */ 1,
               /* properties */ 1, /* cond_prop */ 0,
               /* fields */ 1,
               0, 0, 0);

    pad16(b);
    /* StateMap at b->pos */
    patch_i32(b, (int32_t)hp.state_map_off, (int32_t)b->pos);
    size_t sm_states_patch = put_state_map(b, 1);

    pad16(b);
    /* States at b->pos */
    patch_i32(b, (int32_t)hp.state_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)sm_states_patch, (int32_t)b->pos);
    size_t state_trans_patch = put_state(b, 4);

    pad16(b);
    /* Transitions at b->pos */
    patch_i32(b, (int32_t)hp.transition_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)state_trans_patch, (int32_t)b->pos);
    cond_patch_t c0 = put_condition(b, (cond_spec_t){
        .op = SF_FXR3_OPERATOR_EQUAL, .next_state = 1,
        .left_type = SF_FXR3_OPERAND_LITERAL, .left_has_value = true, .left_lit = 0.5f,
        .right_type = SF_FXR3_OPERAND_STATE_TIME, .right_has_value = false,
    });
    cond_patch_t c1 = put_condition(b, (cond_spec_t){
        .op = SF_FXR3_OPERATOR_GT, .next_state = 2,
        .left_type = SF_FXR3_OPERAND_EXTERNAL, .left_has_value = true, .left_ext = 42,
        .right_type = SF_FXR3_OPERAND_LITERAL, .right_has_value = true, .right_lit = 1.0f,
    });
    cond_patch_t c2 = put_condition(b, (cond_spec_t){
        .op = SF_FXR3_OPERATOR_NOT_EQUAL, .next_state = 3,
        .left_type = SF_FXR3_OPERAND_STATE_TIME, .left_has_value = false,
        .right_type = SF_FXR3_OPERAND_LITERAL, .right_has_value = true, .right_lit = 2.0f,
    });
    cond_patch_t c3 = put_condition(b, (cond_spec_t){
        .op = SF_FXR3_OPERATOR_GE, .next_state = 4,
        .left_type = SF_FXR3_OPERAND_TIME_OF_DAY, .left_has_value = false,
        .right_type = SF_FXR3_OPERAND_LITERAL, .right_has_value = true, .right_lit = 12.0f,
    });

    pad16(b);
    /* Container at b->pos */
    patch_i32(b, (int32_t)hp.container_off, (int32_t)b->pos);
    size_t cont_effects_off = 0, cont_actions_off = 0, cont_children_off = 0;
    put_container(b, 100, 1, 0, 0, &cont_effects_off, &cont_actions_off, &cont_children_off);

    pad16(b);
    /* Effect at b->pos */
    patch_i32(b, (int32_t)hp.effect_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)cont_effects_off, (int32_t)b->pos);
    size_t eff_actions_patch = put_effect(b, 200, 1);

    pad16(b);
    /* Action at b->pos */
    patch_i32(b, (int32_t)hp.action_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)eff_actions_patch, (int32_t)b->pos);
    size_t act_fields_off = 0, act_unk_off = 0, act_props_off = 0;
    put_action(b, 300, 0, 0, 1, 0, &act_fields_off, &act_unk_off, &act_props_off);

    pad16(b);
    /* Property at b->pos */
    patch_i32(b, (int32_t)hp.property_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)act_props_off, (int32_t)b->pos);
    size_t prop_fields_patch =
        put_property(b, SF_FXR3_PROPERTY_TYPE_SCALAR, SF_FXR3_INTERP_CONSTANT, false, 1);

    pad16(b);
    /* Fields at b->pos */
    patch_i32(b, (int32_t)hp.field_off, (int32_t)b->pos);
    /* condition operand fields */
    patch_i32(b, (int32_t)c0.left_patch, (int32_t)b->pos);  put_f32(b, c0.spec.left_lit);
    patch_i32(b, (int32_t)c1.left_patch, (int32_t)b->pos);  put_i32(b, c1.spec.left_ext);
    patch_i32(b, (int32_t)c1.right_patch, (int32_t)b->pos); put_f32(b, c1.spec.right_lit);
    patch_i32(b, (int32_t)c2.right_patch, (int32_t)b->pos); put_f32(b, c2.spec.right_lit);
    patch_i32(b, (int32_t)c3.right_patch, (int32_t)b->pos); put_f32(b, c3.spec.right_lit);
    (void)c0; /* c0.right has no value */
    /* property field (Float = 1.0f, in range so heuristic detects as float) */
    patch_i32(b, (int32_t)prop_fields_patch, (int32_t)b->pos);
    put_f32(b, 1.0f);

    pad16(b);
    return b->pos;
}

static size_t build_sekiro_fixture(buf_t *b) {
    header_patches_t hp;
    put_header(b, SF_FXR3_VERSION_SEKIRO, 42, &hp,
               /* states */ 1, /* transitions */ 1,
               /* containers */ 1, /* effects */ 1, /* actions */ 1,
               /* properties */ 2, /* cond_prop */ 0,
               /* fields */ 4,
               /* references */ 2, /* externals */ 1, /* blood */ 0);

    pad16(b);
    patch_i32(b, (int32_t)hp.state_map_off, (int32_t)b->pos);
    size_t sm_states_patch = put_state_map(b, 1);

    pad16(b);
    patch_i32(b, (int32_t)hp.state_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)sm_states_patch, (int32_t)b->pos);
    size_t state_trans_patch = put_state(b, 1);

    pad16(b);
    patch_i32(b, (int32_t)hp.transition_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)state_trans_patch, (int32_t)b->pos);
    cond_patch_t c0 = put_condition(b, (cond_spec_t){
        .op = SF_FXR3_OPERATOR_EQUAL, .next_state = 1,
        .left_type = SF_FXR3_OPERAND_LITERAL, .left_has_value = true, .left_lit = 0.25f,
        .right_type = SF_FXR3_OPERAND_STATE_TIME, .right_has_value = false,
    });

    pad16(b);
    patch_i32(b, (int32_t)hp.container_off, (int32_t)b->pos);
    size_t cont_effects_off = 0, cont_actions_off = 0, cont_children_off = 0;
    put_container(b, 500, 1, 0, 0, &cont_effects_off, &cont_actions_off, &cont_children_off);

    pad16(b);
    patch_i32(b, (int32_t)hp.effect_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)cont_effects_off, (int32_t)b->pos);
    size_t eff_actions_patch = put_effect(b, 600, 1);

    pad16(b);
    patch_i32(b, (int32_t)hp.action_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)eff_actions_patch, (int32_t)b->pos);
    size_t act_fields_off = 0, act_unk_off = 0, act_props_off = 0;
    put_action(b, 700, 0, 0, 2, 0, &act_fields_off, &act_unk_off, &act_props_off);

    pad16(b);
    patch_i32(b, (int32_t)hp.property_off, (int32_t)b->pos);
    patch_i32(b, (int32_t)act_props_off, (int32_t)b->pos);
    /* Property 0: Scalar/Constant, 1 field (Float = 1.5e-3 → heuristic detects float) */
    size_t p0_fields_patch =
        put_property(b, SF_FXR3_PROPERTY_TYPE_SCALAR, SF_FXR3_INTERP_CONSTANT, false, 1);
    /* Property 1: Scalar/UnkAc6, 2 fields (first FieldInt forced by index 0 of UnkAc6 logic) */
    size_t p1_fields_patch =
        put_property(b, SF_FXR3_PROPERTY_TYPE_SCALAR, SF_FXR3_INTERP_UNK_AC6, true, 2);

    pad16(b);
    patch_i32(b, (int32_t)hp.field_off, (int32_t)b->pos);
    /* condition operand float */
    patch_i32(b, (int32_t)c0.left_patch, (int32_t)b->pos); put_f32(b, c0.spec.left_lit);
    /* property 0 field — float 1.5e-3f (>=1e-4f && <1e6f) */
    patch_i32(b, (int32_t)p0_fields_patch, (int32_t)b->pos);
    put_f32(b, 1.5e-3f);
    /* property 1 (UnkAc6, type=Scalar so type+1=1) fields:
     *   index 0 → first field, heuristic float (no forced int for index 0 in UnkAc6)
     *   index 1 → forced int by UnkAc6 branch (index > 0 && index <= type+1)
     */
    patch_i32(b, (int32_t)p1_fields_patch, (int32_t)b->pos);
    put_f32(b, 2.5f);       /* float (in range) */
    put_i32(b, 0);          /* forced int by UnkAc6 logic */

    pad16(b);
    /* Sekiro side-tables */
    patch_i32(b, (int32_t)hp.reference_off, (int32_t)b->pos);
    put_i32(b, 10);
    put_i32(b, 20);
    pad16(b);
    patch_i32(b, (int32_t)hp.external_off, (int32_t)b->pos);
    put_i32(b, 100);
    pad16(b);
    /* blood_off — empty list, but reader still seeks. Use end-of-buffer. */
    patch_i32(b, (int32_t)hp.blood_off, (int32_t)b->pos);
    pad16(b);
    return b->pos;
}

/* ── Verification ───────────────────────────────────────────────────────── */

static void verify_ds3(const sf_fxr3_t *f) {
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_VERSION_DARK_SOULS_3, sf_fxr3_version(f));
    TEST_ASSERT_EQUAL_INT32(1, sf_fxr3_id(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_fxr3_reference_count(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_fxr3_external_value_count(f));
    TEST_ASSERT_EQUAL_size_t(0u, sf_fxr3_unk_blood_enabler_count(f));

    const sf_fxr3_state_map_t *m = sf_fxr3_root_state_map(f);
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_state_map_state_count(m));
    const sf_fxr3_state_t *s = sf_fxr3_state_map_state(m, 0);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(4u, sf_fxr3_state_condition_count(s));

    /* Condition 0: input is left=Literal(0.5), right=StateTime.
     * Reader's canonical-form swap moves Literal to the right when the other
     * side is not Literal, so the observed shape is left=StateTime, right=Literal. */
    const sf_fxr3_state_condition_t *c = sf_fxr3_state_condition(s, 0);
    TEST_ASSERT_NOT_NULL(c);
    sf_fxr3_operand_t lo = sf_fxr3_condition_left_operand(c);
    sf_fxr3_operand_t ro = sf_fxr3_condition_right_operand(c);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_STATE_TIME, lo.type);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_LITERAL, ro.type);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, ro.value.as_literal);

    /* Condition 1: left=External(42) [after swap from upstream rule: literal-on-left
     * is canonical; here Left is External, Right is Literal — no swap needed] */
    c = sf_fxr3_state_condition(s, 1);
    lo = sf_fxr3_condition_left_operand(c);
    ro = sf_fxr3_condition_right_operand(c);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_EXTERNAL, lo.type);
    TEST_ASSERT_EQUAL_INT32(42, lo.value.as_external);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_LITERAL, ro.type);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, ro.value.as_literal);

    /* Condition 2: left=StateTime, right=Literal(2.0) */
    c = sf_fxr3_state_condition(s, 2);
    lo = sf_fxr3_condition_left_operand(c);
    ro = sf_fxr3_condition_right_operand(c);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_STATE_TIME, lo.type);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_LITERAL, ro.type);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, ro.value.as_literal);

    /* Condition 3: left=TimeOfDay, right=Literal(12.0) */
    c = sf_fxr3_state_condition(s, 3);
    lo = sf_fxr3_condition_left_operand(c);
    ro = sf_fxr3_condition_right_operand(c);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_TIME_OF_DAY, lo.type);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_OPERAND_LITERAL, ro.type);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, ro.value.as_literal);

    const sf_fxr3_container_t *cont = sf_fxr3_root_container(f);
    TEST_ASSERT_NOT_NULL(cont);
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_container_effect_count(cont));
    const sf_fxr3_effect_t *eff = sf_fxr3_container_effect(cont, 0);
    TEST_ASSERT_NOT_NULL(eff);
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_effect_action_count(eff));
    const sf_fxr3_action_t *act = sf_fxr3_effect_action(eff, 0);
    TEST_ASSERT_NOT_NULL(act);
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_action_property_count(act));
    const sf_fxr3_property_t *p = sf_fxr3_action_property(act, 0);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_PROPERTY_TYPE_SCALAR, sf_fxr3_property_type(p));
    TEST_ASSERT_EQUAL_INT(SF_FXR3_INTERP_CONSTANT, sf_fxr3_property_interpolation(p));
    TEST_ASSERT_FALSE(sf_fxr3_property_is_loop(p));
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_property_field_count(p));
    sf_fxr3_field_t fld = sf_fxr3_property_field(p, 0);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_FIELD_TYPE_FLOAT, fld.type);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fld.value.as_float);
}

static void verify_sekiro(const sf_fxr3_t *f) {
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_VERSION_SEKIRO, sf_fxr3_version(f));
    TEST_ASSERT_EQUAL_INT32(42, sf_fxr3_id(f));
    TEST_ASSERT_EQUAL_size_t(2u, sf_fxr3_reference_count(f));
    TEST_ASSERT_EQUAL_INT32(10, sf_fxr3_reference(f, 0));
    TEST_ASSERT_EQUAL_INT32(20, sf_fxr3_reference(f, 1));
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_external_value_count(f));
    TEST_ASSERT_EQUAL_INT32(100, sf_fxr3_external_value(f, 0));
    TEST_ASSERT_EQUAL_size_t(0u, sf_fxr3_unk_blood_enabler_count(f));

    const sf_fxr3_container_t *cont = sf_fxr3_root_container(f);
    const sf_fxr3_effect_t *eff = sf_fxr3_container_effect(cont, 0);
    const sf_fxr3_action_t *act = sf_fxr3_effect_action(eff, 0);
    TEST_ASSERT_EQUAL_size_t(2u, sf_fxr3_action_property_count(act));

    const sf_fxr3_property_t *p0 = sf_fxr3_action_property(act, 0);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_INTERP_CONSTANT, sf_fxr3_property_interpolation(p0));
    TEST_ASSERT_EQUAL_size_t(1u, sf_fxr3_property_field_count(p0));
    sf_fxr3_field_t f0 = sf_fxr3_property_field(p0, 0);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_FIELD_TYPE_FLOAT, f0.type);
    TEST_ASSERT_EQUAL_FLOAT(1.5e-3f, f0.value.as_float);

    const sf_fxr3_property_t *p1 = sf_fxr3_action_property(act, 1);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_INTERP_UNK_AC6, sf_fxr3_property_interpolation(p1));
    TEST_ASSERT_TRUE(sf_fxr3_property_is_loop(p1));
    TEST_ASSERT_EQUAL_size_t(2u, sf_fxr3_property_field_count(p1));
    sf_fxr3_field_t f1a = sf_fxr3_property_field(p1, 0);
    sf_fxr3_field_t f1b = sf_fxr3_property_field(p1, 1);
    TEST_ASSERT_EQUAL_INT(SF_FXR3_FIELD_TYPE_FLOAT, f1a.type);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, f1a.value.as_float);
    /* UnkAc6 forces index 1 to int regardless of value */
    TEST_ASSERT_EQUAL_INT(SF_FXR3_FIELD_TYPE_INT, f1b.type);
    TEST_ASSERT_EQUAL_INT32(0, f1b.value.as_int);
}

/* ── Test cases ─────────────────────────────────────────────────────────── */

static void exercise_round_trip(const uint8_t *in, size_t in_size,
                                void (*verify)(const sf_fxr3_t *)) {
    /* Step 1: read input */
    sf_fxr3_t *f1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr3_read_from_memory(&f1, in, in_size, NULL));
    verify(f1);

    /* Step 2: write to bytes_a */
    void *bytes_a = NULL; size_t size_a = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr3_write_to_memory(f1, &bytes_a, &size_a, NULL));
    TEST_ASSERT_NOT_NULL(bytes_a);
    TEST_ASSERT_TRUE(size_a >= 128u);
    TEST_ASSERT_EQUAL_MEMORY("FXR\0", bytes_a, 4);

    /* Step 3: read bytes_a — confirms writer output is reader-compatible */
    sf_fxr3_t *f2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr3_read_from_memory(&f2, bytes_a, size_a, NULL));
    verify(f2);

    /* Step 4: write again to bytes_b */
    void *bytes_b = NULL; size_t size_b = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_fxr3_write_to_memory(f2, &bytes_b, &size_b, NULL));

    /* Step 5: writer is deterministic */
    TEST_ASSERT_EQUAL_size_t(size_a, size_b);
    TEST_ASSERT_EQUAL_MEMORY(bytes_a, bytes_b, size_a);

    sf_free(NULL, bytes_b);
    sf_fxr3_destroy(f2);
    sf_free(NULL, bytes_a);
    sf_fxr3_destroy(f1);
}

static void test_fxr3_ds3_round_trip(void) {
    static buf_t b;
    memset(&b, 0, sizeof(b));
    size_t sz = build_ds3_fixture(&b);
    TEST_ASSERT_TRUE(sz > 0u);
    TEST_ASSERT_TRUE(sz <= BUF_CAP);
    exercise_round_trip(b.bytes, sz, verify_ds3);
}

static void test_fxr3_sekiro_round_trip(void) {
    static buf_t b;
    memset(&b, 0, sizeof(b));
    size_t sz = build_sekiro_fixture(&b);
    TEST_ASSERT_TRUE(sz > 0u);
    TEST_ASSERT_TRUE(sz <= BUF_CAP);
    exercise_round_trip(b.bytes, sz, verify_sekiro);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fxr3_ds3_round_trip);
    RUN_TEST(test_fxr3_sekiro_round_trip);
    return UNITY_END();
}
