/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS PartsParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBS/PartsParam.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbs_part_type_is_known(uint32_t type) {
    switch (type) {
    case MSBS_PART_MAP_PIECE:
    case MSBS_PART_OBJECT:
    case MSBS_PART_ENEMY:
    case MSBS_PART_PLAYER:
    case MSBS_PART_COLLISION:
    case MSBS_PART_DUMMY_OBJECT:
    case MSBS_PART_DUMMY_ENEMY:
    case MSBS_PART_CONNECT_COLLISION:
        return true;
    default:
        return false;
    }
}

static bool msbs_part_has_unk1(msbs_part_type_t type) {
    return type == MSBS_PART_MAP_PIECE || type == MSBS_PART_OBJECT ||
           type == MSBS_PART_ENEMY || type == MSBS_PART_COLLISION;
}

static bool msbs_part_has_unk2(msbs_part_type_t type) {
    return type == MSBS_PART_COLLISION || type == MSBS_PART_CONNECT_COLLISION;
}

static bool msbs_part_has_gparam(msbs_part_type_t type) {
    return type == MSBS_PART_MAP_PIECE || type == MSBS_PART_OBJECT ||
           type == MSBS_PART_ENEMY || type == MSBS_PART_COLLISION ||
           type == MSBS_PART_DUMMY_OBJECT || type == MSBS_PART_DUMMY_ENEMY;
}

static bool msbs_part_has_scene_gparam(msbs_part_type_t type) {
    return type == MSBS_PART_COLLISION;
}

static bool msbs_part_has_unk7(msbs_part_type_t type) {
    return type == MSBS_PART_MAP_PIECE;
}

void msbs_parts_param_free(sf_msbs_part_t *parts, int32_t count, const sf_allocator_t *a) {
    if (!parts || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, parts[i].data.name);
        sf_xfree(a, parts[i].data.sib_path);
        memset(&parts[i], 0, sizeof(parts[i]));
    }
}

static sf_result_t msbs_part_read_unk1(sf_binary_reader_t *r, msbs_part_unk1_t *out) {
    sf_result_t rc;
    rc = sf_binary_reader_read_u32s(r, 48, out->collision_mask); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &out->condition1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &out->condition2); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_pattern(r, 0xC0, 0x00);
}

static sf_result_t msbs_part_write_unk1(sf_binary_writer_t *w, const msbs_part_unk1_t *unk) {
    sf_result_t rc;
    rc = sf_binary_writer_write_u32s(w, 48, unk->collision_mask); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, unk->condition1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, unk->condition2); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_pattern(w, 0xC0, 0x00);
}

static sf_result_t msbs_part_read_unk2(sf_binary_reader_t *r, msbs_part_unk2_t *out) {
    sf_result_t rc;
    rc = sf_binary_reader_read_i32(r, &out->condition); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32s(r, 8, out->disp_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i16(r, &out->unk24); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i16(r, &out->unk26); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_pattern(r, 0x20, 0x00);
}

static sf_result_t msbs_part_write_unk2(sf_binary_writer_t *w, const msbs_part_unk2_t *unk) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, unk->condition); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32s(w, 8, unk->disp_groups); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, unk->unk24); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, unk->unk26); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_pattern(w, 0x20, 0x00);
}

static sf_result_t msbs_part_read_gparam(sf_binary_reader_t *r, msbs_part_gparam_t *out) {
    sf_result_t rc;
    rc = sf_binary_reader_read_i32(r, &out->light_set_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->fog_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->light_scattering_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->env_map_id); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_pattern(r, 0x10, 0x00);
}

static sf_result_t msbs_part_write_gparam(sf_binary_writer_t *w, const msbs_part_gparam_t *gp) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, gp->light_set_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, gp->fog_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, gp->light_scattering_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, gp->env_map_id); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_pattern(w, 0x10, 0x00);
}

static sf_result_t msbs_part_read_scene_gparam(sf_binary_reader_t *r,
                                               msbs_part_scene_gparam_t *out) {
    sf_result_t rc;
    rc = sf_binary_reader_assert_pattern(r, 0x3C, 0x00); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i8s(r, 4, out->event_ids); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_f32(r, &out->unk40); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

static sf_result_t msbs_part_write_scene_gparam(sf_binary_writer_t *w,
                                                const msbs_part_scene_gparam_t *gp) {
    sf_result_t rc;
    rc = sf_binary_writer_write_pattern(w, 0x3C, 0x00); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i8s(w, 4, gp->event_ids); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_f32(w, gp->unk40); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_i32(w, 0);
}

static sf_result_t msbs_part_read_unk7(sf_binary_reader_t *r, msbs_part_unk7_t *out) {
    sf_result_t rc;
    rc = sf_binary_reader_read_i32(r, &out->unk00); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk04); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->grass_type_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk0c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk10); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk14); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_i32_one(r, 0);
}

static sf_result_t msbs_part_write_unk7(sf_binary_writer_t *w, const msbs_part_unk7_t *unk) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, unk->unk00); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, unk->unk04); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, unk->grass_type_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, unk->unk0c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, unk->unk10); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, unk->unk14); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    return sf_binary_writer_write_i32(w, 0);
}

static sf_result_t msbs_part_read_entity_data(sf_binary_reader_t *r, msbs_part_t *part) {
    sf_result_t rc;
    rc = sf_binary_reader_read_i32(r, &part->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e04); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e05); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e06); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->lantern_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->lod_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e09); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->is_point_light_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e0b); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->is_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->is_static_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->is_cascade3_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e0f); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e10); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->is_shadow_dest); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->is_shadow_only); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->draw_by_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->draw_only_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->enable_on_above_shadow); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_bool(r, &part->disable_point_light_effect); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &part->unk_e17); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &part->unk_e18); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32s(r, 8, part->entity_group_ids); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &part->unk_e3c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &part->unk_e40); if (rc != SF_OK) return rc;
    return sf_binary_reader_assert_pattern(r, 0x10, 0x00);
}

static sf_result_t msbs_part_write_entity_data(sf_binary_writer_t *w, const msbs_part_t *part) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, part->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e04); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e05); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e06); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->lantern_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->lod_param_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e09); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->is_point_light_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e0b); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->is_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->is_static_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->is_cascade3_shadow_src); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e0f); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e10); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->is_shadow_dest); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->is_shadow_only); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->draw_by_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->draw_only_reflect_cam); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->enable_on_above_shadow); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_bool(w, part->disable_point_light_effect); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, part->unk_e17); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->unk_e18); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32s(w, 8, part->entity_group_ids); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->unk_e3c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->unk_e40); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_pattern(w, 0x10, 0x00); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t msbs_part_read_type_data(sf_binary_reader_t *r, msbs_part_t *part) {
    sf_result_t rc;
    switch (part->type) {
    case MSBS_PART_MAP_PIECE:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_PART_OBJECT:
    case MSBS_PART_DUMMY_OBJECT:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.object.obj_part_index1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.object.break_term); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_bool(r, &part->u.object.net_sync_type); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.object.unk_t0e); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_bool(r, &part->u.object.set_main_obj_structure_booleans); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.object.anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.object.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.object.unk_t1a); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.object.obj_part_index2); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_i32(r, &part->u.object.obj_part_index3);
    case MSBS_PART_ENEMY:
    case MSBS_PART_DUMMY_ENEMY:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.think_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.npc_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t10); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.enemy.platoon_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.chara_init_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.collision_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.enemy.unk_t20); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.enemy.unk_t22); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x10, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.backup_event_anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.event_flag_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.event_flag_compare_state); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t48); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t4c); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t50); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x18, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.enemy.unk_t78); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.enemy.unk_t84); if (rc != SF_OK) return rc;
        for (int i = 0; i < 5; i++) {
            rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
            rc = sf_binary_reader_assert_i16_one(r, -1); if (rc != SF_OK) return rc;
            rc = sf_binary_reader_assert_i16_one(r, 0xA); if (rc != SF_OK) return rc;
        }
        return sf_binary_reader_assert_pattern(r, 0x10, 0x00);
    case MSBS_PART_PLAYER:
        return sf_binary_reader_assert_pattern(r, 0x10, 0x00);
    case MSBS_PART_COLLISION:
        rc = sf_binary_reader_read_u8(r, &part->u.collision.hit_filter_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.sound_space_type); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.collision.reflect_plane_height); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.collision.map_name_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_bool(r, &part->u.collision.disable_start); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.unk_t17); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.collision.disable_bonfire_entity_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.unk_t25); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.unk_t26); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &part->u.collision.map_visibility); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.collision.play_region_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &part->u.collision.lock_cam_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.collision.unk_t3c); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.collision.unk_t40); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.collision.unk_t44); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.collision.unk_t48); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &part->u.collision.unk_t4c); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.collision.unk_t50); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &part->u.collision.unk_t54); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_PART_CONNECT_COLLISION:
        rc = sf_binary_reader_read_i32(r, &part->u.connect_collision.collision_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8s(r, 4, part->u.connect_collision.map_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_part_write_type_data(sf_binary_writer_t *w, const msbs_part_t *part) {
    sf_result_t rc;
    switch (part->type) {
    case MSBS_PART_MAP_PIECE:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_PART_OBJECT:
    case MSBS_PART_DUMMY_OBJECT:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.object.obj_part_index1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.object.break_term); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, part->u.object.net_sync_type); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.object.unk_t0e); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, part->u.object.set_main_obj_structure_booleans); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.object.anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.object.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.object.unk_t1a); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.object.obj_part_index2); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, part->u.object.obj_part_index3);
    case MSBS_PART_ENEMY:
    case MSBS_PART_DUMMY_ENEMY:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.think_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.npc_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t10); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.enemy.platoon_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.chara_init_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.collision_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.enemy.unk_t20); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.enemy.unk_t22); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x10, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.backup_event_anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.event_flag_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.event_flag_compare_state); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t48); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t4c); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t50); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x18, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.enemy.unk_t78); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.enemy.unk_t84); if (rc != SF_OK) return rc;
        for (int i = 0; i < 5; i++) {
            rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i16(w, -1); if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i16(w, 0xA); if (rc != SF_OK) return rc;
        }
        return sf_binary_writer_write_pattern(w, 0x10, 0x00);
    case MSBS_PART_PLAYER:
        return sf_binary_writer_write_pattern(w, 0x10, 0x00);
    case MSBS_PART_COLLISION:
        rc = sf_binary_writer_write_u8(w, part->u.collision.hit_filter_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.sound_space_type); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.collision.reflect_plane_height); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.collision.map_name_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, part->u.collision.disable_start); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.unk_t17); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.collision.disable_bonfire_entity_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.unk_t25); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.unk_t26); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, part->u.collision.map_visibility); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.collision.play_region_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, part->u.collision.lock_cam_param_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.collision.unk_t3c); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.collision.unk_t40); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.collision.unk_t44); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.collision.unk_t48); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, part->u.collision.unk_t4c); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.collision.unk_t50); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, part->u.collision.unk_t54); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_PART_CONNECT_COLLISION:
        rc = sf_binary_writer_write_i32(w, part->u.connect_collision.collision_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8s(w, 4, part->u.connect_collision.map_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_part_read_one(sf_binary_reader_t *r, int64_t entry_offset, msbs_part_t *out,
                                      const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->scale.x = 1.0f;
    out->scale.y = 1.0f;
    out->scale.z = 1.0f;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0, sib_offset = 0, unk1_offset = 0, unk2_offset = 0;
    int64_t entity_data_offset = 0, type_data_offset = 0, gparam_offset = 0;
    int64_t scene_gparam_offset = 0, unk7_offset = 0;
    uint32_t type = 0;

    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    if (!msbs_part_type_is_known(type)) return SF_ERR_UNSUPPORTED_VERSION;
    out->type = (msbs_part_type_t)type;
    int32_t id = 0;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_i32(r, &out->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &sib_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &unk1_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &unk2_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &entity_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &gparam_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &scene_gparam_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &unk7_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i64_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i64_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i64_one(r, 0); if (rc != SF_OK) return rc;

    if (name_offset == 0 || sib_offset == 0 || entity_data_offset == 0 || type_data_offset == 0)
        return SF_ERR_BAD_MAGIC;
    if (msbs_part_has_unk1(out->type) != (unk1_offset != 0)) return SF_ERR_BAD_MAGIC;
    if (msbs_part_has_unk2(out->type) != (unk2_offset != 0)) return SF_ERR_BAD_MAGIC;
    if (msbs_part_has_gparam(out->type) != (gparam_offset != 0)) return SF_ERR_BAD_MAGIC;
    if (msbs_part_has_scene_gparam(out->type) != (scene_gparam_offset != 0)) return SF_ERR_BAD_MAGIC;
    if (msbs_part_has_unk7(out->type) != (unk7_offset != 0)) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_get_utf16(r, entry_offset + sib_offset, &out->sib_path, NULL);
    if (rc != SF_OK) return rc;
    if (unk1_offset) { rc = sf_istream_seek(stream, entry_offset + unk1_offset); if (rc != SF_OK) return rc; rc = msbs_part_read_unk1(r, &out->unk1); if (rc != SF_OK) return rc; }
    if (unk2_offset) { rc = sf_istream_seek(stream, entry_offset + unk2_offset); if (rc != SF_OK) return rc; rc = msbs_part_read_unk2(r, &out->unk2); if (rc != SF_OK) return rc; }
    rc = sf_istream_seek(stream, entry_offset + entity_data_offset); if (rc != SF_OK) return rc;
    rc = msbs_part_read_entity_data(r, out); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, entry_offset + type_data_offset); if (rc != SF_OK) return rc;
    rc = msbs_part_read_type_data(r, out); if (rc != SF_OK) return rc;
    if (gparam_offset) { rc = sf_istream_seek(stream, entry_offset + gparam_offset); if (rc != SF_OK) return rc; rc = msbs_part_read_gparam(r, &out->gparam); if (rc != SF_OK) return rc; }
    if (scene_gparam_offset) { rc = sf_istream_seek(stream, entry_offset + scene_gparam_offset); if (rc != SF_OK) return rc; rc = msbs_part_read_scene_gparam(r, &out->scene_gparam); if (rc != SF_OK) return rc; }
    if (unk7_offset) { rc = sf_istream_seek(stream, entry_offset + unk7_offset); if (rc != SF_OK) return rc; rc = msbs_part_read_unk7(r, &out->unk7); if (rc != SF_OK) return rc; }
    return SF_OK;
}

sf_result_t msbs_parts_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->part_count = count;
    out->parts = NULL;
    if (count == 0) return SF_OK;
    size_t bytes = (size_t)count * sizeof(*out->parts);
    out->parts = (sf_msbs_part_t *)sf_xalloc(a, bytes);
    if (!out->parts) return SF_ERR_OOM;
    memset(out->parts, 0, bytes);
    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbs_part_read_one(r, entry_offsets[i], &out->parts[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbs_part_fill_optional_offset(sf_binary_writer_t *w, const char *key,
                                                  int64_t start, bool present) {
    return sf_binary_writer_fill_i64(w, key, present ? sf_binary_writer_position(w) - start : 0);
}

static sf_result_t msbs_part_write_one(sf_binary_writer_t *w, const msbs_part_t *part,
                                       int32_t id, int32_t index) {
    char name_key[32], sib_key[32], unk1_key[32], unk2_key[32], entity_key[32], type_key[32];
    char gparam_key[32], scene_key[32], unk7_key[32];
    snprintf(name_key, sizeof name_key, "MsbsPartName%d", index);
    snprintf(sib_key, sizeof sib_key, "MsbsPartSib%d", index);
    snprintf(unk1_key, sizeof unk1_key, "MsbsPartUnk1%d", index);
    snprintf(unk2_key, sizeof unk2_key, "MsbsPartUnk2%d", index);
    snprintf(entity_key, sizeof entity_key, "MsbsPartEnt%d", index);
    snprintf(type_key, sizeof type_key, "MsbsPartType%d", index);
    snprintf(gparam_key, sizeof gparam_key, "MsbsPartGparam%d", index);
    snprintf(scene_key, sizeof scene_key, "MsbsPartScene%d", index);
    snprintf(unk7_key, sizeof unk7_key, "MsbsPartUnk7%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, name_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)part->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, part->model_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, sib_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, part->scale); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, unk1_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, unk2_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, entity_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, type_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, gparam_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, scene_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, unk7_key); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i64(w, 0); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, name_key, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, part->name ? part->name : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, sib_key, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, part->sib_path ? part->sib_path : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    rc = msbs_part_fill_optional_offset(w, unk1_key, start, msbs_part_has_unk1(part->type));
    if (rc != SF_OK) return rc;
    if (msbs_part_has_unk1(part->type)) { rc = msbs_part_write_unk1(w, &part->unk1); if (rc != SF_OK) return rc; }
    rc = msbs_part_fill_optional_offset(w, unk2_key, start, msbs_part_has_unk2(part->type));
    if (rc != SF_OK) return rc;
    if (msbs_part_has_unk2(part->type)) { rc = msbs_part_write_unk2(w, &part->unk2); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, entity_key, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = msbs_part_write_entity_data(w, part); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, type_key, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = msbs_part_write_type_data(w, part); if (rc != SF_OK) return rc;
    rc = msbs_part_fill_optional_offset(w, gparam_key, start, msbs_part_has_gparam(part->type));
    if (rc != SF_OK) return rc;
    if (msbs_part_has_gparam(part->type)) { rc = msbs_part_write_gparam(w, &part->gparam); if (rc != SF_OK) return rc; }
    rc = msbs_part_fill_optional_offset(w, scene_key, start, msbs_part_has_scene_gparam(part->type));
    if (rc != SF_OK) return rc;
    if (msbs_part_has_scene_gparam(part->type)) { rc = msbs_part_write_scene_gparam(w, &part->scene_gparam); if (rc != SF_OK) return rc; }
    rc = msbs_part_fill_optional_offset(w, unk7_key, start, msbs_part_has_unk7(part->type));
    if (rc != SF_OK) return rc;
    if (msbs_part_has_unk7(part->type)) return msbs_part_write_unk7(w, &part->unk7);
    return SF_OK;
}

sf_result_t msbs_parts_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    if (!w || !msbs || msbs->part_count < 0) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 35); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbs->part_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsPartsNameOff0"); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbs->part_count; i++) {
        char entry_key[32];
        snprintf(entry_key, sizeof entry_key, "MsbsPartEntry%d", i);
        rc = sf_binary_writer_reserve_i64(w, entry_key); if (rc != SF_OK) return rc;
    }
    rc = sf_binary_writer_reserve_i64(w, "MsbsNextList5"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "MsbsPartsNameOff0", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "PARTS_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbs->part_count; i++) {
        char entry_key[32];
        snprintf(entry_key, sizeof entry_key, "MsbsPartEntry%d", i);
        rc = sf_binary_writer_fill_i64(w, entry_key, sf_binary_writer_position(w));
        if (rc != SF_OK) return rc;
        rc = msbs_part_write_one(w, &msbs->parts[i].data, i, i);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
