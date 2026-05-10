/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

void setUp(void) {
    if (sf_oodle_set_search_path(SF_E2E_OODLE_DIR) != SF_OK) TEST_IGNORE_MESSAGE("sf_oodle_set_search_path failed");
    if (sf_oodle_load() != SF_OK) TEST_IGNORE_MESSAGE("oodle dll missing — skipping KRAK tests");
    sf_oodle_version_t version = sf_oodle_version();
    TEST_ASSERT_TRUE(version == SF_OODLE_VERSION_9 || version == SF_OODLE_VERSION_8 ||
                     version == SF_OODLE_VERSION_6);
}
void tearDown(void) { sf_oodle_unload(); }

static void fill(uint8_t *p, size_t n, int random) { uint32_t s=0x12345678u; for(size_t i=0;i<n;i++){ s=s*1664525u+1013904223u; p[i]=random?(uint8_t)(s>>24):0u; } }

static void test_krak_roundtrip(void) {
    const size_t sizes[] = {4096u, 65536u, 1048576u};
    uint8_t *buf = sf_default_allocator()->alloc(1048576u, NULL);
    TEST_ASSERT_NOT_NULL(buf);
    for (size_t s = 0; s < 3u; s++) for (int k = 0; k < 2; k++) {
        fill(buf, sizes[s], k);
        void *cx = NULL; size_t cxn = 0; void *dx = NULL; size_t dxn = 0; sf_dcx_type_t type;
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress(buf, sizes[s], SF_DCX_TYPE_DCX_KRAK, &cx, &cxn, NULL));
        sf_dcx_params_t params;
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_read_params(cx, cxn, &params));
        TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, params.type);
        TEST_ASSERT_EQUAL_UINT8(6u, params.level);
        TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress(cx, cxn, &dx, &dxn, &type, NULL));
        TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, type);
        TEST_ASSERT_EQUAL_UINT(sizes[s], dxn);
        TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizes[s]);
        sf_free(NULL, cx); sf_free(NULL, dx);
    }
    sf_free(NULL, buf);
}

static void test_krak_params_driven_roundtrip(void) {
    uint8_t buf[4096];
    fill(buf, sizeof(buf), 1);

    void *first = NULL;
    size_t first_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress(buf, sizeof(buf), SF_DCX_TYPE_DCX_KRAK,
                                                 &first, &first_size, NULL));

    sf_dcx_params_t params;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_read_params(first, first_size, &params));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, params.type);
    TEST_ASSERT_EQUAL_UINT8(6u, params.level);

    void *second = NULL;
    size_t second_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_compress_ex(buf, sizeof(buf), &params,
                                                    &second, &second_size, NULL));

    sf_dcx_params_t second_params;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_read_params(second, second_size, &second_params));
    TEST_ASSERT_EQUAL_INT(params.type, second_params.type);
    TEST_ASSERT_EQUAL_UINT8(params.level, second_params.level);

    void *dx = NULL;
    size_t dxn = 0;
    sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_dcx_decompress(second, second_size, &dx, &dxn, &type, NULL));
    TEST_ASSERT_EQUAL_INT(SF_DCX_TYPE_DCX_KRAK, type);
    TEST_ASSERT_EQUAL_UINT(sizeof(buf), dxn);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf, dx, sizeof(buf));

    sf_free(NULL, first);
    sf_free(NULL, second);
    sf_free(NULL, dx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_krak_roundtrip);
    RUN_TEST(test_krak_params_driven_roundtrip);
    return UNITY_END();
}
