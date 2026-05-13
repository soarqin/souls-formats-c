/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — EMEVD public surface.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/EMEVD/EMEVD.cs
 *   SoulsFormats/Formats/EMEVD/Event.cs
 *   SoulsFormats/Formats/EMEVD/Instruction.cs
 *   SoulsFormats/Formats/EMEVD/Layer.cs
 *   SoulsFormats/Formats/EMEVD/Parameter.cs
 *
 * ER/AC6/Nightreign format probe: see .sisyphus/evidence/phase4-pre-flight.md
 * Wave 0 result: EMEVD format unavailable (BHD5 parse issue); defaulting to
 * Sekiro alias. If future probe shows Novel flags, extend sf_emevd_format_t
 * with new values.
 */

#ifndef SOULS_FORMATS_SF_EMEVD_H
#define SOULS_FORMATS_SF_EMEVD_H

#include "sf_common.h"
#include "sf_io.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_emevd sf_emevd_t;
typedef struct sf_emevd_event sf_emevd_event_t;
typedef struct sf_emevd_instruction sf_emevd_instruction_t;
typedef struct sf_emevd_layer sf_emevd_layer_t;
typedef struct sf_emevd_parameter sf_emevd_parameter_t;

#if defined(__cplusplus)
#define SF_EMEVD_STATIC_ASSERT static_assert
#else
#define SF_EMEVD_STATIC_ASSERT _Static_assert
#endif

typedef enum sf_emevd_format {
    SF_EMEVD_FORMAT_DARK_SOULS_1 = 0,  /* bigEndian=0, is64Bit=0, unk06=0, unk07=0, ver=0xCC */
    SF_EMEVD_FORMAT_DARK_SOULS_1_BE = 1, /* bigEndian=1, is64Bit=0, unk06=0, unk07=0, ver=0xCC */
    SF_EMEVD_FORMAT_BLOODBORNE = 2,     /* bigEndian=0, is64Bit=1, unk06=0, unk07=0, ver=0xCC */
    SF_EMEVD_FORMAT_DARK_SOULS_3 = 3,   /* bigEndian=0, is64Bit=1, unk06=1, unk07=0, ver=0xCC */
    SF_EMEVD_FORMAT_SEKIRO = 4,         /* bigEndian=0, is64Bit=1, unk06=1, unk07=1, ver=0xCD */
    /* Extension aliases — ER/AC6/Nightreign use Sekiro flag set per Wave 0 probe.
     * See .sisyphus/evidence/phase4-pre-flight.md for empirical verification. */
    SF_EMEVD_FORMAT_ELDEN_RING = SF_EMEVD_FORMAT_SEKIRO,
    SF_EMEVD_FORMAT_ARMORED_CORE_VI = SF_EMEVD_FORMAT_SEKIRO,
    SF_EMEVD_FORMAT_NIGHTREIGN = SF_EMEVD_FORMAT_SEKIRO,
} sf_emevd_format_t;

SF_EMEVD_STATIC_ASSERT(SF_EMEVD_FORMAT_SEKIRO >= 0,
                       "EMEVD format enum must have non-negative values");
SF_EMEVD_STATIC_ASSERT(SF_EMEVD_FORMAT_SEKIRO == 4, "Sekiro format value must be stable");
SF_EMEVD_STATIC_ASSERT(SF_EMEVD_FORMAT_ELDEN_RING == SF_EMEVD_FORMAT_SEKIRO,
                       "Elden Ring format alias must match Sekiro");
SF_EMEVD_STATIC_ASSERT(SF_EMEVD_FORMAT_ARMORED_CORE_VI == SF_EMEVD_FORMAT_SEKIRO,
                       "Armored Core VI format alias must match Sekiro");
SF_EMEVD_STATIC_ASSERT(SF_EMEVD_FORMAT_NIGHTREIGN == SF_EMEVD_FORMAT_SEKIRO,
                       "Nightreign format alias must match Sekiro");

typedef enum sf_emevd_rest_behavior {
    SF_EMEVD_REST_BEHAVIOR_DEFAULT = 0,
    SF_EMEVD_REST_BEHAVIOR_RESTART = 1,
    SF_EMEVD_REST_BEHAVIOR_END = 2,
} sf_emevd_rest_behavior_t;

SF_EMEVD_STATIC_ASSERT(SF_EMEVD_REST_BEHAVIOR_END == 2,
                       "RestBehavior constants must be stable");

SF_API sf_result_t sf_emevd_read_from_memory(sf_emevd_t **out, const uint8_t *data,
                                             size_t size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_emevd_read_from_stream(sf_emevd_t **out, sf_istream_t *stream,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_emevd_read_from_path(sf_emevd_t **out, const wchar_t *path,
                                           const sf_allocator_t *alloc);

SF_API sf_result_t sf_emevd_write_to_memory(const sf_emevd_t *emevd, uint8_t **out,
                                            size_t *out_size, const sf_allocator_t *alloc);
SF_API sf_result_t sf_emevd_write_to_stream(const sf_emevd_t *emevd, sf_ostream_t *stream,
                                            const sf_allocator_t *alloc);
SF_API sf_result_t sf_emevd_write_to_path(const sf_emevd_t *emevd, const wchar_t *path,
                                          const sf_allocator_t *alloc);

SF_API sf_result_t sf_emevd_create(const sf_allocator_t *alloc, sf_emevd_format_t format,
                                   sf_emevd_t **out);
SF_API void sf_emevd_destroy(sf_emevd_t *emevd, const sf_allocator_t *alloc);

SF_API sf_emevd_format_t sf_emevd_get_format(const sf_emevd_t *emevd);
SF_API size_t sf_emevd_get_event_count(const sf_emevd_t *emevd);
SF_API const sf_emevd_event_t *sf_emevd_get_event(const sf_emevd_t *emevd, size_t index);
SF_API size_t sf_emevd_get_linked_file_count(const sf_emevd_t *emevd);
SF_API int64_t sf_emevd_get_linked_file_offset(const sf_emevd_t *emevd, size_t index);
SF_API const uint8_t *sf_emevd_get_string_data(const sf_emevd_t *emevd);
SF_API size_t sf_emevd_get_string_data_size(const sf_emevd_t *emevd);

SF_API int64_t sf_emevd_event_get_id(const sf_emevd_event_t *event);
SF_API sf_emevd_rest_behavior_t sf_emevd_event_get_rest_behavior(
    const sf_emevd_event_t *event);
SF_API size_t sf_emevd_event_get_instruction_count(const sf_emevd_event_t *event);
SF_API const sf_emevd_instruction_t *sf_emevd_event_get_instruction(
    const sf_emevd_event_t *event, size_t index);
SF_API size_t sf_emevd_event_get_parameter_count(const sf_emevd_event_t *event);
SF_API const sf_emevd_parameter_t *sf_emevd_event_get_parameter(const sf_emevd_event_t *event,
                                                                size_t index);

SF_API int32_t sf_emevd_instruction_get_bank(const sf_emevd_instruction_t *instr);
SF_API int32_t sf_emevd_instruction_get_id(const sf_emevd_instruction_t *instr);
SF_API sf_result_t sf_emevd_instruction_get_arg_data(const sf_emevd_instruction_t *instr,
                                                     const uint8_t **out_data,
                                                     size_t *out_size);
SF_API const sf_emevd_layer_t *sf_emevd_instruction_get_layer(
    const sf_emevd_instruction_t *instr);

SF_API uint32_t sf_emevd_layer_get_mask(const sf_emevd_layer_t *layer);

SF_API int64_t sf_emevd_parameter_get_instruction_index(const sf_emevd_parameter_t *parameter);
SF_API int64_t sf_emevd_parameter_get_target_start_byte(const sf_emevd_parameter_t *parameter);
SF_API int64_t sf_emevd_parameter_get_source_start_byte(const sf_emevd_parameter_t *parameter);
SF_API int32_t sf_emevd_parameter_get_byte_count(const sf_emevd_parameter_t *parameter);
SF_API int32_t sf_emevd_parameter_get_unk_id(const sf_emevd_parameter_t *parameter);

#undef SF_EMEVD_STATIC_ASSERT

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_EMEVD_H */
