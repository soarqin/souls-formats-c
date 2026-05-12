/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBD_INTERNAL_H
#define SF_MAP_MSBD_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbd.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbd_event_type {
    MSBD_EVENT_LIGHT     = 0,
    MSBD_EVENT_SOUND     = 1,
    MSBD_EVENT_SFX       = 2,
    MSBD_EVENT_WIND      = 3,
    MSBD_EVENT_TREASURE  = 4,
    MSBD_EVENT_GENERATOR = 5,
    MSBD_EVENT_MESSAGE   = 6,
    MSBD_EVENT_OBJ_ACT   = 7,
    MSBD_EVENT_SPAWN_POINT = 8,
    MSBD_EVENT_MAP_OFFSET = 9,
    MSBD_EVENT_NAVMESH = 10,
    MSBD_EVENT_ENVIRONMENT = 11,
    MSBD_EVENT_PSEUDO_MULTIPLAYER = 12,
} msbd_event_type_t;

typedef enum msbd_region_type { MSBD_REGION_LOGIC = 0 } msbd_region_type_t;

typedef enum msbd_region_shape_type {
    MSBD_REGION_SHAPE_POINT     = 0,
    MSBD_REGION_SHAPE_CIRCLE    = 1,
    MSBD_REGION_SHAPE_SPHERE    = 2,
    MSBD_REGION_SHAPE_CYLINDER  = 3,
    MSBD_REGION_SHAPE_RECTANGLE = 4,
    MSBD_REGION_SHAPE_BOX       = 5,
} msbd_region_shape_type_t;

typedef enum msbd_part_type {
    MSBD_PART_MAP_PIECE         = 0,
    MSBD_PART_OBJECT            = 1,
    MSBD_PART_ENEMY             = 2,
    MSBD_PART_PLAYER            = 4,
    MSBD_PART_COLLISION         = 5,
    MSBD_PART_PROTOBOSS         = 7,
    MSBD_PART_NAVMESH           = 8,
    MSBD_PART_DUMMY_OBJECT      = 9,
    MSBD_PART_DUMMY_ENEMY       = 10,
    MSBD_PART_CONNECT_COLLISION = 11,
} msbd_part_type_t;

typedef struct msbd_model {
    sf_msb_model_kind_t kind;
    char               *name;
    char               *sib_path;
    int32_t             instance_count;
    const sf_allocator_t *alloc;
} msbd_model_t;

typedef struct msbd_event {
    msbd_event_type_t type;
    char             *name;
    int32_t           event_id;
    int32_t           part_index;
    int32_t           region_index;
    int32_t           entity_id;
    int32_t           type_value0;
    const sf_allocator_t *alloc;
} msbd_event_t;

typedef struct msbd_region {
    msbd_region_type_t       type;
    msbd_region_shape_type_t shape_type;
    char                    *name;
    sf_vec3_t                position;
    sf_vec3_t                rotation;
    int32_t                  entity_id;
    union {
        struct { float radius; } circle;
        struct { float radius; } sphere;
        struct { float radius, height; } cylinder;
        struct { float width, depth; } rectangle;
        struct { float width, depth, height; } box;
    } shape;
    const sf_allocator_t *alloc;
} msbd_region_t;

typedef struct msbd_part {
    msbd_part_type_t type;
    char            *name;
    int32_t          model_index;
    char            *sib_path;
    sf_vec3_t        position;
    sf_vec3_t        rotation;
    sf_vec3_t        scale;
    uint32_t         draw_groups[4];
    uint32_t         disp_groups[4];
    int32_t          entity_id;
    uint8_t          light_id, fog_id, scatter_id, lens_flare_id;
    uint8_t          shadow_id, dof_id, tone_map_id, tone_correct_id;
    uint8_t          lantern_id, lod_param_id, is_shadow_src, is_shadow_dest;
    uint8_t          is_shadow_only, draw_by_reflect_cam, draw_only_reflect_cam;
    uint8_t          use_depth_bias_float, disable_point_light_effect;
    const sf_allocator_t *alloc;
} msbd_part_t;

struct sf_msbd_model  { msbd_model_t data; };
struct sf_msbd_event  { msbd_event_t data; };
struct sf_msbd_region { msbd_region_t data; };
struct sf_msbd_part   { msbd_part_t data; };

struct sf_msbd {
    const sf_allocator_t *alloc;
    sf_msbd_model_t  *models;
    int32_t           model_count;
    sf_msbd_event_t  *events;
    int32_t           event_count;
    sf_msbd_region_t *regions;
    int32_t           region_count;
    sf_msbd_part_t   *parts;
    int32_t           part_count;
    int32_t           trees_count;
};

void msbd_model_param_free(sf_msbd_model_t *models, int32_t count, const sf_allocator_t *a);
void msbd_event_param_free(sf_msbd_event_t *events, int32_t count, const sf_allocator_t *a);
void msbd_point_param_free(sf_msbd_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbd_parts_param_free(sf_msbd_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msbd_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbd_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbd_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbd_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a);

sf_result_t msbd_model_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd);
sf_result_t msbd_event_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd);
sf_result_t msbd_point_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd);
sf_result_t msbd_parts_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBD_INTERNAL_H */
