/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSB2_INTERNAL_H
#define SF_MAP_MSB2_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb2.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msb2_event_type { MSB2_EVENT_LIGHT = 1 } msb2_event_type_t;
typedef enum msb2_region_type { MSB2_REGION_START_POINT = 5 } msb2_region_type_t;
typedef enum msb2_region_shape_type { MSB2_REGION_SHAPE_POINT = 0 } msb2_region_shape_type_t;
typedef enum msb2_part_type { MSB2_PART_MAP_PIECE = 0 } msb2_part_type_t;

typedef struct msb2_model {
    sf_msb_model_kind_t  kind;
    char                *name;
    char                *sib_path;
    int32_t              instance_count;
    const sf_allocator_t *alloc;
} msb2_model_t;

typedef struct msb2_event {
    msb2_event_type_t    type;
    char                *name;
    int32_t              event_id;
    int32_t              part_index;
    int32_t              region_index;
    int32_t              entity_id;
    int32_t              type_value0;
    const sf_allocator_t *alloc;
} msb2_event_t;

typedef struct msb2_region {
    msb2_region_type_t       type;
    msb2_region_shape_type_t shape_type;
    char                    *name;
    sf_vec3_t                position;
    sf_vec3_t                rotation;
    int32_t                  entity_id;
    int32_t                  type_value0;
    const sf_allocator_t    *alloc;
} msb2_region_t;

typedef struct msb2_part {
    msb2_part_type_t     type;
    char                *name;
    int32_t              model_index;
    char                *sib_path;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    sf_vec3_t            scale;
    uint32_t             draw_groups[4];
    uint32_t             disp_groups[4];
    int32_t              entity_id;
    const sf_allocator_t *alloc;
} msb2_part_t;

struct sf_msb2_model  { msb2_model_t data; };
struct sf_msb2_event  { msb2_event_t data; };
struct sf_msb2_region { msb2_region_t data; };
struct sf_msb2_part   { msb2_part_t data; };

struct sf_msb2 {
    const sf_allocator_t *alloc;
    sf_msb2_model_t  *models;
    int32_t           model_count;
    sf_msb2_event_t  *events;
    int32_t           event_count;
    sf_msb2_region_t *regions;
    int32_t           region_count;
    sf_msb2_part_t   *parts;
    int32_t           part_count;
    int32_t           parts_pose_count;
};

void msb2_model_param_free(sf_msb2_model_t *models, int32_t count, const sf_allocator_t *a);
void msb2_event_param_free(sf_msb2_event_t *events, int32_t count, const sf_allocator_t *a);
void msb2_point_param_free(sf_msb2_region_t *regions, int32_t count, const sf_allocator_t *a);
void msb2_parts_param_free(sf_msb2_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msb2_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb2_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb2_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb2_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a);

sf_result_t msb2_model_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2);
sf_result_t msb2_event_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2);
sf_result_t msb2_point_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2);
sf_result_t msb2_parts_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSB2_INTERNAL_H */
