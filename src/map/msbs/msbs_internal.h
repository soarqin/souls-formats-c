/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS internal sub-param hooks.
 */

#ifndef SF_MAP_MSBS_INTERNAL_H
#define SF_MAP_MSBS_INTERNAL_H

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbs.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msbs_model {
    sf_msb_model_kind_t kind;
    char               *name;
    char               *sib_path;
    int32_t             instance_count;
    int32_t             unk1c;
    union {
        struct {
            bool  unk_t00;
            bool  unk_t01;
            bool  unk_t02;
            float unk_t04;
            float unk_t08;
            float unk_t0c;
            float unk_t10;
            float unk_t14;
            float unk_t18;
        } map_piece;
    } u;
    const sf_allocator_t *alloc;
} msbs_model_t;

typedef enum msbs_event_type {
    MSBS_EVENT_TREASURE                  = 4,
    MSBS_EVENT_GENERATOR                 = 5,
    MSBS_EVENT_OBJ_ACT                   = 7,
    MSBS_EVENT_MAP_OFFSET                = 9,
    MSBS_EVENT_PATROL_INFO               = 14,
    MSBS_EVENT_PLATOON_INFO              = 15,
    MSBS_EVENT_RESOURCE_ITEM_INFO        = 17,
    MSBS_EVENT_GRASS_LOD_PARAM           = 18,
    MSBS_EVENT_SKIT_INFO                 = 20,
    MSBS_EVENT_PLACEMENT_GROUP           = 21,
    MSBS_EVENT_PARTS_GROUP               = 22,
    MSBS_EVENT_TALK                      = 23,
    MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION = 24,
    MSBS_EVENT_OTHER                     = UINT32_MAX,
} msbs_event_type_t;

typedef struct msbs_event_wr_entry {
    int16_t region_index;
    int32_t unk04;
    int32_t unk08;
} msbs_event_wr_entry_t;

typedef struct msbs_event {
    msbs_event_type_t type;
    char             *name;
    int32_t           event_id;
    int32_t           part_index;
    int32_t           region_index;
    int32_t           entity_id;
    union {
        struct {
            int32_t treasure_part_index;
            int32_t item_lot_id;
            int32_t action_button_id;
            int32_t pickup_anim_id;
            bool    in_chest;
            bool    start_disabled;
        } treasure;
        struct {
            uint8_t max_num;
            int8_t  gen_type;
            int16_t limit_num;
            int16_t min_gen_num;
            int16_t max_gen_num;
            float   min_interval;
            float   max_interval;
            uint8_t initial_spawn_count;
            float   unk_t14;
            float   unk_t18;
            int32_t spawn_region_indices[8];
            int32_t spawn_part_indices[32];
        } generator;
        struct {
            int32_t obj_act_entity_id;
            int32_t obj_act_part_index;
            int32_t obj_act_id;
            uint8_t state_type;
            int32_t event_flag_id;
        } obj_act;
        struct {
            sf_vec3_t position;
            float     degree;
        } map_offset;
        struct {
            int32_t               unk_t00;
            int16_t               walk_region_indices[32];
            msbs_event_wr_entry_t wr_entries[5];
        } patrol_info;
        struct {
            int32_t platoon_id_script_active;
            int32_t state;
            int32_t group_part_indices[32];
        } platoon_info;
        struct { int32_t resource_item_lot_param_id; } resource_item_info;
        struct { int32_t grass_lod_range_param_id; } grass_lod_param;
        struct {
            int32_t unk_t00;
            uint8_t unk_t04;
            uint8_t unk_t05;
            uint8_t unk_t06;
            uint8_t unk_t07;
        } skit_info;
        struct { int32_t event21_part_indices[32]; } placement_group;
        struct {
            int32_t unk_t00;
            int32_t enemy_indices[8];
            int32_t talk_ids[8];
            int16_t unk_t44;
            int16_t unk_t46;
            int32_t unk_t48;
        } talk;
        struct {
            int32_t auto_draw_group_point_index;
            int32_t owning_collision_index;
        } auto_draw_group_collision;
    } u;
    const sf_allocator_t *alloc;
} msbs_event_t;

typedef enum msbs_region_type {
    MSBS_REGION_INVASION_POINT             = 1,
    MSBS_REGION_ENVIRONMENT_MAP_POINT      = 2,
    MSBS_REGION_SOUND                      = 4,
    MSBS_REGION_SFX                        = 5,
    MSBS_REGION_WIND_SFX                   = 6,
    MSBS_REGION_SPAWN_POINT                = 8,
    MSBS_REGION_PATROL_ROUTE               = 11,
    MSBS_REGION_WARP_POINT                 = 13,
    MSBS_REGION_ACTIVATION_AREA            = 14,
    MSBS_REGION_EVENT                      = 15,
    MSBS_REGION_LOGIC                      = 0,
    MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX = 17,
    MSBS_REGION_WIND_AREA                  = 18,
    MSBS_REGION_MUFFLING_BOX               = 20,
    MSBS_REGION_MUFFLING_PORTAL            = 21,
    MSBS_REGION_SOUND_SPACE_OVERRIDE       = 23,
    MSBS_REGION_MUFFLING_PLANE             = 24,
    MSBS_REGION_PARTS_GROUP_AREA           = 25,
    MSBS_REGION_AUTO_DRAW_GROUP_POINT      = 26,
    MSBS_REGION_OTHER                      = UINT32_MAX,
} msbs_region_type_t;

typedef enum msbs_region_shape_type {
    MSBS_REGION_SHAPE_POINT     = 0,
    MSBS_REGION_SHAPE_CIRCLE    = 1,
    MSBS_REGION_SHAPE_SPHERE    = 2,
    MSBS_REGION_SHAPE_CYLINDER  = 3,
    MSBS_REGION_SHAPE_RECTANGLE = 4,
    MSBS_REGION_SHAPE_BOX       = 5,
    MSBS_REGION_SHAPE_COMPOSITE = 6,
} msbs_region_shape_type_t;

typedef struct msbs_region_shape_child {
    int32_t region_index;
    int32_t unk04;
} msbs_region_shape_child_t;

typedef struct msbs_region {
    msbs_region_type_t       type;
    msbs_region_shape_type_t shape_type;
    char                    *name;
    sf_vec3_t                position;
    sf_vec3_t                rotation;
    int32_t                  unk2c;
    uint32_t                 map_studio_layer;
    int16_t                 *unk_a;
    int16_t                  unk_a_count;
    int16_t                 *unk_b;
    int16_t                  unk_b_count;
    int32_t                  activation_part_index;
    int32_t                  entity_id;
    union {
        struct { float radius; } circle;
        struct { float radius; } sphere;
        struct { float radius, height; } cylinder;
        struct { float width, depth; } rectangle;
        struct { float width, depth, height; } box;
        struct { msbs_region_shape_child_t children[8]; } composite;
    } shape;
    union {
        struct { int32_t priority; } invasion_point;
        struct {
            float   unk_t00;
            int32_t unk_t04;
            int32_t unk_t0c;
            float   unk_t10;
            float   unk_t14;
            int32_t unk_t18;
            int32_t unk_t1c;
            int32_t unk_t20;
            int32_t unk_t24;
            int32_t unk_t28;
        } environment_map_point;
        struct {
            int32_t sound_type;
            int32_t sound_id;
            int32_t child_region_indices[16];
            int32_t unk_t48;
        } sound;
        struct { int32_t effect_id, unk_t04, start_disabled; } sfx;
        struct { int32_t effect_id, wind_area_index; float unk_t18; } wind_sfx;
        struct {
            float   unk_t00;
            float   compare;
            uint8_t unk_t08;
            uint8_t unk_t09;
            int16_t unk_t0a;
            int32_t unk_t24;
            float   unk_t28;
            float   unk_t2c;
        } environment_map_effect_box;
        struct { int32_t unk_t00; } muffling_box;
        struct { int32_t unk_t00; } muffling_portal;
        struct { uint8_t unk_t00, unk_t01; } sound_space_override;
        struct { int64_t unk_t00; } parts_group_area;
        struct { int64_t unk_t00; } auto_draw_group_point;
    } u;
    const sf_allocator_t *alloc;
} msbs_region_t;

struct sf_msbs_model { msbs_model_t data; };
struct sf_msbs_event  { msbs_event_t data; };
struct sf_msbs_region { msbs_region_t data; };
typedef enum msbs_part_type {
    MSBS_PART_MAP_PIECE         = 0,
    MSBS_PART_OBJECT            = 1,
    MSBS_PART_ENEMY             = 2,
    MSBS_PART_PLAYER            = 4,
    MSBS_PART_COLLISION         = 5,
    MSBS_PART_DUMMY_OBJECT      = 9,
    MSBS_PART_DUMMY_ENEMY       = 10,
    MSBS_PART_CONNECT_COLLISION = 11,
} msbs_part_type_t;

typedef struct msbs_part_unk1 {
    uint32_t collision_mask[48];
    uint8_t  condition1;
    uint8_t  condition2;
} msbs_part_unk1_t;

typedef struct msbs_part_unk2 {
    int32_t condition;
    int32_t disp_groups[8];
    int16_t unk24;
    int16_t unk26;
} msbs_part_unk2_t;

typedef struct msbs_part_gparam {
    int32_t light_set_id;
    int32_t fog_param_id;
    int32_t light_scattering_id;
    int32_t env_map_id;
} msbs_part_gparam_t;

typedef struct msbs_part_scene_gparam {
    int8_t event_ids[4];
    float  unk40;
} msbs_part_scene_gparam_t;

typedef struct msbs_part_unk7 {
    int32_t unk00;
    int32_t unk04;
    int32_t grass_type_param_id;
    int32_t unk0c;
    int32_t unk10;
    int32_t unk14;
} msbs_part_unk7_t;

typedef struct msbs_part {
    msbs_part_type_t type;
    char            *name;
    int32_t          model_index;
    char            *sib_path;
    sf_vec3_t        position;
    sf_vec3_t        rotation;
    sf_vec3_t        scale;
    int32_t          entity_id;
    uint8_t          unk_e04;
    uint8_t          unk_e05;
    uint8_t          unk_e06;
    uint8_t          lantern_id;
    uint8_t          lod_param_id;
    uint8_t          unk_e09;
    bool             is_point_light_shadow_src;
    uint8_t          unk_e0b;
    bool             is_shadow_src;
    uint8_t          is_static_shadow_src;
    uint8_t          is_cascade3_shadow_src;
    uint8_t          unk_e0f;
    uint8_t          unk_e10;
    bool             is_shadow_dest;
    bool             is_shadow_only;
    bool             draw_by_reflect_cam;
    bool             draw_only_reflect_cam;
    uint8_t          enable_on_above_shadow;
    bool             disable_point_light_effect;
    uint8_t          unk_e17;
    int32_t          unk_e18;
    int32_t          entity_group_ids[8];
    int32_t          unk_e3c;
    int32_t          unk_e40;
    msbs_part_unk1_t unk1;
    msbs_part_unk2_t unk2;
    msbs_part_gparam_t gparam;
    msbs_part_scene_gparam_t scene_gparam;
    msbs_part_unk7_t unk7;
    union {
        struct {
            int32_t obj_part_index1;
            uint8_t break_term;
            bool    net_sync_type;
            uint8_t unk_t0e;
            bool    set_main_obj_structure_booleans;
            int16_t anim_id;
            int16_t unk_t18;
            int16_t unk_t1a;
            int32_t obj_part_index2;
            int32_t obj_part_index3;
        } object;
        struct {
            int32_t think_param_id;
            int32_t npc_param_id;
            int32_t unk_t10;
            int16_t platoon_id;
            int32_t chara_init_id;
            int32_t collision_part_index;
            int16_t unk_t20;
            int16_t unk_t22;
            int32_t unk_t24;
            int32_t backup_event_anim_id;
            int32_t event_flag_id;
            int32_t event_flag_compare_state;
            int32_t unk_t48;
            int32_t unk_t4c;
            int32_t unk_t50;
            int32_t unk_t78;
            float   unk_t84;
        } enemy;
        struct {
            uint8_t hit_filter_id;
            uint8_t sound_space_type;
            float   reflect_plane_height;
            int16_t map_name_id;
            bool    disable_start;
            uint8_t unk_t17;
            int32_t disable_bonfire_entity_id;
            uint8_t unk_t24;
            uint8_t unk_t25;
            uint8_t unk_t26;
            uint8_t map_visibility;
            int32_t play_region_id;
            int16_t lock_cam_param_id;
            int32_t unk_t3c;
            int32_t unk_t40;
            float   unk_t44;
            float   unk_t48;
            int32_t unk_t4c;
            float   unk_t50;
            float   unk_t54;
        } collision;
        struct {
            int32_t collision_index;
            uint8_t map_id[4];
        } connect_collision;
    } u;
} msbs_part_t;

struct sf_msbs_part   { msbs_part_t data; };

typedef enum msbs_route_type {
    MSBS_ROUTE_MUFFLING_PORTAL_LINK = 3,
    MSBS_ROUTE_MUFFLING_BOX_LINK    = 4,
} msbs_route_type_t;

typedef struct msbs_route {
    msbs_route_type_t     type;
    char                 *name;
    int32_t               unk08;
    int32_t               unk0c;
    const sf_allocator_t *alloc;
} msbs_route_t;

struct sf_msbs_route  { msbs_route_t data; };

struct sf_msbs {
    const sf_allocator_t *alloc;

    sf_msbs_model_t  *models;
    int32_t           model_count;
    sf_msbs_event_t  *events;
    int32_t           event_count;
    sf_msbs_region_t *regions;
    int32_t           region_count;
    sf_msbs_route_t  *routes;
    int32_t           route_count;
    sf_msbs_part_t   *parts;
    int32_t           part_count;
};

void msbs_model_param_free(sf_msbs_model_t *models, int32_t count, const sf_allocator_t *a);
void msbs_event_param_free(sf_msbs_event_t *events, int32_t count, const sf_allocator_t *a);
void msbs_point_param_free(sf_msbs_region_t *regions, int32_t count, const sf_allocator_t *a);
void msbs_parts_param_free(sf_msbs_part_t *parts, int32_t count, const sf_allocator_t *a);
void msbs_route_param_free(sf_msbs_route_t *routes, int32_t count, const sf_allocator_t *a);

/* Internal: sub-param readers/writers called from msbs.c dispatcher. */
sf_result_t msbs_model_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);
sf_result_t msbs_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a);

sf_result_t msbs_model_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_event_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_point_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_parts_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);
sf_result_t msbs_route_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs);

#ifdef __cplusplus
}
#endif

#endif /* SF_MAP_MSBS_INTERNAL_H */
