/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBV_INTERNAL_H
#define SF_MAP_MSBV_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbv.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbv_event_type { MSBV_EVENT_SCRIPT = 5 } msbv_event_type_t;
typedef enum msbv_region_type { MSBV_REGION_SPAWN = 200 } msbv_region_type_t;
typedef enum msbv_region_shape_type { MSBV_REGION_SHAPE_POINT = 0 } msbv_region_shape_type_t;
typedef enum msbv_part_type { MSBV_PART_MAP_PIECE = 0 } msbv_part_type_t;

typedef struct msbv_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *resource_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msbv_model_t;

typedef struct msbv_event {
    msbv_event_type_t  type;
    char                *name;
    int32_t              unique_id;
    const sf_allocator_t *alloc;
} msbv_event_t;

typedef struct msbv_region {
    msbv_region_type_t       type;
    msbv_region_shape_type_t shape_type;
    char                      *name;
    sf_vec3_t                  position;
    sf_vec3_t                  rotation;
    int32_t                    unique_id;
    int32_t                    point_id;
    const sf_allocator_t      *alloc;
} msbv_region_t;

typedef struct msbv_part {
    msbv_part_type_t   type;
    char                *name;
    int32_t              model_index;
    char                *resource_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    int32_t              entity_group_id;
    int32_t              entity_id;
    const sf_allocator_t *alloc;
} msbv_part_t;

struct sf_msbv_model  { msbv_model_t data; };
struct sf_msbv_event  { msbv_event_t data; };
struct sf_msbv_region { msbv_region_t data; };
struct sf_msbv_part   { msbv_part_t data; };

struct sf_msbv {
    const sf_allocator_t *alloc;
    sf_msbv_model_t  *models;
    int32_t             model_count;
    sf_msbv_event_t  *events;
    int32_t             event_count;
    sf_msbv_region_t *regions;
    int32_t             region_count;
    sf_msbv_part_t   *parts;
    int32_t             part_count;
    int32_t             route_count;
    int32_t             layer_count;
    int32_t             drawing_tree_count;
    int32_t             collision_tree_count;
};

void msbv_model_param_free(sf_msbv_model_t *models, int32_t count, const sf_allocator_t *a);
void msbv_event_param_free(sf_msbv_event_t *events, int32_t count, const sf_allocator_t *a);
void msbv_point_param_free(sf_msbv_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbv_parts_param_free(sf_msbv_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbv_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbv_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbv_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbv_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbv_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbv_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbv_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbv_t *out,
                                    const sf_allocator_t *a);

sf_result_t msbv_model_param_write(sf_binary_writer_t *w, const sf_msbv_t *msb);
sf_result_t msbv_event_param_write(sf_binary_writer_t *w, const sf_msbv_t *msb);
sf_result_t msbv_point_param_write(sf_binary_writer_t *w, const sf_msbv_t *msb);
sf_result_t msbv_parts_param_write(sf_binary_writer_t *w, const sf_msbv_t *msb);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBV_INTERNAL_H */
