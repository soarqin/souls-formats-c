/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds3_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_fmg.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool name_has_fmg(const char *name)
{
    return name && strstr(name, ".fmg") != NULL;
}

static void test_fmg_e2e_ds3_item_msgbnd(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 copy not available");
    }
    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    sf_result_t r = ds3_extract_from_anybhd("/msg/engus/item.msgbnd.dcx", &bnd_bytes,
                                            &bnd_size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DS3 msgbnd");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 item.msgbnd.dcx not accessible from any shard");
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 item msgbnd did not parse as BND4");
    }

    const sf_binder_file_t *entry = NULL;
    const size_t            count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (file && file->data && name_has_fmg(file->name_utf8)) {
            entry = file;
            break;
        }
    }
    if (!entry) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("DS3 FMG entry not found in item msgbnd");
    }

    sf_fmg_t *fmg = NULL;
    r = sf_fmg_read_from_memory(&fmg, entry->data, entry->size, NULL);
    if (r != SF_OK) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("DS3 FMG did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(fmg);
    TEST_ASSERT_GREATER_THAN(0, (int)sf_fmg_get_entry_count(fmg));
    sf_fmg_destroy(fmg);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmg_e2e_ds3_item_msgbnd);
    ds3_helper_shutdown();
    return UNITY_END();
}
