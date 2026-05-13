/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DS1R TAE legacy read test.
 * Reads a TAE from a DS1R .anibnd file and verifies basic structure.
 */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tae.h"

#include "unity.h"

#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

static void test_tae_ds1r_read(void) {
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R not available — skipping");
        return;
    }

    void  *tae_bytes = NULL;
    size_t tae_size  = 0;
    sf_result_t r = ds1r_extract_bnd3_entry("/chr/c0000.anibnd", ".tae", &tae_bytes, &tae_size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R c0000.anibnd or .tae entry not found — skipping");
        return;
    }

    sf_tae_t *tae = NULL;
    r = sf_tae_read_from_memory(&tae, tae_bytes, tae_size, NULL);
    sf_free(NULL, tae_bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, "Failed to read DS1R TAE");
    TEST_ASSERT_NOT_NULL(tae);

    sf_tae_format_t fmt = sf_tae_format(tae);
    TEST_ASSERT_TRUE(fmt == SF_TAE_FORMAT_DS1 || fmt == SF_TAE_FORMAT_DES);
    TEST_ASSERT_TRUE(sf_tae_animation_count(tae) > 0u);

    sf_tae_destroy(tae);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tae_ds1r_read);
    return UNITY_END();
}
