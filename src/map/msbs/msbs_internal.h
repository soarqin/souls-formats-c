/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS internal sub-param hooks.
 */

#ifndef SF_MAP_MSBS_INTERNAL_H
#define SF_MAP_MSBS_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal: sub-param readers/writers called from msbs.c dispatcher. */
sf_result_t msbs_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);

sf_result_t msbs_model_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_event_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_point_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_parts_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_route_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBS_INTERNAL_H */
