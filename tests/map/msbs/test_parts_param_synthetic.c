/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Sekiro MSBS PartsParam synthetic round-trips.
 */

#include "map/msbs/msbs_internal.h"

#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void fill_common(msbs_part_t *part, msbs_part_type_t type, const char *name, int index) {
    memset(part, 0, sizeof(*part));
    part->type = type;
    part->name = (char *)name;
    part->model_index = index;
    part->sib_path = (char *)"N:\\synthetic.sib";
    part->position = (sf_vec3_t){ 1.0f + (float)index, 2.0f, 3.0f };
    part->rotation = (sf_vec3_t){ 4.0f, 5.0f + (float)index, 6.0f };
    part->scale = (sf_vec3_t){ 1.0f, 1.0f, 1.0f };
    part->entity_id = 1000 + index;
    part->unk_e04 = (uint8_t)(1 + index);
    part->unk_e05 = (uint8_t)(2 + index);
    part->unk_e06 = (uint8_t)(3 + index);
    part->lantern_id = (uint8_t)(4 + index);
    part->lod_param_id = (uint8_t)(5 + index);
    part->unk_e09 = (uint8_t)(6 + index);
    part->is_point_light_shadow_src = (index & 1) != 0;
    part->unk_e0b = (uint8_t)(7 + index);
    part->is_shadow_src = (index & 1) == 0;
    part->is_static_shadow_src = (uint8_t)(8 + index);
    part->is_cascade3_shadow_src = (uint8_t)(9 + index);
    part->unk_e0f = (uint8_t)(10 + index);
    part->unk_e10 = (uint8_t)(11 + index);
    part->is_shadow_dest = true;
    part->is_shadow_only = false;
    part->draw_by_reflect_cam = (index & 1) != 0;
    part->draw_only_reflect_cam = (index & 1) == 0;
    part->enable_on_above_shadow = (uint8_t)(12 + index);
    part->disable_point_light_effect = true;
    part->unk_e17 = (uint8_t)(13 + index);
    part->unk_e18 = 2000 + index;
    for (int i = 0; i < 8; i++) part->entity_group_ids[i] = -1 + i + index;
    part->unk_e3c = 3000 + index;
    part->unk_e40 = 4000 + index;
}

static void fill_unk1(msbs_part_t *part, int index) {
    for (int i = 0; i < 48; i++) part->unk1.collision_mask[i] = (uint32_t)(0x1000 + index + i);
    part->unk1.condition1 = (uint8_t)(20 + index);
    part->unk1.condition2 = (uint8_t)(21 + index);
}

static void fill_unk2(msbs_part_t *part, int index) {
    part->unk2.condition = 5000 + index;
    for (int i = 0; i < 8; i++) part->unk2.disp_groups[i] = 6000 + index + i;
    part->unk2.unk24 = (int16_t)(30 + index);
    part->unk2.unk26 = (int16_t)(31 + index);
}

static void fill_gparam(msbs_part_t *part, int index) {
    part->gparam.light_set_id = 7000 + index;
    part->gparam.fog_param_id = 7100 + index;
    part->gparam.light_scattering_id = 7200 + index;
    part->gparam.env_map_id = 7300 + index;
}

static void fill_part(msbs_part_t *part, msbs_part_type_t type, const char *name, int index) {
    fill_common(part, type, name, index);
    if (type == MSBS_PART_MAP_PIECE || type == MSBS_PART_OBJECT ||
        type == MSBS_PART_ENEMY || type == MSBS_PART_COLLISION) {
        fill_unk1(part, index);
    }
    if (type == MSBS_PART_COLLISION || type == MSBS_PART_CONNECT_COLLISION) fill_unk2(part, index);
    if (type != MSBS_PART_PLAYER && type != MSBS_PART_CONNECT_COLLISION) fill_gparam(part, index);

    switch (type) {
    case MSBS_PART_MAP_PIECE:
        part->unk7 = (msbs_part_unk7_t){ 1, 2, 3 + index, 4, 5, 6 };
        break;
    case MSBS_PART_OBJECT:
    case MSBS_PART_DUMMY_OBJECT:
        part->u.object.obj_part_index1 = -1;
        part->u.object.break_term = (uint8_t)(40 + index);
        part->u.object.net_sync_type = true;
        part->u.object.unk_t0e = (uint8_t)(41 + index);
        part->u.object.set_main_obj_structure_booleans = (index & 1) != 0;
        part->u.object.anim_id = (int16_t)(42 + index);
        part->u.object.unk_t18 = (int16_t)(43 + index);
        part->u.object.unk_t1a = (int16_t)(44 + index);
        part->u.object.obj_part_index2 = -1;
        part->u.object.obj_part_index3 = -1;
        break;
    case MSBS_PART_ENEMY:
    case MSBS_PART_DUMMY_ENEMY:
        part->u.enemy.think_param_id = 8000 + index;
        part->u.enemy.npc_param_id = 8100 + index;
        part->u.enemy.unk_t10 = -1;
        part->u.enemy.platoon_id = (int16_t)(50 + index);
        part->u.enemy.chara_init_id = 8200 + index;
        part->u.enemy.collision_part_index = -1;
        part->u.enemy.unk_t20 = (int16_t)(51 + index);
        part->u.enemy.unk_t22 = (int16_t)(52 + index);
        part->u.enemy.unk_t24 = 8300 + index;
        part->u.enemy.backup_event_anim_id = -1;
        part->u.enemy.event_flag_id = -1;
        part->u.enemy.event_flag_compare_state = index;
        part->u.enemy.unk_t48 = 8400 + index;
        part->u.enemy.unk_t4c = 8500 + index;
        part->u.enemy.unk_t50 = 8600 + index;
        part->u.enemy.unk_t78 = 8700 + index;
        part->u.enemy.unk_t84 = 88.0f + (float)index;
        break;
    case MSBS_PART_COLLISION:
        part->scene_gparam.event_ids[0] = 1;
        part->scene_gparam.event_ids[1] = 2;
        part->scene_gparam.event_ids[2] = 3;
        part->scene_gparam.event_ids[3] = 4;
        part->scene_gparam.unk40 = 9.5f;
        part->u.collision.hit_filter_id = (uint8_t)(60 + index);
        part->u.collision.sound_space_type = (uint8_t)(61 + index);
        part->u.collision.reflect_plane_height = 10.5f;
        part->u.collision.map_name_id = (int16_t)(62 + index);
        part->u.collision.disable_start = true;
        part->u.collision.unk_t17 = (uint8_t)(63 + index);
        part->u.collision.disable_bonfire_entity_id = -1;
        part->u.collision.unk_t24 = 64;
        part->u.collision.unk_t25 = 65;
        part->u.collision.unk_t26 = 66;
        part->u.collision.map_visibility = 67;
        part->u.collision.play_region_id = 9000 + index;
        part->u.collision.lock_cam_param_id = (int16_t)(68 + index);
        part->u.collision.unk_t3c = 9100 + index;
        part->u.collision.unk_t40 = 9200 + index;
        part->u.collision.unk_t44 = 11.5f;
        part->u.collision.unk_t48 = 12.5f;
        part->u.collision.unk_t4c = 9300 + index;
        part->u.collision.unk_t50 = 13.5f;
        part->u.collision.unk_t54 = 14.5f;
        break;
    case MSBS_PART_CONNECT_COLLISION:
        part->u.connect_collision.collision_index = -1;
        part->u.connect_collision.map_id[0] = 10;
        part->u.connect_collision.map_id[1] = 0;
        part->u.connect_collision.map_id[2] = 0;
        part->u.connect_collision.map_id[3] = 0;
        break;
    default:
        break;
    }
}

static void assert_round_trips(msbs_part_type_t type, const char *name, int index) {
    sf_msbs_part_t part;
    fill_part(&part.data, type, name, index);
    sf_msbs_t fixture = { 0 };
    fixture.parts = &part;
    fixture.part_count = 1;

    uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(&fixture, &bytes, &size, NULL));

    sf_msbs_t *read = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&read, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_INT32(1, sf_msbs_part_count(read));
    const sf_msbs_part_t *read_part = sf_msbs_part_at(read, 0);
    TEST_ASSERT_NOT_NULL(read_part);
    TEST_ASSERT_EQUAL(type, read_part->data.type);
    TEST_ASSERT_EQUAL_STRING(name, read_part->data.name);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(read, &written, &written_size, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, size);

    sf_msbs_destroy(read);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

static void test_map_piece_round_trip(void) { assert_round_trips(MSBS_PART_MAP_PIECE, "m100000_0000", 0); }
static void test_object_round_trip(void) { assert_round_trips(MSBS_PART_OBJECT, "o100000_0000", 1); }
static void test_enemy_round_trip(void) { assert_round_trips(MSBS_PART_ENEMY, "c1000_0000", 2); }
static void test_player_round_trip(void) { assert_round_trips(MSBS_PART_PLAYER, "c0000_0000", 3); }
static void test_collision_round_trip(void) { assert_round_trips(MSBS_PART_COLLISION, "h100000", 4); }
static void test_dummy_object_round_trip(void) { assert_round_trips(MSBS_PART_DUMMY_OBJECT, "o200000_0000", 5); }
static void test_dummy_enemy_round_trip(void) { assert_round_trips(MSBS_PART_DUMMY_ENEMY, "c2000_0000", 6); }
static void test_connect_collision_round_trip(void) { assert_round_trips(MSBS_PART_CONNECT_COLLISION, "h200000_0000", 7); }

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_map_piece_round_trip);
    RUN_TEST(test_object_round_trip);
    RUN_TEST(test_enemy_round_trip);
    RUN_TEST(test_player_round_trip);
    RUN_TEST(test_collision_round_trip);
    RUN_TEST(test_dummy_object_round_trip);
    RUN_TEST(test_dummy_enemy_round_trip);
    RUN_TEST(test_connect_collision_round_trip);
    return UNITY_END();
}
