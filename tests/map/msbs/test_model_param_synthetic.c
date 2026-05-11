/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Sekiro MSBS ModelParam synthetic round-trips.
 */

#include "map/msb_internal.h"

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msb.h"
#include "souls_formats/sf_msbs.h"

#include "unity.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct list_spec {
    const char *name;
    int32_t     version;
} list_spec_t;

static const list_spec_t k_lists[] = {
    { "MODEL_PARAM_ST", 35 },
    { "EVENT_PARAM_ST", 35 },
    { "POINT_PARAM_ST", 35 },
    { "ROUTE_PARAM_ST", 35 },
    { "LAYER_PARAM_ST", 0x23 },
    { "PARTS_PARAM_ST", 35 },
    { "MAPSTUDIO_PARTS_POSE_ST", 0 },
    { "MAPSTUDIO_BONE_NAME_STRING", 0 },
};

typedef struct model_case {
    uint32_t            type;
    sf_msb_model_kind_t kind;
    const char         *name;
    const char         *sib;
} model_case_t;

static sf_result_t write_empty_param(sf_binary_writer_t *w, const char *name,
                                     int32_t version, int reserve_id) {
    char next_name[32];
    char name_offset_name[32];
    snprintf(next_name, sizeof next_name, "MsbsNextList%d", reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbsNameOff%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, version); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, name_offset_name); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, next_name); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, name_offset_name, sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, name, true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbsNextList%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

static sf_result_t write_model_entry(sf_binary_writer_t *w, const model_case_t *tc) {
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, "ModelName"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, tc->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "ModelSib"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 7); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 11); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "ModelTypeData"); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, "ModelName", sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, tc->name, true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "ModelSib", sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, tc->sib, true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;

    if (tc->type == 0) {
        rc = sf_binary_writer_fill_i64(w, "ModelTypeData", sf_binary_writer_position(w) - start);
        if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, true); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, false); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_bool(w, true); if (rc != SF_OK) return rc;
        rc = sf_binary_writer_write_u8(w, 0); if (rc != SF_OK) return rc;
        for (int i = 0; i < 6; i++) {
            rc = sf_binary_writer_write_f32(w, (float)(i + 1));
            if (rc != SF_OK) return rc;
        }
        return sf_binary_writer_write_i32(w, 0);
    }
    return sf_binary_writer_fill_i64(w, "ModelTypeData", 0);
}

static sf_result_t write_model_param(sf_binary_writer_t *w, const model_case_t *tc) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 35); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 2); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsNameOff0"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "ModelEntry"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsNextList0"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "MsbsNameOff0", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "MODEL_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "ModelEntry", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    return write_model_entry(w, tc);
}

static void produce_fixture(const model_case_t *tc, uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));
    for (int i = 0; i < 8; i++) {
        int64_t list_start = sf_binary_writer_position(w);
        if (i > 0) TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, i - 1, list_start));

        if (i == 0) {
            TEST_ASSERT_EQUAL(SF_OK, write_model_param(w, tc));
        } else {
            TEST_ASSERT_EQUAL(SF_OK, write_empty_param(w, k_lists[i].name, k_lists[i].version, i));
        }
    }
    TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, 7, 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, out_bytes, out_size));
    sf_ostream_close(s);
}

static void assert_case_round_trips(const model_case_t *tc) {
    uint8_t *bytes = NULL;
    size_t size = 0;
    produce_fixture(tc, &bytes, &size);

    sf_msbs_t *msbs = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&msbs, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(msbs);
    TEST_ASSERT_EQUAL_INT32(1, sf_msbs_model_count(msbs));

    const sf_msbs_model_t *model = sf_msbs_model_at(msbs, 0);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL(tc->kind, sf_msb_model_get_kind((const sf_msb_model_t *)model));
    char *name = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msb_model_get_name((const sf_msb_model_t *)model, &name));
    TEST_ASSERT_EQUAL_STRING(tc->name, name);
    sf_free(NULL, name);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(msbs, &written, &written_size, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, size);

    sf_msbs_destroy(msbs);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

static void test_map_piece_round_trip(void) {
    const model_case_t tc = { 0, SF_MSB_MODEL_MAP_PIECE, "m100000", "N:\\m100000.sib" };
    assert_case_round_trips(&tc);
}

static void test_object_round_trip(void) {
    const model_case_t tc = { 1, SF_MSB_MODEL_OBJECT, "o100000", "N:\\o100000.sib" };
    assert_case_round_trips(&tc);
}

static void test_enemy_round_trip(void) {
    const model_case_t tc = { 2, SF_MSB_MODEL_CHARACTER, "c1000", "N:\\c1000.sib" };
    assert_case_round_trips(&tc);
}

static void test_player_round_trip(void) {
    const model_case_t tc = { 4, SF_MSB_MODEL_PLAYER, "c0000", "N:\\c0000.sib" };
    assert_case_round_trips(&tc);
}

static void test_collision_round_trip(void) {
    const model_case_t tc = { 5, SF_MSB_MODEL_COLLISION, "h100000", "N:\\h100000.sib" };
    assert_case_round_trips(&tc);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_map_piece_round_trip);
    RUN_TEST(test_object_round_trip);
    RUN_TEST(test_enemy_round_trip);
    RUN_TEST(test_player_round_trip);
    RUN_TEST(test_collision_round_trip);
    return UNITY_END();
}
