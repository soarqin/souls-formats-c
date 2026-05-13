/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_luainfo.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_luainfo_is_function(void) {
    const uint8_t good[]   = {'L', 'U', 'A', 'I', 0x01, 0x00, 0x00, 0x00};
    const uint8_t bad[]    = {'X', 'Y', 'Z', 'W'};
    const uint8_t short_[] = {'L', 'U', 'A'};
    TEST_ASSERT_TRUE (sf_luainfo_is(good,   sizeof(good)));
    TEST_ASSERT_FALSE(sf_luainfo_is(bad,    sizeof(bad)));
    TEST_ASSERT_FALSE(sf_luainfo_is(short_, sizeof(short_)));
    TEST_ASSERT_FALSE(sf_luainfo_is(NULL,   0));
}

static void test_luainfo_create_destroy(void) {
    sf_luainfo_t *info = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luainfo_create(&info, false, false, NULL));
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_FALSE(sf_luainfo_big_endian(info));
    TEST_ASSERT_FALSE(sf_luainfo_long_format(info));
    TEST_ASSERT_EQUAL_size_t(0, sf_luainfo_goal_count(info));
    sf_luainfo_destroy(info);
}

static void assert_round_trip(bool big_endian, bool long_format) {
    sf_luainfo_t *src = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_create(&src, big_endian, long_format, NULL));

    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_add_goal(src, 100, "GoalAlpha", true, false, "AlphaInterrupt"));
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_add_goal(src, 200, "GoalBeta", false, true, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_luainfo_goal_count(src));

    uint8_t *bytes = NULL;
    size_t   size  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_write_to_memory(src, &bytes, &size, NULL));
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_UINT64(0, size);
    TEST_ASSERT_EQUAL_UINT64(0, size % 0x10u);
    TEST_ASSERT_TRUE(sf_luainfo_is(bytes, size));

    sf_luainfo_t *dst = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_read_from_memory(&dst, bytes, size, NULL));
    TEST_ASSERT_NOT_NULL(dst);
    TEST_ASSERT_EQUAL_INT(big_endian,  sf_luainfo_big_endian(dst));
    TEST_ASSERT_EQUAL_INT(long_format, sf_luainfo_long_format(dst));
    TEST_ASSERT_EQUAL_size_t(2, sf_luainfo_goal_count(dst));

    const sf_luainfo_goal_t *g0 = NULL;
    const sf_luainfo_goal_t *g1 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luainfo_get_goal(dst, 0, &g0));
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_luainfo_get_goal(dst, 1, &g1));

    TEST_ASSERT_EQUAL_INT32(100, g0->id);
    TEST_ASSERT_EQUAL_STRING("GoalAlpha", g0->name);
    TEST_ASSERT_TRUE (g0->battle_interrupt);
    TEST_ASSERT_FALSE(g0->logic_interrupt);
    TEST_ASSERT_NOT_NULL(g0->logic_interrupt_name);
    TEST_ASSERT_EQUAL_STRING("AlphaInterrupt", g0->logic_interrupt_name);

    TEST_ASSERT_EQUAL_INT32(200, g1->id);
    TEST_ASSERT_EQUAL_STRING("GoalBeta", g1->name);
    TEST_ASSERT_FALSE(g1->battle_interrupt);
    TEST_ASSERT_TRUE (g1->logic_interrupt);
    TEST_ASSERT_NULL(g1->logic_interrupt_name);

    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE,
        sf_luainfo_get_goal(dst, 2, &g0));

    uint8_t *bytes2 = NULL;
    size_t   size2  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK,
        sf_luainfo_write_to_memory(dst, &bytes2, &size2, NULL));
    TEST_ASSERT_EQUAL_UINT64(size, size2);
    TEST_ASSERT_EQUAL_MEMORY(bytes, bytes2, size);

    sf_free(NULL, bytes);
    sf_free(NULL, bytes2);
    sf_luainfo_destroy(src);
    sf_luainfo_destroy(dst);
}

static void test_luainfo_round_trip_short(void) { assert_round_trip(false, false); }
static void test_luainfo_round_trip_long (void) { assert_round_trip(false, true);  }

static void test_luainfo_bad_magic(void) {
    const uint8_t junk[] = {
        'J', 'U', 'N', 'K',
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    sf_luainfo_t *info = NULL;
    sf_result_t r = sf_luainfo_read_from_memory(&info, junk, sizeof(junk), NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC, r);
    TEST_ASSERT_NULL(info);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_luainfo_is_function);
    RUN_TEST(test_luainfo_create_destroy);
    RUN_TEST(test_luainfo_round_trip_short);
    RUN_TEST(test_luainfo_round_trip_long);
    RUN_TEST(test_luainfo_bad_magic);
    return UNITY_END();
}
