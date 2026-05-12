/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Dark Souls 1 MSB internal sub-param hooks.
 */

#ifndef SF_MAP_MSB1_INTERNAL_H
#define SF_MAP_MSB1_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb1.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msb1_event_type {
    MSB1_EVENT_LIGHT              = 0,
    MSB1_EVENT_SOUND              = 1,
    MSB1_EVENT_SFX                = 2,
    MSB1_EVENT_WIND               = 3,
    MSB1_EVENT_TREASURE           = 4,
    MSB1_EVENT_GENERATOR          = 5,
    MSB1_EVENT_MESSAGE            = 6,
    MSB1_EVENT_OBJ_ACT            = 7,
    MSB1_EVENT_SPAWN_POINT        = 8,
    MSB1_EVENT_MAP_OFFSET         = 9,
    MSB1_EVENT_NAVMESH            = 10,
    MSB1_EVENT_ENVIRONMENT        = 11,
    MSB1_EVENT_PSEUDO_MULTIPLAYER = 12,
} msb1_event_type_t;

typedef enum msb1_region_type {
    MSB1_REGION_OTHER                      = -1,
    MSB1_REGION_LOGIC                      = 0,
    MSB1_REGION_INVASION_POINT             = 1,
    MSB1_REGION_ENVIRONMENT_MAP_POINT      = 2,
    MSB1_REGION_SOUND                      = 4,
    MSB1_REGION_SFX                        = 5,
    MSB1_REGION_WIND_SFX                   = 6,
    MSB1_REGION_SPAWN_POINT                = 8,
    MSB1_REGION_PATROL_ROUTE               = 11,
    MSB1_REGION_WARP_POINT                 = 13,
    MSB1_REGION_ACTIVATION_AREA            = 14,
    MSB1_REGION_EVENT                      = 15,
    MSB1_REGION_ENVIRONMENT_MAP_EFFECT_BOX = 17,
    MSB1_REGION_WIND_AREA                  = 18,
    MSB1_REGION_MUFFLING_BOX               = 20,
    MSB1_REGION_MUFFLING_PORTAL            = 21,
} msb1_region_type_t;

typedef enum msb1_region_shape_type {
    MSB1_REGION_SHAPE_POINT     = 0,
    MSB1_REGION_SHAPE_CIRCLE    = 1,
    MSB1_REGION_SHAPE_SPHERE    = 2,
    MSB1_REGION_SHAPE_CYLINDER  = 3,
    MSB1_REGION_SHAPE_RECTANGLE = 4,
    MSB1_REGION_SHAPE_BOX       = 5,
} msb1_region_shape_type_t;

typedef enum msb1_part_type {
    MSB1_PART_MAP_PIECE         = 0,
    MSB1_PART_OBJECT            = 1,
    MSB1_PART_ENEMY             = 2,
    MSB1_PART_PLAYER            = 4,
    MSB1_PART_COLLISION         = 5,
    MSB1_PART_NAVMESH           = 8,
    MSB1_PART_DUMMY_OBJECT      = 9,
    MSB1_PART_DUMMY_ENEMY       = 10,
    MSB1_PART_CONNECT_COLLISION = 11,
} msb1_part_type_t;

typedef struct msb1_model {
    sf_msb_model_kind_t kind;
    char               *name;
    char               *sib_path;
    int32_t             instance_count;
    const sf_allocator_t *alloc;
} msb1_model_t;

typedef struct msb1_event {
    msb1_event_type_t type;
    char             *name;
    int32_t           event_id;
    int32_t           part_index;
    int32_t           region_index;
    int32_t           entity_id;
    int32_t           type_value0;
    const sf_allocator_t *alloc;
} msb1_event_t;

typedef struct msb1_region {
    msb1_region_type_t       type;
    msb1_region_shape_type_t shape_type;
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
} msb1_region_t;

typedef struct msb1_part {
    msb1_part_type_t type;
    char            *name;
    int32_t          model_index;
    char            *sib_path;
    sf_vec3_t        position;
    sf_vec3_t        rotation;
    sf_vec3_t        scale;
    uint32_t         draw_groups[4];
    uint32_t         disp_groups[4];
    int32_t          entity_id;
    uint8_t          light_id;
    uint8_t          fog_id;
    uint8_t          scatter_id;
    uint8_t          lens_flare_id;
    uint8_t          shadow_id;
    uint8_t          dof_id;
    uint8_t          tone_map_id;
    uint8_t          tone_correct_id;
    uint8_t          lantern_id;
    uint8_t          lod_param_id;
    uint8_t          is_shadow_src;
    uint8_t          is_shadow_dest;
    uint8_t          is_shadow_only;
    uint8_t          draw_by_reflect_cam;
    uint8_t          draw_only_reflect_cam;
    uint8_t          use_depth_bias_float;
    uint8_t          disable_point_light_effect;
    const sf_allocator_t *alloc;
} msb1_part_t;

struct sf_msb1_model  { msb1_model_t data; };
struct sf_msb1_event  { msb1_event_t data; };
struct sf_msb1_region { msb1_region_t data; };
struct sf_msb1_part   { msb1_part_t data; };

struct sf_msb1 {
    const sf_allocator_t *alloc;
    sf_msb1_model_t  *models;
    int32_t           model_count;
    sf_msb1_event_t  *events;
    int32_t           event_count;
    sf_msb1_region_t *regions;
    int32_t           region_count;
    sf_msb1_part_t   *parts;
    int32_t           part_count;
};

void msb1_model_param_free(sf_msb1_model_t *models, int32_t count, const sf_allocator_t *a);
void msb1_event_param_free(sf_msb1_event_t *events, int32_t count, const sf_allocator_t *a);
void msb1_point_param_free(sf_msb1_region_t *regions, int32_t count, const sf_allocator_t *a);
void msb1_parts_param_free(sf_msb1_part_t *parts, int32_t count, const sf_allocator_t *a);

sf_result_t msb1_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msb1_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb1_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msb1_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb1_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msb1_t *out,
                                  const sf_allocator_t *a);
sf_result_t msb1_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msb1_t *out,
                                  const sf_allocator_t *a);

sf_result_t msb1_model_param_write(sf_binary_writer_t *w, const sf_msb1_t *msb1);
sf_result_t msb1_event_param_write(sf_binary_writer_t *w, const sf_msb1_t *msb1);
sf_result_t msb1_point_param_write(sf_binary_writer_t *w, const sf_msb1_t *msb1);
sf_result_t msb1_parts_param_write(sf_binary_writer_t *w, const sf_msb1_t *msb1);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSB1_INTERNAL_H */
