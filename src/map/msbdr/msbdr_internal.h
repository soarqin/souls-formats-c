/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_MAP_MSBDR_INTERNAL_H
#define SF_MAP_MSBDR_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbdr.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum msbdr_event_type {
    MSBDR_EVENT_LIGHT = 0, MSBDR_EVENT_SOUND = 1, MSBDR_EVENT_SFX = 2,
    MSBDR_EVENT_WIND = 3, MSBDR_EVENT_TREASURE = 4, MSBDR_EVENT_GENERATOR = 5,
    MSBDR_EVENT_MESSAGE = 6, MSBDR_EVENT_OBJ_ACT = 7, MSBDR_EVENT_SPAWN_POINT = 8,
    MSBDR_EVENT_MAP_OFFSET = 9, MSBDR_EVENT_NAVMESH = 10, MSBDR_EVENT_ENVIRONMENT = 11,
    MSBDR_EVENT_PSEUDO_MULTIPLAYER = 12,
} msbdr_event_type_t;
typedef enum msbdr_region_type { MSBDR_REGION_LOGIC = 0 } msbdr_region_type_t;
typedef enum msbdr_region_shape_type {
    MSBDR_REGION_SHAPE_POINT = 0, MSBDR_REGION_SHAPE_CIRCLE = 1,
    MSBDR_REGION_SHAPE_SPHERE = 2, MSBDR_REGION_SHAPE_CYLINDER = 3,
    MSBDR_REGION_SHAPE_RECTANGLE = 4, MSBDR_REGION_SHAPE_BOX = 5,
} msbdr_region_shape_type_t;
typedef enum msbdr_part_type {
    MSBDR_PART_MAP_PIECE = 0, MSBDR_PART_OBJECT = 1, MSBDR_PART_ENEMY = 2,
    MSBDR_PART_PLAYER = 4, MSBDR_PART_COLLISION = 5, MSBDR_PART_PROTOBOSS = 7,
    MSBDR_PART_NAVMESH = 8, MSBDR_PART_DUMMY_OBJECT = 9, MSBDR_PART_DUMMY_ENEMY = 10,
    MSBDR_PART_CONNECT_COLLISION = 11,
} msbdr_part_type_t;

typedef struct msbdr_model { sf_msb_model_kind_t kind; char *name; char *sib_path; int32_t instance_count; const sf_allocator_t *alloc; } msbdr_model_t;
typedef struct msbdr_event { msbdr_event_type_t type; char *name; int32_t event_id; int32_t part_index; int32_t region_index; int32_t entity_id; int32_t type_value0; const sf_allocator_t *alloc; } msbdr_event_t;
typedef struct msbdr_region {
    msbdr_region_type_t type; msbdr_region_shape_type_t shape_type; char *name; sf_vec3_t position; sf_vec3_t rotation; int32_t entity_id;
    union { struct { float radius; } circle; struct { float radius; } sphere; struct { float radius, height; } cylinder; struct { float width, depth; } rectangle; struct { float width, depth, height; } box; } shape;
    const sf_allocator_t *alloc;
} msbdr_region_t;
typedef struct msbdr_part {
    msbdr_part_type_t type; char *name; int32_t model_index; char *sib_path; sf_vec3_t position, rotation, scale; uint32_t draw_groups[4], disp_groups[4]; int32_t entity_id;
    uint8_t light_id, fog_id, scatter_id, lens_flare_id, shadow_id, dof_id, tone_map_id, tone_correct_id, lantern_id, lod_param_id, is_shadow_src, is_shadow_dest, is_shadow_only, draw_by_reflect_cam, draw_only_reflect_cam, use_depth_bias_float, disable_point_light_effect;
    const sf_allocator_t *alloc;
} msbdr_part_t;

struct sf_msbdr_model { msbdr_model_t data; };
struct sf_msbdr_event { msbdr_event_t data; };
struct sf_msbdr_region { msbdr_region_t data; };
struct sf_msbdr_part { msbdr_part_t data; };
struct sf_msbdr {
    const sf_allocator_t *alloc; sf_msbdr_model_t *models; int32_t model_count; sf_msbdr_event_t *events; int32_t event_count; sf_msbdr_region_t *regions; int32_t region_count; sf_msbdr_part_t *parts; int32_t part_count; int32_t trees_count;
};

void msbdr_model_param_free(sf_msbdr_model_t *models, int32_t count, const sf_allocator_t *a);
void msbdr_event_param_free(sf_msbdr_event_t *events, int32_t count, const sf_allocator_t *a);
void msbdr_point_param_free(sf_msbdr_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbdr_parts_param_free(sf_msbdr_part_t *parts, int32_t count, const sf_allocator_t *a);
sf_result_t msbdr_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbdr_t *out, const sf_allocator_t *a);
sf_result_t msbdr_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbdr_t *out, const sf_allocator_t *a);
sf_result_t msbdr_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbdr_t *out, const sf_allocator_t *a);
sf_result_t msbdr_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbdr_t *out, const sf_allocator_t *a);
sf_result_t msbdr_model_param_write(sf_binary_writer_t *w, const sf_msbdr_t *msbdr);
sf_result_t msbdr_event_param_write(sf_binary_writer_t *w, const sf_msbdr_t *msbdr);
sf_result_t msbdr_point_param_write(sf_binary_writer_t *w, const sf_msbdr_t *msbdr);
sf_result_t msbdr_parts_param_write(sf_binary_writer_t *w, const sf_msbdr_t *msbdr);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBDR_INTERNAL_H */
