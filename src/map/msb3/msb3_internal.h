/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSB3_INTERNAL_H
#define SF_MAP_MSB3_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb3.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msb3_event_type { MSB3_EVENT_TREASURE = 4 } msb3_event_type_t;
typedef enum msb3_region_type { MSB3_REGION_SPAWN_POINT = 8 } msb3_region_type_t;
typedef enum msb3_region_shape_type { MSB3_REGION_SHAPE_POINT = 0 } msb3_region_shape_type_t;
typedef enum msb3_part_type { MSB3_PART_MAP_PIECE = 0 } msb3_part_type_t;

typedef struct msb3_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *sib_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msb3_model_t;

typedef struct msb3_event {
    msb3_event_type_t    type;
    char                *name;
    int32_t              event_id;
    int32_t              part_index;
    int32_t              region_index;
    int32_t              entity_id;
    int32_t              treasure_part_index;
    int32_t              item_lot1;
    const sf_allocator_t *alloc;
} msb3_event_t;

typedef struct msb3_region {
    msb3_region_type_t       type;
    msb3_region_shape_type_t shape_type;
    char                    *name;
    sf_vec3_t                position;
    sf_vec3_t                rotation;
    int32_t                  entity_id;
    int32_t                  activation_part_index;
    int32_t                  type_value0;
    const sf_allocator_t    *alloc;
} msb3_region_t;

typedef struct msb3_part {
    msb3_part_type_t     type;
    char                *name;
    int32_t              model_index;
    char                *sib_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    uint32_t             draw_groups[8];
    uint32_t             disp_groups[8];
    uint32_t             backread_groups[8];
    int32_t              entity_id;
    int32_t              entity_groups[8];
    const sf_allocator_t *alloc;
} msb3_part_t;

struct sf_msb3_model  { msb3_model_t data; };
struct sf_msb3_event  { msb3_event_t data; };
struct sf_msb3_region { msb3_region_t data; };
struct sf_msb3_part   { msb3_part_t data; };

struct sf_msb3 {
    const sf_allocator_t *alloc;
    sf_msb3_model_t  *models;
    int32_t           model_count;
    sf_msb3_event_t  *events;
    int32_t           event_count;
    sf_msb3_region_t *regions;
    int32_t           region_count;
    sf_msb3_part_t   *parts;
    int32_t           part_count;
    int32_t           route_count;
    int32_t           layer_count;
    int32_t           parts_pose_count;
    int32_t           bone_name_count;
};

void msb3_model_param_free(sf_msb3_model_t *models, int32_t count, const sf_allocator_t *a);
void msb3_event_param_free(sf_msb3_event_t *events, int32_t count, const sf_allocator_t *a);
void msb3_point_param_free(sf_msb3_region_t *regions, int32_t count, const sf_allocator_t *a);
void msb3_parts_param_free(sf_msb3_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msb3_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msb3_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb3_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msb3_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb3_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msb3_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb3_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msb3_t *out,
                                  const sf_allocator_t *a);

sf_result_t msb3_model_param_write(sf_binary_writer_t *w, const sf_msb3_t *msb3);
sf_result_t msb3_event_param_write(sf_binary_writer_t *w, const sf_msb3_t *msb3);
sf_result_t msb3_point_param_write(sf_binary_writer_t *w, const sf_msb3_t *msb3);
sf_result_t msb3_parts_param_write(sf_binary_writer_t *w, const sf_msb3_t *msb3);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSB3_INTERNAL_H */
