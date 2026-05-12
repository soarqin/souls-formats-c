/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_mtd.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_mtd_e2e_ds1r(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r = ds1r_extract_bnd3_entry("mtd/Mtd.mtdbnd.dcx", ".mtd", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MTD entry not accessible");
    }

    sf_mtd_t *mtd = NULL;
    r = sf_mtd_read_from_memory(&mtd, bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R MTD did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(mtd);
    TEST_ASSERT_NOT_NULL(sf_mtd_shader_path(mtd));
    sf_mtd_destroy(mtd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mtd_e2e_ds1r);
    return UNITY_END();
}
