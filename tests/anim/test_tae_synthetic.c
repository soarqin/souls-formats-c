/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 7 T14 — TAE synthetic round-trip: construct a minimal SDT TAE in
 * memory via internal struct headers, write it, read it back, verify every
 * field, then write again and check byte-level equality. Mirrors the
 * established FLVER2/EMEVD synthetic-test pattern.
 *
 * Two fixtures cover both AnimMiniHeader variants:
 *   1. Standard          (Animation.cs:51-147)
 *   2. ImportOtherAnim   (Animation.cs:152-217)
 *
 * Each fixture has exactly 1 animation × 1 event × 1 event group with
 * named skeleton + sib references, kept well under 1024 bytes.
 */

#include "effects/tae_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_tae.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t kSyntheticFlags[8] = {
    0x01, 0x00, 0x01, 0x02, 0x02, 0x01, 0x01, 0x01,
};

static const uint8_t kSyntheticParams[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
};

static sf_tae_animation_t *build_animation_shell(int64_t anim_id) {
    sf_tae_animation_t *anim = (sf_tae_animation_t *)sf_xalloc(NULL, sizeof(*anim));
    TEST_ASSERT_NOT_NULL(anim);
    memset(anim, 0, sizeof(*anim));
    anim->id             = anim_id;
    anim->anim_file_name = sf_strdup(NULL, "");
    TEST_ASSERT_NOT_NULL(anim->anim_file_name);

    anim->event_count = 1;
    anim->events      = (sf_tae_event_t **)sf_xalloc(NULL, sizeof(*anim->events));
    TEST_ASSERT_NOT_NULL(anim->events);
    sf_tae_event_t *ev = (sf_tae_event_t *)sf_xalloc(NULL, sizeof(*ev));
    TEST_ASSERT_NOT_NULL(ev);
    memset(ev, 0, sizeof(*ev));
    ev->start_time      = 0.0f;
    ev->end_time        = 1.0f;
    ev->type            = 300;
    ev->unk04           = 0;
    ev->parameters_size = sizeof(kSyntheticParams);
    ev->parameters      = (uint8_t *)sf_xalloc(NULL, ev->parameters_size);
    TEST_ASSERT_NOT_NULL(ev->parameters);
    memcpy(ev->parameters, kSyntheticParams, ev->parameters_size);
    anim->events[0] = ev;

    anim->event_group_count = 1;
    anim->event_groups = (sf_tae_event_group_t **)sf_xalloc(NULL, sizeof(*anim->event_groups));
    TEST_ASSERT_NOT_NULL(anim->event_groups);
    sf_tae_event_group_t *eg = (sf_tae_event_group_t *)sf_xalloc(NULL, sizeof(*eg));
    TEST_ASSERT_NOT_NULL(eg);
    memset(eg, 0, sizeof(*eg));
    eg->group_type   = 10;
    eg->member_count = 1;
    eg->members      = (int32_t *)sf_xalloc(NULL, sizeof(*eg->members));
    TEST_ASSERT_NOT_NULL(eg->members);
    eg->members[0]        = 0;
    anim->event_groups[0] = eg;

    return anim;
}

static sf_tae_t *build_synthetic_tae(sf_tae_animation_t *anim, int32_t tae_id) {
    sf_tae_t *t = (sf_tae_t *)sf_xalloc(NULL, sizeof(*t));
    TEST_ASSERT_NOT_NULL(t);
    memset(t, 0, sizeof(*t));
    t->alloc      = NULL;
    t->format     = SF_TAE_FORMAT_SDT;
    t->id         = tae_id;
    t->event_bank = 0;
    memcpy(t->flags, kSyntheticFlags, sizeof(t->flags));
    t->skeleton_name = sf_strdup(NULL, "c0000.hkt");
    TEST_ASSERT_NOT_NULL(t->skeleton_name);
    t->sib_name = sf_strdup(NULL, "c0000.sib");
    TEST_ASSERT_NOT_NULL(t->sib_name);

    t->animation_count = 1;
    t->animations      = (sf_tae_animation_t **)sf_xalloc(NULL, sizeof(*t->animations));
    TEST_ASSERT_NOT_NULL(t->animations);
    t->animations[0] = anim;
    return t;
}

static sf_tae_t *build_standard_fixture(void) {
    sf_tae_animation_t *anim    = build_animation_shell(42);
    anim->mini_header.type      = SF_TAE_MINI_HEADER_STANDARD;
    anim->mini_header.is_null_header = false;
    anim->mini_header.payload.standard.is_loop_by_default        = true;
    anim->mini_header.payload.standard.imports_hkx               = false;
    anim->mini_header.payload.standard.allow_delay_load          = false;
    anim->mini_header.payload.standard.import_hkx_source_anim_id = 100;
    return build_synthetic_tae(anim, 7);
}

static sf_tae_t *build_import_other_fixture(void) {
    sf_tae_animation_t *anim    = build_animation_shell(43);
    anim->mini_header.type      = SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM;
    anim->mini_header.is_null_header                            = false;
    anim->mini_header.payload.import_other.import_from_anim_id = 200;
    anim->mini_header.payload.import_other.unknown             = -1;
    return build_synthetic_tae(anim, 8);
}

static void verify_common_fields(const sf_tae_t *read, int32_t expect_tae_id,
                                 int64_t expect_anim_id) {
    TEST_ASSERT_NOT_NULL(read);
    TEST_ASSERT_EQUAL_INT(SF_TAE_FORMAT_SDT, sf_tae_format(read));
    TEST_ASSERT_EQUAL_INT32(expect_tae_id, sf_tae_id(read));
    TEST_ASSERT_EQUAL_STRING("c0000.hkt", sf_tae_skeleton_name(read));
    TEST_ASSERT_EQUAL_STRING("c0000.sib", sf_tae_sib_name(read));
    TEST_ASSERT_EQUAL_INT64(0, sf_tae_event_bank(read));
    TEST_ASSERT_EQUAL_size_t(1u, sf_tae_animation_count(read));

    const sf_tae_animation_t *a = sf_tae_animation(read, 0);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_EQUAL_INT64(expect_anim_id, sf_tae_animation_id(a));

    TEST_ASSERT_EQUAL_size_t(1u, sf_tae_animation_event_count(a));
    const sf_tae_event_t *e = sf_tae_animation_event(a, 0);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sf_tae_event_start_time(e));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sf_tae_event_end_time(e));
    TEST_ASSERT_EQUAL_INT32(300, sf_tae_event_type(e));

    size_t param_size = 0;
    const uint8_t *p  = sf_tae_event_parameters(e, &param_size);
    TEST_ASSERT_TRUE(param_size >= sizeof(kSyntheticParams));
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_MEMORY(kSyntheticParams, p, sizeof(kSyntheticParams));
    for (size_t k = sizeof(kSyntheticParams); k < param_size; ++k) {
        TEST_ASSERT_EQUAL_UINT8(0u, p[k]);
    }

    TEST_ASSERT_EQUAL_size_t(1u, sf_tae_animation_event_group_count(a));
    const sf_tae_event_group_t *g = sf_tae_animation_event_group(a, 0);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_EQUAL_INT32(10, sf_tae_event_group_type(g));
    TEST_ASSERT_EQUAL_size_t(1u, sf_tae_event_group_member_count(g));
    TEST_ASSERT_EQUAL_INT32(0, sf_tae_event_group_member(g, 0));
}

static void assert_round_trip_byte_equal(sf_tae_t *synth, int32_t expect_tae_id,
                                         int64_t expect_anim_id) {
    void  *bytes_a = NULL;
    size_t size_a  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_write_to_memory(synth, &bytes_a, &size_a, NULL));
    TEST_ASSERT_NOT_NULL(bytes_a);
    TEST_ASSERT_TRUE(size_a > 0u);
    TEST_ASSERT_TRUE(size_a <= 1024u);
    TEST_ASSERT_EQUAL_MEMORY("TAE ", bytes_a, 4);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, ((const uint8_t *)bytes_a)[7]);

    sf_tae_t *read = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_read_from_memory(&read, bytes_a, size_a, NULL));
    verify_common_fields(read, expect_tae_id, expect_anim_id);

    void  *bytes_b = NULL;
    size_t size_b  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_write_to_memory(read, &bytes_b, &size_b, NULL));
    TEST_ASSERT_EQUAL_size_t(size_a, size_b);
    TEST_ASSERT_EQUAL_MEMORY(bytes_a, bytes_b, size_a);

    sf_free(NULL, bytes_b);
    sf_tae_destroy(read);
    sf_free(NULL, bytes_a);
}

static void test_tae_synthetic_standard_round_trip(void) {
    sf_tae_t *synth = build_standard_fixture();
    assert_round_trip_byte_equal(synth, 7, 42);

    void  *bytes_a = NULL;
    size_t size_a  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_write_to_memory(synth, &bytes_a, &size_a, NULL));
    sf_tae_t *read = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_read_from_memory(&read, bytes_a, size_a, NULL));
    const sf_tae_animation_t        *a  = sf_tae_animation(read, 0);
    const sf_tae_anim_mini_header_t *mh = sf_tae_animation_mini_header(a);
    TEST_ASSERT_NOT_NULL(mh);
    TEST_ASSERT_EQUAL_INT(SF_TAE_MINI_HEADER_STANDARD, mh->type);
    TEST_ASSERT_FALSE(mh->is_null_header);
    TEST_ASSERT_TRUE(mh->payload.standard.is_loop_by_default);
    TEST_ASSERT_FALSE(mh->payload.standard.imports_hkx);
    TEST_ASSERT_FALSE(mh->payload.standard.allow_delay_load);
    TEST_ASSERT_EQUAL_INT32(100, mh->payload.standard.import_hkx_source_anim_id);

    sf_tae_destroy(read);
    sf_free(NULL, bytes_a);
    sf_tae_destroy(synth);
}

static void test_tae_synthetic_import_other_round_trip(void) {
    sf_tae_t *synth = build_import_other_fixture();
    assert_round_trip_byte_equal(synth, 8, 43);

    void  *bytes_a = NULL;
    size_t size_a  = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_write_to_memory(synth, &bytes_a, &size_a, NULL));
    sf_tae_t *read = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_read_from_memory(&read, bytes_a, size_a, NULL));
    const sf_tae_animation_t        *a  = sf_tae_animation(read, 0);
    const sf_tae_anim_mini_header_t *mh = sf_tae_animation_mini_header(a);
    TEST_ASSERT_NOT_NULL(mh);
    TEST_ASSERT_EQUAL_INT(SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM, mh->type);
    TEST_ASSERT_FALSE(mh->is_null_header);
    TEST_ASSERT_EQUAL_INT32(200, mh->payload.import_other.import_from_anim_id);
    TEST_ASSERT_EQUAL_INT32(-1, mh->payload.import_other.unknown);

    sf_tae_destroy(read);
    sf_free(NULL, bytes_a);
    sf_tae_destroy(synth);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tae_synthetic_standard_round_trip);
    RUN_TEST(test_tae_synthetic_import_other_round_trip);
    return UNITY_END();
}
