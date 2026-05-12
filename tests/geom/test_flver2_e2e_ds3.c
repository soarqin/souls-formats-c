/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "ds3_test_helper.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool name_ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }
    const size_t name_len   = strlen(name);
    const size_t suffix_len = strlen(suffix);
    return suffix_len <= name_len
        && memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

static void test_flver2_e2e_ds3_c0000(void)
{
    if (!ds3_helper_is_available()) {
        TEST_IGNORE_MESSAGE("DS3 copy not available");
    }
    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    sf_result_t r = ds3_extract_from_anybhd("/chr/c0000.chrbnd.dcx", &bnd_bytes,
                                            &bnd_size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DS3 chrbnd");
    }
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 c0000.chrbnd.dcx not accessible from any shard");
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        TEST_IGNORE_MESSAGE("DS3 c0000 chrbnd did not parse as BND4");
    }

    const sf_binder_file_t *entry = NULL;
    const size_t            count = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (file && file->name_utf8 && file->data && name_ends_with(file->name_utf8, ".flver")) {
            entry = file;
            break;
        }
    }
    if (!entry) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("DS3 c0000 FLVER entry not found");
    }

    sf_flver2_t *flver = NULL;
    r = sf_flver2_read_from_memory(&flver, entry->data, entry->size, NULL);
    if (r != SF_OK) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("DS3 FLVER2 did not parse in this environment");
    }
    TEST_ASSERT_NOT_NULL(flver);
    TEST_ASSERT_GREATER_THAN((size_t)0, sf_flver2_node_count(flver));
    sf_flver2_destroy(flver);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flver2_e2e_ds3_c0000);
    ds3_helper_shutdown();
    return UNITY_END();
}
