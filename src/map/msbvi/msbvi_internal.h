/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Armored Core VI MSBVI internal sub-param hooks.
 */

#ifndef SF_MAP_MSBVI_INTERNAL_H
#define SF_MAP_MSBVI_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbvi.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msbvi_model {
    uint32_t              type;
    char                 *name;
    char                 *sib_path;
    int32_t               instance_count;
    const sf_allocator_t *alloc;
} msbvi_model_t;

typedef struct msbvi_event {
    uint32_t              type;
    char                 *name;
    int32_t               event_id;
    int32_t               type_index;
    int32_t               entity_id;
    const sf_allocator_t *alloc;
} msbvi_event_t;

typedef struct msbvi_region {
    uint32_t              type;
    uint32_t              shape_type;
    char                 *name;
    uint32_t              entity_id;
    const sf_allocator_t *alloc;
} msbvi_region_t;

typedef struct msbvi_part {
    uint32_t              type;
    char                 *name;
    char                 *layout_path;
    int32_t               model_index;
    const sf_allocator_t *alloc;
} msbvi_part_t;

typedef struct msbvi_route {
    char                 *name;
    int32_t               unk08;
    int32_t               unk0c;
    const sf_allocator_t *alloc;
} msbvi_route_t;

typedef struct msbvi_layer {
    char                 *name;
    int32_t               unk08;
    int32_t               unk10;
    int32_t               unk14;
    const sf_allocator_t *alloc;
} msbvi_layer_t;

struct sf_msbvi_model  { msbvi_model_t data; };
struct sf_msbvi_event  { msbvi_event_t data; };
struct sf_msbvi_region { msbvi_region_t data; };
struct sf_msbvi_part   { msbvi_part_t data; };
struct sf_msbvi_route  { msbvi_route_t data; };
struct sf_msbvi_layer  { msbvi_layer_t data; };

struct sf_msbvi {
    const sf_allocator_t *alloc;

    sf_msbvi_model_t  *models;
    int32_t            model_count;
    sf_msbvi_event_t  *events;
    int32_t            event_count;
    sf_msbvi_region_t *regions;
    int32_t            region_count;
    sf_msbvi_route_t  *routes;
    int32_t            route_count;
    sf_msbvi_layer_t  *layers;
    int32_t            layer_count;
    sf_msbvi_part_t   *parts;
    int32_t            part_count;
};

void msbvi_model_param_free(sf_msbvi_model_t *models, int32_t count, const sf_allocator_t *a);
void msbvi_event_param_free(sf_msbvi_event_t *events, int32_t count, const sf_allocator_t *a);
void msbvi_point_param_free(sf_msbvi_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbvi_parts_param_free(sf_msbvi_part_t *parts, int32_t count, const sf_allocator_t *a);
void msbvi_route_param_free(sf_msbvi_route_t *routes, int32_t count, const sf_allocator_t *a);
void msbvi_layer_param_free(sf_msbvi_layer_t *layers, int32_t count, const sf_allocator_t *a);

/* Internal sub-param readers/writers invoked by msbvi.c dispatcher.
 * MSBVI's Layer segment is a typed LayerParam (NOT EmptyParam) so the
 * layer hook is present alongside the other five typed params. */
sf_result_t msbvi_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);
sf_result_t msbvi_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);
sf_result_t msbvi_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);
sf_result_t msbvi_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);
sf_result_t msbvi_layer_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);
sf_result_t msbvi_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a);

sf_result_t msbvi_model_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);
sf_result_t msbvi_event_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);
sf_result_t msbvi_point_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);
sf_result_t msbvi_route_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);
sf_result_t msbvi_layer_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);
sf_result_t msbvi_parts_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBVI_INTERNAL_H */
