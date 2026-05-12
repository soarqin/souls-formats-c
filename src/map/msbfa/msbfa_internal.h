/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBFA_INTERNAL_H
#define SF_MAP_MSBFA_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbfa.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbfa_event_type { MSBFA_EVENT_SCRIPT = 5 } msbfa_event_type_t;
typedef enum msbfa_region_type { MSBFA_REGION_SPAWN = 200 } msbfa_region_type_t;
typedef enum msbfa_region_shape_type { MSBFA_REGION_SHAPE_POINT = 0 } msbfa_region_shape_type_t;
typedef enum msbfa_part_type { MSBFA_PART_MAP_PIECE = 0 } msbfa_part_type_t;

typedef struct msbfa_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *resource_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msbfa_model_t;

typedef struct msbfa_event {
    msbfa_event_type_t  type;
    char                *name;
    int32_t              unique_id;
    const sf_allocator_t *alloc;
} msbfa_event_t;

typedef struct msbfa_region {
    msbfa_region_type_t       type;
    msbfa_region_shape_type_t shape_type;
    char                      *name;
    sf_vec3_t                  position;
    sf_vec3_t                  rotation;
    int32_t                    unique_id;
    int32_t                    point_id;
    const sf_allocator_t      *alloc;
} msbfa_region_t;

typedef struct msbfa_part {
    msbfa_part_type_t   type;
    char                *name;
    int32_t              model_index;
    char                *resource_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    int32_t              entity_group_id;
    int32_t              entity_id;
    const sf_allocator_t *alloc;
} msbfa_part_t;

struct sf_msbfa_model  { msbfa_model_t data; };
struct sf_msbfa_event  { msbfa_event_t data; };
struct sf_msbfa_region { msbfa_region_t data; };
struct sf_msbfa_part   { msbfa_part_t data; };

struct sf_msbfa {
    const sf_allocator_t *alloc;
    sf_msbfa_model_t  *models;
    int32_t             model_count;
    sf_msbfa_event_t  *events;
    int32_t             event_count;
    sf_msbfa_region_t *regions;
    int32_t             region_count;
    sf_msbfa_part_t   *parts;
    int32_t             part_count;
    int32_t             route_count;
    int32_t             layer_count;
    int32_t             drawing_tree_count;
    int32_t             collision_tree_count;
};

void msbfa_model_param_free(sf_msbfa_model_t *models, int32_t count, const sf_allocator_t *a);
void msbfa_event_param_free(sf_msbfa_event_t *events, int32_t count, const sf_allocator_t *a);
void msbfa_point_param_free(sf_msbfa_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbfa_parts_param_free(sf_msbfa_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbfa_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbfa_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbfa_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbfa_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbfa_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbfa_t *out,
                                    const sf_allocator_t *a);
sf_result_t msbfa_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbfa_t *out,
                                    const sf_allocator_t *a);

sf_result_t msbfa_model_param_write(sf_binary_writer_t *w, const sf_msbfa_t *msb);
sf_result_t msbfa_event_param_write(sf_binary_writer_t *w, const sf_msbfa_t *msb);
sf_result_t msbfa_point_param_write(sf_binary_writer_t *w, const sf_msbfa_t *msb);
sf_result_t msbfa_parts_param_write(sf_binary_writer_t *w, const sf_msbfa_t *msb);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBFA_INTERNAL_H */
