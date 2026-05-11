/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Sekiro MSBS PointParam synthetic round-trips.
 */

#include "map/msbs/msbs_internal.h"

#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const msbs_region_type_t k_region_types[] = {
    MSBS_REGION_INVASION_POINT,
    MSBS_REGION_ENVIRONMENT_MAP_POINT,
    MSBS_REGION_SOUND,
    MSBS_REGION_SFX,
    MSBS_REGION_WIND_SFX,
    MSBS_REGION_SPAWN_POINT,
    MSBS_REGION_PATROL_ROUTE,
    MSBS_REGION_WARP_POINT,
    MSBS_REGION_ACTIVATION_AREA,
    MSBS_REGION_EVENT,
    MSBS_REGION_LOGIC,
    MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX,
    MSBS_REGION_WIND_AREA,
    MSBS_REGION_MUFFLING_BOX,
    MSBS_REGION_MUFFLING_PORTAL,
    MSBS_REGION_SOUND_SPACE_OVERRIDE,
    MSBS_REGION_MUFFLING_PLANE,
    MSBS_REGION_PARTS_GROUP_AREA,
    MSBS_REGION_AUTO_DRAW_GROUP_POINT,
    MSBS_REGION_OTHER,
};

static const char *k_region_names[] = {
    "Region: InvasionPoint",
    "Region: EnvironmentMapPoint",
    "Region: Sound",
    "Region: SFX",
    "Region: WindSFX",
    "Region: SpawnPoint",
    "Region: PatrolRoute",
    "Region: WarpPoint",
    "Region: ActivationArea",
    "Region: Event",
    "Region: Logic",
    "Region: EnvironmentMapEffectBox",
    "Region: WindArea",
    "Region: MufflingBox",
    "Region: MufflingPortal",
    "Region: SoundSpaceOverride",
    "Region: MufflingPlane",
    "Region: PartsGroupArea",
    "Region: AutoDrawGroupPoint",
    "Region: Other",
};

static void fill_shape(msbs_region_t *region, int index) {
    region->shape_type = (msbs_region_shape_type_t)(index % 7);
    switch (region->shape_type) {
    case MSBS_REGION_SHAPE_CIRCLE:
        region->shape.circle.radius = 1.25f + (float)index;
        break;
    case MSBS_REGION_SHAPE_SPHERE:
        region->shape.sphere.radius = 2.25f + (float)index;
        break;
    case MSBS_REGION_SHAPE_CYLINDER:
        region->shape.cylinder.radius = 3.25f + (float)index;
        region->shape.cylinder.height = 4.25f + (float)index;
        break;
    case MSBS_REGION_SHAPE_RECTANGLE:
        region->shape.rectangle.width = 5.25f + (float)index;
        region->shape.rectangle.depth = 6.25f + (float)index;
        break;
    case MSBS_REGION_SHAPE_BOX:
        region->shape.box.width = 7.25f + (float)index;
        region->shape.box.depth = 8.25f + (float)index;
        region->shape.box.height = 9.25f + (float)index;
        break;
    case MSBS_REGION_SHAPE_COMPOSITE:
        for (int i = 0; i < 8; i++) {
            region->shape.composite.children[i].region_index = i - 1;
            region->shape.composite.children[i].unk04 = 100 + index + i;
        }
        break;
    default:
        break;
    }
}

static void fill_type_data(msbs_region_t *region, int index) {
    switch (region->type) {
    case MSBS_REGION_INVASION_POINT:
        region->u.invasion_point.priority = 10 + index;
        break;
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
        region->u.environment_map_point.unk_t00 = 1.5f;
        region->u.environment_map_point.unk_t04 = 101;
        region->u.environment_map_point.unk_t0c = 102;
        region->u.environment_map_point.unk_t10 = 10.5f;
        region->u.environment_map_point.unk_t14 = 14.5f;
        region->u.environment_map_point.unk_t18 = 118;
        region->u.environment_map_point.unk_t1c = 128;
        region->u.environment_map_point.unk_t20 = 132;
        region->u.environment_map_point.unk_t24 = 136;
        region->u.environment_map_point.unk_t28 = 140;
        break;
    case MSBS_REGION_SOUND:
        region->u.sound.sound_type = 2;
        region->u.sound.sound_id = 400020;
        for (int i = 0; i < 16; i++) region->u.sound.child_region_indices[i] = i - 1;
        region->u.sound.unk_t48 = 48;
        break;
    case MSBS_REGION_SFX:
        region->u.sfx.effect_id = 9000;
        region->u.sfx.unk_t04 = 44;
        region->u.sfx.start_disabled = 1;
        break;
    case MSBS_REGION_WIND_SFX:
        region->u.wind_sfx.effect_id = 9100;
        region->u.wind_sfx.wind_area_index = 12;
        region->u.wind_sfx.unk_t18 = 18.75f;
        break;
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
        region->u.environment_map_effect_box.unk_t00 = 0.75f;
        region->u.environment_map_effect_box.compare = 2.5f;
        region->u.environment_map_effect_box.unk_t08 = 8;
        region->u.environment_map_effect_box.unk_t09 = 9;
        region->u.environment_map_effect_box.unk_t0a = 10;
        region->u.environment_map_effect_box.unk_t24 = 24;
        region->u.environment_map_effect_box.unk_t28 = 28.5f;
        region->u.environment_map_effect_box.unk_t2c = 44.5f;
        break;
    case MSBS_REGION_MUFFLING_BOX:
        region->u.muffling_box.unk_t00 = 200;
        break;
    case MSBS_REGION_MUFFLING_PORTAL:
        region->u.muffling_portal.unk_t00 = 300;
        break;
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
        region->u.sound_space_override.unk_t00 = 3;
        region->u.sound_space_override.unk_t01 = 4;
        break;
    case MSBS_REGION_PARTS_GROUP_AREA:
        region->u.parts_group_area.unk_t00 = 0x1122334455667788LL;
        break;
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
        region->u.auto_draw_group_point.unk_t00 = 0x2233445566778899LL;
        break;
    default:
        break;
    }
}

static void init_region(sf_msbs_region_t *slot, int index) {
    msbs_region_t *region = &slot->data;
    memset(region, 0, sizeof(*region));
    region->type = k_region_types[index];
    region->name = (char *)k_region_names[index];
    region->position = (sf_vec3_t){ 1.0f + (float)index, 2.0f + (float)index, 3.0f + (float)index };
    region->rotation = (sf_vec3_t){ 4.0f + (float)index, 5.0f + (float)index, 6.0f + (float)index };
    region->unk2c = 200 + index;
    region->map_studio_layer = 0xA0000000u + (uint32_t)index;
    region->activation_part_index = index - 2;
    region->entity_id = 100000 + index;
    fill_shape(region, index);
    fill_type_data(region, index);
}

static void assert_vec3_equal(sf_vec3_t expected, sf_vec3_t actual) {
    TEST_ASSERT_EQUAL_FLOAT(expected.x, actual.x);
    TEST_ASSERT_EQUAL_FLOAT(expected.y, actual.y);
    TEST_ASSERT_EQUAL_FLOAT(expected.z, actual.z);
}

static void assert_shape_equal(const msbs_region_t *expected, const msbs_region_t *actual) {
    TEST_ASSERT_EQUAL_INT(expected->shape_type, actual->shape_type);
    switch (expected->shape_type) {
    case MSBS_REGION_SHAPE_CIRCLE:
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.circle.radius, actual->shape.circle.radius);
        break;
    case MSBS_REGION_SHAPE_SPHERE:
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.sphere.radius, actual->shape.sphere.radius);
        break;
    case MSBS_REGION_SHAPE_CYLINDER:
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.cylinder.radius, actual->shape.cylinder.radius);
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.cylinder.height, actual->shape.cylinder.height);
        break;
    case MSBS_REGION_SHAPE_RECTANGLE:
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.rectangle.width, actual->shape.rectangle.width);
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.rectangle.depth, actual->shape.rectangle.depth);
        break;
    case MSBS_REGION_SHAPE_BOX:
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.box.width, actual->shape.box.width);
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.box.depth, actual->shape.box.depth);
        TEST_ASSERT_EQUAL_FLOAT(expected->shape.box.height, actual->shape.box.height);
        break;
    case MSBS_REGION_SHAPE_COMPOSITE:
        for (int i = 0; i < 8; i++) {
            TEST_ASSERT_EQUAL_INT32(expected->shape.composite.children[i].region_index,
                                    actual->shape.composite.children[i].region_index);
            TEST_ASSERT_EQUAL_INT32(expected->shape.composite.children[i].unk04,
                                    actual->shape.composite.children[i].unk04);
        }
        break;
    default:
        break;
    }
}

static void assert_type_data_equal(const msbs_region_t *expected, const msbs_region_t *actual) {
    switch (expected->type) {
    case MSBS_REGION_INVASION_POINT:
        TEST_ASSERT_EQUAL_INT32(expected->u.invasion_point.priority, actual->u.invasion_point.priority);
        break;
    case MSBS_REGION_ENVIRONMENT_MAP_POINT:
        TEST_ASSERT_EQUAL_MEMORY(&expected->u.environment_map_point, &actual->u.environment_map_point,
                                 sizeof(expected->u.environment_map_point));
        break;
    case MSBS_REGION_SOUND:
        TEST_ASSERT_EQUAL_MEMORY(&expected->u.sound, &actual->u.sound, sizeof(expected->u.sound));
        break;
    case MSBS_REGION_SFX:
        TEST_ASSERT_EQUAL_MEMORY(&expected->u.sfx, &actual->u.sfx, sizeof(expected->u.sfx));
        break;
    case MSBS_REGION_WIND_SFX:
        TEST_ASSERT_EQUAL_INT32(expected->u.wind_sfx.effect_id, actual->u.wind_sfx.effect_id);
        TEST_ASSERT_EQUAL_INT32(expected->u.wind_sfx.wind_area_index, actual->u.wind_sfx.wind_area_index);
        TEST_ASSERT_EQUAL_FLOAT(expected->u.wind_sfx.unk_t18, actual->u.wind_sfx.unk_t18);
        break;
    case MSBS_REGION_ENVIRONMENT_MAP_EFFECT_BOX:
        TEST_ASSERT_EQUAL_FLOAT(expected->u.environment_map_effect_box.unk_t00,
                                actual->u.environment_map_effect_box.unk_t00);
        TEST_ASSERT_EQUAL_FLOAT(expected->u.environment_map_effect_box.compare,
                                actual->u.environment_map_effect_box.compare);
        TEST_ASSERT_EQUAL_UINT8(expected->u.environment_map_effect_box.unk_t08,
                                actual->u.environment_map_effect_box.unk_t08);
        TEST_ASSERT_EQUAL_UINT8(expected->u.environment_map_effect_box.unk_t09,
                                actual->u.environment_map_effect_box.unk_t09);
        TEST_ASSERT_EQUAL_INT16(expected->u.environment_map_effect_box.unk_t0a,
                                actual->u.environment_map_effect_box.unk_t0a);
        TEST_ASSERT_EQUAL_INT32(expected->u.environment_map_effect_box.unk_t24,
                                actual->u.environment_map_effect_box.unk_t24);
        TEST_ASSERT_EQUAL_FLOAT(expected->u.environment_map_effect_box.unk_t28,
                                actual->u.environment_map_effect_box.unk_t28);
        TEST_ASSERT_EQUAL_FLOAT(expected->u.environment_map_effect_box.unk_t2c,
                                actual->u.environment_map_effect_box.unk_t2c);
        break;
    case MSBS_REGION_MUFFLING_BOX:
        TEST_ASSERT_EQUAL_INT32(expected->u.muffling_box.unk_t00, actual->u.muffling_box.unk_t00);
        break;
    case MSBS_REGION_MUFFLING_PORTAL:
        TEST_ASSERT_EQUAL_INT32(expected->u.muffling_portal.unk_t00, actual->u.muffling_portal.unk_t00);
        break;
    case MSBS_REGION_SOUND_SPACE_OVERRIDE:
        TEST_ASSERT_EQUAL_UINT8(expected->u.sound_space_override.unk_t00,
                                actual->u.sound_space_override.unk_t00);
        TEST_ASSERT_EQUAL_UINT8(expected->u.sound_space_override.unk_t01,
                                actual->u.sound_space_override.unk_t01);
        break;
    case MSBS_REGION_PARTS_GROUP_AREA:
        TEST_ASSERT_EQUAL_INT64(expected->u.parts_group_area.unk_t00, actual->u.parts_group_area.unk_t00);
        break;
    case MSBS_REGION_AUTO_DRAW_GROUP_POINT:
        TEST_ASSERT_EQUAL_INT64(expected->u.auto_draw_group_point.unk_t00,
                                actual->u.auto_draw_group_point.unk_t00);
        break;
    default:
        break;
    }
}

static void assert_region_equal(const msbs_region_t *expected, const msbs_region_t *actual) {
    TEST_ASSERT_EQUAL_INT(expected->type, actual->type);
    TEST_ASSERT_EQUAL_STRING(expected->name, actual->name);
    assert_vec3_equal(expected->position, actual->position);
    assert_vec3_equal(expected->rotation, actual->rotation);
    TEST_ASSERT_EQUAL_INT32(expected->unk2c, actual->unk2c);
    TEST_ASSERT_EQUAL_UINT32(expected->map_studio_layer, actual->map_studio_layer);
    TEST_ASSERT_EQUAL_INT16(0, actual->unk_a_count);
    TEST_ASSERT_EQUAL_INT16(0, actual->unk_b_count);
    TEST_ASSERT_EQUAL_INT32(expected->activation_part_index, actual->activation_part_index);
    TEST_ASSERT_EQUAL_INT32(expected->entity_id, actual->entity_id);
    assert_shape_equal(expected, actual);
    assert_type_data_equal(expected, actual);
}

static void test_msbs_point_param_roundtrips_all_sekiro_region_subtypes(void) {
    sf_msbs_region_t regions[sizeof(k_region_types) / sizeof(k_region_types[0])];
    for (int i = 0; i < (int)(sizeof(k_region_types) / sizeof(k_region_types[0])); i++) {
        init_region(&regions[i], i);
    }

    sf_msbs_t msbs = { 0 };
    msbs.regions = regions;
    msbs.region_count = (int32_t)(sizeof(k_region_types) / sizeof(k_region_types[0]));

    uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_msbs_write_to_memory(&msbs, &data, &size, NULL));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_GREATER_THAN(0, size);

    sf_msbs_t *read_msbs = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_msbs_read_from_memory(&read_msbs, data, size, NULL));
    TEST_ASSERT_NOT_NULL(read_msbs);
    TEST_ASSERT_EQUAL_INT32(msbs.region_count, sf_msbs_region_count(read_msbs));

    for (int32_t i = 0; i < msbs.region_count; i++) {
        const sf_msbs_region_t *actual = sf_msbs_region_at(read_msbs, i);
        TEST_ASSERT_NOT_NULL(actual);
        assert_region_equal(&regions[i].data, &actual->data);
    }

    sf_msbs_destroy(read_msbs);
    sf_free(NULL, data);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_msbs_point_param_roundtrips_all_sekiro_region_subtypes);
    return UNITY_END();
}
