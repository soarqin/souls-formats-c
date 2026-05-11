/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS internal sub-param hooks.
 */

#ifndef SF_MAP_MSBS_INTERNAL_H
#define SF_MAP_MSBS_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbs.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msbs_model {
    sf_msb_model_kind_t kind;
    char               *name;
    char               *sib_path;
    int32_t             instance_count;
    int32_t             unk1c;
    union {
        struct {
            bool  unk_t00;
            bool  unk_t01;
            bool  unk_t02;
            float unk_t04;
            float unk_t08;
            float unk_t0c;
            float unk_t10;
            float unk_t14;
            float unk_t18;
        } map_piece;
    } u;
    const sf_allocator_t *alloc;
} msbs_model_t;

struct sf_msbs_model { msbs_model_t data; };
struct sf_msbs_event  { uint8_t reserved; };
struct sf_msbs_region { uint8_t reserved; };
struct sf_msbs_part   { uint8_t reserved; };
struct sf_msbs_route  { uint8_t reserved; };

struct sf_msbs {
    const sf_allocator_t *alloc;

    sf_msbs_model_t  *models;
    int32_t           model_count;
    sf_msbs_event_t  *events;
    int32_t           event_count;
    sf_msbs_region_t *regions;
    int32_t           region_count;
    sf_msbs_route_t  *routes;
    int32_t           route_count;
    sf_msbs_part_t   *parts;
    int32_t           part_count;
};

void msbs_model_param_free(sf_msbs_model_t *models, int32_t count, const sf_allocator_t *a);

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
