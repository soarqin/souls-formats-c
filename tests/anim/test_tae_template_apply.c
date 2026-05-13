/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TAE apply_template test.
 * Verifies sf_tae_apply_template() resizes event parameters correctly.
 */

#include "effects/tae_internal.h" /* IWYU pragma: keep */
#include "internal/sf_internal.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_tae.h"
#include "souls_formats/sf_tae_template.h"

#include "unity.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const char kTemplateXml[] =
    "<event_template game=\"SDT\">"
    "  <bank id=\"0\" name=\"TestBank\" basedon=\"-1\">"
    "    <event id=\"300\" name=\"TestEvent\">"
    "      <s32 name=\"Field1\"/>"
    "      <f32 name=\"Field2\"/>"
    "      <aob name=\"Pad\" length=\"8\"/>"
    "    </event>"
    "  </bank>"
    "</event_template>";

static sf_tae_t *build_tae_with_event(int32_t event_type, size_t param_size) {
    sf_tae_t *t = (sf_tae_t *)sf_xalloc(NULL, sizeof(*t));
    TEST_ASSERT_NOT_NULL(t);
    memset(t, 0, sizeof(*t));
    t->format        = SF_TAE_FORMAT_SDT;
    t->id            = 1;
    t->event_bank    = 0;
    t->skeleton_name = sf_strdup(NULL, "test.hkt");
    t->sib_name      = sf_strdup(NULL, "test.sib");
    TEST_ASSERT_NOT_NULL(t->skeleton_name);
    TEST_ASSERT_NOT_NULL(t->sib_name);

    sf_tae_animation_t *anim = (sf_tae_animation_t *)sf_xalloc(NULL, sizeof(*anim));
    TEST_ASSERT_NOT_NULL(anim);
    memset(anim, 0, sizeof(*anim));
    anim->id             = 0;
    anim->anim_file_name = sf_strdup(NULL, "");
    TEST_ASSERT_NOT_NULL(anim->anim_file_name);
    anim->event_count = 1;
    anim->events      = (sf_tae_event_t **)sf_xalloc(NULL, sizeof(*anim->events));
    TEST_ASSERT_NOT_NULL(anim->events);

    sf_tae_event_t *ev = (sf_tae_event_t *)sf_xalloc(NULL, sizeof(*ev));
    TEST_ASSERT_NOT_NULL(ev);
    memset(ev, 0, sizeof(*ev));
    ev->type       = event_type;
    ev->start_time = 0.0f;
    ev->end_time   = 1.0f;
    if (param_size > 0) {
        ev->parameters = (uint8_t *)sf_xalloc(NULL, param_size);
        TEST_ASSERT_NOT_NULL(ev->parameters);
        memset(ev->parameters, 0xAB, param_size);
        ev->parameters_size = param_size;
    }
    anim->events[0] = ev;

    t->animation_count = 1;
    t->animations      = (sf_tae_animation_t **)sf_xalloc(NULL, sizeof(*t->animations));
    TEST_ASSERT_NOT_NULL(t->animations);
    t->animations[0] = anim;
    return t;
}

static sf_tae_template_t *parse_template(void) {
    sf_tae_template_t *tmpl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK,
                          sf_tae_template_read_from_memory(&tmpl, kTemplateXml,
                                                            strlen(kTemplateXml), NULL));
    TEST_ASSERT_NOT_NULL(tmpl);
    return tmpl;
}

static void test_apply_template_resizes_params(void) {
    sf_tae_template_t *tmpl = parse_template();
    sf_tae_t          *t    = build_tae_with_event(300, 4);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_apply_template(t, tmpl, true, true));

    const sf_tae_animation_t *anim = sf_tae_animation(t, 0);
    const sf_tae_event_t     *ev   = sf_tae_animation_event(anim, 0);
    size_t                    param_size = 0;
    const uint8_t            *params     = sf_tae_event_parameters(ev, &param_size);
    TEST_ASSERT_NOT_NULL(params);
    TEST_ASSERT_EQUAL_size_t(16u, param_size);

    sf_tae_destroy(t);
    sf_tae_template_destroy(tmpl);
}

static void test_apply_template_wrong_game(void) {
    sf_tae_template_t *tmpl = parse_template();
    sf_tae_t          *t    = build_tae_with_event(300, 4);
    t->format              = SF_TAE_FORMAT_DS1;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_tae_apply_template(t, tmpl, true, true));

    sf_tae_destroy(t);
    sf_tae_template_destroy(tmpl);
}

static void test_apply_template_missing_bank(void) {
    sf_tae_template_t *tmpl = parse_template();
    sf_tae_t          *t    = build_tae_with_event(300, 4);
    t->event_bank          = 99;
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_tae_apply_template(t, tmpl, true, true));

    sf_tae_destroy(t);
    sf_tae_template_destroy(tmpl);
}

static void test_apply_template_unknown_event_strict(void) {
    sf_tae_template_t *tmpl = parse_template();
    sf_tae_t          *t    = build_tae_with_event(999, 4);
    TEST_ASSERT_NOT_EQUAL(SF_OK, sf_tae_apply_template(t, tmpl, true, true));

    sf_tae_destroy(t);
    sf_tae_template_destroy(tmpl);
}

static void test_apply_template_unknown_event_lenient(void) {
    sf_tae_template_t *tmpl = parse_template();
    sf_tae_t          *t    = build_tae_with_event(999, 4);
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_tae_apply_template(t, tmpl, true, false));

    sf_tae_destroy(t);
    sf_tae_template_destroy(tmpl);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_template_resizes_params);
    RUN_TEST(test_apply_template_wrong_game);
    RUN_TEST(test_apply_template_missing_bank);
    RUN_TEST(test_apply_template_unknown_event_strict);
    RUN_TEST(test_apply_template_unknown_event_lenient);
    return UNITY_END();
}
