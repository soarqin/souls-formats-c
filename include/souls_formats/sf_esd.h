/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — ESD public surface.
 *
 * Upstream reference (pinned commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/ESD.cs
 */

#ifndef SOULS_FORMATS_SF_ESD_H
#define SOULS_FORMATS_SF_ESD_H

#include "sf_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_esd sf_esd_t;
typedef struct sf_esd_state sf_esd_state_t;
typedef struct sf_esd_condition sf_esd_condition_t;
typedef struct sf_esd_command_call sf_esd_command_call_t;

#ifdef __cplusplus
#define _Static_assert static_assert
#endif

SF_API bool sf_esd_is_long_format(const sf_esd_t *esd);
SF_API int32_t sf_esd_get_format_version(const sf_esd_t *esd);
SF_API sf_result_t sf_esd_get_name(const sf_esd_t *esd, char **out_name);
SF_API int32_t sf_esd_get_state_group_count(const sf_esd_t *esd);
SF_API sf_result_t sf_esd_get_state_group_id(const sf_esd_t *esd, int32_t idx, int64_t *out_id);
SF_API int32_t sf_esd_get_state_count(const sf_esd_t *esd, int64_t group_id);
SF_API const sf_esd_state_t *sf_esd_get_state(const sf_esd_t *esd, int64_t group_id, int32_t state_idx);

SF_API int64_t sf_esd_state_get_id(const sf_esd_state_t *s);
SF_API int32_t sf_esd_state_get_condition_count(const sf_esd_state_t *s);
SF_API const sf_esd_condition_t *sf_esd_state_get_condition(const sf_esd_state_t *s, int32_t idx);
SF_API int32_t sf_esd_state_get_entry_command_count(const sf_esd_state_t *s);
SF_API const sf_esd_command_call_t *sf_esd_state_get_entry_command(const sf_esd_state_t *s, int32_t i);
SF_API int32_t sf_esd_state_get_exit_command_count(const sf_esd_state_t *s);
SF_API const sf_esd_command_call_t *sf_esd_state_get_exit_command(const sf_esd_state_t *s, int32_t i);
SF_API int32_t sf_esd_state_get_while_command_count(const sf_esd_state_t *s);
SF_API const sf_esd_command_call_t *sf_esd_state_get_while_command(const sf_esd_state_t *s, int32_t i);

SF_API int64_t sf_esd_condition_get_target_state(const sf_esd_condition_t *c);
SF_API sf_result_t sf_esd_condition_get_evaluator(const sf_esd_condition_t *c,
                                                  const uint8_t **out_bytes,
                                                  size_t *out_size);
SF_API int32_t sf_esd_condition_get_subcondition_count(const sf_esd_condition_t *c);
SF_API const sf_esd_condition_t *sf_esd_condition_get_subcondition(const sf_esd_condition_t *c, int32_t i);
SF_API int32_t sf_esd_condition_get_pass_command_count(const sf_esd_condition_t *c);
SF_API const sf_esd_command_call_t *sf_esd_condition_get_pass_command(const sf_esd_condition_t *c, int32_t i);

SF_API int32_t sf_esd_command_call_get_bank(const sf_esd_command_call_t *cc);
SF_API int32_t sf_esd_command_call_get_id(const sf_esd_command_call_t *cc);
SF_API int32_t sf_esd_command_call_get_argument_count(const sf_esd_command_call_t *cc);
SF_API sf_result_t sf_esd_command_call_get_argument(const sf_esd_command_call_t *cc, int32_t idx,
                                                     const uint8_t **out_bytes, size_t *out_size);

SF_API sf_result_t sf_esd_read_from_memory(sf_esd_t **out, const uint8_t *data, size_t size,
                                           const sf_allocator_t *alloc);
SF_API sf_result_t sf_esd_write_to_memory(const sf_esd_t *esd, uint8_t **out_data, size_t *out_size,
                                          const sf_allocator_t *alloc);
SF_API void sf_esd_destroy(sf_esd_t *esd);

_Static_assert(sizeof(int32_t) == 4, "sf_esd.h: int32_t size check");
_Static_assert(sizeof(int64_t) == 8, "sf_esd.h: int64_t size check");

#ifdef __cplusplus
#undef _Static_assert
}
#endif

#endif /* SOULS_FORMATS_SF_ESD_H */
