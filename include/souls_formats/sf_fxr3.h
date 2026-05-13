/* SPDX-License-Identifier: GPL-3.0-or-later */
/**
 * @file sf_fxr3.h
 * @brief FXR3 — Rainbow Stone FXR particle effects format (DS3 / Sekiro /
 *        Elden Ring / Nightreign / AC6).
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FXR3.cs
 *
 * Class hierarchy (mirrors upstream):
 *
 *   FXR3
 *   ├── RootStateMap          : StateMap
 *   │   └── States[]          : State
 *   │       └── Conditions[]  : StateCondition
 *   ├── RootContainer         : Container
 *   │   ├── Containers[]      : Container (children, recursive)
 *   │   ├── Effects[]         : Effect
 *   │   │   └── Actions[]     : Action
 *   │   │       ├── Properties[] : Property
 *   │   │       │   ├── Fields[]    : Field (Int | Float)
 *   │   │       │   └── Modifiers[] : PropertyModifier
 *   │   │       └── UnkFieldLists[] : UnkFieldList
 *   │   └── Actions[]         : Action
 *   ├── ReferenceList[]       : int (Sekiro+ only)
 *   ├── ExternalValueList[]   : int (Sekiro+ only)
 *   └── UnkBloodEnabler[]     : int (Sekiro+ only)
 *
 * Design adaptations from upstream (see docs/api-mapping/extensions.md):
 *   - PropertyModifier and UnkFieldList are intentionally fully opaque;
 *     their internal layouts are accessed only via future accessor
 *     additions, not exposed structurally.
 *   - Field (FieldInt / FieldFloat polymorphic class) is collapsed into a
 *     tagged-union POD `sf_fxr3_field_t` returned by value.
 *   - StateCondition.ConditionOperand (abstract class with 4 subclasses) is
 *     collapsed into a tagged-union POD `sf_fxr3_operand_t` returned by
 *     value.
 *   - mxml is the underlying XML serializer; it manages its own malloc/free
 *     and ignores `sf_allocator_t` for parsing buffers (see extensions.md).
 *   - XML parse/serialize failures return SF_ERR_INVALID_ARG (malformed
 *     XML, schema violations) or SF_ERR_INTERNAL (libmxml allocation /
 *     unexpected state); no new sf_result_t variants are introduced.
 */
#ifndef SOULS_FORMATS_SF_FXR3_H
#define SOULS_FORMATS_SF_FXR3_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Opaque forward declarations
 *
 * All FXR3 aggregate types are opaque to public API consumers; access is
 * via the `sf_fxr3_*` accessor functions further down this header.
 *===========================================================================*/
typedef struct sf_fxr3                   sf_fxr3_t;
typedef struct sf_fxr3_state_map         sf_fxr3_state_map_t;
typedef struct sf_fxr3_state             sf_fxr3_state_t;
typedef struct sf_fxr3_state_condition   sf_fxr3_state_condition_t;
typedef struct sf_fxr3_container         sf_fxr3_container_t;
typedef struct sf_fxr3_effect            sf_fxr3_effect_t;
typedef struct sf_fxr3_action            sf_fxr3_action_t;
typedef struct sf_fxr3_property          sf_fxr3_property_t;
typedef struct sf_fxr3_property_modifier sf_fxr3_property_modifier_t;
typedef struct sf_fxr3_unk_field_list    sf_fxr3_unk_field_list_t;

/*===========================================================================
 * FXRVersion — wire ushort at file offset 6
 *
 * Mirrors upstream FXR3.cs:285-289 (`public enum FXRVersion : ushort`).
 * Elden Ring, Nightreign, and AC6 all store version=5 (Sekiro).
 *===========================================================================*/
typedef enum sf_fxr3_version {
    SF_FXR3_VERSION_DARK_SOULS_3 = 4,
    SF_FXR3_VERSION_SEKIRO       = 5,
} sf_fxr3_version_t;

_Static_assert(SF_FXR3_VERSION_DARK_SOULS_3 == 4, "FXRVersion drift (DarkSouls3)");
_Static_assert(SF_FXR3_VERSION_SEKIRO       == 5, "FXRVersion drift (Sekiro)");

/*===========================================================================
 * StateCondition.OperatorType — comparison operator used by transitions
 *
 * Mirrors upstream FXR3.cs:376-384 (`public enum OperatorType`).
 * Note: LE / LT are decoded but not present in any known shipping file.
 *===========================================================================*/
typedef enum sf_fxr3_operator_type {
    SF_FXR3_OPERATOR_NOT_EQUAL = 0,
    SF_FXR3_OPERATOR_EQUAL     = 1,
    SF_FXR3_OPERATOR_GE        = 2,
    SF_FXR3_OPERATOR_GT        = 3,
    SF_FXR3_OPERATOR_LE        = 4, /* decoded but not present in format */
    SF_FXR3_OPERATOR_LT        = 5, /* decoded but not present in format */
} sf_fxr3_operator_type_t;

_Static_assert(SF_FXR3_OPERATOR_NOT_EQUAL == 0, "OperatorType drift (NotEqual)");
_Static_assert(SF_FXR3_OPERATOR_LT        == 5, "OperatorType drift (LessThan)");

/*===========================================================================
 * StateCondition.OperandType — discriminator for ConditionOperand
 *
 * Mirrors upstream FXR3.cs:386-392 (`public enum OperandType`). Wire
 * format stores these as a SIGNED int32; values are NEGATIVE (-4..-1).
 * Do NOT switch this enum to unsigned.
 *===========================================================================*/
typedef enum sf_fxr3_operand_type {
    SF_FXR3_OPERAND_LITERAL     = -4,
    SF_FXR3_OPERAND_EXTERNAL    = -3,
    SF_FXR3_OPERAND_TIME_OF_DAY = -2,
    SF_FXR3_OPERAND_STATE_TIME  = -1,
} sf_fxr3_operand_type_t;

_Static_assert(SF_FXR3_OPERAND_LITERAL     == -4, "OperandType drift (Literal)");
_Static_assert(SF_FXR3_OPERAND_EXTERNAL    == -3, "OperandType drift (External)");
_Static_assert(SF_FXR3_OPERAND_TIME_OF_DAY == -2, "OperandType drift (TimeOfDay)");
_Static_assert(SF_FXR3_OPERAND_STATE_TIME  == -1, "OperandType drift (StateTime)");

/*===========================================================================
 * FieldType — discriminator for Field (FieldInt | FieldFloat)
 *
 * Mirrors upstream FXR3.cs:1054-1058 (`public enum FieldType`).
 *===========================================================================*/
typedef enum sf_fxr3_field_type {
    SF_FXR3_FIELD_TYPE_INT   = 0,
    SF_FXR3_FIELD_TYPE_FLOAT = 1,
} sf_fxr3_field_type_t;

_Static_assert(SF_FXR3_FIELD_TYPE_INT   == 0, "FieldType drift (Int)");
_Static_assert(SF_FXR3_FIELD_TYPE_FLOAT == 1, "FieldType drift (Float)");

/*===========================================================================
 * Property.PropertyType — vector arity of a property
 *
 * Mirrors upstream FXR3.cs:1231-1237 (`public enum PropertyType`). Encoded
 * in the low 2 bits of TypeEnumA.
 *===========================================================================*/
typedef enum sf_fxr3_property_type {
    SF_FXR3_PROPERTY_TYPE_SCALAR  = 0,
    SF_FXR3_PROPERTY_TYPE_VECTOR2 = 1,
    SF_FXR3_PROPERTY_TYPE_VECTOR3 = 2,
    SF_FXR3_PROPERTY_TYPE_COLOR   = 3,
} sf_fxr3_property_type_t;

_Static_assert(SF_FXR3_PROPERTY_TYPE_SCALAR == 0, "PropertyType drift (Scalar)");
_Static_assert(SF_FXR3_PROPERTY_TYPE_COLOR  == 3, "PropertyType drift (Color)");

/*===========================================================================
 * Property.PropertyInterpolationType — temporal interpolation curve
 *
 * Mirrors upstream FXR3.cs:1239-1249 (`public enum PropertyInterpolationType`).
 * Encoded in bits 4..7 of TypeEnumA. UnkAc6 (=7) is observed in AC6 files
 * and must round-trip cleanly; do NOT reject it.
 *===========================================================================*/
typedef enum sf_fxr3_property_interpolation_type {
    SF_FXR3_INTERP_ZERO     = 0,
    SF_FXR3_INTERP_ONE      = 1,
    SF_FXR3_INTERP_CONSTANT = 2,
    SF_FXR3_INTERP_STEPPED  = 3,
    SF_FXR3_INTERP_LINEAR   = 4,
    SF_FXR3_INTERP_CURVE1   = 5,
    SF_FXR3_INTERP_CURVE2   = 6,
    SF_FXR3_INTERP_UNK_AC6  = 7,
} sf_fxr3_property_interpolation_type_t;

_Static_assert(SF_FXR3_INTERP_ZERO    == 0, "InterpolationType drift (Zero)");
_Static_assert(SF_FXR3_INTERP_UNK_AC6 == 7, "InterpolationType drift (UnkAc6)");

/*===========================================================================
 * sf_fxr3_field_t — tagged-union value POD
 *
 * Collapses upstream Field / FieldInt / FieldFloat (FXR3.cs:1060-1229).
 * Returned by value from `sf_fxr3_property_field()`. The layout is
 * intentionally stable and may be embedded by callers.
 *===========================================================================*/
typedef struct sf_fxr3_field {
    sf_fxr3_field_type_t type;
    union {
        int32_t as_int;   /* valid when type == SF_FXR3_FIELD_TYPE_INT */
        float   as_float; /* valid when type == SF_FXR3_FIELD_TYPE_FLOAT */
    } value;
} sf_fxr3_field_t;

/*===========================================================================
 * sf_fxr3_operand_t — tagged-union value POD
 *
 * Collapses upstream ConditionOperand / ConditionOperandLiteral /
 * ConditionOperandExternal / ConditionOperandStateTime /
 * ConditionOperandUnkMinus2 (FXR3.cs:394-...). Returned by value from
 * `sf_fxr3_condition_left_operand()` and `_right_operand()`. The layout
 * is stable and may be embedded by callers.
 *
 * Payload validity by tag:
 *   SF_FXR3_OPERAND_LITERAL     -> value.as_literal
 *   SF_FXR3_OPERAND_EXTERNAL    -> value.as_external
 *   SF_FXR3_OPERAND_TIME_OF_DAY -> no payload (union content undefined)
 *   SF_FXR3_OPERAND_STATE_TIME  -> no payload (union content undefined)
 *===========================================================================*/
typedef struct sf_fxr3_operand {
    sf_fxr3_operand_type_t type;
    union {
        float   as_literal;
        int32_t as_external;
    } value;
} sf_fxr3_operand_t;

/*===========================================================================
 * Binary I/O — read / write / destroy
 *===========================================================================*/

/**
 * Parse a .fxr file from an in-memory buffer.
 *
 * @param out   Output pointer. Caller owns and must call sf_fxr3_destroy.
 * @param bytes File contents (must start with the "FXR\0" magic).
 * @param size  Length of @p bytes in bytes.
 * @param a     Allocator (NULL = default malloc/free).
 *
 * @return SF_OK on success; SF_ERR_BAD_MAGIC if magic is wrong;
 *         SF_ERR_UNSUPPORTED_VERSION if version is not 4 or 5;
 *         SF_ERR_TRUNCATED on incomplete data; SF_ERR_INVALID_ARG on
 *         malformed structure; SF_ERR_OOM on allocation failure.
 */
SF_API sf_result_t sf_fxr3_read_from_memory(sf_fxr3_t **out,
                                             const void *bytes, size_t size,
                                             const sf_allocator_t *a);

/**
 * Serialize an FXR3 back to a fresh heap-allocated byte buffer.
 *
 * @param f         FXR3 to serialize.
 * @param out_bytes Output buffer. Caller owns; free with sf_free(a, ptr)
 *                  (or matching realloc/free of the passed allocator).
 * @param out_size  Length in bytes written to @p *out_bytes.
 * @param a         Allocator (NULL = default malloc/free).
 */
SF_API sf_result_t sf_fxr3_write_to_memory(const sf_fxr3_t *f,
                                            void **out_bytes, size_t *out_size,
                                            const sf_allocator_t *a);

/** Release an FXR3 and every nested object it owns. NULL-safe. */
SF_API void sf_fxr3_destroy(sf_fxr3_t *f);

/*===========================================================================
 * Top-level accessors
 *===========================================================================*/

SF_API sf_fxr3_version_t sf_fxr3_version(const sf_fxr3_t *f);
SF_API int32_t           sf_fxr3_id     (const sf_fxr3_t *f);

SF_API const sf_fxr3_state_map_t *sf_fxr3_root_state_map (const sf_fxr3_t *f);
SF_API const sf_fxr3_container_t *sf_fxr3_root_container (const sf_fxr3_t *f);

/* Sekiro+ only side-tables. Empty (count = 0) on DS3 files. */
SF_API size_t  sf_fxr3_reference_count        (const sf_fxr3_t *f);
SF_API int32_t sf_fxr3_reference              (const sf_fxr3_t *f, size_t i);
SF_API size_t  sf_fxr3_external_value_count   (const sf_fxr3_t *f);
SF_API int32_t sf_fxr3_external_value         (const sf_fxr3_t *f, size_t i);
SF_API size_t  sf_fxr3_unk_blood_enabler_count(const sf_fxr3_t *f);
SF_API int32_t sf_fxr3_unk_blood_enabler      (const sf_fxr3_t *f, size_t i);

/*===========================================================================
 * StateMap → State → StateCondition
 *===========================================================================*/

SF_API size_t                  sf_fxr3_state_map_state_count(const sf_fxr3_state_map_t *m);
SF_API const sf_fxr3_state_t * sf_fxr3_state_map_state      (const sf_fxr3_state_map_t *m, size_t i);

SF_API size_t                            sf_fxr3_state_condition_count(const sf_fxr3_state_t *s);
SF_API const sf_fxr3_state_condition_t * sf_fxr3_state_condition      (const sf_fxr3_state_t *s, size_t i);

SF_API sf_fxr3_operator_type_t sf_fxr3_condition_operator     (const sf_fxr3_state_condition_t *c);
SF_API sf_fxr3_operand_t       sf_fxr3_condition_left_operand (const sf_fxr3_state_condition_t *c);
SF_API sf_fxr3_operand_t       sf_fxr3_condition_right_operand(const sf_fxr3_state_condition_t *c);
SF_API int32_t                 sf_fxr3_condition_next_state   (const sf_fxr3_state_condition_t *c);

/*===========================================================================
 * Container → Effect → Action → Property
 *===========================================================================*/

SF_API size_t                     sf_fxr3_container_id          (const sf_fxr3_container_t *c);
SF_API size_t                     sf_fxr3_container_child_count (const sf_fxr3_container_t *c);
SF_API const sf_fxr3_container_t *sf_fxr3_container_child       (const sf_fxr3_container_t *c, size_t i);
SF_API size_t                     sf_fxr3_container_effect_count(const sf_fxr3_container_t *c);
SF_API const sf_fxr3_effect_t *   sf_fxr3_container_effect      (const sf_fxr3_container_t *c, size_t i);

SF_API size_t                  sf_fxr3_effect_action_count(const sf_fxr3_effect_t *e);
SF_API const sf_fxr3_action_t *sf_fxr3_effect_action      (const sf_fxr3_effect_t *e, size_t i);

SF_API size_t                    sf_fxr3_action_property_count(const sf_fxr3_action_t *a);
SF_API const sf_fxr3_property_t *sf_fxr3_action_property      (const sf_fxr3_action_t *a, size_t i);

SF_API sf_fxr3_property_type_t               sf_fxr3_property_type          (const sf_fxr3_property_t *p);
SF_API sf_fxr3_property_interpolation_type_t sf_fxr3_property_interpolation (const sf_fxr3_property_t *p);
SF_API bool                                  sf_fxr3_property_is_loop       (const sf_fxr3_property_t *p);
SF_API size_t                                sf_fxr3_property_field_count   (const sf_fxr3_property_t *p);
SF_API sf_fxr3_field_t                       sf_fxr3_property_field         (const sf_fxr3_property_t *p, size_t i);
SF_API size_t                                sf_fxr3_property_modifier_count(const sf_fxr3_property_t *p);
SF_API const sf_fxr3_property_modifier_t *   sf_fxr3_property_modifier      (const sf_fxr3_property_t *p, size_t i);

/*===========================================================================
 * XML serialization
 *
 * Round-trips an FXR3 through Rainbow Stone-compatible XML. Implementation
 * is backed by libmxml; see docs/api-mapping/extensions.md for the
 * allocator caveat.
 *
 * Equality after round-trip is STRUCTURAL (per the upstream XmlSerializer
 * contract), not byte-exact. Whitespace, attribute order, and quote style
 * may differ between writer outputs.
 *
 * Errors:
 *   SF_ERR_INVALID_ARG  — malformed XML / unknown elements / schema error
 *   SF_ERR_INTERNAL     — mxml allocation failure or unexpected state
 *   SF_ERR_OOM          — allocator failure
 *===========================================================================*/

SF_API sf_result_t sf_fxr3_from_xml(sf_fxr3_t **out,
                                     const char *xml_utf8, size_t xml_size,
                                     const sf_allocator_t *a);
SF_API sf_result_t sf_fxr3_to_xml  (const sf_fxr3_t *f,
                                     char **out_xml_utf8, size_t *out_size,
                                     const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FXR3_H */
