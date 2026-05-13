/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — EDD (ESD Description) public surface.
 *
 * A description format for ESDs, published only for DS2. It is NOT read
 * by the game itself; it's a developer tool format that documents the
 * built-in functions and commands available in ESD state machines.
 *
 * EDD has a 32-bit ("fSSL") and a 64-bit ("fsSL") format, controlled by
 * the magic. The 64-bit format uses 8-byte varints for offsets and IDs.
 * Both are always little-endian.
 *
 * This implementation is read-only: the format is large and complex, and
 * writing is not required by any consumer.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/EDD.cs
 */

#ifndef SOULS_FORMATS_SF_EDD_H
#define SOULS_FORMATS_SF_EDD_H

#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_edd sf_edd_t;

/* A description of a built-in function callable from an ESD. The `name`
 * field is a copy of the resolved name string from the EDD's internal
 * string table; it is owned by the EDD and lives until the EDD is
 * destroyed. Callers must not free `name`. */
typedef struct sf_edd_function_spec {
    int32_t id;
    const char *name;
    uint8_t unk06;
    uint8_t unk07;
} sf_edd_function_spec_t;

/* A description of a built-in command callable from an ESD. The `name`
 * field is owned by the EDD (see sf_edd_function_spec_t notes). */
typedef struct sf_edd_command_spec {
    int64_t id;
    const char *name;
    int16_t unk0e;
} sf_edd_command_spec_t;

SF_API sf_result_t sf_edd_create(sf_edd_t **out, bool long_format,
                                 const sf_allocator_t *alloc);
SF_API void sf_edd_destroy(sf_edd_t *edd);

SF_API sf_result_t sf_edd_read_from_memory(sf_edd_t **out, const void *bytes,
                                           size_t size, const sf_allocator_t *alloc);

SF_API bool sf_edd_long_format(const sf_edd_t *edd);
SF_API int32_t sf_edd_unk80(const sf_edd_t *edd);

SF_API size_t sf_edd_function_spec_count(const sf_edd_t *edd);
SF_API sf_result_t sf_edd_get_function_spec(const sf_edd_t *edd, size_t index,
                                            sf_edd_function_spec_t *out);

SF_API size_t sf_edd_command_spec_count(const sf_edd_t *edd);
SF_API sf_result_t sf_edd_get_command_spec(const sf_edd_t *edd, size_t index,
                                           sf_edd_command_spec_t *out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_EDD_H */
