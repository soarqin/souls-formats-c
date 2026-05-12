/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBB_INTERNAL_H
#define SF_MAP_MSBB_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbb.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbb_event_type {
    MSBB_EVENT_LIGHT        = 0,
    MSBB_EVENT_SOUND        = 1,
    MSBB_EVENT_SFX          = 2,
    MSBB_EVENT_WIND         = 3,
    MSBB_EVENT_TREASURE     = 4,
    MSBB_EVENT_GENERATOR    = 5,
    MSBB_EVENT_MESSAGE      = 6,
    MSBB_EVENT_OBJ_ACT      = 7,
    MSBB_EVENT_SPAWN_POINT  = 8,
    MSBB_EVENT_MAP_OFFSET   = 9,
    MSBB_EVENT_NAVMESH      = 10,
    MSBB_EVENT_ENVIRONMENT  = 11,
    MSBB_EVENT_PSEUDO_MULTIPLAYER = 12,
    MSBB_EVENT_WIND_SFX     = 13,
    MSBB_EVENT_PATROL_INFO  = 14,
    MSBB_EVENT_DARK_LOCK    = 15,
    MSBB_EVENT_PLATOON_INFO = 16,
    MSBB_EVENT_MULTI_SUMMON = 17,
    MSBB_EVENT_OTHER        = (int32_t)0xFFFFFFFFu,
} msbb_event_type_t;

typedef enum msbb_region_type { MSBB_REGION_LOGIC = 0 } msbb_region_type_t;
typedef enum msbb_region_shape_type {
    MSBB_REGION_SHAPE_POINT = 0, MSBB_REGION_SHAPE_CIRCLE = 1,
    MSBB_REGION_SHAPE_SPHERE = 2, MSBB_REGION_SHAPE_CYLINDER = 3,
    MSBB_REGION_SHAPE_RECTANGLE = 4, MSBB_REGION_SHAPE_BOX = 5,
} msbb_region_shape_type_t;

typedef enum msbb_part_type {
    MSBB_PART_MAP_PIECE = 0, MSBB_PART_OBJECT = 1, MSBB_PART_ENEMY = 2,
    MSBB_PART_PLAYER = 4, MSBB_PART_COLLISION = 5, MSBB_PART_NAVMESH = 8,
    MSBB_PART_DUMMY_OBJECT = 9, MSBB_PART_DUMMY_ENEMY = 10,
    MSBB_PART_CONNECT_COLLISION = 11, MSBB_PART_OTHER = (int32_t)0xFFFFFFFFu,
} msbb_part_type_t;

typedef struct msbb_model {
    sf_msb_model_kind_t kind; char *name; char *sib_path; int32_t instance_count;
    const sf_allocator_t *alloc;
} msbb_model_t;
typedef struct msbb_event {
    msbb_event_type_t type; char *name; int32_t event_id; int32_t part_index;
    int32_t region_index; int32_t entity_id; int32_t type_value0; const sf_allocator_t *alloc;
} msbb_event_t;
typedef struct msbb_region {
    msbb_region_type_t type; msbb_region_shape_type_t shape_type; char *name;
    sf_vec3_t position; sf_vec3_t rotation; int32_t entity_id;
    union { struct { float radius; } circle; struct { float radius; } sphere;
        struct { float radius, height; } cylinder; struct { float width, depth; } rectangle;
        struct { float width, depth, height; } box; } shape;
    const sf_allocator_t *alloc;
} msbb_region_t;
typedef struct msbb_part {
    msbb_part_type_t type; char *name; int32_t model_index; char *sib_path;
    sf_vec3_t position, rotation, scale; uint32_t draw_groups[4], disp_groups[4];
    int32_t entity_id; uint8_t light_id, fog_id, scatter_id, lens_flare_id;
    uint8_t shadow_id, dof_id, tone_map_id, tone_correct_id, lantern_id, lod_param_id;
    uint8_t is_shadow_src, is_shadow_dest, is_shadow_only, draw_by_reflect_cam;
    uint8_t draw_only_reflect_cam, use_depth_bias_float, disable_point_light_effect;
    const sf_allocator_t *alloc;
} msbb_part_t;

struct sf_msbb_model { msbb_model_t data; };
struct sf_msbb_event { msbb_event_t data; };
struct sf_msbb_region { msbb_region_t data; };
struct sf_msbb_part { msbb_part_t data; };
struct sf_msbb {
    const sf_allocator_t *alloc; sf_msbb_model_t *models; int32_t model_count;
    sf_msbb_event_t *events; int32_t event_count; sf_msbb_region_t *regions;
    int32_t region_count; sf_msbb_part_t *parts; int32_t part_count;
};

void msbb_model_param_free(sf_msbb_model_t *models, int32_t count, const sf_allocator_t *a);
void msbb_event_param_free(sf_msbb_event_t *events, int32_t count, const sf_allocator_t *a);
void msbb_point_param_free(sf_msbb_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbb_parts_param_free(sf_msbb_part_t *parts, int32_t count, const sf_allocator_t *a);
sf_result_t msbb_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbb_t *out, const sf_allocator_t *a);
sf_result_t msbb_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbb_t *out, const sf_allocator_t *a);
sf_result_t msbb_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbb_t *out, const sf_allocator_t *a);
sf_result_t msbb_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbb_t *out, const sf_allocator_t *a);
sf_result_t msbb_model_param_write(sf_binary_writer_t *w, const sf_msbb_t *msbb);
sf_result_t msbb_event_param_write(sf_binary_writer_t *w, const sf_msbb_t *msbb);
sf_result_t msbb_point_param_write(sf_binary_writer_t *w, const sf_msbb_t *msbb);
sf_result_t msbb_parts_param_write(sf_binary_writer_t *w, const sf_msbb_t *msbb);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBB_INTERNAL_H */
