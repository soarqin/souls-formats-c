/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — Elden Ring MSBE root dispatcher shell.
 */

#include "map/msb_internal.h"

#include "souls_formats/sf_io.h"
#include "souls_formats/sf_msbe.h"

#include "unity.h"

#include <stdint.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct msbe_test_list_spec {
    const char *name;
    int32_t     version;
} msbe_test_list_spec_t;

static const msbe_test_list_spec_t k_empty_msbe_lists[] = {
    { "MODEL_PARAM_ST", 73 },
    { "EVENT_PARAM_ST", 73 },
    { "POINT_PARAM_ST", 73 },
    { "ROUTE_PARAM_ST", 73 },
    { "LAYER_PARAM_ST", 0x49 },
    { "PARTS_PARAM_ST", 73 },
};

#define MSBE_TEST_LIST_COUNT \
    ((int)(sizeof k_empty_msbe_lists / sizeof k_empty_msbe_lists[0]))

static sf_result_t write_empty_param(sf_binary_writer_t *w, const char *name,
                                     int32_t version, int reserve_id) {
    char next_name[32];
    char name_offset_name[32];
    snprintf(next_name, sizeof next_name, "MsbeSmokeNext%d", reserve_id);
    snprintf(name_offset_name, sizeof name_offset_name, "MsbeSmokeName%d", reserve_id);

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
    snprintf(next_name, sizeof next_name, "MsbeSmokeNext%d", reserve_id);
    return sf_binary_writer_fill_i64(w, next_name, offset);
}

static void produce_empty_msbe(uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));
    for (int i = 0; i < MSBE_TEST_LIST_COUNT; i++) {
        int64_t list_start = sf_binary_writer_position(w);
        if (i > 0) {
            TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, i - 1, list_start));
        }

        TEST_ASSERT_EQUAL(SF_OK,
                          write_empty_param(w, k_empty_msbe_lists[i].name,
                                            k_empty_msbe_lists[i].version, i));
    }
    TEST_ASSERT_EQUAL(SF_OK, fill_next_param(w, MSBE_TEST_LIST_COUNT - 1, 0));

    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, out_bytes, out_size));
    sf_ostream_close(s);
}

static void assert_empty_counts(const sf_msbe_t *msbe) {
    TEST_ASSERT_NOT_NULL(msbe);
    TEST_ASSERT_EQUAL_INT32(0, sf_msbe_model_count(msbe));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbe_event_count(msbe));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbe_region_count(msbe));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbe_route_count(msbe));
    TEST_ASSERT_EQUAL_INT32(0, sf_msbe_part_count(msbe));
    TEST_ASSERT_NULL(sf_msbe_model_at(msbe, 0));
    TEST_ASSERT_NULL(sf_msbe_event_at(msbe, 0));
    TEST_ASSERT_NULL(sf_msbe_region_at(msbe, 0));
    TEST_ASSERT_NULL(sf_msbe_route_at(msbe, 0));
    TEST_ASSERT_NULL(sf_msbe_part_at(msbe, 0));
}

static void test_empty_msbe_roundtrip(void) {
    uint8_t *bytes = NULL;
    size_t size = 0;
    produce_empty_msbe(&bytes, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_size_t(16, size);
    TEST_ASSERT_EQUAL_MEMORY("MSB ", bytes, 4);

    sf_msbe_t *msbe = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&msbe, bytes, size, NULL));
    assert_empty_counts(msbe);

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_write_to_memory(msbe, &written, &written_size, NULL));
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_EQUAL_size_t(size, written_size);

    sf_msbe_t *roundtripped = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_msbe_read_from_memory(&roundtripped, written, written_size, NULL));
    assert_empty_counts(roundtripped);

    sf_msbe_destroy(roundtripped);
    sf_msbe_destroy(msbe);
    sf_free(NULL, written);
    sf_free(NULL, bytes);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_msbe_roundtrip);
    return UNITY_END();
}
