/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * e2e test: EMEVD mutation API against a real Elden Ring event script.
 *
 * Validates the new EMEVD mutation functions end-to-end:
 *   1. er_extract_from_data0 extracts common.emevd.dcx from Data0.bhd
 *      (RSA-unwrap -> BHD5 lookup -> DCX_KRAK decompress).
 *   2. sf_emevd_read_from_memory parses the event script.
 *   3. sf_emevd_event_find_by_id locates a known event.
 *   4. sf_emevd_event_clear_parameters removes all parameters.
 *   5. sf_emevd_write_to_memory serialises the modified script.
 *   6. Re-parsing verifies the parameter count is now 0.
 *
 * A second sub-test exercises sf_emevd_add_event + sf_emevd_event_insert_instruction
 * on a synthetic in-memory EMEVD to avoid depending on specific event IDs
 * that may change across game patches.
 *
 * The test SKIPs gracefully (TEST_IGNORE_MESSAGE) whenever the ER game
 * directory or Oodle DLL is unavailable, so it never FAILs in a clean
 * checkout.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_emevd.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static const char *const k_emevd_candidates[] = {
    "/event/common.emevd.dcx",
    "/event/m60_42_36_00.emevd.dcx",
    "/event/m11_00_00_00.emevd.dcx",
    NULL,
};

/* ---------------------------------------------------------------------------
 * Helper: extract first available EMEVD from Data0.
 * Caller frees *out via sf_free(NULL, *out).
 * ------------------------------------------------------------------------- */
static bool extract_emevd(void **out, size_t *out_size)
{
    *out      = NULL;
    *out_size = 0;
    for (size_t i = 0; k_emevd_candidates[i]; ++i) {
        sf_result_t r = er_extract_from_data0(k_emevd_candidates[i],
                                              out, out_size);
        if (r == SF_OK && *out && *out_size > 0) return true;
        if (*out) { sf_free(NULL, *out); *out = NULL; }
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Test: clear_parameters on a live event, verify via write+re-read
 * ------------------------------------------------------------------------- */
static void test_emevd_clear_parameters_e2e(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER game directory not available; skipping EMEVD mutation e2e");
        return;
    }

    void   *emevd_bytes = NULL;
    size_t  emevd_size  = 0;
    if (!extract_emevd(&emevd_bytes, &emevd_size)) {
        TEST_IGNORE_MESSAGE("No EMEVD candidate found in Data0; skipping");
        return;
    }

    sf_emevd_t *emevd = NULL;
    sf_result_t r = sf_emevd_read_from_memory(&emevd,
                                              (const uint8_t *)emevd_bytes,
                                              emevd_size, NULL);
    sf_free(NULL, emevd_bytes);
    TEST_ASSERT_EQUAL_INT_MESSAGE(SF_OK, r, "sf_emevd_read_from_memory failed");
    TEST_ASSERT_NOT_NULL(emevd);

    /* Find the first event that has at least one parameter */
    const size_t event_count = sf_emevd_get_event_count(emevd);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, event_count, "EMEVD has no events");

    int64_t target_id    = -1;
    size_t  initial_params = 0;
    for (size_t i = 0; i < event_count; ++i) {
        const sf_emevd_event_t *ev = sf_emevd_get_event(emevd, i);
        if (!ev) continue;
        const size_t pc = sf_emevd_event_get_parameter_count(ev);
        if (pc > 0) {
            target_id     = sf_emevd_event_get_id(ev);
            initial_params = pc;
            break;
        }
    }

    if (target_id < 0) {
        sf_emevd_destroy(emevd, NULL);
        TEST_IGNORE_MESSAGE("No event with parameters found; skipping clear test");
        return;
    }

    /* Find by ID and clear parameters */
    sf_emevd_event_t *ev = NULL;
    r = sf_emevd_event_find_by_id(emevd, target_id, &ev);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(ev);
    TEST_ASSERT_EQUAL_size_t(initial_params,
                             sf_emevd_event_get_parameter_count(ev));

    r = sf_emevd_event_clear_parameters(ev);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_EQUAL_size_t(0, sf_emevd_event_get_parameter_count(ev));

    /* Serialise and re-parse */
    uint8_t *out_bytes = NULL;
    size_t   out_size  = 0;
    r = sf_emevd_write_to_memory(emevd, &out_bytes, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(out_bytes);
    sf_emevd_destroy(emevd, NULL);

    sf_emevd_t *emevd2 = NULL;
    r = sf_emevd_read_from_memory(&emevd2, out_bytes, out_size, NULL);
    sf_free(NULL, out_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(emevd2);

    sf_emevd_event_t *ev2 = NULL;
    r = sf_emevd_event_find_by_id(emevd2, target_id, &ev2);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(ev2);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0,
                                     sf_emevd_event_get_parameter_count(ev2),
                                     "Parameters should be 0 after clear+round-trip");

    sf_emevd_destroy(emevd2, NULL);
}

/* ---------------------------------------------------------------------------
 * Test: add_event + insert_instruction on a live EMEVD, verify via round-trip
 * ------------------------------------------------------------------------- */
static void test_emevd_add_event_and_insert_instruction_e2e(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER game directory not available; skipping");
        return;
    }

    void   *emevd_bytes = NULL;
    size_t  emevd_size  = 0;
    if (!extract_emevd(&emevd_bytes, &emevd_size)) {
        TEST_IGNORE_MESSAGE("No EMEVD candidate found in Data0; skipping");
        return;
    }

    sf_emevd_t *emevd = NULL;
    sf_result_t r = sf_emevd_read_from_memory(&emevd,
                                              (const uint8_t *)emevd_bytes,
                                              emevd_size, NULL);
    sf_free(NULL, emevd_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(emevd);

    const size_t event_count_before = sf_emevd_get_event_count(emevd);

    /* Add a new event with a unique ID unlikely to collide */
    const int64_t new_id = 999999999LL;
    sf_emevd_event_t *new_ev = NULL;
    r = sf_emevd_add_event(emevd, new_id, SF_EMEVD_REST_BEHAVIOR_DEFAULT, &new_ev);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(new_ev);
    TEST_ASSERT_EQUAL_size_t(event_count_before + 1,
                             sf_emevd_get_event_count(emevd));

    /* Insert a no-op instruction (bank=1014, id=0 = Label0) */
    const uint8_t no_args[] = {0};
    r = sf_emevd_event_insert_instruction(new_ev, 0, 1014, 0,
                                          no_args, sizeof(no_args));
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_EQUAL_size_t(1, sf_emevd_event_get_instruction_count(new_ev));

    /* Serialise and re-parse */
    uint8_t *out_bytes = NULL;
    size_t   out_size  = 0;
    r = sf_emevd_write_to_memory(emevd, &out_bytes, &out_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    sf_emevd_destroy(emevd, NULL);

    sf_emevd_t *emevd2 = NULL;
    r = sf_emevd_read_from_memory(&emevd2, out_bytes, out_size, NULL);
    sf_free(NULL, out_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(emevd2);

    TEST_ASSERT_EQUAL_size_t(event_count_before + 1,
                             sf_emevd_get_event_count(emevd2));

    sf_emevd_event_t *found = NULL;
    r = sf_emevd_event_find_by_id(emevd2, new_id, &found);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1,
                                     sf_emevd_event_get_instruction_count(found),
                                     "Inserted instruction should survive round-trip");

    sf_emevd_destroy(emevd2, NULL);
}

/* ---------------------------------------------------------------------------
 * Test: find_by_id miss returns SF_ERR_NOT_FOUND
 * ------------------------------------------------------------------------- */
static void test_emevd_find_by_id_miss_e2e(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER game directory not available; skipping");
        return;
    }

    void   *emevd_bytes = NULL;
    size_t  emevd_size  = 0;
    if (!extract_emevd(&emevd_bytes, &emevd_size)) {
        TEST_IGNORE_MESSAGE("No EMEVD candidate found in Data0; skipping");
        return;
    }

    sf_emevd_t *emevd = NULL;
    sf_result_t r = sf_emevd_read_from_memory(&emevd,
                                              (const uint8_t *)emevd_bytes,
                                              emevd_size, NULL);
    sf_free(NULL, emevd_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);

    sf_emevd_event_t *ev = NULL;
    r = sf_emevd_event_find_by_id(emevd, -1LL /* impossible ID */, &ev);
    TEST_ASSERT_EQUAL_INT(SF_ERR_NOT_FOUND, r);
    TEST_ASSERT_NULL(ev);

    sf_emevd_destroy(emevd, NULL);
}

/* ---------------------------------------------------------------------------
 * Runner
 * ------------------------------------------------------------------------- */
int main(void)
{
    env_ok = (er_helper_init() == SF_OK);

    UNITY_BEGIN();
    RUN_TEST(test_emevd_clear_parameters_e2e);
    RUN_TEST(test_emevd_add_event_and_insert_instruction_e2e);
    RUN_TEST(test_emevd_find_by_id_miss_e2e);
    return UNITY_END();
}
