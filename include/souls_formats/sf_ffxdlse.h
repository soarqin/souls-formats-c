/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FFXDLSE (DS2 SFX configuration) public surface.
 *
 * Little-endian only. Magic: ASCII "DLsE" at offset 0.
 *
 * FFXDLSE encodes a recursive node graph: every node is an FXSerializable
 * with a fixed wire layout of:
 *   int16 classIndex (1-based index into the file-level class-name table)
 *   int32 version
 *   int32 length (total bytes from start of object to end)
 *   <object-specific payload>
 *
 * The class hierarchy stored in the file follows:
 *   FXEffect ("FXSerializableEffect", v5)
 *     ├── ParamList1 ("FXSerializableParamList", v2)
 *     │     └── Param[] ("FXSerializableParam", v2) — 36 subclasses
 *     ├── ParamList2
 *     ├── StateMap ("FXSerializableStateMap", v1)
 *     │     └── State[] ("FXSerializableState", v1)
 *     │           ├── Action[] ("FXSerializableAction", v1)
 *     │           │     └── ParamList
 *     │           └── Trigger[] ("FXSerializableTrigger", v1)
 *     │                 └── Evaluatable ("FXSerializableEvaluatable<dl_int32>", v1)
 *     └── ResourceSet ("FXResourceSet", v1)
 *
 * The public C API exposes lifecycle, byte-level I/O, and a minimal set of
 * structured accessors. Internal Param/Evaluatable subclasses are read and
 * written faithfully but exposed only as opaque tree nodes for round-trip
 * fidelity. Test consumers can construct empty/minimal trees through the
 * StateMap / State / Action / Trigger / Evaluatable factories below; richer
 * sub-tree construction is an explicit deferred extension.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FFXDLSE/FFXDLSE.cs
 *   SoulsFormats/Formats/FFXDLSE/FXEffect.cs
 *   SoulsFormats/Formats/FFXDLSE/State.cs
 *   SoulsFormats/Formats/FFXDLSE/StateMap.cs
 *   SoulsFormats/Formats/FFXDLSE/Trigger.cs
 *   SoulsFormats/Formats/FFXDLSE/Action.cs
 *   SoulsFormats/Formats/FFXDLSE/Param.cs
 *   SoulsFormats/Formats/FFXDLSE/ParamList.cs
 *   SoulsFormats/Formats/FFXDLSE/Primitive.cs
 *   SoulsFormats/Formats/FFXDLSE/ResourceSet.cs
 *   SoulsFormats/Formats/FFXDLSE/Evaluatable.cs
 */

#ifndef SOULS_FORMATS_SF_FFXDLSE_H
#define SOULS_FORMATS_SF_FFXDLSE_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Evaluatable opcode — wire int32 at offset +0 inside the object payload.
 *
 * Mirrors upstream Evaluatable.Read switch (Evaluatable.cs:60-86) and the
 * `Opcode` overrides of every concrete subclass.
 *===========================================================================*/
typedef enum sf_ffxdlse_evaluatable_opcode {
    SF_FFXDLSE_EVAL_CONSTANT             = 1,
    SF_FFXDLSE_EVAL_2                    = 2,
    SF_FFXDLSE_EVAL_3                    = 3,
    SF_FFXDLSE_EVAL_CURRENT_TICK         = 4,
    SF_FFXDLSE_EVAL_TOTAL_TICK           = 5,
    SF_FFXDLSE_EVAL_AND                  = 8,
    SF_FFXDLSE_EVAL_OR                   = 9,
    SF_FFXDLSE_EVAL_GE                   = 10,
    SF_FFXDLSE_EVAL_GT                   = 11,
    SF_FFXDLSE_EVAL_LE                   = 12,
    SF_FFXDLSE_EVAL_LT                   = 13,
    SF_FFXDLSE_EVAL_EQ                   = 14,
    SF_FFXDLSE_EVAL_NE                   = 15,
    SF_FFXDLSE_EVAL_NOT                  = 20,
    SF_FFXDLSE_EVAL_CHILD_EXISTS         = 21,
    SF_FFXDLSE_EVAL_PARENT_EXISTS        = 22,
    SF_FFXDLSE_EVAL_DISTANCE_FROM_CAMERA = 23,
    SF_FFXDLSE_EVAL_EMITTERS_STOPPED     = 24,
} sf_ffxdlse_evaluatable_opcode_t;

_Static_assert(SF_FFXDLSE_EVAL_CONSTANT             ==  1, "FFXDLSE eval opcode drift (Constant)");
_Static_assert(SF_FFXDLSE_EVAL_2                    ==  2, "FFXDLSE eval opcode drift (2)");
_Static_assert(SF_FFXDLSE_EVAL_3                    ==  3, "FFXDLSE eval opcode drift (3)");
_Static_assert(SF_FFXDLSE_EVAL_CURRENT_TICK         ==  4, "FFXDLSE eval opcode drift (CurrentTick)");
_Static_assert(SF_FFXDLSE_EVAL_TOTAL_TICK           ==  5, "FFXDLSE eval opcode drift (TotalTick)");
_Static_assert(SF_FFXDLSE_EVAL_AND                  ==  8, "FFXDLSE eval opcode drift (And)");
_Static_assert(SF_FFXDLSE_EVAL_OR                   ==  9, "FFXDLSE eval opcode drift (Or)");
_Static_assert(SF_FFXDLSE_EVAL_GE                   == 10, "FFXDLSE eval opcode drift (GE)");
_Static_assert(SF_FFXDLSE_EVAL_GT                   == 11, "FFXDLSE eval opcode drift (GT)");
_Static_assert(SF_FFXDLSE_EVAL_LE                   == 12, "FFXDLSE eval opcode drift (LE)");
_Static_assert(SF_FFXDLSE_EVAL_LT                   == 13, "FFXDLSE eval opcode drift (LT)");
_Static_assert(SF_FFXDLSE_EVAL_EQ                   == 14, "FFXDLSE eval opcode drift (EQ)");
_Static_assert(SF_FFXDLSE_EVAL_NE                   == 15, "FFXDLSE eval opcode drift (NE)");
_Static_assert(SF_FFXDLSE_EVAL_NOT                  == 20, "FFXDLSE eval opcode drift (Not)");
_Static_assert(SF_FFXDLSE_EVAL_CHILD_EXISTS         == 21, "FFXDLSE eval opcode drift (ChildExists)");
_Static_assert(SF_FFXDLSE_EVAL_PARENT_EXISTS        == 22, "FFXDLSE eval opcode drift (ParentExists)");
_Static_assert(SF_FFXDLSE_EVAL_DISTANCE_FROM_CAMERA == 23, "FFXDLSE eval opcode drift (DistanceFromCamera)");
_Static_assert(SF_FFXDLSE_EVAL_EMITTERS_STOPPED     == 24, "FFXDLSE eval opcode drift (EmittersStopped)");

/*===========================================================================
 * Param type — wire int32 at the head of every Param subclass payload.
 *
 * Mirrors upstream Param.Read switch (Param.cs:73-119) and the `Type`
 * overrides of every concrete subclass.
 *===========================================================================*/
typedef enum sf_ffxdlse_param_type {
    SF_FFXDLSE_PARAM_1  =  1,
    SF_FFXDLSE_PARAM_2  =  2,
    SF_FFXDLSE_PARAM_5  =  5,
    SF_FFXDLSE_PARAM_6  =  6,
    SF_FFXDLSE_PARAM_7  =  7,
    SF_FFXDLSE_PARAM_9  =  9,
    SF_FFXDLSE_PARAM_11 = 11,
    SF_FFXDLSE_PARAM_12 = 12,
    SF_FFXDLSE_PARAM_13 = 13,
    SF_FFXDLSE_PARAM_15 = 15,
    SF_FFXDLSE_PARAM_17 = 17,
    SF_FFXDLSE_PARAM_18 = 18,
    SF_FFXDLSE_PARAM_19 = 19,
    SF_FFXDLSE_PARAM_20 = 20,
    SF_FFXDLSE_PARAM_21 = 21,
    SF_FFXDLSE_PARAM_37 = 37,
    SF_FFXDLSE_PARAM_38 = 38,
    SF_FFXDLSE_PARAM_40 = 40,
    SF_FFXDLSE_PARAM_41 = 41,
    SF_FFXDLSE_PARAM_44 = 44,
    SF_FFXDLSE_PARAM_45 = 45,
    SF_FFXDLSE_PARAM_46 = 46,
    SF_FFXDLSE_PARAM_47 = 47,
    SF_FFXDLSE_PARAM_59 = 59,
    SF_FFXDLSE_PARAM_60 = 60,
    SF_FFXDLSE_PARAM_66 = 66,
    SF_FFXDLSE_PARAM_68 = 68,
    SF_FFXDLSE_PARAM_69 = 69,
    SF_FFXDLSE_PARAM_70 = 70,
    SF_FFXDLSE_PARAM_71 = 71,
    SF_FFXDLSE_PARAM_79 = 79,
    SF_FFXDLSE_PARAM_81 = 81,
    SF_FFXDLSE_PARAM_82 = 82,
    SF_FFXDLSE_PARAM_83 = 83,
    SF_FFXDLSE_PARAM_84 = 84,
    SF_FFXDLSE_PARAM_85 = 85,
    SF_FFXDLSE_PARAM_87 = 87,
} sf_ffxdlse_param_type_t;

_Static_assert(SF_FFXDLSE_PARAM_1  ==  1, "FFXDLSE param type drift (1)");
_Static_assert(SF_FFXDLSE_PARAM_2  ==  2, "FFXDLSE param type drift (2)");
_Static_assert(SF_FFXDLSE_PARAM_5  ==  5, "FFXDLSE param type drift (5)");
_Static_assert(SF_FFXDLSE_PARAM_15 == 15, "FFXDLSE param type drift (15)");
_Static_assert(SF_FFXDLSE_PARAM_37 == 37, "FFXDLSE param type drift (37)");
_Static_assert(SF_FFXDLSE_PARAM_38 == 38, "FFXDLSE param type drift (38)");
_Static_assert(SF_FFXDLSE_PARAM_68 == 68, "FFXDLSE param type drift (68)");
_Static_assert(SF_FFXDLSE_PARAM_85 == 85, "FFXDLSE param type drift (85)");
_Static_assert(SF_FFXDLSE_PARAM_87 == 87, "FFXDLSE param type drift (87)");

/*===========================================================================
 * Resource-set vector slot — five parallel `List<int>` vectors per upstream
 * ResourceSet (ResourceSet.cs:14-22).
 *===========================================================================*/
typedef enum sf_ffxdlse_resource_vector {
    SF_FFXDLSE_RES_VECTOR1 = 0,
    SF_FFXDLSE_RES_VECTOR2 = 1,
    SF_FFXDLSE_RES_VECTOR3 = 2,
    SF_FFXDLSE_RES_VECTOR4 = 3,
    SF_FFXDLSE_RES_VECTOR5 = 4,
    SF_FFXDLSE_RES_VECTOR_COUNT = 5,
} sf_ffxdlse_resource_vector_t;

_Static_assert(SF_FFXDLSE_RES_VECTOR_COUNT == 5, "FFXDLSE resource vector count drift");

/*===========================================================================
 * Opaque forward declarations
 *===========================================================================*/
typedef struct sf_ffxdlse              sf_ffxdlse_t;
typedef struct sf_ffxdlse_effect       sf_ffxdlse_effect_t;
typedef struct sf_ffxdlse_param_list   sf_ffxdlse_param_list_t;
typedef struct sf_ffxdlse_state_map    sf_ffxdlse_state_map_t;
typedef struct sf_ffxdlse_state        sf_ffxdlse_state_t;
typedef struct sf_ffxdlse_action       sf_ffxdlse_action_t;
typedef struct sf_ffxdlse_trigger      sf_ffxdlse_trigger_t;
typedef struct sf_ffxdlse_resource_set sf_ffxdlse_resource_set_t;
typedef struct sf_ffxdlse_evaluatable  sf_ffxdlse_evaluatable_t;
typedef struct sf_ffxdlse_param        sf_ffxdlse_param_t;

/*===========================================================================
 * Lifecycle and I/O
 *===========================================================================*/

/* Allocate a default-initialised FFXDLSE (effect with empty param lists,
 * empty state map, empty resource set, ID=0). */
SF_API sf_result_t sf_ffxdlse_create(sf_ffxdlse_t **out, const sf_allocator_t *alloc);

/* Free an FFXDLSE and every nested object it owns. NULL-safe. */
SF_API void sf_ffxdlse_destroy(sf_ffxdlse_t *ffx);

/* Parse an FFXDLSE from an in-memory buffer. */
SF_API sf_result_t sf_ffxdlse_read_from_memory(sf_ffxdlse_t **out,
                                               const void *bytes, size_t size,
                                               const sf_allocator_t *alloc);

/* Serialise an FFXDLSE to a fresh heap-allocated buffer. Caller frees
 * with sf_free(alloc, *out_bytes). */
SF_API sf_result_t sf_ffxdlse_write_to_memory(const sf_ffxdlse_t *ffx,
                                              void **out_bytes, size_t *out_size,
                                              const sf_allocator_t *alloc);

/* Returns true if @bytes starts with the FFXDLSE magic ("DLsE"). At least
 * 4 bytes are required, mirroring upstream's Is() guard. */
SF_API bool sf_ffxdlse_is(const void *bytes, size_t size);

/*===========================================================================
 * Effect accessors
 *
 * The Effect is owned by its parent FFXDLSE; the returned pointer is valid
 * until sf_ffxdlse_destroy is called.
 *===========================================================================*/
SF_API sf_ffxdlse_effect_t *sf_ffxdlse_effect(const sf_ffxdlse_t *ffx);

SF_API int32_t  sf_ffxdlse_effect_id     (const sf_ffxdlse_effect_t *e);
SF_API void     sf_ffxdlse_effect_set_id (sf_ffxdlse_effect_t *e, int32_t id);

SF_API sf_ffxdlse_param_list_t   *sf_ffxdlse_effect_param_list1  (const sf_ffxdlse_effect_t *e);
SF_API sf_ffxdlse_param_list_t   *sf_ffxdlse_effect_param_list2  (const sf_ffxdlse_effect_t *e);
SF_API sf_ffxdlse_state_map_t    *sf_ffxdlse_effect_state_map    (const sf_ffxdlse_effect_t *e);
SF_API sf_ffxdlse_resource_set_t *sf_ffxdlse_effect_resource_set (const sf_ffxdlse_effect_t *e);

/*===========================================================================
 * ParamList accessors
 *
 * ParamList holds an `Unk04` scratch int and a list of Param subclasses.
 * The synthetic-test API does not currently expose param constructors;
 * params are read from disk and round-tripped opaquely.
 *===========================================================================*/
SF_API int32_t sf_ffxdlse_param_list_unk04    (const sf_ffxdlse_param_list_t *pl);
SF_API void    sf_ffxdlse_param_list_set_unk04(sf_ffxdlse_param_list_t *pl, int32_t v);
SF_API size_t  sf_ffxdlse_param_list_count    (const sf_ffxdlse_param_list_t *pl);
SF_API sf_ffxdlse_param_t *sf_ffxdlse_param_list_at(const sf_ffxdlse_param_list_t *pl,
                                                    size_t i);

/* Read-only Param introspection. */
SF_API sf_ffxdlse_param_type_t sf_ffxdlse_param_type(const sf_ffxdlse_param_t *p);

/*===========================================================================
 * StateMap accessors
 *===========================================================================*/
SF_API size_t              sf_ffxdlse_state_map_count(const sf_ffxdlse_state_map_t *sm);
SF_API sf_ffxdlse_state_t *sf_ffxdlse_state_map_at   (const sf_ffxdlse_state_map_t *sm,
                                                      size_t i);

/* Append a fresh, empty state to the map. The new state has no actions and
 * no triggers. Out parameter may be NULL. */
SF_API sf_result_t sf_ffxdlse_state_map_add(sf_ffxdlse_state_map_t *sm,
                                            sf_ffxdlse_state_t **out);

/*===========================================================================
 * State accessors
 *===========================================================================*/
SF_API size_t                sf_ffxdlse_state_action_count (const sf_ffxdlse_state_t *s);
SF_API size_t                sf_ffxdlse_state_trigger_count(const sf_ffxdlse_state_t *s);
SF_API sf_ffxdlse_action_t  *sf_ffxdlse_state_action_at    (const sf_ffxdlse_state_t *s,
                                                            size_t i);
SF_API sf_ffxdlse_trigger_t *sf_ffxdlse_state_trigger_at   (const sf_ffxdlse_state_t *s,
                                                            size_t i);

/* Append a fresh, empty Action. ID defaults to 0; ParamList is empty. */
SF_API sf_result_t sf_ffxdlse_state_add_action (sf_ffxdlse_state_t *s,
                                                sf_ffxdlse_action_t **out);

/* Append a fresh Trigger. state_index defaults to 0; evaluator is NULL —
 * the caller MUST install a non-NULL evaluator via
 * sf_ffxdlse_trigger_set_evaluator before serialisation. */
SF_API sf_result_t sf_ffxdlse_state_add_trigger(sf_ffxdlse_state_t *s,
                                                sf_ffxdlse_trigger_t **out);

/*===========================================================================
 * Action accessors
 *===========================================================================*/
SF_API int32_t                  sf_ffxdlse_action_id        (const sf_ffxdlse_action_t *a);
SF_API void                     sf_ffxdlse_action_set_id    (sf_ffxdlse_action_t *a, int32_t id);
SF_API sf_ffxdlse_param_list_t *sf_ffxdlse_action_param_list(const sf_ffxdlse_action_t *a);

/*===========================================================================
 * Trigger accessors
 *===========================================================================*/
SF_API int32_t                   sf_ffxdlse_trigger_state_index    (const sf_ffxdlse_trigger_t *t);
SF_API void                      sf_ffxdlse_trigger_set_state_index(sf_ffxdlse_trigger_t *t,
                                                                    int32_t v);
SF_API sf_ffxdlse_evaluatable_t *sf_ffxdlse_trigger_evaluator      (const sf_ffxdlse_trigger_t *t);

/* Install an evaluator. The trigger takes ownership of @ev; on success the
 * trigger's previous evaluator (if any) is destroyed. On failure ownership
 * is NOT transferred — caller still owns @ev and must destroy it. */
SF_API sf_result_t sf_ffxdlse_trigger_set_evaluator(sf_ffxdlse_trigger_t *t,
                                                    sf_ffxdlse_evaluatable_t *ev);

/*===========================================================================
 * ResourceSet accessors
 *
 * Five parallel int32 vectors, addressed by sf_ffxdlse_resource_vector_t.
 *===========================================================================*/
SF_API sf_result_t sf_ffxdlse_resource_set_count(const sf_ffxdlse_resource_set_t *rs,
                                                 sf_ffxdlse_resource_vector_t which,
                                                 size_t *out_count);
SF_API sf_result_t sf_ffxdlse_resource_set_at   (const sf_ffxdlse_resource_set_t *rs,
                                                 sf_ffxdlse_resource_vector_t which,
                                                 size_t index, int32_t *out_value);
SF_API sf_result_t sf_ffxdlse_resource_set_add  (sf_ffxdlse_resource_set_t *rs,
                                                 sf_ffxdlse_resource_vector_t which,
                                                 int32_t value);

/*===========================================================================
 * Evaluatable factories + accessors
 *
 * Every factory allocates a fresh evaluatable owned by the caller until it
 * is installed in a trigger via sf_ffxdlse_trigger_set_evaluator (which
 * takes ownership). Standalone evaluatables are destroyed with
 * sf_ffxdlse_evaluatable_destroy.
 *
 * Tree-shaped evaluators (binary/unary) take ownership of their operand(s)
 * on success; on failure the caller still owns the operand(s).
 *===========================================================================*/
SF_API sf_ffxdlse_evaluatable_opcode_t sf_ffxdlse_evaluatable_opcode(
    const sf_ffxdlse_evaluatable_t *ev);

SF_API void sf_ffxdlse_evaluatable_destroy(sf_ffxdlse_evaluatable_t *ev);

/* Leaf factories. */
SF_API sf_result_t sf_ffxdlse_evaluatable_create_constant(
    sf_ffxdlse_evaluatable_t **out, int32_t value, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_current_tick(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_total_tick(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_child_exists(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_parent_exists(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_distance_from_camera(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);
SF_API sf_result_t sf_ffxdlse_evaluatable_create_emitters_stopped(
    sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc);

/* Constant-value getter (only meaningful when opcode is
 * SF_FFXDLSE_EVAL_CONSTANT). */
SF_API sf_result_t sf_ffxdlse_evaluatable_constant_value(
    const sf_ffxdlse_evaluatable_t *ev, int32_t *out_value);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FFXDLSE_H */
