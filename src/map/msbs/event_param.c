/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS EventParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBS/EventParam.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h" /* IWYU pragma: keep */

#include <stdio.h>
#include <string.h>

static bool msbs_event_type_is_known(uint32_t type) {
    switch (type) {
    case MSBS_EVENT_TREASURE:
    case MSBS_EVENT_GENERATOR:
    case MSBS_EVENT_OBJ_ACT:
    case MSBS_EVENT_MAP_OFFSET:
    case MSBS_EVENT_PATROL_INFO:
    case MSBS_EVENT_PLATOON_INFO:
    case MSBS_EVENT_RESOURCE_ITEM_INFO:
    case MSBS_EVENT_GRASS_LOD_PARAM:
    case MSBS_EVENT_SKIT_INFO:
    case MSBS_EVENT_PLACEMENT_GROUP:
    case MSBS_EVENT_PARTS_GROUP:
    case MSBS_EVENT_TALK:
    case MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION:
    case MSBS_EVENT_OTHER:
        return true;
    default:
        return false;
    }
}

static bool msbs_event_has_type_data(msbs_event_type_t type) {
    return type != MSBS_EVENT_PARTS_GROUP && type != MSBS_EVENT_OTHER;
}

void msbs_event_param_free(sf_msbs_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msbs_event_read_type_data(sf_binary_reader_t *r, msbs_event_t *event) {
    sf_result_t rc;
    switch (event->type) {
    case MSBS_EVENT_TREASURE:
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.treasure.treasure_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.treasure.item_lot_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x24, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.treasure.action_button_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.treasure.pickup_anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_bool(r, &event->u.treasure.in_chest); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_bool(r, &event->u.treasure.start_disabled); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);

    case MSBS_EVENT_GENERATOR:
        rc = sf_binary_reader_read_u8(r, &event->u.generator.max_num); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i8(r, &event->u.generator.gen_type); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &event->u.generator.limit_num); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &event->u.generator.min_gen_num); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &event->u.generator.max_gen_num); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &event->u.generator.min_interval); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &event->u.generator.max_interval); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.generator.initial_spawn_count); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &event->u.generator.unk_t14); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &event->u.generator.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x14, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32s(r, 8, event->u.generator.spawn_region_indices); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x10, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32s(r, 32, event->u.generator.spawn_part_indices); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x20, 0x00);

    case MSBS_EVENT_OBJ_ACT:
        rc = sf_binary_reader_read_i32(r, &event->u.obj_act.obj_act_entity_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.obj_act.obj_act_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.obj_act.obj_act_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.obj_act.state_type); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_u8_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.obj_act.event_flag_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);

    case MSBS_EVENT_MAP_OFFSET:
        rc = sf_binary_reader_read_vec3(r, &event->u.map_offset.position); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &event->u.map_offset.degree);

    case MSBS_EVENT_PATROL_INFO:
        rc = sf_binary_reader_read_i32(r, &event->u.patrol_info.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16s(r, 32, event->u.patrol_info.walk_region_indices); if (rc != SF_OK) return rc;
        for (int i = 0; i < 5; i++) {
            rc = sf_binary_reader_read_i16(r, &event->u.patrol_info.wr_entries[i].region_index); if (rc != SF_OK) return rc;
            rc = sf_binary_reader_assert_i16_one(r, 0); if (rc != SF_OK) return rc;
            rc = sf_binary_reader_read_i32(r, &event->u.patrol_info.wr_entries[i].unk04); if (rc != SF_OK) return rc;
            rc = sf_binary_reader_read_i32(r, &event->u.patrol_info.wr_entries[i].unk08); if (rc != SF_OK) return rc;
        }
        return sf_binary_reader_assert_pattern(r, 0x14, 0x00);

    case MSBS_EVENT_PLATOON_INFO:
        rc = sf_binary_reader_read_i32(r, &event->u.platoon_info.platoon_id_script_active); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.platoon_info.state); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_i32s(r, 32, event->u.platoon_info.group_part_indices);

    case MSBS_EVENT_RESOURCE_ITEM_INFO:
        rc = sf_binary_reader_read_i32(r, &event->u.resource_item_info.resource_item_lot_param_id); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x1C, 0x00);

    case MSBS_EVENT_GRASS_LOD_PARAM:
        rc = sf_binary_reader_read_i32(r, &event->u.grass_lod_param.grass_lod_range_param_id); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x1C, 0x00);

    case MSBS_EVENT_SKIT_INFO:
        rc = sf_binary_reader_read_i32(r, &event->u.skit_info.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.skit_info.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.skit_info.unk_t05); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.skit_info.unk_t06); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &event->u.skit_info.unk_t07); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x18, 0x00);

    case MSBS_EVENT_PLACEMENT_GROUP:
        return sf_binary_reader_read_i32s(r, 32, event->u.placement_group.event21_part_indices);

    case MSBS_EVENT_TALK:
        rc = sf_binary_reader_read_i32(r, &event->u.talk.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32s(r, 8, event->u.talk.enemy_indices); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32s(r, 8, event->u.talk.talk_ids); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &event->u.talk.unk_t44); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &event->u.talk.unk_t46); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.talk.unk_t48); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x34, 0x00);

    case MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION:
        rc = sf_binary_reader_read_i32(r, &event->u.auto_draw_group_collision.auto_draw_group_point_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &event->u.auto_draw_group_collision.owning_collision_index); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x18, 0x00);

    case MSBS_EVENT_PARTS_GROUP:
    case MSBS_EVENT_OTHER:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_event_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbs_event_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0;
    uint32_t type = 0;
    int32_t id = 0;
    int64_t base_data_offset = 0;
    int64_t type_data_offset = 0;

    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset); if (rc != SF_OK) return rc;

    if (!msbs_event_type_is_known(type)) return SF_ERR_UNSUPPORTED_VERSION;
    out->type = (msbs_event_type_t)type;
    if (name_offset == 0 || base_data_offset == 0) return SF_ERR_BAD_MAGIC;
    if (msbs_event_has_type_data(out->type) != (type_data_offset != 0)) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, entry_offset + base_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    if (type_data_offset != 0) {
        rc = sf_istream_seek(stream, entry_offset + type_data_offset); if (rc != SF_OK) return rc;
        rc = msbs_event_read_type_data(r, out); if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

sf_result_t msbs_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;

    out->event_count = count;
    out->events = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->events);
    out->events = (sf_msbs_event_t *)sf_xalloc(a, bytes);
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, bytes);

    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) {
        sf_xfree(a, out->events);
        out->events = NULL;
        out->event_count = 0;
        return SF_ERR_OOM;
    }

    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbs_event_read_one(r, entry_offsets[i], &out->events[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbs_event_param_free(out->events, count, a);
        sf_xfree(a, out->events);
        out->events = NULL;
        out->event_count = 0;
    }
    return rc;
}

static sf_result_t msbs_event_write_type_data(sf_binary_writer_t *w, const msbs_event_t *event) {
    sf_result_t rc;
    switch (event->type) {
    case MSBS_EVENT_TREASURE:
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.treasure.treasure_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.treasure.item_lot_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x24, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.treasure.action_button_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.treasure.pickup_anim_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, event->u.treasure.in_chest); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, event->u.treasure.start_disabled); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);

    case MSBS_EVENT_GENERATOR:
        rc = sf_binary_writer_write_u8(w, event->u.generator.max_num); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i8(w, event->u.generator.gen_type); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, event->u.generator.limit_num); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, event->u.generator.min_gen_num); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, event->u.generator.max_gen_num); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, event->u.generator.min_interval); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, event->u.generator.max_interval); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.generator.initial_spawn_count); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, event->u.generator.unk_t14); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, event->u.generator.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x14, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32s(w, 8, event->u.generator.spawn_region_indices); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x10, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32s(w, 32, event->u.generator.spawn_part_indices); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x20, 0x00);

    case MSBS_EVENT_OBJ_ACT:
        rc = sf_binary_writer_write_i32(w, event->u.obj_act.obj_act_entity_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.obj_act.obj_act_part_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.obj_act.obj_act_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.obj_act.state_type); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.obj_act.event_flag_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);

    case MSBS_EVENT_MAP_OFFSET:
        rc = sf_binary_writer_write_vec3(w, event->u.map_offset.position); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, event->u.map_offset.degree);

    case MSBS_EVENT_PATROL_INFO:
        rc = sf_binary_writer_write_i32(w, event->u.patrol_info.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16s(w, 32, event->u.patrol_info.walk_region_indices); if (rc != SF_OK) return rc;
        for (int i = 0; i < 5; i++) {
            rc = sf_binary_writer_write_i16(w, event->u.patrol_info.wr_entries[i].region_index); if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i16(w, 0); if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i32(w, event->u.patrol_info.wr_entries[i].unk04); if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i32(w, event->u.patrol_info.wr_entries[i].unk08); if (rc != SF_OK) return rc;
        }
        return sf_binary_writer_write_pattern(w, 0x14, 0x00);

    case MSBS_EVENT_PLATOON_INFO:
        rc = sf_binary_writer_write_i32(w, event->u.platoon_info.platoon_id_script_active); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.platoon_info.state); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32s(w, 32, event->u.platoon_info.group_part_indices);

    case MSBS_EVENT_RESOURCE_ITEM_INFO:
        rc = sf_binary_writer_write_i32(w, event->u.resource_item_info.resource_item_lot_param_id); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x1C, 0x00);

    case MSBS_EVENT_GRASS_LOD_PARAM:
        rc = sf_binary_writer_write_i32(w, event->u.grass_lod_param.grass_lod_range_param_id); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x1C, 0x00);

    case MSBS_EVENT_SKIT_INFO:
        rc = sf_binary_writer_write_i32(w, event->u.skit_info.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.skit_info.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.skit_info.unk_t05); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.skit_info.unk_t06); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, event->u.skit_info.unk_t07); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x18, 0x00);

    case MSBS_EVENT_PLACEMENT_GROUP:
        return sf_binary_writer_write_i32s(w, 32, event->u.placement_group.event21_part_indices);

    case MSBS_EVENT_TALK:
        rc = sf_binary_writer_write_i32(w, event->u.talk.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32s(w, 8, event->u.talk.enemy_indices); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32s(w, 8, event->u.talk.talk_ids); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, event->u.talk.unk_t44); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, event->u.talk.unk_t46); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.talk.unk_t48); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x34, 0x00);

    case MSBS_EVENT_AUTO_DRAW_GROUP_COLLISION:
        rc = sf_binary_writer_write_i32(w, event->u.auto_draw_group_collision.auto_draw_group_point_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, event->u.auto_draw_group_collision.owning_collision_index); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x18, 0x00);

    case MSBS_EVENT_PARTS_GROUP:
    case MSBS_EVENT_OTHER:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_event_write_one(sf_binary_writer_t *w, const msbs_event_t *event,
                                        int32_t id, int32_t index) {
    char name_offset_key[32];
    char base_data_offset_key[32];
    char type_data_offset_key[32];
    snprintf(name_offset_key, sizeof name_offset_key, "MsbsEventName%d", index);
    snprintf(base_data_offset_key, sizeof base_data_offset_key, "MsbsEventBase%d", index);
    snprintf(type_data_offset_key, sizeof type_data_offset_key, "MsbsEventType%d", index);

    if (!msbs_event_type_is_known((uint32_t)event->type)) return SF_ERR_UNSUPPORTED_VERSION;

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_offset_key), return rc);
    rc = sf_binary_writer_write_i32(w, event->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)event->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, base_data_offset_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, type_data_offset_key), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_offset_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, event->name ? event->name : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, base_data_offset_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_i32(w, event->part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    if (msbs_event_has_type_data(event->type)) {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_data_offset_key, sf_binary_writer_position(w) - start), return rc);
        return msbs_event_write_type_data(w, event);
    }
    return sf_binary_writer_fill_i64(w, type_data_offset_key, 0);
}

static sf_result_t msbs_event_write_entry(sf_binary_writer_t *w,
                                          const void         *entry,
                                          size_t              index,
                                          void               *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbs_event_t *event = (const sf_msbs_event_t *)entry;
    return msbs_event_write_one(w, &event->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbs_event_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    if (!w || !msbs || msbs->event_count < 0) return SF_ERR_INVALID_ARG;
    return msb_entry_list_write(w, 35, "EVENT_PARAM_ST", "MsbsNextList1", msbs->events,
                                (size_t)msbs->event_count, sizeof(*msbs->events),
                                msbs_event_write_entry, NULL);
}
