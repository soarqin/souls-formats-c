/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — GPARAM (Lighting Param) format implementation.
 * Upstream: SoulsFormats/Formats/GPARAM.cs
 *
 * This translation unit currently provides:
 *   - Concrete definitions of the opaque structs declared in
 *     <souls_formats/sf_gparam.h> (via lighting/gparam_internal.h).
 *   - The full object-lifecycle teardown (sf_gparam_destroy), mirroring the
 *     ownership graph documented in gparam_internal.h. The single bulk
 *     `name_pool` allocation owns every borrowed string in the graph, so the
 *     destroy walk only releases the per-array allocations it actually owns.
 *   - All public accessor functions; these are pure pointer/index reads with
 *     null/bounds checks and never allocate.
 *   - Stubbed sf_gparam_read_from_memory / sf_gparam_write_to_buffer entry
 *     points returning SF_ERR_INTERNAL. The wire format implementation lands
 *     in T11 (read) / T12 (write) per the post-v1 lighting plan.
 *
 * Upstream layering note: GPARAM has *no* Group abstraction. GPARAM.Params
 * (GPARAM.cs:36) is the top-level list of Param objects, so the C model
 * exposes `params` directly on sf_gparam_t (matching upstream).
 */

#include "souls_formats/sf_gparam.h"
#include "internal/sf_internal.h"
#include "lighting/gparam_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*===========================================================================
 * Field-type coverage guard
 *
 * Every public GPARAM API treats the 16 FieldType variants as a closed enum.
 * If upstream ever expands this set we want a hard build break here rather
 * than a silent fall-through in reader/writer/accessor code. The numeric
 * boundaries are also asserted in sf_gparam.h, but we restate them locally
 * to guard the implementation file against header drift.
 *===========================================================================*/
_Static_assert(SF_GPARAM_FIELD_TYPE_SBYTE == 1,
               "GPARAM FieldType coverage drift: SBYTE must be 1");
_Static_assert(SF_GPARAM_FIELD_TYPE_STRING == 16,
               "GPARAM FieldType coverage drift: STRING must be 16 (16-type set)");
_Static_assert((int)SF_GPARAM_FIELD_TYPE_STRING - (int)SF_GPARAM_FIELD_TYPE_SBYTE + 1 == 16,
               "GPARAM FieldType coverage drift: expected exactly 16 contiguous types");

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

SF_API void sf_gparam_destroy(sf_gparam_t *gparam)
{
    if (!gparam) return;
    const sf_allocator_t *a = gparam->alloc;

    /*  Tear down params → fields → values, then param comments.
     *  All `key`/`name`/`comments[]`/`as_string` payloads point INTO the
     *  shared `name_pool`; they are not individually owned and must not be
     *  freed here. The only per-array allocations we own per param are the
     *  fields array, its per-field values arrays, and the comments array. */
    if (gparam->params) {
        for (size_t pi = 0; pi < gparam->param_count; pi++) {
            struct sf_gparam_param *p = &gparam->params[pi];
            if (p->fields) {
                for (size_t fi = 0; fi < p->field_count; fi++) {
                    struct sf_gparam_field *f = &p->fields[fi];
                    sf_xfree(a, f->values);
                }
                sf_xfree(a, p->fields);
            }
            sf_xfree(a, p->comments);
        }
        sf_xfree(a, gparam->params);
    }

    /*  Tear down UnkParamExtra array (each entry owns its `ids` int array). */
    if (gparam->unk_param_extras) {
        for (size_t i = 0; i < gparam->unk_param_extra_count; i++) {
            sf_xfree(a, gparam->unk_param_extras[i].ids);
        }
        sf_xfree(a, gparam->unk_param_extras);
    }

    /*  Verbatim "data30" opaque blob (preserved for byte-exact round-trip). */
    sf_xfree(a, gparam->data30);

    /*  The single bulk allocation that backs every borrowed string in the
     *  entire object graph (param.name, param.comments[i], field.name,
     *  field.key, value.as_string). Freeing this releases all of them. */
    sf_xfree(a, gparam->name_pool);

    /*  Finally release the GPARAM header itself. */
    sf_xfree(a, gparam);
}

/*===========================================================================
 * Read / write entry points (stubs)
 *
 * Wire-format implementations are scheduled for T11 (read) and T12 (write).
 * The accompanying tests gate on SF_ERR_INTERNAL so that downstream test
 * harnesses can compile against the public API before those tasks land.
 *===========================================================================*/

SF_API sf_result_t sf_gparam_read_from_memory(sf_gparam_t **out, const void *bytes, size_t size,
                                              const sf_allocator_t *alloc)
{
    (void)out;
    (void)bytes;
    (void)size;
    (void)alloc;
    return SF_ERR_INTERNAL;
}

SF_API sf_result_t sf_gparam_write_to_buffer(const sf_gparam_t *gparam, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *alloc)
{
    (void)gparam;
    (void)out_bytes;
    (void)out_size;
    (void)alloc;
    return SF_ERR_INTERNAL;
}

/*===========================================================================
 * Top-level header accessors
 *===========================================================================*/

SF_API sf_gparam_version_t sf_gparam_get_version(const sf_gparam_t *gparam)
{
    return gparam ? gparam->version : SF_GPARAM_VERSION_V3;
}

SF_API bool sf_gparam_get_unk0d(const sf_gparam_t *gparam)
{
    return gparam ? gparam->unk0d : false;
}

SF_API int32_t sf_gparam_get_count14(const sf_gparam_t *gparam)
{
    return gparam ? gparam->count14 : 0;
}

SF_API float sf_gparam_get_unk40(const sf_gparam_t *gparam)
{
    return gparam ? gparam->unk40 : 0.0f;
}

SF_API float sf_gparam_get_unk50(const sf_gparam_t *gparam)
{
    return gparam ? gparam->unk50 : 0.0f;
}

SF_API const uint8_t *sf_gparam_get_data30(const sf_gparam_t *gparam, size_t *out_size)
{
    if (!gparam) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (out_size) *out_size = gparam->data30_size;
    return gparam->data30;
}

/*===========================================================================
 * Param container accessors
 *===========================================================================*/

SF_API size_t sf_gparam_param_count(const sf_gparam_t *gparam)
{
    return gparam ? gparam->param_count : 0;
}

SF_API const sf_gparam_param_t *sf_gparam_get_param(const sf_gparam_t *gparam, size_t index)
{
    if (!gparam || index >= gparam->param_count) return NULL;
    return &gparam->params[index];
}

/*===========================================================================
 * Param-level accessors
 *===========================================================================*/

SF_API const char *sf_gparam_param_get_key(const sf_gparam_param_t *param)
{
    return param ? param->key : NULL;
}

SF_API const char *sf_gparam_param_get_name(const sf_gparam_param_t *param)
{
    return param ? param->name : NULL;
}

SF_API size_t sf_gparam_param_field_count(const sf_gparam_param_t *param)
{
    return param ? param->field_count : 0;
}

SF_API const sf_gparam_field_t *sf_gparam_param_get_field(const sf_gparam_param_t *param,
                                                          size_t index)
{
    if (!param || index >= param->field_count) return NULL;
    return &param->fields[index];
}

SF_API size_t sf_gparam_param_comment_count(const sf_gparam_param_t *param)
{
    return param ? param->comment_count : 0;
}

SF_API const char *sf_gparam_param_get_comment(const sf_gparam_param_t *param, size_t index)
{
    if (!param || index >= param->comment_count) return NULL;
    return param->comments[index];
}

/*===========================================================================
 * Field-level accessors
 *===========================================================================*/

SF_API const char *sf_gparam_field_get_key(const sf_gparam_field_t *field)
{
    return field ? field->key : NULL;
}

SF_API const char *sf_gparam_field_get_name(const sf_gparam_field_t *field)
{
    return field ? field->name : NULL;
}

SF_API sf_gparam_field_type_t sf_gparam_field_get_type(const sf_gparam_field_t *field)
{
    /*  No sentinel "invalid" enumerator exists; SBYTE is the conventional
     *  return on null per the rest of the project's get_type accessors. */
    return field ? field->type : SF_GPARAM_FIELD_TYPE_SBYTE;
}

SF_API size_t sf_gparam_field_value_count(const sf_gparam_field_t *field)
{
    return field ? field->value_count : 0;
}

SF_API sf_gparam_value_t sf_gparam_field_get_value(const sf_gparam_field_t *field, size_t index)
{
    /*  Values are stored as fully-tagged sf_gparam_value_t PODs inside the
     *  field, so the accessor is a single bounds-checked struct copy.
     *  No per-type switch is needed here — the reader (T11) is responsible
     *  for populating the discriminator and union member correctly. */
    sf_gparam_value_t zero;
    memset(&zero, 0, sizeof(zero));
    if (!field || index >= field->value_count) return zero;
    return field->values[index];
}

/*===========================================================================
 * UnkParamExtra accessors
 *===========================================================================*/

SF_API size_t sf_gparam_unk_param_extra_count(const sf_gparam_t *gparam)
{
    return gparam ? gparam->unk_param_extra_count : 0;
}

SF_API const sf_gparam_unk_param_extra_t *sf_gparam_get_unk_param_extra(const sf_gparam_t *gparam,
                                                                        size_t index)
{
    if (!gparam || index >= gparam->unk_param_extra_count) return NULL;
    return &gparam->unk_param_extras[index];
}
