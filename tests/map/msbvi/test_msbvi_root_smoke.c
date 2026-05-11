/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Armored Core VI MSBVI root dispatcher shell.
 *
 * Round-trips an empty MSBVI buffer through write -> read -> write -> read
 * and verifies all six segment counters (including the typed Layer param
 * unique to MSBVI vs. MSBE) report zero.
 */

#include "map/msb_internal.h"

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbvi.h"

#include "unity.h"

#include <stdint.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct msbvi_test_list_spec {
    const char *name;
    int32_t     version;
} msbvi_test_list_spec_t;

static const msbvi_test_list_spec_t k_empty_msbvi_lists[] = {
    { "MODEL_PARAM_ST", 52 },
    { "EVENT_PARAM_ST", 52 },
    { "POINT_PARAM_ST", 52 },
    { "ROUTE_PARAM_ST", 52 },
    { "LAYER_PARAM_ST", 52 },
    { "PARTS_PARAM_ST", 52 },
};

#define MSBVI_TEST_LIST_COUNT \
    ((int)(sizeof k_empty_msbvi_lists / sizeof k_empty_msbvi_lists[0]))

static sf_result_t write_empty_param(sf_binary_writer_t *w, const char *name,
                                     int32_t version, int reserve_id) {
    char next_name[32];
    char name_offset_name[32];
    snprintf(next_name, sizeof next_name, "MsbviSmokeNext%d", reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbviSmokeName%d", reserve_id);

    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, version); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 1);       if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, name_offset_name); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, next_name);        if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, name_offset_name, sf_binary_writer_position(w));
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, name, true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t fill_next_param(sf_binary_writer_t *w, int reserve_id, int64_t offset) {
    char next_name[32];
    snprintf(next_name, sizeof next_name, "MsbviSmokeNext%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

static void produce_empty_msbvi(uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));
    for (int i = 0; i < MSBVI_TEST_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(w);
        if (i > 0) {
            TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, i - 1, list_start));
        }

        TEST_ASSERT_EQUAL(SF_OK,
                          write_empty_param(w, k_empty_msbvi_lists[i].name,
                                            k_empty_msbvi_lists[i].version, i));
    }
    TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, MSBVI_TEST_LIST_COUNT - 1, 0));

    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, out_bytes, out_size));
    sf_ostream_close(s);
}

static void assert_empty_counts(const sf_msbvi_t *msbvi) {
    TEST_ASSERT_NOT_NULL(msbvi);
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_model_count(msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_event_count(msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_region_count(msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_route_count(msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_layer_count(msbvi));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbvi_part_count(msbvi));
    TEST_ASSERT_NULL(sf_msbvi_model_at(msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_event_at(msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_region_at(msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_route_at(msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_layer_at(msbvi, 0));
    TEST_ASSERT_NULL(sf_msbvi_part_at(msbvi, 0));
}

static void test_empty_msbvi_roundtrip(void) {
    uint8_t *bytes = NULL;
    size_t size = 0;
    produce_empty_msbvi(&bytes, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_size_t(16, size);
    TEST_ASSERT_EQUAL_MEMORY("MSB ", bytes, 4);

    sf_msbvi_t *msbvi = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbvi_read_from_memory(&msbvi, bytes, size, NULL));
    assert_empty_counts(msbvi);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbvi_write_to_memory(msbvi, &written, &written_size, NULL));
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_EQUAL_size_t(size, written_size);

    sf_msbvi_t *roundtripped = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbvi_read_from_memory(&roundtripped, written, written_size, NULL));
    assert_empty_counts(roundtripped);

    sf_msbvi_destroy(roundtripped);
    sf_msbvi_destroy(msbvi);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_msbvi_roundtrip);
    return UNITY_END();
}
