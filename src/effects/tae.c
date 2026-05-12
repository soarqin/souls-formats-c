/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TAE (Time Act Editor) SDT format implementation.
 *
 * Mirrors:
 *   SoulsFormats/Formats/TAE/TAE.cs
 *   SoulsFormats/Formats/TAE/Animation.cs
 *   SoulsFormats/Formats/TAE/Event.cs
 *   SoulsFormats/Formats/TAE/EventGroup.cs
 *
 * Wave 2 (T11-T13) implements the actual read/write logic.
 */

#include "souls_formats/sf_tae.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sf_tae_event_group {
    int32_t group_type;
    int32_t *members;
    size_t member_count;
};

struct sf_tae_event {
    float start_time;
    float end_time;
    int32_t type;
    uint8_t *parameters;
    size_t parameters_size;
};

struct sf_tae_animation {
    int64_t id;
    sf_tae_anim_mini_header_t mini_header;
    sf_tae_event_t **events;
    size_t event_count;
    sf_tae_event_group_t **event_groups;
    size_t event_group_count;
};

struct sf_tae {
    const sf_allocator_t *alloc;
    sf_tae_format_t format;
    int32_t id;
    char *skeleton_name;
    char *sib_name;
    int64_t event_bank;
    sf_tae_animation_t **animations;
    size_t animation_count;
};

SF_API sf_result_t sf_tae_read_from_memory(sf_tae_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *a) {
    (void)bytes;
    (void)size;
    (void)a;
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    return SF_ERR_INTERNAL; /* Wave 2 T11 implements this */
}

SF_API sf_result_t sf_tae_write_to_memory(const sf_tae_t *t, void **out_bytes, size_t *out_size,
                                          const sf_allocator_t *a) {
    (void)t;
    (void)out_bytes;
    (void)out_size;
    (void)a;
    return SF_ERR_INTERNAL; /* Wave 2 T13 implements this */
}

SF_API void sf_tae_destroy(sf_tae_t *t) {
    if (!t)
        return;
    const sf_allocator_t *alloc = t->alloc;
    sf_xfree(alloc, t->skeleton_name);
    sf_xfree(alloc, t->sib_name);
    /* Nested animations / events / event groups are freed in Wave 2 once the
     * reader populates them. */
    sf_xfree(alloc, t->animations);
    sf_xfree(alloc, t);
}

SF_API sf_tae_format_t sf_tae_format(const sf_tae_t *t) {
    return t ? t->format : SF_TAE_FORMAT_SDT;
}

SF_API int32_t sf_tae_id(const sf_tae_t *t) {
    return t ? t->id : 0;
}

SF_API const char *sf_tae_skeleton_name(const sf_tae_t *t) {
    return t ? t->skeleton_name : NULL;
}

SF_API const char *sf_tae_sib_name(const sf_tae_t *t) {
    return t ? t->sib_name : NULL;
}

SF_API int64_t sf_tae_event_bank(const sf_tae_t *t) {
    return t ? t->event_bank : 0;
}

SF_API size_t sf_tae_animation_count(const sf_tae_t *t) {
    return t ? t->animation_count : 0;
}

SF_API const sf_tae_animation_t *sf_tae_animation(const sf_tae_t *t, size_t i) {
    return (t && i < t->animation_count) ? t->animations[i] : NULL;
}

SF_API int64_t sf_tae_animation_id(const sf_tae_animation_t *a) {
    return a ? a->id : 0;
}

SF_API const sf_tae_anim_mini_header_t *sf_tae_animation_mini_header(const sf_tae_animation_t *a) {
    return a ? &a->mini_header : NULL;
}

SF_API size_t sf_tae_animation_event_count(const sf_tae_animation_t *a) {
    return a ? a->event_count : 0;
}

SF_API const sf_tae_event_t *sf_tae_animation_event(const sf_tae_animation_t *a, size_t i) {
    return (a && i < a->event_count) ? a->events[i] : NULL;
}

SF_API size_t sf_tae_animation_event_group_count(const sf_tae_animation_t *a) {
    return a ? a->event_group_count : 0;
}

SF_API const sf_tae_event_group_t *sf_tae_animation_event_group(const sf_tae_animation_t *a,
                                                                size_t i) {
    return (a && i < a->event_group_count) ? a->event_groups[i] : NULL;
}

SF_API float sf_tae_event_start_time(const sf_tae_event_t *e) {
    return e ? e->start_time : 0.0f;
}

SF_API float sf_tae_event_end_time(const sf_tae_event_t *e) {
    return e ? e->end_time : 0.0f;
}

SF_API int32_t sf_tae_event_type(const sf_tae_event_t *e) {
    return e ? e->type : 0;
}

SF_API const uint8_t *sf_tae_event_parameters(const sf_tae_event_t *e, size_t *out_size) {
    if (out_size)
        *out_size = e ? e->parameters_size : 0;
    return e ? e->parameters : NULL;
}

SF_API int32_t sf_tae_event_group_type(const sf_tae_event_group_t *g) {
    return g ? g->group_type : 0;
}

SF_API size_t sf_tae_event_group_member_count(const sf_tae_event_group_t *g) {
    return g ? g->member_count : 0;
}

SF_API int32_t sf_tae_event_group_member(const sf_tae_event_group_t *g, size_t i) {
    return (g && i < g->member_count) ? g->members[i] : 0;
}
