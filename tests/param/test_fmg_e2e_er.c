/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FMG e2e against a real Elden Ring ItemName.fmg extracted from Data0.
 */

#include "er_test_helper.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_fmg.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

static void test_fmg_itemname_e2e(void)
{
    if (!er_helper_is_available()) {
        TEST_IGNORE_MESSAGE("ER copy not available in this environment");
    }

    static const char *k_msgbnd_paths[] = {
        "/msg/engus/item.msgbnd.dcx",
        "/msg/engUS/item.msgbnd.dcx",
        "/msg/en-US/item.msgbnd.dcx",
        NULL,
    };
    static const int32_t k_test_ids[] = {
        1000000,
        1030000,
        2000000,
        3000000,
        10000,
    };

    void       *fmg_bytes = NULL;
    size_t      fmg_size  = 0;
    sf_result_t r         = SF_ERR_NOT_FOUND;

    for (size_t i = 0; k_msgbnd_paths[i] != NULL; ++i) {
        r = er_load_msgbnd_entry(k_msgbnd_paths[i], "ItemName.fmg", &fmg_bytes, &fmg_size,
                                 NULL);
        if (r == SF_OK) {
            break;
        }
        if (r == SF_ERR_OODLE_NOT_FOUND) {
            TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress ItemName.fmg");
        }
    }

    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("ER ItemName.fmg not accessible (Data0 unavailable)");
    }

    TEST_ASSERT_NOT_NULL(fmg_bytes);
    TEST_ASSERT_GREATER_THAN_size_t(1024u, fmg_size);

    sf_fmg_t *fmg = NULL;
    r = sf_fmg_read_from_memory(&fmg, (const uint8_t *)fmg_bytes, fmg_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(fmg);

    TEST_ASSERT_GREATER_THAN_size_t(100u, sf_fmg_get_entry_count(fmg));

    bool found_any = false;
    for (size_t i = 0; i < sizeof(k_test_ids) / sizeof(k_test_ids[0]); ++i) {
        const sf_fmg_entry_t *entry = sf_fmg_find_entry_by_id(fmg, k_test_ids[i]);
        if (entry != NULL && sf_fmg_entry_get_text(entry) != NULL) {
            found_any = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found_any);

    sf_fmg_destroy(fmg);
    sf_free(NULL, fmg_bytes);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fmg_itemname_e2e);
    return UNITY_END();
}
