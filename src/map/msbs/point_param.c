/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS PointParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBS/PointParam.cs
 *   SoulsFormats/Formats/MSB/Shape.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbs_region_type_is_known(uint32_t type) {
    switch (type) {
    case MSBS_REGION_INVASION_POINT:
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
    case MSBS_REGION_SOUND:
    case MSBS_REGION_SFX:
    case MSBS_REGION_WIND_SFX:
    case MSBS_REGION_SPAWN_POINT:
    case MSBS_REGION_PATROL_ROUTE:
    case MSBS_REGION_WARP_POINT:
    case MSBS_REGION_ACTIVATION_AREA:
    case MSBS_REGION_EVENT:
    case MSBS_REGION_LOGIC:
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
    case MSBS_REGION_WIND_AREA:
    case MSBS_REGION_MUFFLING_BOX:
    case MSBS_REGION_MUFFLING_PORTAL:
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
    case MSBS_REGION_MUFFLING_PLANE:
    case MSBS_REGION_PARTS_GROUP_AREA:
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
    case MSBS_REGION_OTHER:
        return true;
    default:
        return false;
    }
}

static bool msbs_region_has_type_data(msbs_region_type_t type) {
    switch (type) {
    case MSBS_REGION_INVASION_POINT:
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
    case MSBS_REGION_SOUND:
    case MSBS_REGION_SFX:
    case MSBS_REGION_WIND_SFX:
    case MSBS_REGION_SPAWN_POINT:
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
    case MSBS_REGION_MUFFLING_BOX:
    case MSBS_REGION_MUFFLING_PORTAL:
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
    case MSBS_REGION_PARTS_GROUP_AREA:
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
        return true;
    default:
        return false;
    }
}

static bool msbs_region_shape_type_is_known(uint32_t type) {
    return type <= MSBS_REGION_SHAPE_COMPOSITE;
}

static bool msbs_region_shape_has_data(msbs_region_shape_type_t type) {
    return type != MSBS_REGION_SHAPE_POINT;
}

void msbs_point_param_free(sf_msbs_region_t *regions, int32_t count, const sf_allocator_t *a) {
    if (!regions || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, regions[i].data.name);
        sf_xfree(a, regions[i].data.unk_a);
        sf_xfree(a, regions[i].data.unk_b);
        memset(&regions[i], 0, sizeof(regions[i]));
    }
}

static sf_result_t msbs_region_read_i16_list(sf_binary_reader_t *r, int16_t **out_values,
                                             int16_t *out_count, const sf_allocator_t *a) {
    int16_t count;
    sf_result_t rc = sf_binary_reader_read_i16(r, &count);
    if (rc != SF_OK) return rc;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;

    *out_values = NULL;
    *out_count = count;
    if (count == 0) return SF_OK;

    int16_t *values = (int16_t *)sf_xalloc(a, (size_t)count * sizeof(*values));
    if (!values) return SF_ERR_OOM;
    rc = sf_binary_reader_read_i16s(r, (size_t)count, values);
    if (rc != SF_OK) {
        sf_xfree(a, values);
        return rc;
    }
    *out_values = values;
    return SF_OK;
}

static sf_result_t msbs_region_read_shape_data(sf_binary_reader_t *r, msbs_region_t *region) {
    sf_result_t rc;
    switch (region->shape_type) {
    case MSBS_REGION_SHAPE_CIRCLE:
        return sf_binary_reader_read_f32(r, &region->shape.circle.radius);
    case MSBS_REGION_SHAPE_SPHERE:
        return sf_binary_reader_read_f32(r, &region->shape.sphere.radius);
    case MSBS_REGION_SHAPE_CYLINDER:
        rc = sf_binary_reader_read_f32(r, &region->shape.cylinder.radius); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.cylinder.height);
    case MSBS_REGION_SHAPE_RECTANGLE:
        rc = sf_binary_reader_read_f32(r, &region->shape.rectangle.width); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.rectangle.depth);
    case MSBS_REGION_SHAPE_BOX:
        rc = sf_binary_reader_read_f32(r, &region->shape.box.width); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->shape.box.depth); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_f32(r, &region->shape.box.height);
    case MSBS_REGION_SHAPE_COMPOSITE:
        for (int i = 0; i < 8; i++) {
            rc = sf_binary_reader_read_i32(r, &region->shape.composite.children[i].region_index);
            if (rc != SF_OK) return rc;
            rc = sf_binary_reader_read_i32(r, &region->shape.composite.children[i].unk04);
            if (rc != SF_OK) return rc;
        }
        return SF_OK;
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_region_write_shape_data(sf_binary_writer_t *w, const msbs_region_t *region) {
    sf_result_t rc;
    switch (region->shape_type) {
    case MSBS_REGION_SHAPE_CIRCLE:
        return sf_binary_writer_write_f32(w, region->shape.circle.radius);
    case MSBS_REGION_SHAPE_SPHERE:
        return sf_binary_writer_write_f32(w, region->shape.sphere.radius);
    case MSBS_REGION_SHAPE_CYLINDER:
        rc = sf_binary_writer_write_f32(w, region->shape.cylinder.radius); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.cylinder.height);
    case MSBS_REGION_SHAPE_RECTANGLE:
        rc = sf_binary_writer_write_f32(w, region->shape.rectangle.width); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.rectangle.depth);
    case MSBS_REGION_SHAPE_BOX:
        rc = sf_binary_writer_write_f32(w, region->shape.box.width); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->shape.box.depth); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_f32(w, region->shape.box.height);
    case MSBS_REGION_SHAPE_COMPOSITE:
        for (int i = 0; i < 8; i++) {
            rc = sf_binary_writer_write_i32(w, region->shape.composite.children[i].region_index);
            if (rc != SF_OK) return rc;
            rc = sf_binary_writer_write_i32(w, region->shape.composite.children[i].unk04);
            if (rc != SF_OK) return rc;
        }
        return SF_OK;
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_region_read_type_data(sf_binary_reader_t *r, msbs_region_t *region) {
    sf_result_t rc;
    switch (region->type) {
    case MSBS_REGION_INVASION_POINT:
        return sf_binary_reader_read_i32(r, &region->u.invasion_point.priority);
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_point.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t0c); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_point.unk_t10); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_point.unk_t14); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t1c); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t20); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_point.unk_t28); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x10, 0x00);
    case MSBS_REGION_SOUND:
        rc = sf_binary_reader_read_i32(r, &region->u.sound.sound_type); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.sound.sound_id); if (rc != SF_OK) return rc;
        for (int i = 0; i < 16; i++) {
            rc = sf_binary_reader_read_i32(r, &region->u.sound.child_region_indices[i]);
            if (rc != SF_OK) return rc;
        }
        return sf_binary_reader_read_i32(r, &region->u.sound.unk_t48);
    case MSBS_REGION_SFX:
        rc = sf_binary_reader_read_i32(r, &region->u.sfx.effect_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.sfx.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        return sf_binary_reader_read_i32(r, &region->u.sfx.start_disabled);
    case MSBS_REGION_WIND_SFX:
        rc = sf_binary_reader_read_i32(r, &region->u.wind_sfx.effect_id); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x10, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.wind_sfx.wind_area_index); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.wind_sfx.unk_t18); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_REGION_SPAWN_POINT:
        rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_effect_box.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_effect_box.compare); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &region->u.environment_map_effect_box.unk_t08); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &region->u.environment_map_effect_box.unk_t09); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i16(r, &region->u.environment_map_effect_box.unk_t0a); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_assert_pattern(r, 0x18, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_i32(r, &region->u.environment_map_effect_box.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_effect_box.unk_t28); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_f32(r, &region->u.environment_map_effect_box.unk_t2c); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_REGION_MUFFLING_BOX:
        return sf_binary_reader_read_i32(r, &region->u.muffling_box.unk_t00);
    case MSBS_REGION_MUFFLING_PORTAL:
        rc = sf_binary_reader_read_i32(r, &region->u.muffling_portal.unk_t00); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_i32_one(r, 0);
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
        rc = sf_binary_reader_read_u8(r, &region->u.sound_space_override.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_reader_read_u8(r, &region->u.sound_space_override.unk_t01); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x1E, 0x00);
    case MSBS_REGION_PARTS_GROUP_AREA:
        return sf_binary_reader_read_i64(r, &region->u.parts_group_area.unk_t00);
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
        rc = sf_binary_reader_read_i64(r, &region->u.auto_draw_group_point.unk_t00); if (rc != SF_OK) return rc;
        return sf_binary_reader_assert_pattern(r, 0x18, 0x00);
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_region_write_type_data(sf_binary_writer_t *w, const msbs_region_t *region) {
    sf_result_t rc;
    switch (region->type) {
    case MSBS_REGION_INVASION_POINT:
        return sf_binary_writer_write_i32(w, region->u.invasion_point.priority);
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_point.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t0c); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_point.unk_t10); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_point.unk_t14); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t18); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t1c); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t20); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_point.unk_t28); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x10, 0x00);
    case MSBS_REGION_SOUND:
        rc = sf_binary_writer_write_i32(w, region->u.sound.sound_type); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.sound.sound_id); if (rc != SF_OK) return rc;
        for (int i = 0; i < 16; i++) {
            rc = sf_binary_writer_write_i32(w, region->u.sound.child_region_indices[i]);
            if (rc != SF_OK) return rc;
        }
        return sf_binary_writer_write_i32(w, region->u.sound.unk_t48);
    case MSBS_REGION_SFX:
        rc = sf_binary_writer_write_i32(w, region->u.sfx.effect_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.sfx.unk_t04); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, region->u.sfx.start_disabled);
    case MSBS_REGION_WIND_SFX:
        rc = sf_binary_writer_write_i32(w, region->u.wind_sfx.effect_id); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x10, 0xFF); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.wind_sfx.wind_area_index); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.wind_sfx.unk_t18); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_REGION_SPAWN_POINT:
        rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_effect_box.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_effect_box.compare); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, region->u.environment_map_effect_box.unk_t08); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, region->u.environment_map_effect_box.unk_t09); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i16(w, region->u.environment_map_effect_box.unk_t0a); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_pattern(w, 0x18, 0x00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_i32(w, region->u.environment_map_effect_box.unk_t24); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_effect_box.unk_t28); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_f32(w, region->u.environment_map_effect_box.unk_t2c); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_REGION_MUFFLING_BOX:
        return sf_binary_writer_write_i32(w, region->u.muffling_box.unk_t00);
    case MSBS_REGION_MUFFLING_PORTAL:
        rc = sf_binary_writer_write_i32(w, region->u.muffling_portal.unk_t00); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_i32(w, 0);
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
        rc = sf_binary_writer_write_u8(w, region->u.sound_space_override.unk_t00); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, region->u.sound_space_override.unk_t01); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x1E, 0x00);
    case MSBS_REGION_PARTS_GROUP_AREA:
        return sf_binary_writer_write_i64(w, region->u.parts_group_area.unk_t00);
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
        rc = sf_binary_writer_write_i64(w, region->u.auto_draw_group_point.unk_t00); if (rc != SF_OK) return rc;
        return sf_binary_writer_write_pattern(w, 0x18, 0x00);
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbs_region_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                        msbs_region_t *out, const sf_allocator_t *a) {
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset, base_data_offset1, base_data_offset2, shape_data_offset;
    int64_t base_data_offset3, type_data_offset;
    uint32_t type, shape_type;
    int32_t id;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_u32(r, &shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->position); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_vec3(r, &out->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk2c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base_data_offset1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base_data_offset2); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->map_studio_layer); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &shape_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base_data_offset3); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset); if (rc != SF_OK) return rc;

    if (!msbs_region_type_is_known(type) || !msbs_region_shape_type_is_known(shape_type)) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    out->type = (msbs_region_type_t)type;
    out->shape_type = (msbs_region_shape_type_t)shape_type;
    out->alloc = a;
    if (name_offset == 0 || base_data_offset1 == 0 || base_data_offset2 == 0 ||
        base_data_offset3 == 0) {
        return SF_ERR_BAD_MAGIC;
    }
    if (msbs_region_shape_has_data(out->shape_type) != (shape_data_offset != 0) ||
        msbs_region_has_type_data(out->type) != (type_data_offset != 0)) {
        return SF_ERR_BAD_MAGIC;
    }

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, entry_offset + base_data_offset1); if (rc != SF_OK) return rc;
    rc = msbs_region_read_i16_list(r, &out->unk_a, &out->unk_a_count, a); if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, entry_offset + base_data_offset2); if (rc != SF_OK) return rc;
    rc = msbs_region_read_i16_list(r, &out->unk_b, &out->unk_b_count, a); if (rc != SF_OK) return rc;

    if (shape_data_offset != 0) {
        rc = sf_istream_seek(stream, entry_offset + shape_data_offset); if (rc != SF_OK) return rc;
        rc = msbs_region_read_shape_data(r, out); if (rc != SF_OK) return rc;
    }

    rc = sf_istream_seek(stream, entry_offset + base_data_offset3); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->activation_part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;

    if (type_data_offset != 0) {
        rc = sf_istream_seek(stream, entry_offset + type_data_offset); if (rc != SF_OK) return rc;
        rc = msbs_region_read_type_data(r, out); if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

sf_result_t msbs_point_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;

    out->region_count = count;
    out->regions = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->regions);
    out->regions = (sf_msbs_region_t *)sf_xalloc(a, bytes);
    if (!out->regions) return SF_ERR_OOM;
    memset(out->regions, 0, bytes);

    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;

    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbs_region_read_one(r, entry_offsets[i], &out->regions[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbs_point_param_free(out->regions, count, a);
        sf_xfree(a, out->regions);
        out->regions = NULL;
        out->region_count = 0;
    }
    return rc;
}

static sf_result_t msbs_region_write_one(sf_binary_writer_t *w, const msbs_region_t *region,
                                         int32_t id) {
    char name_off[32], base1_off[32], base2_off[32], shape_off[32], base3_off[32], type_off[32];
    snprintf(name_off, sizeof name_off, "RegionName%ld", (long)sf_binary_writer_position(w));
    snprintf(base1_off, sizeof base1_off, "RegionBase1%ld", (long)sf_binary_writer_position(w));
    snprintf(base2_off, sizeof base2_off, "RegionBase2%ld", (long)sf_binary_writer_position(w));
    snprintf(shape_off, sizeof shape_off, "RegionShape%ld", (long)sf_binary_writer_position(w));
    snprintf(base3_off, sizeof base3_off, "RegionBase3%ld", (long)sf_binary_writer_position(w));
    snprintf(type_off, sizeof type_off, "RegionType%ld", (long)sf_binary_writer_position(w));

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, name_off); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)region->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)region->shape_type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->position); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_vec3(w, region->rotation); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, region->unk2c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base1_off); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base2_off); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, region->map_studio_layer); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, shape_off); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, base3_off); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, type_off); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, name_off, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, region->name ? region->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, base1_off, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, region->unk_a_count); if (rc != SF_OK) return rc;
    if (region->unk_a_count > 0) {
        rc = sf_binary_writer_write_i16s(w, (size_t)region->unk_a_count, region->unk_a);
        if (rc != SF_OK) return rc;
    }
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, base2_off, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i16(w, region->unk_b_count); if (rc != SF_OK) return rc;
    if (region->unk_b_count > 0) {
        rc = sf_binary_writer_write_i16s(w, (size_t)region->unk_b_count, region->unk_b);
        if (rc != SF_OK) return rc;
    }
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    if (msbs_region_shape_has_data(region->shape_type)) {
        rc = sf_binary_writer_fill_i64(w, shape_off, sf_binary_writer_position(w) - start);
        if (rc != SF_OK) return rc;
        rc = msbs_region_write_shape_data(w, region); if (rc != SF_OK) return rc;
    } else {
        rc = sf_binary_writer_fill_i64(w, shape_off, 0); if (rc != SF_OK) return rc;
    }

    rc = sf_binary_writer_fill_i64(w, base3_off, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, region->activation_part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, region->entity_id); if (rc != SF_OK) return rc;

    if (msbs_region_has_type_data(region->type)) {
        if (region->type == MSBS_REGION_SOUND_SPACE_OVERRIDE ||
            region->type == MSBS_REGION_PARTS_GROUP_AREA ||
            region->type == MSBS_REGION_AUTO_DRAW_GROUP_POINT) {
            rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
        }
        rc = sf_binary_writer_fill_i64(w, type_off, sf_binary_writer_position(w) - start);
        if (rc != SF_OK) return rc;
        rc = msbs_region_write_type_data(w, region); if (rc != SF_OK) return rc;
    } else {
        rc = sf_binary_writer_fill_i64(w, type_off, 0); if (rc != SF_OK) return rc;
    }
    return sf_binary_writer_pad(w, 8);
}

sf_result_t msbs_point_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    if (!w || !msbs) return SF_ERR_INVALID_ARG;
    if (msbs->region_count < 0) return SF_ERR_OUT_OF_RANGE;

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 35); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbs->region_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsNameOff2"); if (rc != SF_OK) return rc;

    for (int32_t i = 0; i < msbs->region_count; i++) {
        char offset_name[32];
        snprintf(offset_name, sizeof offset_name, "RegionOffset%d", i);
        rc = sf_binary_writer_reserve_i64(w, offset_name); if (rc != SF_OK) return rc;
    }
    rc = sf_binary_writer_reserve_i64(w, "MsbsNextList2"); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, "MsbsNameOff2", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "POINT_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    for (int32_t i = 0; i < msbs->region_count; i++) {
        char offset_name[32];
        snprintf(offset_name, sizeof offset_name, "RegionOffset%d", i);
        rc = sf_binary_writer_fill_i64(w, offset_name, sf_binary_writer_position(w));
        if (rc != SF_OK) return rc;
        rc = msbs_region_write_one(w, &msbs->regions[i].data, i);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}
