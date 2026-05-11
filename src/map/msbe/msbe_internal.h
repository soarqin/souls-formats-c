/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Elden Ring MSBE internal sub-param hooks.
 */

#ifndef SF_MAP_MSBE_INTERNAL_H
#define SF_MAP_MSBE_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbe.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msbe_model {
    uint32_t            type;
    char               *name;
    char               *sib_path;
    int32_t             instance_count;
    int32_t             unk1c;
    const sf_allocator_t *alloc;
} msbe_model_t;

typedef struct msbe_event {
    uint32_t            type;
    char               *name;
    int32_t             event_id;
    int32_t             other_id;
    int32_t             unk14;
    const sf_allocator_t *alloc;
} msbe_event_t;

typedef struct msbe_region {
    uint32_t            type;
    uint32_t            shape_type;
    char               *name;
    int32_t             region_id;
    const sf_allocator_t *alloc;
} msbe_region_t;

typedef struct msbe_part {
    uint32_t            type;
    char               *name;
    int32_t             model_index;
    int32_t             other_id;
    const sf_allocator_t *alloc;
} msbe_part_t;

typedef struct msbe_route {
    uint32_t            type;
    char               *name;
    int32_t             unk08;
    int32_t             unk0c;
    int32_t             other_id;
    const sf_allocator_t *alloc;
} msbe_route_t;

struct sf_msbe_model  { msbe_model_t data; };
struct sf_msbe_event  { msbe_event_t data; };
struct sf_msbe_region { msbe_region_t data; };
struct sf_msbe_part   { msbe_part_t data; };
struct sf_msbe_route  { msbe_route_t data; };

struct sf_msbe {
    const sf_allocator_t *alloc;

    sf_msbe_model_t  *models;
    int32_t           model_count;
    sf_msbe_event_t  *events;
    int32_t           event_count;
    sf_msbe_region_t *regions;
    int32_t           region_count;
    sf_msbe_route_t  *routes;
    int32_t           route_count;
    sf_msbe_part_t   *parts;
    int32_t           part_count;
};

void msbe_model_param_free(sf_msbe_model_t *models, int32_t count, const sf_allocator_t *a);
void msbe_event_param_free(sf_msbe_event_t *events, int32_t count, const sf_allocator_t *a);
void msbe_point_param_free(sf_msbe_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbe_parts_param_free(sf_msbe_part_t *parts, int32_t count, const sf_allocator_t *a);
void msbe_route_param_free(sf_msbe_route_t *routes, int32_t count, const sf_allocator_t *a);

/* Internal: sub-param readers/writers called from msbe.c dispatcher.
 * No layer hook — MSBE's Layer segment is an EmptyParam handled inline. */
sf_result_t msbe_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbe_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbe_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbe_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbe_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a);

sf_result_t msbe_model_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe);
sf_result_t msbe_event_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe);
sf_result_t msbe_point_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe);
sf_result_t msbe_parts_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe);
sf_result_t msbe_route_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBE_INTERNAL_H */
