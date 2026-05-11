/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 5 QA — MSB list-of-lists skeleton round-trip.
 *
 * Writes a synthetic MSB with two empty named param-lists, then reads
 * it back and verifies the layout. No concrete entry types are involved.
 */

#include "map/msb_internal.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char *k_list_a = "TEST_LIST_A";
static const char *k_list_b = "TEST_LIST_B";

typedef struct iter_ctx {
    int   call_count;
    char  names[2][32];
    int32_t counts[2];
    int64_t positions[2];
} iter_ctx_t;

static sf_result_t iter_cb(const char *name, int32_t entry_count,
                            sf_binary_reader_t *r, void *ctx) {
    iter_ctx_t *c = (iter_ctx_t *)ctx;
    if (c->call_count >= 2) return SF_ERR_INTERNAL;
    strncpy(c->names[c->call_count], name ? name : "", 31);
    c->names[c->call_count][31] = '\0';
    c->counts[c->call_count] = entry_count;
    c->positions[c->call_count] = sf_binary_reader_position(r);
    c->call_count++;
    return SF_OK;
}

static void produce_synthetic_msb(uint8_t **out_bytes, size_t *out_size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_memory(&s, NULL));
    sf_binary_writer_t *w = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_create(&w, s, false, NULL));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_write_header(w));

    int64_t list_a_start = sf_binary_writer_position(w);
    TEST_ASSERT_EQUAL(16, list_a_start);
    TEST_ASSERT_EQUAL(SF_OK, msb_common_reserve_list(w, k_list_a, 0));

    int64_t list_b_start = sf_binary_writer_position(w);
    TEST_ASSERT_EQUAL(SF_OK, msb_common_reserve_list(w, k_list_b, 1));

    TEST_ASSERT_EQUAL(SF_OK, msb_common_fill_list(w, 0, 0, list_b_start));
    TEST_ASSERT_EQUAL(SF_OK, msb_common_fill_list(w, 1, 0, 0));

    TEST_ASSERT_EQUAL(SF_OK, sf_binary_writer_finish_bytes(w, out_bytes, out_size));
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
}

static void test_roundtrip_two_empty_lists(void) {
    uint8_t *bytes = NULL;
    size_t   size  = 0;
    produce_synthetic_msb(&bytes, &size);
    TEST_ASSERT_NOT_NULL(bytes);
    TEST_ASSERT_GREATER_THAN_size_t(16, size);

    TEST_ASSERT_EQUAL_MEMORY("MSB ", bytes, 4);

    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_binary_reader_create_from_memory(&r, false, bytes, size, NULL));

    msb_layout_t layout;
    TEST_ASSERT_EQUAL(SF_OK, msb_common_read_header(r, &layout, NULL));
    TEST_ASSERT_EQUAL_INT32(2, layout.list_count);
    TEST_ASSERT_EQUAL(true, layout.is64_bit);
    TEST_ASSERT_EQUAL_INT32(1, layout.version);

    TEST_ASSERT_NOT_NULL(layout.lists[0].name);
    TEST_ASSERT_NOT_NULL(layout.lists[1].name);
    TEST_ASSERT_EQUAL_STRING(k_list_a, layout.lists[0].name);
    TEST_ASSERT_EQUAL_STRING(k_list_b, layout.lists[1].name);
    TEST_ASSERT_EQUAL_INT32(0, layout.lists[0].entry_count);
    TEST_ASSERT_EQUAL_INT32(0, layout.lists[1].entry_count);
    TEST_ASSERT_GREATER_THAN_INT64(16, layout.lists[0].data_offset);
    TEST_ASSERT_GREATER_THAN_INT64(layout.lists[0].data_offset,
                                    layout.lists[1].data_offset);

    iter_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    TEST_ASSERT_EQUAL(SF_OK, msb_common_iter_lists(r, &layout, iter_cb, &ctx, NULL));
    TEST_ASSERT_EQUAL_INT(2, ctx.call_count);
    TEST_ASSERT_EQUAL_STRING(k_list_a, ctx.names[0]);
    TEST_ASSERT_EQUAL_STRING(k_list_b, ctx.names[1]);
    TEST_ASSERT_EQUAL_INT32(0, ctx.counts[0]);
    TEST_ASSERT_EQUAL_INT32(0, ctx.counts[1]);
    TEST_ASSERT_EQUAL_INT64(layout.lists[0].data_offset, ctx.positions[0]);
    TEST_ASSERT_EQUAL_INT64(layout.lists[1].data_offset, ctx.positions[1]);

    msb_common_free_layout(&layout, NULL);
    sf_binary_reader_destroy(r);
}

static void test_assert_header_rejects_bad_magic(void) {
    static const uint8_t bad[16] = {
        'X', 'X', 'X', 'X', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    uint8_t *copy = (uint8_t *)malloc(sizeof bad);
    TEST_ASSERT_NOT_NULL(copy);
    memcpy(copy, bad, sizeof bad);

    sf_binary_reader_t *r = NULL;
    TEST_ASSERT_EQUAL(SF_OK,
        sf_binary_reader_create_from_memory(&r, false, copy, sizeof bad, NULL));

    msb_layout_t layout;
    sf_result_t rc = msb_common_read_header(r, &layout, NULL);
    TEST_ASSERT_NOT_EQUAL(SF_OK, rc);
    TEST_ASSERT_EQUAL_INT32(0, layout.list_count);

    sf_binary_reader_destroy(r);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_two_empty_lists);
    RUN_TEST(test_assert_header_rejects_bad_magic);
    return UNITY_END();
}
