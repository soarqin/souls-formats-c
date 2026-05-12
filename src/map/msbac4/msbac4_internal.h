/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBAC4_INTERNAL_H
#define SF_MAP_MSBAC4_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbac4.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbac4_event_type { MSBAC4_EVENT_SCRIPT = 5 } msbac4_event_type_t;
typedef enum msbac4_region_type { MSBAC4_REGION_SPAWN = 200 } msbac4_region_type_t;
typedef enum msbac4_region_shape_type { MSBAC4_REGION_SHAPE_POINT = 0 } msbac4_region_shape_type_t;
typedef enum msbac4_part_type { MSBAC4_PART_MAP_PIECE = 0 } msbac4_part_type_t;

typedef struct msbac4_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *resource_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msbac4_model_t;

typedef struct msbac4_event {
    msbac4_event_type_t  type;
    char                *name;
    int32_t              unique_id;
    const sf_allocator_t *alloc;
} msbac4_event_t;

typedef struct msbac4_region {
    msbac4_region_type_t       type;
    msbac4_region_shape_type_t shape_type;
    char                      *name;
    sf_vec3_t                  position;
    sf_vec3_t                  rotation;
    int32_t                    unique_id;
    int32_t                    point_id;
    const sf_allocator_t      *alloc;
} msbac4_region_t;

typedef struct msbac4_part {
    msbac4_part_type_t   type;
    char                *name;
    int32_t              model_index;
    char                *resource_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    int32_t              entity_group_id;
    int32_t              entity_id;
    const sf_allocator_t *alloc;
} msbac4_part_t;

struct sf_msbac4_model  { msbac4_model_t data; };
struct sf_msbac4_event  { msbac4_event_t data; };
struct sf_msbac4_region { msbac4_region_t data; };
struct sf_msbac4_part   { msbac4_part_t data; };

struct sf_msbac4 {
    const sf_allocator_t *alloc;
    sf_msbac4_model_t  *models;
    int32_t             model_count;
    sf_msbac4_event_t  *events;
    int32_t             event_count;
    sf_msbac4_region_t *regions;
    int32_t             region_count;
    sf_msbac4_part_t   *parts;
    int32_t             part_count;
    int32_t             route_count;
    int32_t             layer_count;
    int32_t             drawing_tree_count;
    int32_t             collision_tree_count;
};

void msbac4_model_param_free(sf_msbac4_model_t *models, int32_t count, const sf_allocator_t *a);
void msbac4_event_param_free(sf_msbac4_event_t *events, int32_t count, const sf_allocator_t *a);
void msbac4_point_param_free(sf_msbac4_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbac4_parts_param_free(sf_msbac4_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbac4_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbac4_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbac4_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbac4_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbac4_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbac4_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbac4_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbac4_t *out,
                                    const sf_allocator_t *a);

sf_result_t msbac4_model_param_write(sf_binary_writer_t *w, const sf_msbac4_t *msb);
sf_result_t msbac4_event_param_write(sf_binary_writer_t *w, const sf_msbac4_t *msb);
sf_result_t msbac4_point_param_write(sf_binary_writer_t *w, const sf_msbac4_t *msb);
sf_result_t msbac4_parts_param_write(sf_binary_writer_t *w, const sf_msbac4_t *msb);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBAC4_INTERNAL_H */
