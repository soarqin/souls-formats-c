/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_drb.h"
#include "souls_formats/sf_common.h"

#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/* Minimal little-endian DRB blob assembled by hand from the upstream
 * block layout (DRB.cs ReadXxx helpers). Block layout:
 *   DRB\0 null (16) + STR\0 header (16) + STR data (20) +
 *   TEXI header (16) + TEXI entry (16) +
 *   five 16-byte blob blocks (SHPR/CTPR/ANIP/INTP/SCDP) +
 *   nine 16-byte block headers (SHAP..DLGO) +
 *   DLG\0 header (16) + END\0 null (16) = 340 bytes. */
static const uint8_t k_min_drb[] = {
    'D', 'R', 'B', 0,    0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,

    'S', 'T', 'R', 0,   20, 0, 0, 0,    2, 0, 0, 0,    0, 0, 0, 0,
    't', 0, 'e', 0, 'x', 0, '0', 0,   0, 0,
    'd', 0, 'l', 0, 'g', 0, '0', 0,   0, 0,

    'T', 'E', 'X', 'I',  16, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,
     0, 0, 0, 0,        10, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,

    'S', 'H', 'P', 'R',  0, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,
    'C', 'T', 'P', 'R',  0, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,
    'A', 'N', 'I', 'P',  0, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,
    'I', 'N', 'T', 'P',  0, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,
    'S', 'C', 'D', 'P',  0, 0, 0, 0,    1, 0, 0, 0,    0, 0, 0, 0,

    'S', 'H', 'A', 'P',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'C', 'T', 'R', 'L',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'A', 'N', 'I', 'K',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'A', 'N', 'I', 'O',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'A', 'N', 'I', 'M',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'S', 'C', 'D', 'K',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'S', 'C', 'D', 'O',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'S', 'C', 'D', 'L',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
    'D', 'L', 'G', 'O',  0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,

    'D', 'L', 'G', 0,    0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,

    'E', 'N', 'D', 0,    0, 0, 0, 0,    0, 0, 0, 0,    0, 0, 0, 0,
};

static void test_drb_create_destroy(void) {
    sf_drb_t *d = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_drb_create(&d, SF_DRB_VERSION_DARK_SOULS, false, NULL));
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL_INT(SF_DRB_VERSION_DARK_SOULS, sf_drb_version(d));
    TEST_ASSERT_FALSE(sf_drb_big_endian(d));
    TEST_ASSERT_EQUAL_size_t(0, sf_drb_texture_count(d));
    TEST_ASSERT_EQUAL_size_t(0, sf_drb_dlg_count(d));
    sf_drb_destroy(d);
}

static void test_drb_is_function(void) {
    const uint8_t valid_le[] = {'D', 'R', 'B', 0};
    const uint8_t valid_be[] = {0, 'B', 'R', 'D'};
    const uint8_t bad_magic[] = {'X', 'X', 'X', 'X'};
    const uint8_t too_short[] = {'D', 'R'};
    TEST_ASSERT_TRUE(sf_drb_is(valid_le, sizeof(valid_le)));
    TEST_ASSERT_TRUE(sf_drb_is(valid_be, sizeof(valid_be)));
    TEST_ASSERT_TRUE(sf_drb_is(k_min_drb, sizeof(k_min_drb)));
    TEST_ASSERT_FALSE(sf_drb_is(bad_magic, sizeof(bad_magic)));
    TEST_ASSERT_FALSE(sf_drb_is(too_short, sizeof(too_short)));
    TEST_ASSERT_FALSE(sf_drb_is(NULL, 0));
}

static void test_drb_read_minimal(void) {
    sf_drb_t *d = NULL;
    sf_result_t e = sf_drb_read_from_memory(&d, k_min_drb, sizeof(k_min_drb),
                                            SF_DRB_VERSION_DARK_SOULS, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, e);
    TEST_ASSERT_NOT_NULL(d);

    TEST_ASSERT_EQUAL_INT(SF_DRB_VERSION_DARK_SOULS, sf_drb_version(d));
    TEST_ASSERT_FALSE(sf_drb_big_endian(d));

    TEST_ASSERT_EQUAL_size_t(1, sf_drb_texture_count(d));
    const char *tex_name = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_drb_get_texture_name(d, 0, &tex_name));
    TEST_ASSERT_NOT_NULL(tex_name);
    TEST_ASSERT_EQUAL_STRING("tex0", tex_name);

    TEST_ASSERT_EQUAL_INT(SF_ERR_OUT_OF_RANGE,
                          sf_drb_get_texture_name(d, 99, &tex_name));

    TEST_ASSERT_EQUAL_size_t(0, sf_drb_dlg_count(d));

    sf_drb_destroy(d);
}

static void test_drb_read_rejects_bad_magic(void) {
    const uint8_t bogus[] = {'X', 'X', 'X', 'X', 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0};
    sf_drb_t *d = NULL;
    sf_result_t e = sf_drb_read_from_memory(&d, bogus, sizeof(bogus),
                                            SF_DRB_VERSION_DARK_SOULS, NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC, e);
    TEST_ASSERT_NULL(d);
}

static void test_drb_read_rejects_truncated(void) {
    const uint8_t too_short[] = {'D', 'R'};
    sf_drb_t *d = NULL;
    sf_result_t e = sf_drb_read_from_memory(&d, too_short, sizeof(too_short),
                                            SF_DRB_VERSION_DARK_SOULS, NULL);
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC, e);
    TEST_ASSERT_NULL(d);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_drb_create_destroy);
    RUN_TEST(test_drb_is_function);
    RUN_TEST(test_drb_read_minimal);
    RUN_TEST(test_drb_read_rejects_bad_magic);
    RUN_TEST(test_drb_read_rejects_truncated);
    return UNITY_END();
}
