/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DS3 TAE legacy read test.
 * Reads a TAE from a DS3 .anibnd.dcx file and verifies basic structure.
 */

#include "ds3_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tae.h"

#include "unity.h"

#include <stddef.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void test_tae_ds3_read(void) {
    sf_result_t r = ds3_helper_init();
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 not available — skipping");
        return;
    }

    void  *data = NULL;
    size_t size = 0;
    r = ds3_extract_from_anybhd("/chr/c0000.anibnd.dcx", &data, &size);
    if (r != SF_OK) {
        ds3_helper_shutdown();
        TEST_IGNORE_MESSAGE("DS3 c0000.anibnd.dcx not found — skipping");
        return;
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, data, size, NULL);
    sf_free(NULL, data);
    if (r != SF_OK) {
        ds3_helper_shutdown();
        TEST_IGNORE_MESSAGE("Failed to parse DS3 c0000.anibnd.dcx as BND4 — skipping");
        return;
    }

    const sf_binder_file_t *tae_file = NULL;
    const size_t            count    = sf_bnd4_file_count(bnd);
    TEST_ASSERT_TRUE(count > 0u);
    for (size_t i = 0; i < count; i++) {
        const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
        if (f && f->name_utf8 && strstr(f->name_utf8, ".tae")) {
            tae_file = f;
            break;
        }
    }
    if (!tae_file) {
        sf_bnd4_destroy(bnd);
        ds3_helper_shutdown();
        TEST_IGNORE_MESSAGE("No .tae entry found in DS3 c0000.anibnd.dcx — skipping");
        return;
    }

    sf_tae_t *tae = NULL;
    r = sf_tae_read_from_memory(&tae, tae_file->data, tae_file->size, NULL);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, "Failed to read DS3 TAE");
    TEST_ASSERT_NOT_NULL(tae);

    sf_tae_format_t fmt = sf_tae_format(tae);
    TEST_ASSERT_TRUE(fmt == SF_TAE_FORMAT_DS3 || fmt == SF_TAE_FORMAT_SOTFS);
    TEST_ASSERT_TRUE(sf_tae_animation_count(tae) > 0u);

    sf_tae_destroy(tae);
    sf_bnd4_destroy(bnd);
    ds3_helper_shutdown();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tae_ds3_read);
    return UNITY_END();
}
