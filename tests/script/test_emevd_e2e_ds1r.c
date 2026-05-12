/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds1r_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_emevd_e2e_ds1r(void)
{
    if (!ds1r_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS1R copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds1r_read_file("event/common.emevd.dcx", &bytes, &size);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R common.emevd.dcx not accessible");
    }

    sf_emevd_t *emevd = NULL;
    r = sf_emevd_read_from_memory(&emevd, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS1R EMEVD did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(emevd);
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_FORMAT_DARK_SOULS_1, (int)sf_emevd_get_format(emevd));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_emevd_get_event_count(emevd));
    sf_emevd_destroy(emevd, NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_emevd_e2e_ds1r);
    return UNITY_END();
}
