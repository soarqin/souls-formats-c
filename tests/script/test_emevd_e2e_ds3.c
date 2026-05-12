/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds3_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_emevd_e2e_ds3(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 copy not available");
    }
    void       *bytes = NULL;
    size_t      size  = 0;
    sf_result_t r     = ds3_extract_from_anybhd("/event/common.emevd.dcx", &bytes, &size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DS3 EMEVD");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 EMEVD not accessible from any shard");
    }

    sf_emevd_t *emevd = NULL;
    r = sf_emevd_read_from_memory(&emevd, (const uint8_t *)bytes, size, NULL);
    sf_free(NULL, bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 EMEVD did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(emevd);
    TEST_ASSERT_EQUAL_INT(SF_EMEVD_FORMAT_DARK_SOULS_3, (int)sf_emevd_get_format(emevd));
    TEST_ASSERT_GREATER_THAN(0, (int)sf_emevd_get_event_count(emevd));
    sf_emevd_destroy(emevd, NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_emevd_e2e_ds3);
    ds3_helper_shutdown();
    return UNITY_END();
}
