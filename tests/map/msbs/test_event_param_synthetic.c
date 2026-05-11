/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Sekiro MSBS EventParam synthetic round-trips.
 */

#include "map/msbs/msbs_internal.h"

#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void fill_i32s(int32_t *values, int count, int32_t seed) {
    for (int i = 0; i < count; i++) values[i] = seed + i;
}

static void fill_i16s(int16_t *values, int count, int16_t seed) {
    for (int i = 0; i < count; i++) values[i] = (int16_t)(seed + i);
}

static void init_common(msbs_event_t *event, msbs_event_type_t type, const char *name, int index) {
    memset(event, 0, sizeof(*event));
    event->type = type;
    event->name = (char *)name;
    event->event_id = 1000 + index;
    event->part_index = 10 + index;
    event->region_index = 20 + index;
    event->entity_id = 2000 + index;
}

static void init_event_case(sf_msbs_event_t *slot, msbs_event_type_t type, int index) {
    msbs_event_t *event = &slot->data;
    switch (type) {
    case MSBS_EVENT_TREASURE:
        init_common(event, type, "Event: Treasure", index);
        event->u.treasure.treasure_part_index = 31;
        event->u.treasure.item_lot_id = 400010;
        event->u.treasure.action_button_id = 7100;
        event->u.treasure.pickup_anim_id = 9001;
        event->u.treasure.in_chest = true;
        event->u.treasure.start_disabled = true;
        break;
    case MSBS_EVENT_GENERATOR:
        init_common(event, type, "Event: Generator", index);
        event->u.generator.max_num = 6;
        event->u.generator.gen_type = -2;
        event->u.generator.limit_num = 12;
        event->u.generator.min_gen_num = 2;
        event->u.generator.max_gen_num = 5;
        event->u.generator.min_interval = 1.25f;
        event->u.generator.max_interval = 7.5f;
        event->u.generator.initial_spawn_count = 3;
        event->u.generator.unk_t14 = 14.25f;
        event->u.generator.unk_t18 = 18.5f;
        fill_i32s(event->u.generator.spawn_region_indices, 8, 100);
        fill_i32s(event->u.generator.spawn_part_indices, 32, 200);
        break;
    case MSBS_EVENT_OBJ_ACT:
        init_common(event, type, "Event: ObjAct", index);
        event->u.obj_act.obj_act_entity_id = 3001;
        event->u.obj_act.obj_act_part_index = 55;
        event->u.obj_act.obj_act_id = 900;
        event->u.obj_act.state_type = 4;
        event->u.obj_act.event_flag_id = 13000500;
        break;
    case MSBS_EVENT_MAP_OFFSET:
        init_common(event, type, "Event: MapOffset", index);
        event->u.map_offset.position = (sf_vec3_t){ 1.0f, 2.0f, 3.0f };
        event->u.map_offset.degree = 45.0f;
        break;
    case MSBS_EVENT_PATROL_INFO:
        init_common(event, type, "Event: PatrolInfo", index);
        event->u.patrol_info.unk_t00 = 77;
        fill_i16s(event->u.patrol_info.walk_region_indices, 32, 10);
        for (int i = 0; i < 5; i++) {
            event->u.patrol_info.wr_entries[i].region_index = (int16_t)(80 + i);
            event->u.patrol_info.wr_entries[i].unk04 = 400 + i;
            event->u.patrol_info.wr_entries[i].unk08 = 800 + i;
        }
        break;
    case MSBS_EVENT_PLATOON_INFO:
        init_common(event, type, "Event: PlatoonInfo", index);
        event->u.platoon_info.platoon_id_script_active = 101;
        event->u.platoon_info.state = 3;
        fill_i32s(event->u.platoon_info.group_part_indices, 32, 500);
        break;
    case MSBS_EVENT_RESOURCE_ITEM_INFO:
        init_common(event, type, "Event: ResourceItemInfo", index);
        event->u.resource_item_info.resource_item_lot_param_id = 6200;
        break;
    case MSBS_EVENT_GRASS_LOD_PARAM:
        init_common(event, type, "Event: GrassLodParam", index);
        event->u.grass_lod_param.grass_lod_range_param_id = 91;
        break;
    case MSBS_EVENT_SKIT_INFO:
        init_common(event, type, "Event: SkitInfo", index);
        event->u.skit_info.unk_t00 = 1234;
        event->u.skit_info.unk_t04 = 1;
        event->u.skit_info.unk_t05 = 2;
        event->u.skit_info.unk_t06 = 3;
        event->u.skit_info.unk_t07 = 4;
        break;
    case MSBS_EVENT_PLACEMENT_GROUP:
        init_common(event, type, "Event: PlacementGroup", index);
        fill_i32s(event->u.placement_group.event21_part_indices, 32, 700);
        break;
    case MSBS_EVENT_PARTS_GROUP:
        init_common(event, type, "Event: PartsGroup", index);
        break;
    case MSBS_EVENT_TALK:
        init_common(event, type, "Event: Talk", index);
        event->u.talk.unk_t00 = 64;
        fill_i32s(event->u.talk.enemy_indices, 8, 30);
        fill_i32s(event->u.talk.talk_ids, 8, 6000);
        event->u.talk.unk_t44 = 44;
        event->u.talk.unk_t46 = 46;
        event->u.talk.unk_t48 = 48;
        break;
    case MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION:
        init_common(event, type, "Event: AutoDrawGroupCollision", index);
        event->u.auto_draw_group_collision.auto_draw_group_point_index = 8;
        event->u.auto_draw_group_collision.owning_collision_index = 9;
        break;
    case MSBS_EVENT_OTHER:
        init_common(event, type, "Event: Other", index);
        break;
    }
}

static void assert_event_equal(const msbs_event_t *expected, const msbs_event_t *actual) {
    TEST_ASSERT_EQUAL_UINT32((uint32_t)expected->type, (uint32_t)actual->type);
    TEST_ASSERT_EQUAL_STRING(expected->name, actual->name);
    TEST_ASSERT_EQUAL_INT32(expected->event_id, actual->event_id);
    TEST_ASSERT_EQUAL_INT32(expected->part_index, actual->part_index);
    TEST_ASSERT_EQUAL_INT32(expected->region_index, actual->region_index);
    TEST_ASSERT_EQUAL_INT32(expected->entity_id, actual->entity_id);
    TEST_ASSERT_EQUAL_MEMORY(&expected->u, &actual->u, sizeof(expected->u));
}

static void test_all_msbs_event_subtypes_round_trip(void) {
    static const msbs_event_type_t types[] = {
        MSBS_EVENT_TREASURE,
        MSBS_EVENT_GENERATOR,
        MSBS_EVENT_OBJ_ACT,
        MSBS_EVENT_MAP_OFFSET,
        MSBS_EVENT_PATROL_INFO,
        MSBS_EVENT_PLATOON_INFO,
        MSBS_EVENT_RESOURCE_ITEM_INFO,
        MSBS_EVENT_GRASS_LOD_PARAM,
        MSBS_EVENT_SKIT_INFO,
        MSBS_EVENT_PLACEMENT_GROUP,
        MSBS_EVENT_PARTS_GROUP,
        MSBS_EVENT_TALK,
        MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION,
        MSBS_EVENT_OTHER,
    };
    enum { EVENT_COUNT = (int)(sizeof(types) / sizeof(types[0])) };

    sf_msbs_event_t events[EVENT_COUNT];
    memset(events, 0, sizeof(events));
    for (int i = 0; i < EVENT_COUNT; i++) init_event_case(&events[i], types[i], i);

    sf_msbs_t source;
    memset(&source, 0, sizeof(source));
    source.events = events;
    source.event_count = EVENT_COUNT;

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(&source, &data, &size, NULL));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN(0, size);

    sf_msbs_t *read = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&read, data, size, NULL));
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_INT32(EVENT_COUNT, sf_msbs_event_count(read));

    for (int i = 0; i < EVENT_COUNT; i++) assert_event_equal(&events[i].data, &read->events[i].data);

    sf_msbs_destroy(read);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_all_msbs_event_subtypes_round_trip);
    return UNITY_END();
}
