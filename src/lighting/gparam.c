/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — GPARAM (Lighting Param) format implementation.
 * Upstream: SoulsFormats/Formats/GPARAM.cs
 *
 * This translation unit provides:
 *   - Concrete definitions of the opaque structs declared in
 *     <souls_formats/sf_gparam.h> (via lighting/gparam_internal.h).
 *   - sf_gparam_read_from_memory  — wire-format reader (V5/V6 mandatory,
 *     V3 supported as it falls out of the V5 code path, V2 rejected with
 *     SF_ERR_UNSUPPORTED_VERSION).
 *   - sf_gparam_write_to_buffer   — wire-format writer (same version policy).
 *   - sf_gparam_destroy           — releases everything the reader owned.
 *   - All public accessors        — pure pointer/index reads.
 *
 * Upstream layering note: GPARAM has *no* Group abstraction. GPARAM.Params
 * (GPARAM.cs:36) is the top-level list of Param objects, so the C model
 * exposes `params` directly on sf_gparam_t (matching upstream).
 *
 * String ownership: every borrowed `const char *` in the object graph
 * points into a single bulk `name_pool` allocation. Reading collects all
 * UTF-8 strings into a temporary slot list (one slot per borrowed pointer
 * site), then concatenates them into the final pool and retargets every
 * slot to its in-pool address. Writing simply re-serialises each string
 * out of those pool pointers as UTF-16LE.
 */

#include "souls_formats/sf_gparam.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"
#include "lighting/gparam_internal.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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
 * Base offsets struct
 *
 * Mirrors upstream BaseOffsets (GPARAM.cs:995-1009). Captures the section
 * starts read out of the header; used to translate per-record relative
 * offsets back to absolute stream positions during read, and re-emitted via
 * reserve/fill on write.
 *===========================================================================*/
struct gparam_base_offsets {
    int64_t param_offsets;
    int64_t params;
    int64_t field_offsets;
    int64_t fields;
    int64_t values;
    int64_t value_ids;
    int64_t unk30;
    int64_t param_extras;
    int64_t param_extra_ids;
    int64_t param_comments_offsets;
    int64_t comment_offsets;
    int64_t comments;
};

/*===========================================================================
 * String slot bookkeeping (reader side)
 *
 * Each call site that needs a borrowed `const char *` pushes a slot:
 *   - `utf8` is a heap UTF-8 string produced by sf_binary_reader_read_utf16
 *     (allocated with the caller's allocator).
 *   - `target` is the address of the `const char *` field that should point
 *     into the eventual name_pool.
 * After all sections are parsed, finalize_string_pool() concatenates every
 * captured utf8 into one bulk allocation and rewrites each *target with the
 * matching in-pool address. The temp strings are freed during that walk.
 *===========================================================================*/
struct gparam_string_slot {
    char        *utf8;
    const char **target;
};

static sf_result_t string_slot_add(struct gparam_string_slot **arr,
                                   size_t *count, size_t *cap,
                                   char *utf8 /* takes ownership */,
                                   const char **target,
                                   const sf_allocator_t *alloc)
{
    if (*count >= *cap) {
        size_t new_cap = (*cap) ? (*cap) * 2u : 64u;
        struct gparam_string_slot *p = (struct gparam_string_slot *)sf_xrealloc(
            alloc, *arr,
            (*cap) * sizeof(**arr),
            new_cap * sizeof(**arr));
        if (!p) { sf_xfree(alloc, utf8); return SF_ERR_OOM; }
        *arr = p;
        *cap = new_cap;
    }
    (*arr)[*count].utf8   = utf8;
    (*arr)[*count].target = target;
    (*count)++;
    return SF_OK;
}

/*  Read a UTF-16LE NUL-terminated string at the current reader position,
 *  convert to UTF-8, and register a pool slot so the resulting pointer can
 *  be back-patched once the name_pool is built. */
static sf_result_t slot_read_utf16(sf_binary_reader_t *r,
                                   const char **target,
                                   struct gparam_string_slot **slots,
                                   size_t *slot_count, size_t *slot_cap,
                                   const sf_allocator_t *alloc)
{
    char  *utf8 = NULL;
    size_t len  = 0;
    sf_result_t e = sf_binary_reader_read_utf16(r, &utf8, &len);
    if (e != SF_OK) return e;
    return string_slot_add(slots, slot_count, slot_cap, utf8, target, alloc);
}

/*  Read a UTF-16LE NUL-terminated string at an absolute offset (without
 *  moving the cursor), convert to UTF-8, and register a pool slot. */
static sf_result_t slot_get_utf16(sf_binary_reader_t *r, int64_t off,
                                  const char **target,
                                  struct gparam_string_slot **slots,
                                  size_t *slot_count, size_t *slot_cap,
                                  const sf_allocator_t *alloc)
{
    char  *utf8 = NULL;
    size_t len  = 0;
    sf_result_t e = sf_binary_reader_get_utf16(r, off, &utf8, &len);
    if (e != SF_OK) return e;
    return string_slot_add(slots, slot_count, slot_cap, utf8, target, alloc);
}

/*  Consolidate every captured slot into a single bulk name_pool allocation
 *  hung off `gparam->name_pool`. Each slot's `target` is rewritten to its
 *  in-pool address; the temp utf8 buffers are freed as we go. */
static sf_result_t finalize_string_pool(struct sf_gparam *gparam,
                                        struct gparam_string_slot *slots,
                                        size_t slot_count,
                                        const sf_allocator_t *alloc)
{
    size_t pool_size = 0;
    for (size_t i = 0; i < slot_count; i++) {
        const char *s = slots[i].utf8 ? slots[i].utf8 : "";
        pool_size += strlen(s) + 1u;
    }
    if (pool_size == 0u) pool_size = 1u;

    char *pool = (char *)sf_xalloc(alloc, pool_size);
    if (!pool) return SF_ERR_OOM;
    pool[0] = '\0';

    size_t pos = 0;
    for (size_t i = 0; i < slot_count; i++) {
        const char *s    = slots[i].utf8 ? slots[i].utf8 : "";
        size_t      slen = strlen(s);
        memcpy(pool + pos, s, slen);
        pool[pos + slen] = '\0';
        if (slots[i].target) *slots[i].target = pool + pos;
        pos += slen + 1u;
    }

    gparam->name_pool = pool;
    return SF_OK;
}

/*===========================================================================
 * Typed value payload reader (Values section)
 *
 * Upstream: per-FieldType ReadValue (GPARAM.cs:523, 547, 571, 595, 619, 643,
 * 667, 691-696, 721-725, 751, 775, 799, 823, 847, 871, 895). Each typed
 * Field<T>.ReadValue performs the wire read for ONE value entry in the
 * Values section. VEC2 and VEC3 include explicit zero-padding so wire stride
 * is 16 / 16 bytes respectively (GPARAM.cs:694, 724).
 *===========================================================================*/
static sf_result_t read_value_payload(sf_binary_reader_t *r,
                                      sf_gparam_field_type_t type,
                                      sf_gparam_value_t *out_value,
                                      struct gparam_string_slot **slots,
                                      size_t *slot_count, size_t *slot_cap,
                                      const sf_allocator_t *alloc)
{
    sf_result_t e = SF_OK;
    out_value->type = type;
    switch (type) {
        /* Upstream: GPARAM.cs:523 SbyteField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_SBYTE:
            return sf_binary_reader_read_i8(r, &out_value->v.as_sbyte);
        /* Upstream: GPARAM.cs:547 ShortField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_SHORT:
            return sf_binary_reader_read_i16(r, &out_value->v.as_short);
        /* Upstream: GPARAM.cs:571 IntField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_INT:
            return sf_binary_reader_read_i32(r, &out_value->v.as_int);
        /* Upstream: GPARAM.cs:799 LongField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_LONG:
            return sf_binary_reader_read_i64(r, &out_value->v.as_long);
        /* Upstream: GPARAM.cs:595 ByteField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_BYTE:
            return sf_binary_reader_read_u8(r, &out_value->v.as_byte);
        /* Upstream: GPARAM.cs:823 UshortField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_USHORT:
            return sf_binary_reader_read_u16(r, &out_value->v.as_ushort);
        /* Upstream: GPARAM.cs:619 UintField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_UINT:
            return sf_binary_reader_read_u32(r, &out_value->v.as_uint);
        /* Upstream: GPARAM.cs:847 UlongField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_ULONG:
            return sf_binary_reader_read_u64(r, &out_value->v.as_ulong);
        /* Upstream: GPARAM.cs:643 FloatField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_FLOAT:
            return sf_binary_reader_read_f32(r, &out_value->v.as_float);
        /* Upstream: GPARAM.cs:871 DoubleField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_DOUBLE:
            return sf_binary_reader_read_f64(r, &out_value->v.as_double);
        /* Upstream: GPARAM.cs:667 BoolField.ReadValue */
        case SF_GPARAM_FIELD_TYPE_BOOL: {
            bool b = false;
            e = sf_binary_reader_read_bool(r, &b);
            out_value->v.as_bool = b ? 1 : 0;
            return e;
        }
        /* Upstream: GPARAM.cs:691-696 Vector2Field.ReadValue (8-byte padding) */
        case SF_GPARAM_FIELD_TYPE_VEC2:
            e = sf_binary_reader_read_vec2(r, &out_value->v.as_vec2);
            if (e != SF_OK) return e;
            return sf_binary_reader_assert_i64_one(r, 0);
        /* Upstream: GPARAM.cs:721-725 Vector3Field.ReadValue (4-byte padding) */
        case SF_GPARAM_FIELD_TYPE_VEC3:
            e = sf_binary_reader_read_vec3(r, &out_value->v.as_vec3);
            if (e != SF_OK) return e;
            return sf_binary_reader_assert_i32_one(r, 0);
        /* Upstream: GPARAM.cs:751 Vector4Field.ReadValue */
        case SF_GPARAM_FIELD_TYPE_VEC4:
            return sf_binary_reader_read_vec4(r, &out_value->v.as_vec4);
        /* Upstream: GPARAM.cs:775 ColorField.ReadValue (RGBA) */
        case SF_GPARAM_FIELD_TYPE_COLOR:
            return sf_binary_reader_read_rgba(r, &out_value->v.as_color);
        /* Upstream: GPARAM.cs:895 StringField.ReadValue (UTF-16) */
        case SF_GPARAM_FIELD_TYPE_STRING:
            out_value->v.as_string = NULL;
            return slot_read_utf16(r, &out_value->v.as_string,
                                   slots, slot_count, slot_cap, alloc);
        default:
            return SF_ERR_BAD_MAGIC;
    }
}

/*===========================================================================
 * Typed value payload writer (Values section)
 *
 * Upstream: per-FieldType WriteValue (GPARAM.cs:525-528, 549-552, 573-576,
 * 597-600, 621-624, 645-648, 669-672, 698-702, 728-732, 753-756, 777-780,
 * 801-804, 825-828, 849-852, 873-876, 897-900). VEC2/VEC3 emit explicit
 * zero-padding (GPARAM.cs:701, 731) so wire stride matches the reader.
 *===========================================================================*/
static sf_result_t write_value_payload(sf_binary_writer_t *w,
                                       sf_gparam_field_type_t type,
                                       const sf_gparam_value_t *value)
{
    switch (type) {
        case SF_GPARAM_FIELD_TYPE_SBYTE:
            return sf_binary_writer_write_i8(w, value->v.as_sbyte);
        case SF_GPARAM_FIELD_TYPE_SHORT:
            return sf_binary_writer_write_i16(w, value->v.as_short);
        case SF_GPARAM_FIELD_TYPE_INT:
            return sf_binary_writer_write_i32(w, value->v.as_int);
        case SF_GPARAM_FIELD_TYPE_LONG:
            return sf_binary_writer_write_i64(w, value->v.as_long);
        case SF_GPARAM_FIELD_TYPE_BYTE:
            return sf_binary_writer_write_u8(w, value->v.as_byte);
        case SF_GPARAM_FIELD_TYPE_USHORT:
            return sf_binary_writer_write_u16(w, value->v.as_ushort);
        case SF_GPARAM_FIELD_TYPE_UINT:
            return sf_binary_writer_write_u32(w, value->v.as_uint);
        case SF_GPARAM_FIELD_TYPE_ULONG:
            return sf_binary_writer_write_u64(w, value->v.as_ulong);
        case SF_GPARAM_FIELD_TYPE_FLOAT:
            return sf_binary_writer_write_f32(w, value->v.as_float);
        case SF_GPARAM_FIELD_TYPE_DOUBLE:
            return sf_binary_writer_write_f64(w, value->v.as_double);
        case SF_GPARAM_FIELD_TYPE_BOOL:
            return sf_binary_writer_write_bool(w, value->v.as_bool != 0);
        case SF_GPARAM_FIELD_TYPE_VEC2: {
            sf_result_t e = sf_binary_writer_write_vec2(w, value->v.as_vec2);
            if (e != SF_OK) return e;
            return sf_binary_writer_write_i64(w, 0);
        }
        case SF_GPARAM_FIELD_TYPE_VEC3: {
            sf_result_t e = sf_binary_writer_write_vec3(w, value->v.as_vec3);
            if (e != SF_OK) return e;
            return sf_binary_writer_write_i32(w, 0);
        }
        case SF_GPARAM_FIELD_TYPE_VEC4:
            return sf_binary_writer_write_vec4(w, value->v.as_vec4);
        case SF_GPARAM_FIELD_TYPE_COLOR:
            return sf_binary_writer_write_rgba(w, value->v.as_color);
        case SF_GPARAM_FIELD_TYPE_STRING:
            return sf_binary_writer_write_utf16(w, value->v.as_string ? value->v.as_string : "", true);
        default:
            return SF_ERR_BAD_MAGIC;
    }
}

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
 * Reader
 *
 * Upstream: GPARAM.cs:Read() (lines 70-121) — file-level orchestration.
 * Per-section helpers mirror Param/Field/FieldValue/UnkParamExtra ctors.
 *===========================================================================*/

SF_API sf_result_t sf_gparam_read_from_memory(sf_gparam_t **out, const void *bytes, size_t size,
                                              const sf_allocator_t *alloc)
{
    /* Upstream: GPARAM.cs:Read() */
    SF_CHECK_ARG(out != NULL && (size == 0u || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t              *s            = NULL;
    sf_binary_reader_t        *r            = NULL;
    sf_gparam_t               *gparam       = NULL;
    int32_t                   *param_offsets = NULL;
    int32_t                   *field_offsets = NULL;
    int32_t                   *param_comments_offsets = NULL;
    int32_t                   *comment_offsets_tmp    = NULL;
    struct gparam_string_slot *slots        = NULL;
    size_t                     slot_count   = 0;
    size_t                     slot_cap     = 0;
    sf_result_t                e            = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_reader_create(&r, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    /*  Upstream: GPARAM.cs:72-73 — force LE, assert UTF-16 magic "filt". */
    sf_binary_reader_set_big_endian(r, false);
    {
        static const uint8_t k_magic[8] = {
            0x66, 0x00, 0x69, 0x00, 0x6C, 0x00, 0x74, 0x00,
        };
        for (size_t i = 0; i < sizeof(k_magic); i++) {
            e = sf_binary_reader_assert_u8_one(r, k_magic[i]);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:74 — Version = br.ReadEnum32<GparamVersion>(). */
    uint32_t version_raw = 0;
    {
        static const uint32_t k_versions[4] = { 2, 3, 5, 6 };
        e = sf_binary_reader_read_enum_32(r, 4, k_versions, &version_raw);
        if (e != SF_OK) { e = SF_ERR_UNSUPPORTED_VERSION; goto cleanup; }
    }
    sf_gparam_version_t version = (sf_gparam_version_t)version_raw;

    /*  V2 is DS2-only and intentionally unsupported in v1 of this library.
     *  Upstream GPARAM.cs:980 even flags it with a TODO. V3 falls out of
     *  the V5 path; V5/V6 are mandatory. */
    if (version == SF_GPARAM_VERSION_V2) {
        e = SF_ERR_UNSUPPORTED_VERSION;
        goto cleanup;
    }

    /*  Upstream: GPARAM.cs:75-79 — header fields. */
    e = sf_binary_reader_assert_u8_one(r, 0);                 if (e != SF_OK) goto cleanup;
    bool unk0d = false;
    e = sf_binary_reader_read_bool(r, &unk0d);                if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i16_one(r, 0);                if (e != SF_OK) goto cleanup;

    int32_t param_count_i32 = 0;
    int32_t count14         = 0;
    e = sf_binary_reader_read_i32(r, &param_count_i32);       if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_read_i32(r, &count14);               if (e != SF_OK) goto cleanup;
    if (param_count_i32 < 0) { e = SF_ERR_BAD_MAGIC; goto cleanup; }
    size_t param_count = (size_t)param_count_i32;

    /*  Upstream: GPARAM.cs:80-94 — read all twelve BaseOffsets, sandwiched
     *  around the UnkParamExtras `capacity` count and the Unk40 float. */
    struct gparam_base_offsets base = {0};
    int32_t tmp32 = 0;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.param_offsets = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.params        = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.field_offsets = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.fields        = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.values        = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.value_ids     = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.unk30         = tmp32;

    int32_t extra_capacity_i32 = 0;
    e = sf_binary_reader_read_i32(r, &extra_capacity_i32);    if (e != SF_OK) goto cleanup;
    if (extra_capacity_i32 < 0) { e = SF_ERR_BAD_MAGIC; goto cleanup; }
    size_t extra_capacity = (size_t)extra_capacity_i32;

    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.param_extras     = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.param_extra_ids  = tmp32;

    float unk40 = 0.0f;
    e = sf_binary_reader_read_f32(r, &unk40);                 if (e != SF_OK) goto cleanup;

    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.param_comments_offsets = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.comment_offsets        = tmp32;
    e = sf_binary_reader_read_i32(r, &tmp32); if (e != SF_OK) goto cleanup; base.comments               = tmp32;

    /*  Upstream: GPARAM.cs:95-96 — V>=V5 adds Unk50. */
    float unk50 = 0.0f;
    if (version >= SF_GPARAM_VERSION_V5) {
        e = sf_binary_reader_read_f32(r, &unk50);
        if (e != SF_OK) goto cleanup;
    }

    /*  Allocate the top-level header now that we have the basic geometry. */
    gparam = (sf_gparam_t *)sf_xalloc(alloc, sizeof(*gparam));
    if (!gparam) { e = SF_ERR_OOM; goto cleanup; }
    memset(gparam, 0, sizeof(*gparam));
    gparam->alloc                 = alloc;
    gparam->version               = version;
    gparam->unk0d                 = unk0d;
    gparam->count14               = count14;
    gparam->unk40                 = unk40;
    gparam->unk50                 = unk50;
    gparam->param_count           = param_count;
    gparam->unk_param_extra_count = extra_capacity;

    if (param_count > 0) {
        gparam->params = (struct sf_gparam_param *)sf_xalloc(
            alloc, param_count * sizeof(*gparam->params));
        if (!gparam->params) { e = SF_ERR_OOM; goto cleanup; }
        memset(gparam->params, 0, param_count * sizeof(*gparam->params));
    }
    if (extra_capacity > 0) {
        gparam->unk_param_extras = (struct sf_gparam_unk_param_extra *)sf_xalloc(
            alloc, extra_capacity * sizeof(*gparam->unk_param_extras));
        if (!gparam->unk_param_extras) { e = SF_ERR_OOM; goto cleanup; }
        memset(gparam->unk_param_extras, 0,
               extra_capacity * sizeof(*gparam->unk_param_extras));
    }

    /*  Upstream: GPARAM.cs:97-103 — read ParamOffsets table then iterate
     *  params at Params + paramOffsets[i]. */
    if (param_count > 0) {
        param_offsets = (int32_t *)sf_xalloc(alloc, param_count * sizeof(int32_t));
        if (!param_offsets) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_get_i32s(r, base.param_offsets, param_count, param_offsets);
        if (e != SF_OK) goto cleanup;
    }

    for (size_t pi = 0; pi < param_count; pi++) {
        struct sf_gparam_param *param = &gparam->params[pi];
        int64_t param_pos = base.params + (int64_t)param_offsets[pi];
        e = sf_binary_reader_step_in(r, param_pos);
        if (e != SF_OK) goto cleanup;

        /*  Upstream: GPARAM.cs:1054-1070 — Param ctor. */
        int32_t field_count_i32 = 0;
        int32_t field_offsets_off = 0;
        e = sf_binary_reader_read_i32(r, &field_count_i32);
        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_i32(r, &field_offsets_off);
        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        if (field_count_i32 < 0) {
            sf_binary_reader_step_out(r);
            e = SF_ERR_BAD_MAGIC;
            goto cleanup;
        }
        size_t field_count = (size_t)field_count_i32;
        param->field_count = field_count;

        e = slot_read_utf16(r, &param->key, &slots, &slot_count, &slot_cap, alloc);
        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = slot_read_utf16(r, &param->name, &slots, &slot_count, &slot_cap, alloc);
        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }

        e = sf_binary_reader_step_out(r);
        if (e != SF_OK) goto cleanup;

        if (field_count > 0) {
            param->fields = (struct sf_gparam_field *)sf_xalloc(
                alloc, field_count * sizeof(*param->fields));
            if (!param->fields) { e = SF_ERR_OOM; goto cleanup; }
            memset(param->fields, 0, field_count * sizeof(*param->fields));

            /*  Per-param field offsets table at (FieldOffsets + fieldOffsetsOff). */
            field_offsets = (int32_t *)sf_xrealloc(
                alloc, field_offsets,
                0, field_count * sizeof(int32_t));
            if (!field_offsets) { e = SF_ERR_OOM; goto cleanup; }
            e = sf_binary_reader_get_i32s(
                r, base.field_offsets + (int64_t)field_offsets_off,
                field_count, field_offsets);
            if (e != SF_OK) goto cleanup;
        }

        /*  Upstream: GPARAM.cs:1063-1069 — for each field offset, step in
         *  to Fields + fieldOffset and dispatch IField.Read. */
        for (size_t fi = 0; fi < field_count; fi++) {
            struct sf_gparam_field *field = &param->fields[fi];
            int64_t field_pos = base.fields + (int64_t)field_offsets[fi];
            e = sf_binary_reader_step_in(r, field_pos);
            if (e != SF_OK) goto cleanup;

            /*  Upstream: GPARAM.cs:391-411 — Field<T> ctor. The two int32
             *  payload offsets are constant across versions; the byte-order
             *  of (type|capacity|unk) flips on V>=V6. */
            int32_t values_off    = 0;
            int32_t value_ids_off = 0;
            e = sf_binary_reader_read_i32(r, &values_off);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            e = sf_binary_reader_read_i32(r, &value_ids_off);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }

            uint8_t type_byte = 0;
            int32_t capacity  = 0;
            int16_t unk_field = 0;
            if (version < SF_GPARAM_VERSION_V6) {
                /*  V<V6 header: [type byte][capacity sbyte][unk int16 = 0] */
                e = sf_binary_reader_read_u8(r, &type_byte);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                int8_t cap8 = 0;
                e = sf_binary_reader_read_i8(r, &cap8);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                capacity = (int32_t)cap8;
                e = sf_binary_reader_assert_i16_one(r, 0);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                unk_field = 0;
            } else {
                /*  V>=V6 header: [capacity int16][type byte][unk byte] */
                int16_t cap16 = 0;
                e = sf_binary_reader_read_i16(r, &cap16);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                capacity = (int32_t)cap16;
                e = sf_binary_reader_read_u8(r, &type_byte);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                uint8_t unk8 = 0;
                e = sf_binary_reader_read_u8(r, &unk8);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                unk_field = (int16_t)unk8;
            }

            if (!sfi_gparam_field_type_valid(type_byte)) {
                sf_binary_reader_step_out(r);
                e = SF_ERR_BAD_MAGIC;
                goto cleanup;
            }
            if (capacity < 0) {
                sf_binary_reader_step_out(r);
                e = SF_ERR_BAD_MAGIC;
                goto cleanup;
            }
            field->type     = (sf_gparam_field_type_t)type_byte;
            field->capacity = capacity;
            field->unk      = unk_field;

            /*  Upstream: GPARAM.cs:410-411 — key + name read straight after header. */
            e = slot_read_utf16(r, &field->key,  &slots, &slot_count, &slot_cap, alloc);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            e = slot_read_utf16(r, &field->name, &slots, &slot_count, &slot_cap, alloc);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }

            /*  Done with the field's own record; pop and jump to Values
             *  section to read the typed payload array. */
            e = sf_binary_reader_step_out(r);
            if (e != SF_OK) goto cleanup;

            size_t cap = (size_t)capacity;
            field->value_count = cap;
            if (cap > 0) {
                field->values = (sf_gparam_value_t *)sf_xalloc(
                    alloc, cap * sizeof(*field->values));
                if (!field->values) { e = SF_ERR_OOM; goto cleanup; }
                memset(field->values, 0, cap * sizeof(*field->values));
            }

            /*  Upstream: GPARAM.cs:412-415 — Values section payload reads. */
            if (cap > 0) {
                e = sf_binary_reader_step_in(r, base.values + (int64_t)values_off);
                if (e != SF_OK) goto cleanup;
                for (size_t vi = 0; vi < cap; vi++) {
                    e = read_value_payload(r, field->type, &field->values[vi],
                                           &slots, &slot_count, &slot_cap, alloc);
                    if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                }
                e = sf_binary_reader_step_out(r);
                if (e != SF_OK) goto cleanup;
            }

            /*  Upstream: GPARAM.cs:416-419 — ValueIds section: per-value Id,
             *  plus Unk04 float on V>=V5 (GPARAM.cs:955-961). */
            if (cap > 0) {
                e = sf_binary_reader_step_in(r, base.value_ids + (int64_t)value_ids_off);
                if (e != SF_OK) goto cleanup;
                for (size_t vi = 0; vi < cap; vi++) {
                    e = sf_binary_reader_read_i32(r, &field->values[vi].id);
                    if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                    if (version >= SF_GPARAM_VERSION_V5) {
                        e = sf_binary_reader_read_f32(r, &field->values[vi].unk04);
                        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
                    } else {
                        field->values[vi].unk04 = 0.0f;
                    }
                }
                e = sf_binary_reader_step_out(r);
                if (e != SF_OK) goto cleanup;
            }
        }
    }

    /*  Upstream: GPARAM.cs:104-105 — Data30 opaque blob spans [Unk30, ParamExtras). */
    if (base.param_extras < base.unk30) {
        e = SF_ERR_BAD_MAGIC;
        goto cleanup;
    }
    size_t data30_size = (size_t)(base.param_extras - base.unk30);
    gparam->data30_size = data30_size;
    if (data30_size > 0) {
        gparam->data30 = (uint8_t *)sf_xalloc(alloc, data30_size);
        if (!gparam->data30) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_get_bytes(r, base.unk30, gparam->data30, data30_size);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:106-109 — UnkParamExtras read at ParamExtras base. */
    if (extra_capacity > 0) {
        e = sf_binary_reader_step_in(r, base.param_extras);
        if (e != SF_OK) goto cleanup;
        for (size_t ei = 0; ei < extra_capacity; ei++) {
            struct sf_gparam_unk_param_extra *ex = &gparam->unk_param_extras[ei];
            /*  Upstream: GPARAM.cs:1223-1234 — UnkParamExtra ctor. */
            e = sf_binary_reader_read_i32(r, &ex->unk00);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            int32_t id_count_i32 = 0;
            int32_t ids_off      = 0;
            e = sf_binary_reader_read_i32(r, &id_count_i32);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            e = sf_binary_reader_read_i32(r, &ids_off);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            if (id_count_i32 < 0) {
                sf_binary_reader_step_out(r);
                e = SF_ERR_BAD_MAGIC;
                goto cleanup;
            }
            ex->id_count = (size_t)id_count_i32;
            ex->unk0c    = 0;
            if (version >= SF_GPARAM_VERSION_V5) {
                e = sf_binary_reader_read_i32(r, &ex->unk0c);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            }
            if (ex->id_count > 0) {
                ex->ids = (int32_t *)sf_xalloc(alloc, ex->id_count * sizeof(int32_t));
                if (!ex->ids) {
                    sf_binary_reader_step_out(r);
                    e = SF_ERR_OOM;
                    goto cleanup;
                }
                e = sf_binary_reader_get_i32s(
                    r, base.param_extra_ids + (int64_t)ids_off,
                    ex->id_count, ex->ids);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
            }
        }
        e = sf_binary_reader_step_out(r);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:110-120 — comments are a two-level offset table.
     *  ParamCommentsOffsets[i] points into CommentOffsets at the start of
     *  param i's comment-offsets sub-array; each entry in that sub-array
     *  points into Comments at a UTF-16 string. */
    if (param_count > 0) {
        param_comments_offsets = (int32_t *)sf_xalloc(alloc, param_count * sizeof(int32_t));
        if (!param_comments_offsets) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_get_i32s(
            r, base.param_comments_offsets, param_count, param_comments_offsets);
        if (e != SF_OK) goto cleanup;
    }

    for (size_t pi = 0; pi < param_count; pi++) {
        struct sf_gparam_param *param = &gparam->params[pi];
        int64_t this_off = base.comment_offsets + (int64_t)param_comments_offsets[pi];
        int64_t end_off;
        if (pi + 1u >= param_count) {
            end_off = base.comments;
        } else {
            end_off = base.comment_offsets + (int64_t)param_comments_offsets[pi + 1u];
        }
        if (end_off < this_off) { e = SF_ERR_BAD_MAGIC; goto cleanup; }
        int64_t span = end_off - this_off;
        if ((span % 4) != 0) { e = SF_ERR_BAD_MAGIC; goto cleanup; }
        size_t comment_count = (size_t)(span / 4);
        param->comment_count = comment_count;
        if (comment_count == 0) {
            param->comments = NULL;
            continue;
        }

        comment_offsets_tmp = (int32_t *)sf_xrealloc(
            alloc, comment_offsets_tmp,
            0, comment_count * sizeof(int32_t));
        if (!comment_offsets_tmp) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_get_i32s(
            r, this_off, comment_count, comment_offsets_tmp);
        if (e != SF_OK) goto cleanup;

        param->comments = (const char **)sf_xalloc(
            alloc, comment_count * sizeof(*param->comments));
        if (!param->comments) { e = SF_ERR_OOM; goto cleanup; }
        memset(param->comments, 0, comment_count * sizeof(*param->comments));

        for (size_t ci = 0; ci < comment_count; ci++) {
            int64_t com_off = base.comments + (int64_t)comment_offsets_tmp[ci];
            e = slot_get_utf16(r, com_off, &param->comments[ci],
                               &slots, &slot_count, &slot_cap, alloc);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  All wire reads are complete. Materialise the name_pool and back-patch
     *  every captured string slot to its final in-pool address. */
    e = finalize_string_pool(gparam, slots, slot_count, alloc);
    if (e != SF_OK) goto cleanup;

    *out   = gparam;
    gparam = NULL; /* ownership transferred */

cleanup:
    /*  Reader-owned temp strings still hanging off slots[].utf8. */
    if (slots) {
        for (size_t i = 0; i < slot_count; i++) sf_xfree(alloc, slots[i].utf8);
        sf_xfree(alloc, slots);
    }
    sf_xfree(alloc, param_offsets);
    sf_xfree(alloc, field_offsets);
    sf_xfree(alloc, param_comments_offsets);
    sf_xfree(alloc, comment_offsets_tmp);
    if (gparam) sf_gparam_destroy(gparam);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    return e;
}

/*===========================================================================
 * Writer helpers — reservation-name formatting
 *
 * Upstream uses C# interpolated strings; we synthesise the same names via
 * snprintf so reservation lookups match between reserve() and fill() pairs.
 *===========================================================================*/

static sf_result_t fmt_name(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap) return SF_ERR_INTERNAL;
    return SF_OK;
}

/*===========================================================================
 * Writer
 *
 * Upstream: GPARAM.cs:Write() (lines 123-218) — file-level orchestration.
 *===========================================================================*/

SF_API sf_result_t sf_gparam_write_to_buffer(const sf_gparam_t *gparam, void **out_bytes,
                                             size_t *out_size, const sf_allocator_t *alloc)
{
    /* Upstream: GPARAM.cs:Write() */
    SF_CHECK_ARG(gparam != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size  = 0u;
    alloc = sf_alloc_or_default(alloc);

    /*  V2 is rejected on read; rejecting on write keeps the policy symmetric. */
    if (gparam->version == SF_GPARAM_VERSION_V2) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    if (gparam->version != SF_GPARAM_VERSION_V3 &&
        gparam->version != SF_GPARAM_VERSION_V5 &&
        gparam->version != SF_GPARAM_VERSION_V6) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    sf_ostream_t       *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t         e = SF_OK;

    e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_writer_create(&w, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    /*  Upstream: GPARAM.cs:126-127 — force LE, write UTF-16 "filt" signature. */
    sf_binary_writer_set_big_endian(w, false);
    e = sf_binary_writer_write_utf16(w, "filt", false);
    if (e != SF_OK) goto cleanup;

    /*  Upstream: GPARAM.cs:128-133 — header. */
    e = sf_binary_writer_write_u32(w, (uint32_t)gparam->version);          if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_u8 (w, 0);                                  if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_bool(w, gparam->unk0d);                     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i16(w, 0);                                  if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, (int32_t)gparam->param_count);       if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, gparam->count14);                    if (e != SF_OK) goto cleanup;

    /*  Upstream: GPARAM.cs:134-147 — reserve all BaseOffsets section pointers. */
    e = sf_binary_writer_reserve_i32(w, "ParamOffsetsBase");               if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ParamsBase");                     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "FieldOffsetsBase");               if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "FieldsBase");                     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ValuesBase");                     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ValueIdsBase");                   if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "Unk30Base");                      if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, (int32_t)gparam->unk_param_extra_count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ParamExtrasBase");                if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ParamExtraIdsBase");              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_f32(w, gparam->unk40);                      if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "ParamCommentsOffsetsBase");       if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "CommentOffsetsBase");             if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "CommentsBase");                   if (e != SF_OK) goto cleanup;

    /*  Upstream: GPARAM.cs:148-149 — V>=V5 adds Unk50. */
    if (gparam->version >= SF_GPARAM_VERSION_V5) {
        e = sf_binary_writer_write_f32(w, gparam->unk50);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:150-162 — ParamOffsets section: one reserved int32
     *  per param, name-keyed by index. */
    int32_t param_offsets_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ParamOffsetsBase", param_offsets_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        char nm[48];
        e = fmt_name(nm, sizeof(nm), "ParamOffset[%zu]", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(w, nm);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:163-177 — Params section: one record per param. */
    int32_t params_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ParamsBase", params_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        char nm[48];
        e = fmt_name(nm, sizeof(nm), "ParamOffset[%zu]", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_fill_i32(
            w, nm,
            (int32_t)(sf_binary_writer_position(w) - (int64_t)params_base));
        if (e != SF_OK) goto cleanup;

        /*  Upstream: GPARAM.cs:1072-1084 — Param.Write. */
        e = sf_binary_writer_write_i32(w, (int32_t)p->field_count);
        if (e != SF_OK) goto cleanup;
        e = fmt_name(nm, sizeof(nm), "Param[%zu]FieldOffsetsOffset", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(w, nm);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_utf16(w, p->key  ? p->key  : "", true);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_utf16(w, p->name ? p->name : "", true);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_pad(w, 4);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:178-181 — FieldOffsets section: per-param sub-table. */
    int32_t field_offsets_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "FieldOffsetsBase", field_offsets_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        char nm[64];
        /*  Upstream: GPARAM.cs:1086-1098 — fill Param[i]FieldOffsetsOffset. */
        e = fmt_name(nm, sizeof(nm), "Param[%zu]FieldOffsetsOffset", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_fill_i32(
            w, nm,
            (int32_t)(sf_binary_writer_position(w) - (int64_t)field_offsets_base));
        if (e != SF_OK) goto cleanup;
        /*  Upstream: GPARAM.cs:1099-1110 — reserve per-field offset entries. */
        for (size_t fi = 0; fi < p->field_count; fi++) {
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]Offset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_reserve_i32(w, nm);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:182-185 — Fields section: typed-field records. */
    int32_t fields_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "FieldsBase", fields_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        for (size_t fi = 0; fi < p->field_count; fi++) {
            const struct sf_gparam_field *f = &p->fields[fi];
            char nm[64];
            /*  Upstream: GPARAM.cs:1113-1129 — Param.WriteFields. */
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]Offset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_fill_i32(
                w, nm,
                (int32_t)(sf_binary_writer_position(w) - (int64_t)fields_base));
            if (e != SF_OK) goto cleanup;

            /*  Upstream: GPARAM.cs:424-461 — Field<T>.Write header. */
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]ValuesOffset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_reserve_i32(w, nm);
            if (e != SF_OK) goto cleanup;
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]ValueIdsOffset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_reserve_i32(w, nm);
            if (e != SF_OK) goto cleanup;

            if (gparam->version < SF_GPARAM_VERSION_V6) {
                /*  V<V6 header: [type byte][capacity sbyte][unk int16] */
                e = sf_binary_writer_write_u8(w, (uint8_t)f->type);
                if (e != SF_OK) goto cleanup;
                e = sf_binary_writer_write_i8(w, (int8_t)f->capacity);
                if (e != SF_OK) goto cleanup;
                e = sf_binary_writer_write_i16(w, f->unk);
                if (e != SF_OK) goto cleanup;
            } else {
                /*  V>=V6 header: [capacity int16][type byte][unk byte] */
                e = sf_binary_writer_write_i16(w, (int16_t)f->capacity);
                if (e != SF_OK) goto cleanup;
                e = sf_binary_writer_write_u8(w, (uint8_t)f->type);
                if (e != SF_OK) goto cleanup;
                e = sf_binary_writer_write_u8(w, (uint8_t)f->unk);
                if (e != SF_OK) goto cleanup;
            }

            e = sf_binary_writer_write_utf16(w, f->key  ? f->key  : "", true);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_utf16(w, f->name ? f->name : "", true);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_pad(w, 4);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:186-189 — Values section: typed payloads per field. */
    int32_t values_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ValuesBase", values_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        for (size_t fi = 0; fi < p->field_count; fi++) {
            const struct sf_gparam_field *f = &p->fields[fi];
            char nm[64];
            /*  Upstream: GPARAM.cs:463-481 — Field<T>.WriteValues. */
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]ValuesOffset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_fill_i32(
                w, nm,
                (int32_t)(sf_binary_writer_position(w) - (int64_t)values_base));
            if (e != SF_OK) goto cleanup;
            for (size_t vi = 0; vi < f->value_count; vi++) {
                e = write_value_payload(w, f->type, &f->values[vi]);
                if (e != SF_OK) goto cleanup;
            }
            /*  Upstream: GPARAM.cs:1137 — pad(4) after each field's values. */
            e = sf_binary_writer_pad(w, 4);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:190-193 — ValueIds section: per-value Id (+ Unk04 on V>=V5). */
    int32_t value_ids_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ValueIdsBase", value_ids_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        for (size_t fi = 0; fi < p->field_count; fi++) {
            const struct sf_gparam_field *f = &p->fields[fi];
            char nm[64];
            /*  Upstream: GPARAM.cs:485-504 — Field<T>.WriteValueIds. */
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Field[%zu]ValueIdsOffset", pi, fi);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_fill_i32(
                w, nm,
                (int32_t)(sf_binary_writer_position(w) - (int64_t)value_ids_base));
            if (e != SF_OK) goto cleanup;
            for (size_t vi = 0; vi < f->value_count; vi++) {
                /*  Upstream: GPARAM.cs:963-969 — FieldValue<T>.Write. */
                e = sf_binary_writer_write_i32(w, f->values[vi].id);
                if (e != SF_OK) goto cleanup;
                if (gparam->version >= SF_GPARAM_VERSION_V5) {
                    e = sf_binary_writer_write_f32(w, f->values[vi].unk04);
                    if (e != SF_OK) goto cleanup;
                }
            }
        }
    }

    /*  Upstream: GPARAM.cs:194-197 — Unk30 (Data30) opaque blob + pad(4). */
    int32_t unk30_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "Unk30Base", unk30_base);
    if (e != SF_OK) goto cleanup;
    if (gparam->data30_size > 0) {
        e = sf_binary_writer_write_bytes(w, gparam->data30, gparam->data30_size);
        if (e != SF_OK) goto cleanup;
    }
    e = sf_binary_writer_pad(w, 4);
    if (e != SF_OK) goto cleanup;

    /*  Upstream: GPARAM.cs:198-201 — ParamExtras section. */
    int32_t param_extras_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ParamExtrasBase", param_extras_base);
    if (e != SF_OK) goto cleanup;
    for (size_t ei = 0; ei < gparam->unk_param_extra_count; ei++) {
        const struct sf_gparam_unk_param_extra *ex = &gparam->unk_param_extras[ei];
        char nm[48];
        /*  Upstream: GPARAM.cs:1236-1250 — UnkParamExtra.Write. */
        e = sf_binary_writer_write_i32(w, ex->unk00);                      if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, (int32_t)ex->id_count);          if (e != SF_OK) goto cleanup;
        e = fmt_name(nm, sizeof(nm), "ParamExtra[%zu]IdsOffset", ei);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(w, nm);                           if (e != SF_OK) goto cleanup;
        if (gparam->version >= SF_GPARAM_VERSION_V5) {
            e = sf_binary_writer_write_i32(w, ex->unk0c);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:202-205 — ParamExtraIds section: per-extra ID array. */
    int32_t param_extra_ids_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ParamExtraIdsBase", param_extra_ids_base);
    if (e != SF_OK) goto cleanup;
    for (size_t ei = 0; ei < gparam->unk_param_extra_count; ei++) {
        const struct sf_gparam_unk_param_extra *ex = &gparam->unk_param_extras[ei];
        char nm[48];
        e = fmt_name(nm, sizeof(nm), "ParamExtra[%zu]IdsOffset", ei);
        if (e != SF_OK) goto cleanup;
        /*  Upstream: GPARAM.cs:1252-1275 — empty list fills with 0 (no data
         *  contributed), populated lists fill with the relative offset. */
        if (ex->id_count == 0) {
            e = sf_binary_writer_fill_i32(w, nm, 0);
            if (e != SF_OK) goto cleanup;
        } else {
            e = sf_binary_writer_fill_i32(
                w, nm,
                (int32_t)(sf_binary_writer_position(w) - (int64_t)param_extra_ids_base));
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_i32s(w, ex->id_count, ex->ids);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:206-209 — ParamCommentsOffsets: per-param sub-table pointer. */
    int32_t param_comments_offsets_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "ParamCommentsOffsetsBase", param_comments_offsets_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        char nm[64];
        e = fmt_name(nm, sizeof(nm), "Param[%zu]CommentOffsetsOffset", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_reserve_i32(w, nm);
        if (e != SF_OK) goto cleanup;
    }

    /*  Upstream: GPARAM.cs:210-213 — CommentOffsets: per-comment offset arrays. */
    int32_t comment_offsets_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "CommentOffsetsBase", comment_offsets_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        char nm[64];
        /*  Upstream: GPARAM.cs:1162-1187 — Param.WriteCommentOffsets. */
        e = fmt_name(nm, sizeof(nm), "Param[%zu]CommentOffsetsOffset", pi);
        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_fill_i32(
            w, nm,
            (int32_t)(sf_binary_writer_position(w) - (int64_t)comment_offsets_base));
        if (e != SF_OK) goto cleanup;
        for (size_t ci = 0; ci < p->comment_count; ci++) {
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Comment[%zu]Offset", pi, ci);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_reserve_i32(w, nm);
            if (e != SF_OK) goto cleanup;
        }
    }

    /*  Upstream: GPARAM.cs:214-217 — Comments section: UTF-16 strings + pad(4). */
    int32_t comments_base = (int32_t)sf_binary_writer_position(w);
    e = sf_binary_writer_fill_i32(w, "CommentsBase", comments_base);
    if (e != SF_OK) goto cleanup;
    for (size_t pi = 0; pi < gparam->param_count; pi++) {
        const struct sf_gparam_param *p = &gparam->params[pi];
        for (size_t ci = 0; ci < p->comment_count; ci++) {
            char nm[64];
            /*  Upstream: GPARAM.cs:1189-1209 — Param.WriteComments. */
            e = fmt_name(nm, sizeof(nm), "Param[%zu]Comment[%zu]Offset", pi, ci);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_fill_i32(
                w, nm,
                (int32_t)(sf_binary_writer_position(w) - (int64_t)comments_base));
            if (e != SF_OK) goto cleanup;
            const char *comment = p->comments[ci] ? p->comments[ci] : "";
            e = sf_binary_writer_write_utf16(w, comment, true);
            if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_pad(w, 4);
            if (e != SF_OK) goto cleanup;
        }
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);
    w = NULL; /* finish_bytes destroyed the writer on success */

cleanup:
    if (w) sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
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
     *  field, so the accessor is a single bounds-checked struct copy. */
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
