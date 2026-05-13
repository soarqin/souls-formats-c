/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Post-v1 Wave 5 (T14) — BTL e2e: probe every available game archive
 * (ER, AC6, Nightreign, Sekiro) for a .btl entry, parse it through the
 * production sf_btl_read_from_memory path, and verify the
 * write→read round-trip preserves version and light count.
 *
 * The Wave-0 probe found 0 lighting files in any scanned Data0 archive;
 * non-Data0 shards are not in scope for v0.5.0. The test therefore
 * SKIPs gracefully with an informative message whenever none of the
 * candidate paths resolve.
 */

#include "ac6_test_helper.h"
#include "er_test_helper.h"
#include "nightreign_test_helper.h"
#include "sekiro_test_helper.h"

#include "souls_formats/sf_btl.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

typedef sf_result_t (*extract_fn_t)(const char *path, void **out, size_t *out_size);
typedef bool (*available_fn_t)(void);

typedef struct {
    const char        *game_name;
    available_fn_t     available;
    extract_fn_t       extract;
    const char *const *candidates;
} game_probe_t;

static const char *const k_er_paths[] = {
    "/map/mapstudio/m60_42_36_00.btl.dcx",
    "/map/mapstudio/m10_00_00_00.btl.dcx",
    "/map/m60/m60_42_36_00/m60_42_36_00.btl.dcx",
    NULL,
};

static const char *const k_ac6_paths[] = {
    "/map/mapstudio/m01_00_00_00.btl.dcx",
    "/map/mapstudio/m10_00_00_00.btl.dcx",
    NULL,
};

static const char *const k_nr_paths[] = {
    "/map/mapstudio/m60_10_09_02.btl.dcx",
    "/map/mapstudio/m21_20_00_00.btl.dcx",
    NULL,
};

static const char *const k_sekiro_paths[] = {
    "/map/mapstudio/m10_00_00_00.btl.dcx",
    "/map/mapstudio/m11_00_00_00.btl.dcx",
    NULL,
};

static void test_btl_e2e_multi_game(void)
{
    const game_probe_t games[] = {
        {"ER", er_helper_is_available, er_extract_from_data0, k_er_paths},
        {"AC6", ac6_helper_is_available, ac6_extract_from_data0, k_ac6_paths},
        {"Nightreign", nightreign_helper_is_available,
         nightreign_extract_from_data0, k_nr_paths},
        {"Sekiro", sekiro_helper_is_available, sekiro_extract_from_anybhd,
         k_sekiro_paths},
    };
    const size_t game_count = sizeof(games) / sizeof(games[0]);

    void       *bytes     = NULL;
    size_t      size      = 0;
    const char *used_game = NULL;
    const char *used_path = NULL;

    for (size_t g = 0; g < game_count && !used_path; ++g) {
        if (!games[g].available()) {
            continue;
        }
        for (size_t i = 0; games[g].candidates[i] != NULL; ++i) {
            sf_result_t r =
                games[g].extract(games[g].candidates[i], &bytes, &size);
            if (r == SF_OK) {
                used_game = games[g].game_name;
                used_path = games[g].candidates[i];
                break;
            }
            if (bytes) {
                sf_free(NULL, bytes);
                bytes = NULL;
                size  = 0;
            }
        }
    }

    if (!used_path) {
        TEST_IGNORE_MESSAGE(
            "btl e2e: no .btl entries found in Data0 across ER/AC6/NR/Sekiro "
            "(Wave-0 probe confirmed 0; upstream notes BTL is BB/DS3/Sekiro-"
            "only — ER/NR/AC6 may not ship BTL at all)");
    }
    (void)used_game;

    sf_btl_t   *obj = NULL;
    sf_result_t r   = sf_btl_read_from_memory(&obj, bytes, size, NULL);
    sf_free(NULL, bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(obj);

    const int32_t orig_version = sf_btl_version(obj);
    const size_t  orig_count   = sf_btl_light_count(obj);

    void  *out_bytes = NULL;
    size_t out_size  = 0;
    r = sf_btl_write_to_buffer(obj, &out_bytes, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(out_bytes);
    TEST_ASSERT_GREATER_THAN((size_t)0, out_size);

    sf_btl_t *obj2 = NULL;
    r = sf_btl_read_from_memory(&obj2, out_bytes, out_size, NULL);
    sf_free(NULL, out_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(obj2);

    TEST_ASSERT_EQUAL_INT32(orig_version, sf_btl_version(obj2));
    TEST_ASSERT_EQUAL_size_t(orig_count, sf_btl_light_count(obj2));

    sf_btl_destroy(obj);
    sf_btl_destroy(obj2);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_btl_e2e_multi_game);
    return UNITY_END();
}
