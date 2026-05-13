/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FMB (.expb) public surface.
 *
 * "expression" container introduced in Elden Ring. Extension: .expb.
 * Magic: "FMB " (always little-endian). The file is a flat list of
 * 0x30-byte entries indexed via a table of 64-bit offsets. Each entry
 * carries a 32-bit type code and one of four payload shapes:
 *
 *   - PLAIN   : no extra data         (types 2,5,6,12,14,21,31,32,33,34,43)
 *   - STRING  : ASCII string pointer  (types 7,11)
 *   - DOUBLE  : single double         (types 1,3,4,8,51,61)
 *   - DOUBLE2 : two doubles           (type  52)
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/FMB.cs
 */

#ifndef SOULS_FORMATS_SF_FMB_H
#define SOULS_FORMATS_SF_FMB_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*  Payload-shape classification of an FMB entry, derived from the type
 *  code. Used to know which fields of sf_fmb_entry_t carry data. */
typedef enum sf_fmb_entry_kind {
    SF_FMB_ENTRY_KIND_PLAIN   = 0, /* types: 2,5,6,12,14,21,31,32,33,34,43 */
    SF_FMB_ENTRY_KIND_STRING  = 1, /* types: 7,11                          */
    SF_FMB_ENTRY_KIND_DOUBLE  = 2, /* types: 1,3,4,8,51,61                 */
    SF_FMB_ENTRY_KIND_DOUBLE2 = 3, /* type:  52                            */
} sf_fmb_entry_kind_t;

_Static_assert(SF_FMB_ENTRY_KIND_PLAIN   == 0, "FMB entry kind enum drift");
_Static_assert(SF_FMB_ENTRY_KIND_STRING  == 1, "FMB entry kind enum drift");
_Static_assert(SF_FMB_ENTRY_KIND_DOUBLE  == 2, "FMB entry kind enum drift");
_Static_assert(SF_FMB_ENTRY_KIND_DOUBLE2 == 3, "FMB entry kind enum drift");

/*  Public POD describing a single entry. `string_value` is heap-owned by
 *  the parent sf_fmb_t and freed on sf_fmb_destroy(); do not free it. */
typedef struct sf_fmb_entry {
    int32_t              type;
    sf_fmb_entry_kind_t  kind;
    char                *string_value;   /* UTF-8, only for KIND_STRING; NULL otherwise */
    double               double_value;   /* for KIND_DOUBLE and DOUBLE2.value1          */
    double               double_value2;  /* for KIND_DOUBLE2                            */
} sf_fmb_entry_t;

typedef struct sf_fmb sf_fmb_t;

/*  Create an empty FMB with zero entries. */
SF_API sf_result_t sf_fmb_create(sf_fmb_t **out, const sf_allocator_t *alloc);

/*  Destroy. NULL-safe. */
SF_API void sf_fmb_destroy(sf_fmb_t *fmb);

/*  Parse a complete FMB from an in-memory byte buffer. */
SF_API sf_result_t sf_fmb_read_from_memory(sf_fmb_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *alloc);

/*  Serialize the FMB into a freshly allocated buffer. Caller frees
 *  `*out_data` via sf_free(alloc, *out_data). */
SF_API sf_result_t sf_fmb_write_to_memory(const sf_fmb_t *fmb, uint8_t **out_data,
                                          size_t *out_size, const sf_allocator_t *alloc);

/*  Magic probe — returns true iff the buffer starts with "FMB ". */
SF_API bool sf_fmb_is(const void *bytes, size_t size);

SF_API int32_t sf_fmb_get_unk20(const sf_fmb_t *fmb);
SF_API void    sf_fmb_set_unk20(sf_fmb_t *fmb, int32_t v);

SF_API size_t  sf_fmb_entry_count(const sf_fmb_t *fmb);

/*  Borrow an internal pointer to an entry. The pointer is valid until
 *  the next mutation or destroy. */
SF_API sf_result_t sf_fmb_get_entry(const sf_fmb_t *fmb, size_t index,
                                    const sf_fmb_entry_t **out);

/*  Append a new entry. `type` must match the requested kind in the
 *  upstream classification table; mismatches return SF_ERR_INVALID_ARG. */
SF_API sf_result_t sf_fmb_add_plain_entry  (sf_fmb_t *fmb, int32_t type);
SF_API sf_result_t sf_fmb_add_string_entry (sf_fmb_t *fmb, int32_t type, const char *value);
SF_API sf_result_t sf_fmb_add_double_entry (sf_fmb_t *fmb, int32_t type, double value);
SF_API sf_result_t sf_fmb_add_double2_entry(sf_fmb_t *fmb, int32_t type,
                                            double value1, double value2);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_FMB_H */
