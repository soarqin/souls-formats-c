/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * P0 verification: after sf_oodle_load() succeeds once, sf_dcx_compress_to_buffer
 * and sf_dcx_decompress_from_buffer are safe to call concurrently from many
 * threads against independent buffers. This test fans 8 worker threads out
 * on the same Oodle DLL and verifies every roundtrip independently.
 */

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

#define WORKER_COUNT 8u
#define ITER_PER_WORKER 4u
#define INPUT_SIZE (256u * 1024u)

void setUp(void) {
    if (sf_oodle_set_search_path(SF_E2E_OODLE_DIR) != SF_OK)
        TEST_IGNORE_MESSAGE("sf_oodle_set_search_path failed");
    if (sf_oodle_load() != SF_OK)
        TEST_IGNORE_MESSAGE("oodle dll missing — skipping parallel test");
}

void tearDown(void) { sf_oodle_unload(); }

typedef struct worker_arg {
    unsigned seed;
    sf_result_t status;
    int iterations_completed;
} worker_arg_t;

static void fill_pseudo_random(uint8_t *p, size_t n, unsigned seed) {
    uint32_t s = 0x9E3779B9u ^ seed;
    for (size_t i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        p[i] = (uint8_t)(s >> 24);
    }
}

static sf_result_t one_roundtrip(unsigned seed) {
    uint8_t *src = (uint8_t *)malloc(INPUT_SIZE);
    if (!src) return SF_ERR_OOM;
    fill_pseudo_random(src, INPUT_SIZE, seed);

    sf_dcx_compression_info_t info;
    sf_result_t r = sf_dcx_compression_info_from_krak_preset(
        SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING, &info);
    if (r != SF_OK) { free(src); return r; }

    uint8_t *cx = NULL;
    size_t cxn = 0;
    r = sf_dcx_compress_to_buffer(src, INPUT_SIZE, &info, &cx, &cxn, NULL);
    if (r != SF_OK) { free(src); return r; }

    uint8_t *dx = NULL;
    size_t dxn = 0;
    r = sf_dcx_decompress_from_buffer(cx, cxn, &dx, &dxn, NULL, NULL);
    sf_free(NULL, cx);
    if (r != SF_OK) { free(src); return r; }

    if (dxn != INPUT_SIZE || memcmp(src, dx, INPUT_SIZE) != 0) {
        sf_free(NULL, dx);
        free(src);
        return SF_ERR_DECOMPRESS;
    }
    sf_free(NULL, dx);
    free(src);
    return SF_OK;
}

static DWORD WINAPI worker_thread(LPVOID raw) {
    worker_arg_t *arg = (worker_arg_t *)raw;
    for (unsigned i = 0; i < ITER_PER_WORKER; i++) {
        sf_result_t r = one_roundtrip(arg->seed + i * 0x100u);
        if (r != SF_OK) {
            arg->status = r;
            return 1;
        }
        arg->iterations_completed++;
    }
    arg->status = SF_OK;
    return 0;
}

static void test_parallel_krak_roundtrip(void) {
    HANDLE threads[WORKER_COUNT];
    worker_arg_t args[WORKER_COUNT];

    for (unsigned i = 0; i < WORKER_COUNT; i++) {
        args[i].seed = 0xDEAD0000u + i;
        args[i].status = SF_ERR_INTERNAL;
        args[i].iterations_completed = 0;
        threads[i] = CreateThread(NULL, 0, worker_thread, &args[i], 0, NULL);
        TEST_ASSERT_NOT_NULL(threads[i]);
    }

    DWORD waited = WaitForMultipleObjects(WORKER_COUNT, threads, TRUE, 5u * 60u * 1000u);
    TEST_ASSERT_TRUE(waited == WAIT_OBJECT_0);

    for (unsigned i = 0; i < WORKER_COUNT; i++) {
        CloseHandle(threads[i]);
        TEST_ASSERT_EQUAL_MESSAGE(SF_OK, args[i].status, "worker thread returned non-OK");
        TEST_ASSERT_EQUAL_INT_MESSAGE((int)ITER_PER_WORKER, args[i].iterations_completed,
                                      "worker thread completed fewer iterations than expected");
    }
}

static DWORD WINAPI load_thread(LPVOID raw) {
    (void)raw;
    return (DWORD)sf_oodle_load();
}

static void test_parallel_load_race(void) {
    sf_oodle_unload();

    HANDLE threads[WORKER_COUNT];
    for (unsigned i = 0; i < WORKER_COUNT; i++) {
        threads[i] = CreateThread(NULL, 0, load_thread, NULL, 0, NULL);
        TEST_ASSERT_NOT_NULL(threads[i]);
    }
    DWORD waited = WaitForMultipleObjects(WORKER_COUNT, threads, TRUE, 30u * 1000u);
    TEST_ASSERT_TRUE(waited == WAIT_OBJECT_0);

    for (unsigned i = 0; i < WORKER_COUNT; i++) {
        DWORD ec = 0;
        GetExitCodeThread(threads[i], &ec);
        CloseHandle(threads[i]);
        TEST_ASSERT_EQUAL_MESSAGE((DWORD)SF_OK, ec, "concurrent sf_oodle_load() returned non-OK");
    }
    TEST_ASSERT_TRUE(sf_oodle_version() != SF_OODLE_VERSION_UNKNOWN);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parallel_load_race);
    RUN_TEST(test_parallel_krak_roundtrip);
    return UNITY_END();
}
