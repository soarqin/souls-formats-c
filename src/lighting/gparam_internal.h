/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Upstream: SoulsFormats/Formats/GPARAM.cs — internal layout for sf_gparam_t. */
#ifndef SF_LIGHTING_GPARAM_INTERNAL_H
#define SF_LIGHTING_GPARAM_INTERNAL_H

#include "souls_formats/sf_gparam.h"
#include "souls_formats/sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*  Mirrors upstream GPARAM.UnkParamExtra (GPARAM.cs:1212-1277).
 *  `unk0c` is read/written only when version >= V5; left zero for V3. */
struct sf_gparam_unk_param_extra {
    int32_t  unk00;
    int32_t *ids;
    size_t   id_count;
    int32_t  unk0c;
};

/*  Mirrors upstream GPARAM.IField/Field<T> (GPARAM.cs:335-505).
 *  `name` is borrowed from the parent sf_gparam.name_pool (never owned).
 *  `unk` holds the post-type byte (V<V6: int16 padding asserted = 0;
 *  V>=V6: single unk byte zero-extended for round-trip). */
struct sf_gparam_field {
    int32_t                key;
    const char            *name;
    sf_gparam_field_type_t type;
    int32_t                capacity;
    int16_t                unk;
    sf_gparam_value_t     *values;
    size_t                 value_count;
};

/*  Mirrors upstream GPARAM.Param (GPARAM.cs:1014-1210). Top-level — there
 *  is NO Group layer in upstream. `name` and each `comments[i]` are
 *  borrowed from sf_gparam.name_pool. */
struct sf_gparam_param {
    int32_t                  key;
    const char              *name;
    struct sf_gparam_field  *fields;
    size_t                   field_count;
    const char             **comments;
    size_t                   comment_count;
};

/*  Mirrors upstream GPARAM (GPARAM.cs:16-218). `unk40`/`unk50` are floats
 *  per upstream (`bw.WriteSingle`). `unk50` is V>=V5 only. `data30` is the
 *  opaque blob between Unk30Base and ParamExtrasBase, preserved verbatim
 *  for byte-exact round-trip. `name_pool` is a single bulk allocation
 *  whose lifetime owns every borrowed string in this object graph. */
struct sf_gparam {
    sf_gparam_version_t                 version;
    bool                                unk0d;
    int32_t                             count14;
    float                               unk40;
    float                               unk50;
    struct sf_gparam_param             *params;
    size_t                              param_count;
    uint8_t                            *data30;
    size_t                              data30_size;
    struct sf_gparam_unk_param_extra   *unk_param_extras;
    size_t                              unk_param_extra_count;
    char                               *name_pool;
    const sf_allocator_t               *alloc;
};

/*  Natural in-memory size (bytes) of the value payload selected by a
 *  FieldType. Used by the binary reader/writer to size temporary buffers
 *  before promoting values into sf_gparam_value_t.v.
 *
 *  IMPORTANT — wire size may differ from this:
 *    - VEC2 wire = 16 (8 vec2 + 8 padding, GPARAM.cs:694, 701)
 *    - VEC3 wire = 16 (12 vec3 + 4 padding, GPARAM.cs:724, 731)
 *    - STRING wire = variable null-terminated UTF-16 (GPARAM.cs:895, 899)
 *  Caller code is responsible for handling those wire-side adjustments. */
static inline size_t sfi_gparam_field_payload_size(sf_gparam_field_type_t t)
{
    static const size_t k_sizes[16] = {
        1,   /* SBYTE  = 1  (sbyte)                 */
        2,   /* SHORT  = 2  (int16)                 */
        4,   /* INT    = 3  (int32)                 */
        8,   /* LONG   = 4  (int64)                 */
        1,   /* BYTE   = 5  (byte)                  */
        2,   /* USHORT = 6  (uint16)                */
        4,   /* UINT   = 7  (uint32)                */
        8,   /* ULONG  = 8  (uint64)                */
        4,   /* FLOAT  = 9  (single)                */
        8,   /* DOUBLE = 10 (double)                */
        1,   /* BOOL   = 11 (1-byte wire boolean)   */
        8,   /* VEC2   = 12 (sf_vec2_t in-memory)   */
        12,  /* VEC3   = 13 (sf_vec3_t in-memory)   */
        16,  /* VEC4   = 14 (sf_vec4_t)             */
        4,   /* COLOR  = 15 (RGBA, 1 byte/channel)  */
        8,   /* STRING = 16 (const char* pointer)   */
    };
    _Static_assert(sizeof(k_sizes) / sizeof(k_sizes[0]) == 16,
                   "sfi_gparam_field_payload_size table must have 16 entries");
    if ((unsigned)t < 1u || (unsigned)t > 16u) return 0;
    return k_sizes[(unsigned)t - 1u];
}

/*  Validate a raw FieldType byte read from the wire. Accepts the inclusive
 *  range [1, 16]; rejects 0 and >=17. Upstream throws NotImplementedException
 *  for unknown values (GPARAM.cs:308-313); we surface SF_ERR_INVALID_ARG. */
static inline bool sfi_gparam_field_type_valid(uint8_t raw)
{
    return raw >= 1u && raw <= 16u;
}

#endif /* SF_LIGHTING_GPARAM_INTERNAL_H */
