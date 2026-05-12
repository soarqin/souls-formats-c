/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBVD_INTERNAL_H
#define SF_MAP_MSBVD_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbvd.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbvd_event_type { MSBVD_EVENT_SCRIPT = 5 } msbvd_event_type_t;
typedef enum msbvd_region_type { MSBVD_REGION_SPAWN = 200 } msbvd_region_type_t;
typedef enum msbvd_region_shape_type { MSBVD_REGION_SHAPE_POINT = 0 } msbvd_region_shape_type_t;
typedef enum msbvd_part_type { MSBVD_PART_MAP_PIECE = 0 } msbvd_part_type_t;

typedef struct msbvd_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *resource_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msbvd_model_t;

typedef struct msbvd_event {
    msbvd_event_type_t  type;
    char                *name;
    int32_t              unique_id;
    const sf_allocator_t *alloc;
} msbvd_event_t;

typedef struct msbvd_region {
    msbvd_region_type_t       type;
    msbvd_region_shape_type_t shape_type;
    char                      *name;
    sf_vec3_t                  position;
    sf_vec3_t                  rotation;
    int32_t                    unique_id;
    int32_t                    point_id;
    const sf_allocator_t      *alloc;
} msbvd_region_t;

typedef struct msbvd_part {
    msbvd_part_type_t   type;
    char                *name;
    int32_t              model_index;
    char                *resource_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    int32_t              entity_group_id;
    int32_t              entity_id;
    const sf_allocator_t *alloc;
} msbvd_part_t;

struct sf_msbvd_model  { msbvd_model_t data; };
struct sf_msbvd_event  { msbvd_event_t data; };
struct sf_msbvd_region { msbvd_region_t data; };
struct sf_msbvd_part   { msbvd_part_t data; };

struct sf_msbvd {
    const sf_allocator_t *alloc;
    sf_msbvd_model_t  *models;
    int32_t             model_count;
    sf_msbvd_event_t  *events;
    int32_t             event_count;
    sf_msbvd_region_t *regions;
    int32_t             region_count;
    sf_msbvd_part_t   *parts;
    int32_t             part_count;
    int32_t             route_count;
    int32_t             layer_count;
    int32_t             drawing_tree_count;
    int32_t             collision_tree_count;
};

void msbvd_model_param_free(sf_msbvd_model_t *models, int32_t count, const sf_allocator_t *a);
void msbvd_event_param_free(sf_msbvd_event_t *events, int32_t count, const sf_allocator_t *a);
void msbvd_point_param_free(sf_msbvd_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbvd_parts_param_free(sf_msbvd_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbvd_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvd_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbvd_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvd_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbvd_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvd_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbvd_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvd_t *out,
                                    const sf_allocator_t *a);

sf_result_t msbvd_model_param_write(sf_binary_writer_t *w, const sf_msbvd_t *msb);
sf_result_t msbvd_event_param_write(sf_binary_writer_t *w, const sf_msbvd_t *msb);
sf_result_t msbvd_point_param_write(sf_binary_writer_t *w, const sf_msbvd_t *msb);
sf_result_t msbvd_parts_param_write(sf_binary_writer_t *w, const sf_msbvd_t *msb);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBVD_INTERNAL_H */
