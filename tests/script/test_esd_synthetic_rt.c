/* SPDX-License-Identifier: GPL-3.0-or-later */

#define setUp esd_read_setUp
#define tearDown esd_read_tearDown
#define main esd_read_fixture_main
int esd_read_fixture_main(void);
#include "test_esd_read.c"
#undef main
#undef tearDown
#undef setUp

#include "souls_formats/sf_io.h"

void setUp(void);
void tearDown(void);

void setUp(void) {}
void tearDown(void) {}

static void assert_counts_match(const sf_esd_t *expected, const sf_esd_t *actual) {
    TEST_ASSERT_EQUAL_INT32(sf_esd_get_state_group_count(expected), sf_esd_get_state_group_count(actual));
    for (int32_t g = 0; g < sf_esd_get_state_group_count(expected); g++) {
        int64_t group_id = 0;
        TEST_ASSERT_EQUAL(SF_OK, sf_esd_get_state_group_id(expected, g, &group_id));
        TEST_ASSERT_EQUAL_INT32(sf_esd_get_state_count(expected, group_id),
                                sf_esd_get_state_count(actual, group_id));
        for (int32_t s = 0; s < sf_esd_get_state_count(expected, group_id); s++) {
            const sf_esd_state_t *a = sf_esd_get_state(expected, group_id, s);
            const sf_esd_state_t *b = sf_esd_get_state(actual, group_id, s);
            TEST_ASSERT_NOT_NULL(a);
            TEST_ASSERT_NOT_NULL(b);
            TEST_ASSERT_EQUAL_INT64(sf_esd_state_get_id(a), sf_esd_state_get_id(b));
            TEST_ASSERT_EQUAL_INT32(sf_esd_state_get_condition_count(a),
                                    sf_esd_state_get_condition_count(b));
            TEST_ASSERT_EQUAL_INT32(sf_esd_state_get_entry_command_count(a),
                                    sf_esd_state_get_entry_command_count(b));
            TEST_ASSERT_EQUAL_INT32(sf_esd_state_get_exit_command_count(a),
                                    sf_esd_state_get_exit_command_count(b));
            TEST_ASSERT_EQUAL_INT32(sf_esd_state_get_while_command_count(a),
                                    sf_esd_state_get_while_command_count(b));
        }
    }
}

static void assert_round_trip(bool long_format, int32_t dark_souls_count) {
    fixture_buf_t fixture;
    build_fixture(&fixture, long_format, dark_souls_count);

    sf_esd_t *original = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_read_from_memory(&original, fixture.data, fixture.size, NULL));

    uint8_t *written = NULL;
    size_t written_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_write_to_memory(original, &written, &written_size, NULL));
    TEST_ASSERT_NOT_NULL(written);
    TEST_ASSERT_GREATER_THAN_size_t(0, written_size);

    sf_esd_t *actual = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_esd_read_from_memory(&actual, written, written_size, NULL));
    TEST_ASSERT_EQUAL(long_format, sf_esd_is_long_format(actual));
    TEST_ASSERT_EQUAL_INT32(dark_souls_count, sf_esd_get_format_version(actual));
    assert_counts_match(original, actual);

    sf_esd_destroy(actual);
    sf_free(NULL, written);
    sf_esd_destroy(original);
}

static void test_esd_synthetic_roundtrip(void) {
    assert_round_trip(false, 1);
    assert_round_trip(true, 3);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esd_synthetic_roundtrip);
    return UNITY_END();
}
