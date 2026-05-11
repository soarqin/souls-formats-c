/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Sekiro MSBS RouteParam synthetic round-trips.
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

typedef struct route_case {
    uint32_t    type;
    const char *name;
    int32_t     unk08;
    int32_t     unk0c;
} route_case_t;

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

static sf_result_t write_route_entry(sf_binary_writer_t *w, const route_case_t *tc, int32_t id) {
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, "RouteName"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, tc->unk08); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, tc->unk0c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, tc->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_pattern(w, 0x68, 0x00); if (rc != SF_OK) return rc;

    rc = sf_binary_writer_fill_i64(w, "RouteName", sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, tc->name, true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t write_route_param(sf_binary_writer_t *w, const route_case_t *tc) {
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 35); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 2); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsNameOff3"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "RouteEntry"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbsNextList3"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "MsbsNameOff3", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "ROUTE_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "RouteEntry", sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    return write_route_entry(w, tc, 0);
}

static void produce_fixture(const route_case_t *tc, uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));
    for (int i = 0; i < 8; i++) {
        int64_t list_start = sf_binary_writer_position(w);
        if (i > 0) TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, i - 1, list_start));

        if (i == 3) {
            TEST_ASSERT_EQUAL(SF_OK, write_route_param(w, tc));
        } else {
            TEST_ASSERT_EQUAL(SF_OK, write_empty_param(w, k_lists[i].name, k_lists[i].version, i));
        }
    }
    TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, 7, 0));
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, out_bytes, out_size));
    sf_ostream_close(s);
}

static void assert_case_round_trips(const route_case_t *tc) {
    uint8_t *bytes = NULL;
    size_t size = 0;
    produce_fixture(tc, &bytes, &size);

    sf_msbs_t *msbs = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&msbs, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(msbs);
    TEST_ASSERT_EQUAL_INT32(1, sf_msbs_route_count(msbs));

    const sf_msbs_route_t *route = sf_msbs_route_at(msbs, 0);
    TEST_ASSERT_NOT_NULL(route);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(msbs, &written, &written_size, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, size);

    sf_msbs_destroy(msbs);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

static void test_muffling_portal_link_round_trip(void) {
    const route_case_t tc = { 3, "MufflingPortalRoute0", 11, 22 };
    assert_case_round_trips(&tc);
}

static void test_muffling_box_link_round_trip(void) {
    const route_case_t tc = { 4, "MufflingBoxRoute0", 33, 44 };
    assert_case_round_trips(&tc);
}

static void test_empty_route_param_round_trips(void) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));
    for (int i = 0; i < 8; i++) {
        int64_t list_start = sf_binary_writer_position(w);
        if (i > 0) TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, i - 1, list_start));
        TEST_ASSERT_EQUAL(SF_OK,
            write_empty_param(w, k_lists[i].name, k_lists[i].version, i));
    }
    TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, 7, 0));

    uint8_t *bytes = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, &bytes, &size));
    sf_ostream_close(s);

    sf_msbs_t *msbs = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_read_from_memory(&msbs, bytes, size, NULL));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbs_route_count(msbs));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbs_write_to_memory(msbs, &written, &written_size, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, written_size);
    TEST_ASSERT_EQUAL_MEMORY(bytes, written, size);

    sf_msbs_destroy(msbs);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_muffling_portal_link_round_trip);
    RUN_TEST(test_muffling_box_link_round_trip);
    RUN_TEST(test_empty_route_param_round_trips);
    return UNITY_END();
}
