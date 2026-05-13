/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_ani.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

#define ANI_MAGIC          ((int32_t)0x20051014)
#define ANI_HEADER_SIZE    120  /* Upstream: nodes start at fixed offset 120. */
#define ANI_NODE_SIZE      244  /* int32+int32+6*int16+3*vec3+int32+pad4+int32+pad176. */
/* NodeAnimation header on disk:
 *   int32 framesOffset + int32 frameCount + int32 format + vec3 unk10 +
 *   vec3 unk20 + int32 0 = 4+4+4+12+12+4 = 40 bytes.
 *
 * Upstream's Write emits `bw.Position + 36` for framesOffset (ANI.cs:449)
 * which is 4 bytes short of the true header size; the resulting file
 * cannot be re-read by the same library. We emit the correct 40 so that
 * round-trips succeed. */
#define ANI_ANIM_HEADER    40

/*===========================================================================
 * Storage
 *===========================================================================*/

struct sf_ani_node_animation {
    const sf_allocator_t  *alloc;
    sf_ani_frame_format_t  format;
    sf_vec3_t              unk10;
    sf_vec3_t              unk20;
    sf_ani_frame_t        *frames;
    size_t                 frame_count;
    size_t                 frame_cap;
};

struct sf_ani_node {
    const sf_allocator_t      *alloc;
    sf_ani_node_type_t         type;
    int16_t                    geom_index;
    int16_t                    parent_index;
    int16_t                    first_child_index;
    int16_t                    next_sibling_index;
    int16_t                    unk_index_12;
    sf_vec3_t                  translation;
    sf_vec3_t                  rotation;
    sf_vec3_t                  scale;
    char                      *name;       /* heap, UTF-8, NUL-terminated. */
    sf_ani_node_animation_t   *animation;  /* heap, optional.              */
};

struct sf_ani {
    const sf_allocator_t  *alloc;
    sf_ani_node_t        **nodes;
    size_t                 node_count;
    size_t                 node_cap;
    sf_vec3_t             *translations;
    size_t                 translation_count;
    size_t                 translation_cap;
    sf_vec3_t             *rotations;
    size_t                 rotation_count;
    size_t                 rotation_cap;
};

/*===========================================================================
 * Internal helpers
 *===========================================================================*/

static sf_result_t ani_grow_nodes(sf_ani_t *a) {
    if (a->node_count < a->node_cap) return SF_OK;
    size_t new_cap = a->node_cap == 0 ? 4u : a->node_cap * 2u;
    sf_ani_node_t **na = (sf_ani_node_t **)sf_xalloc(a->alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    if (a->nodes) {
        memcpy(na, a->nodes, a->node_count * sizeof(*na));
        sf_xfree(a->alloc, a->nodes);
    }
    a->nodes = na;
    a->node_cap = new_cap;
    return SF_OK;
}

static sf_result_t ani_grow_vec3_array(const sf_allocator_t *alloc,
                                       sf_vec3_t **arr, size_t count, size_t *cap) {
    if (count < *cap) return SF_OK;
    size_t new_cap = *cap == 0 ? 8u : *cap * 2u;
    sf_vec3_t *na = (sf_vec3_t *)sf_xalloc(alloc, new_cap * sizeof(*na));
    if (!na) return SF_ERR_OOM;
    if (*arr) {
        memcpy(na, *arr, count * sizeof(*na));
        sf_xfree(alloc, *arr);
    }
    *arr = na;
    *cap = new_cap;
    return SF_OK;
}

static sf_result_t ani_grow_frames(sf_ani_node_animation_t *anim) {
    if (anim->frame_count < anim->frame_cap) return SF_OK;
    size_t new_cap = anim->frame_cap == 0 ? 8u : anim->frame_cap * 2u;
    sf_ani_frame_t *nf = (sf_ani_frame_t *)sf_xalloc(anim->alloc, new_cap * sizeof(*nf));
    if (!nf) return SF_ERR_OOM;
    if (anim->frames) {
        memcpy(nf, anim->frames, anim->frame_count * sizeof(*nf));
        sf_xfree(anim->alloc, anim->frames);
    }
    anim->frames = nf;
    anim->frame_cap = new_cap;
    return SF_OK;
}

static void ani_destroy_animation(sf_ani_node_animation_t *anim) {
    if (!anim) return;
    sf_xfree(anim->alloc, anim->frames);
    sf_xfree(anim->alloc, anim);
}

static void ani_destroy_node(sf_ani_node_t *n) {
    if (!n) return;
    sf_xfree(n->alloc, n->name);
    ani_destroy_animation(n->animation);
    sf_xfree(n->alloc, n);
}

/* Mirrors upstream Node() default ctor (ANI.cs:274-286). */
static void ani_node_init_defaults(sf_ani_node_t *n) {
    n->type               = SF_ANI_NODE_TYPE_GEOM;
    n->geom_index         = -1;
    n->parent_index       = -1;
    n->first_child_index  = -1;
    n->next_sibling_index = -1;
    n->unk_index_12       = -1;
    n->translation.x = n->translation.y = n->translation.z = 0.0f;
    n->rotation.x    = n->rotation.y    = n->rotation.z    = 0.0f;
    n->scale.x = n->scale.y = n->scale.z = 1.0f;
    n->name = NULL;
    n->animation = NULL;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

sf_result_t sf_ani_create(sf_ani_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_ani_t *a = (sf_ani_t *)sf_xalloc(alloc, sizeof(*a));
    if (!a) return SF_ERR_OOM;
    memset(a, 0, sizeof(*a));
    a->alloc = alloc;
    *out = a;
    return SF_OK;
}

void sf_ani_destroy(sf_ani_t *a) {
    if (!a) return;
    for (size_t i = 0; i < a->node_count; i++) ani_destroy_node(a->nodes[i]);
    sf_xfree(a->alloc, a->nodes);
    sf_xfree(a->alloc, a->translations);
    sf_xfree(a->alloc, a->rotations);
    sf_xfree(a->alloc, a);
}

bool sf_ani_is(const void *bytes, size_t size) {
    if (!bytes || size < 64) return false;
    const uint8_t *p = (const uint8_t *)bytes;
    /* Big-endian int32 0x20051014 -> bytes 20 05 10 14. */
    return p[0] == 0x20 && p[1] == 0x05 && p[2] == 0x10 && p[3] == 0x14;
}

/*===========================================================================
 * Top-level accessors
 *===========================================================================*/

size_t sf_ani_translation_count(const sf_ani_t *a) { return a ? a->translation_count : 0u; }
size_t sf_ani_rotation_count   (const sf_ani_t *a) { return a ? a->rotation_count    : 0u; }
size_t sf_ani_node_count       (const sf_ani_t *a) { return a ? a->node_count        : 0u; }

sf_result_t sf_ani_get_translation(const sf_ani_t *a, size_t index, sf_vec3_t *out) {
    SF_CHECK_ARG(a != NULL && out != NULL);
    if (index >= a->translation_count) return SF_ERR_OUT_OF_RANGE;
    *out = a->translations[index];
    return SF_OK;
}

sf_result_t sf_ani_add_translation(sf_ani_t *a, sf_vec3_t v) {
    SF_CHECK_ARG(a != NULL);
    TRY(ani_grow_vec3_array(a->alloc, &a->translations,
                            a->translation_count, &a->translation_cap));
    a->translations[a->translation_count++] = v;
    return SF_OK;
}

sf_result_t sf_ani_get_rotation(const sf_ani_t *a, size_t index, sf_vec3_t *out) {
    SF_CHECK_ARG(a != NULL && out != NULL);
    if (index >= a->rotation_count) return SF_ERR_OUT_OF_RANGE;
    *out = a->rotations[index];
    return SF_OK;
}

sf_result_t sf_ani_add_rotation(sf_ani_t *a, sf_vec3_t v) {
    SF_CHECK_ARG(a != NULL);
    TRY(ani_grow_vec3_array(a->alloc, &a->rotations,
                            a->rotation_count, &a->rotation_cap));
    a->rotations[a->rotation_count++] = v;
    return SF_OK;
}

sf_ani_node_t *sf_ani_node_at(const sf_ani_t *a, size_t index) {
    if (!a || index >= a->node_count) return NULL;
    return a->nodes[index];
}

sf_result_t sf_ani_add_node(sf_ani_t *a, sf_ani_node_t **out_node) {
    SF_CHECK_ARG(a != NULL);
    if (out_node) *out_node = NULL;
    TRY(ani_grow_nodes(a));
    sf_ani_node_t *n = (sf_ani_node_t *)sf_xalloc(a->alloc, sizeof(*n));
    if (!n) return SF_ERR_OOM;
    n->alloc = a->alloc;
    ani_node_init_defaults(n);
    /* Mirror upstream default name "" — store an empty heap string so the
     * write path's WriteShiftJIS(name, true) always has a buffer to emit. */
    n->name = (char *)sf_xalloc(a->alloc, 1);
    if (!n->name) { sf_xfree(a->alloc, n); return SF_ERR_OOM; }
    n->name[0] = '\0';
    a->nodes[a->node_count++] = n;
    if (out_node) *out_node = n;
    return SF_OK;
}

/*===========================================================================
 * Node field accessors
 *===========================================================================*/

sf_ani_node_type_t sf_ani_node_type(const sf_ani_node_t *n) {
    return n ? n->type : SF_ANI_NODE_TYPE_GEOM;
}
void sf_ani_node_set_type(sf_ani_node_t *n, sf_ani_node_type_t t) {
    if (n) n->type = t;
}

int16_t sf_ani_node_geom_index       (const sf_ani_node_t *n) { return n ? n->geom_index         : -1; }
int16_t sf_ani_node_parent_index     (const sf_ani_node_t *n) { return n ? n->parent_index       : -1; }
int16_t sf_ani_node_first_child_index(const sf_ani_node_t *n) { return n ? n->first_child_index  : -1; }
int16_t sf_ani_node_next_sibling_index(const sf_ani_node_t *n){ return n ? n->next_sibling_index : -1; }
int16_t sf_ani_node_unk_index_12     (const sf_ani_node_t *n) { return n ? n->unk_index_12       : -1; }

void sf_ani_node_set_geom_index        (sf_ani_node_t *n, int16_t v) { if (n) n->geom_index         = v; }
void sf_ani_node_set_parent_index      (sf_ani_node_t *n, int16_t v) { if (n) n->parent_index       = v; }
void sf_ani_node_set_first_child_index (sf_ani_node_t *n, int16_t v) { if (n) n->first_child_index  = v; }
void sf_ani_node_set_next_sibling_index(sf_ani_node_t *n, int16_t v) { if (n) n->next_sibling_index = v; }
void sf_ani_node_set_unk_index_12      (sf_ani_node_t *n, int16_t v) { if (n) n->unk_index_12       = v; }

static const sf_vec3_t SF_VEC3_ZERO_ = { 0.0f, 0.0f, 0.0f };

sf_vec3_t sf_ani_node_translation(const sf_ani_node_t *n) { return n ? n->translation : SF_VEC3_ZERO_; }
sf_vec3_t sf_ani_node_rotation   (const sf_ani_node_t *n) { return n ? n->rotation    : SF_VEC3_ZERO_; }
sf_vec3_t sf_ani_node_scale      (const sf_ani_node_t *n) { return n ? n->scale       : SF_VEC3_ZERO_; }

void sf_ani_node_set_translation(sf_ani_node_t *n, sf_vec3_t v) { if (n) n->translation = v; }
void sf_ani_node_set_rotation   (sf_ani_node_t *n, sf_vec3_t v) { if (n) n->rotation    = v; }
void sf_ani_node_set_scale      (sf_ani_node_t *n, sf_vec3_t v) { if (n) n->scale       = v; }

const char *sf_ani_node_name(const sf_ani_node_t *n) {
    if (!n) return "";
    return n->name ? n->name : "";
}

sf_result_t sf_ani_node_set_name(sf_ani_node_t *n, const char *utf8) {
    SF_CHECK_ARG(n != NULL);
    const char *src = utf8 ? utf8 : "";
    size_t len = strlen(src);
    char *dup = (char *)sf_xalloc(n->alloc, len + 1);
    if (!dup) return SF_ERR_OOM;
    memcpy(dup, src, len + 1);
    sf_xfree(n->alloc, n->name);
    n->name = dup;
    return SF_OK;
}

/*===========================================================================
 * Node animation accessors
 *===========================================================================*/

sf_ani_node_animation_t *sf_ani_node_animation(const sf_ani_node_t *n) {
    return n ? n->animation : NULL;
}

sf_result_t sf_ani_node_create_animation(sf_ani_node_t *n,
                                         sf_ani_node_animation_t **out_anim) {
    SF_CHECK_ARG(n != NULL);
    if (out_anim) *out_anim = NULL;
    if (n->animation) return SF_ERR_ALREADY_EXISTS;
    sf_ani_node_animation_t *anim = (sf_ani_node_animation_t *)sf_xalloc(
        n->alloc, sizeof(*anim));
    if (!anim) return SF_ERR_OOM;
    memset(anim, 0, sizeof(*anim));
    anim->alloc  = n->alloc;
    anim->format = SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS; /* upstream default. */
    n->animation = anim;
    if (out_anim) *out_anim = anim;
    return SF_OK;
}

void sf_ani_node_clear_animation(sf_ani_node_t *n) {
    if (!n) return;
    ani_destroy_animation(n->animation);
    n->animation = NULL;
}

sf_ani_frame_format_t sf_ani_animation_format(const sf_ani_node_animation_t *anim) {
    return anim ? anim->format : SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS;
}

void sf_ani_animation_set_format(sf_ani_node_animation_t *anim, sf_ani_frame_format_t fmt) {
    if (anim) anim->format = fmt;
}

sf_vec3_t sf_ani_animation_unk10(const sf_ani_node_animation_t *anim) {
    return anim ? anim->unk10 : SF_VEC3_ZERO_;
}
sf_vec3_t sf_ani_animation_unk20(const sf_ani_node_animation_t *anim) {
    return anim ? anim->unk20 : SF_VEC3_ZERO_;
}
void sf_ani_animation_set_unk10(sf_ani_node_animation_t *anim, sf_vec3_t v) {
    if (anim) anim->unk10 = v;
}
void sf_ani_animation_set_unk20(sf_ani_node_animation_t *anim, sf_vec3_t v) {
    if (anim) anim->unk20 = v;
}

size_t sf_ani_animation_frame_count(const sf_ani_node_animation_t *anim) {
    return anim ? anim->frame_count : 0u;
}

sf_result_t sf_ani_animation_get_frame(const sf_ani_node_animation_t *anim, size_t index,
                                       sf_ani_frame_t *out) {
    SF_CHECK_ARG(anim != NULL && out != NULL);
    if (index >= anim->frame_count) return SF_ERR_OUT_OF_RANGE;
    *out = anim->frames[index];
    return SF_OK;
}

sf_result_t sf_ani_animation_add_frame(sf_ani_node_animation_t *anim, sf_ani_frame_t frame) {
    SF_CHECK_ARG(anim != NULL);
    TRY(ani_grow_frames(anim));
    anim->frames[anim->frame_count++] = frame;
    return SF_OK;
}

/*===========================================================================
 * Read path — mirrors ANI.Read (ANI.cs:36-85) and its nested Read methods.
 *===========================================================================*/

static sf_result_t ani_read_vec3_short(sf_binary_reader_t *r, sf_vec3_t *out) {
    int16_t x, y, z;
    TRY(sf_binary_reader_read_i16(r, &x));
    TRY(sf_binary_reader_read_i16(r, &y));
    TRY(sf_binary_reader_read_i16(r, &z));
    out->x = (float)x / 1000.0f;
    out->y = (float)y / 1000.0f;
    out->z = (float)z / 1000.0f;
    return SF_OK;
}

/* Mirrors NodeAnimation.Frame.Read (ANI.cs:527-562). */
static sf_result_t ani_read_frame(sf_binary_reader_t *r, sf_ani_frame_format_t fmt,
                                  sf_ani_frame_t *out) {
    memset(out, 0, sizeof(*out));
    TRY(sf_binary_reader_read_i16(r, &out->key_frame));
    switch (fmt) {
    case SF_ANI_FRAME_FORMAT_POS_ROT_BYTES: {
        int8_t b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->translation_index             = (int16_t)(uint8_t)b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->translation_in_tangent_index  = (int16_t)(uint8_t)b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->translation_out_tangent_index = (int16_t)(uint8_t)b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->rotation_index                = (int16_t)(uint8_t)b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->rotation_in_tangent_index     = (int16_t)(uint8_t)b;
        TRY(sf_binary_reader_read_i8(r, &b)); out->rotation_out_tangent_index    = (int16_t)(uint8_t)b;
        out->unk_index = 1;
        return SF_OK;
    }
    case SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS:
        TRY(sf_binary_reader_read_i16(r, &out->translation_index));
        TRY(sf_binary_reader_read_i16(r, &out->translation_in_tangent_index));
        TRY(sf_binary_reader_read_i16(r, &out->translation_out_tangent_index));
        TRY(sf_binary_reader_read_i16(r, &out->rotation_index));
        TRY(sf_binary_reader_read_i16(r, &out->rotation_in_tangent_index));
        TRY(sf_binary_reader_read_i16(r, &out->rotation_out_tangent_index));
        TRY(sf_binary_reader_read_i16(r, &out->unk_index));
        return SF_OK;
    case SF_ANI_FRAME_FORMAT_ROT_SHORTS:
        out->translation_index             = -1;
        out->translation_in_tangent_index  = -1;
        out->translation_out_tangent_index = -1;
        TRY(sf_binary_reader_read_i16(r, &out->rotation_index));
        TRY(sf_binary_reader_read_i16(r, &out->rotation_in_tangent_index));
        TRY(sf_binary_reader_read_i16(r, &out->rotation_out_tangent_index));
        out->unk_index = 1;
        return SF_OK;
    default:
        return SF_ERR_INVALID_ARG;
    }
}

/* Mirrors NodeAnimation.Read (ANI.cs:428-441). */
static sf_result_t ani_read_animation(sf_binary_reader_t *r, sf_ani_node_animation_t *anim) {
    int32_t frames_offset = 0;
    int32_t frame_count   = 0;
    uint32_t format_u32   = 0;
    static const uint32_t k_format_options[] = {
        (uint32_t)SF_ANI_FRAME_FORMAT_POS_ROT_BYTES,
        (uint32_t)SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS,
        (uint32_t)SF_ANI_FRAME_FORMAT_ROT_SHORTS,
    };

    TRY(sf_binary_reader_read_i32(r, &frames_offset));
    TRY(sf_binary_reader_read_i32(r, &frame_count));
    TRY(sf_binary_reader_read_enum_32(r, sizeof(k_format_options)/sizeof(k_format_options[0]),
                                      k_format_options, &format_u32));
    anim->format = (sf_ani_frame_format_t)format_u32;
    TRY(sf_binary_reader_read_vec3(r, &anim->unk10));
    TRY(sf_binary_reader_read_vec3(r, &anim->unk20));
    TRY(sf_binary_reader_assert_i32_one(r, 0));

    if (frame_count < 0) return SF_ERR_INVALID_ARG;
    if (frame_count == 0) return SF_OK;

    TRY(sf_binary_reader_step_in(r, (int64_t)frames_offset));
    sf_result_t e = SF_OK;
    for (int32_t i = 0; i < frame_count; i++) {
        sf_ani_frame_t frame;
        e = ani_read_frame(r, anim->format, &frame);
        if (e != SF_OK) break;
        e = sf_ani_animation_add_frame(anim, frame);
        if (e != SF_OK) break;
    }
    sf_result_t e2 = sf_binary_reader_step_out(r);
    return (e != SF_OK) ? e : e2;
}

/* Mirrors Node.Read (ANI.cs:294-323). */
static sf_result_t ani_read_node(sf_binary_reader_t *r, sf_ani_node_t *n, int32_t node_index) {
    int32_t name_offset = 0;
    int32_t animation_offset = 0;
    int32_t unknown_data_offset = 0;
    uint32_t type_u32 = 0;
    static const uint32_t k_type_options[] = {
        (uint32_t)SF_ANI_NODE_TYPE_GEOM,
        (uint32_t)SF_ANI_NODE_TYPE_DUMMY,
    };

    TRY(sf_binary_reader_read_i32(r, &name_offset));
    if (name_offset < 1) return SF_ERR_INVALID_ARG;

    char *name_utf8 = NULL;
    TRY(sf_binary_reader_get_shift_jis(r, (int64_t)name_offset, &name_utf8, NULL));
    sf_xfree(n->alloc, n->name);
    n->name = name_utf8; /* Borrowed by node; freed in ani_destroy_node. */

    TRY(sf_binary_reader_read_enum_32(r, sizeof(k_type_options)/sizeof(k_type_options[0]),
                                      k_type_options, &type_u32));
    n->type = (sf_ani_node_type_t)type_u32;

    TRY(sf_binary_reader_assert_i16_one(r, (int16_t)node_index));
    TRY(sf_binary_reader_read_i16(r, &n->geom_index));
    TRY(sf_binary_reader_read_i16(r, &n->parent_index));
    TRY(sf_binary_reader_read_i16(r, &n->first_child_index));
    TRY(sf_binary_reader_read_i16(r, &n->next_sibling_index));
    TRY(sf_binary_reader_read_i16(r, &n->unk_index_12));
    TRY(sf_binary_reader_read_vec3(r, &n->translation));
    TRY(sf_binary_reader_read_vec3(r, &n->rotation));
    TRY(sf_binary_reader_read_vec3(r, &n->scale));
    TRY(sf_binary_reader_read_i32(r, &animation_offset));
    TRY(sf_binary_reader_assert_pattern(r, 4, 0));
    TRY(sf_binary_reader_read_i32(r, &unknown_data_offset));
    (void)unknown_data_offset; /* discarded per upstream. */
    TRY(sf_binary_reader_assert_pattern(r, 176, 0));

    if (animation_offset > 0) {
        sf_ani_node_animation_t *anim = NULL;
        TRY(sf_ani_node_create_animation(n, &anim));
        TRY(sf_binary_reader_step_in(r, (int64_t)animation_offset));
        sf_result_t e = ani_read_animation(r, anim);
        sf_result_t e2 = sf_binary_reader_step_out(r);
        if (e  != SF_OK) return e;
        if (e2 != SF_OK) return e2;
    }
    return SF_OK;
}

sf_result_t sf_ani_read_from_memory(sf_ani_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t       *s   = NULL;
    sf_binary_reader_t *r   = NULL;
    sf_ani_t           *ani = NULL;
    sf_result_t         e   = SF_OK;

    alloc = sf_alloc_or_default(alloc);

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, true /* big_endian */, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_i32_one(r, ANI_MAGIC); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0);         if (e != SF_OK) goto done;

    int32_t frame_count = 0;
    int32_t nodes_offset = 0, node_count = 0;
    int32_t translations_offset = 0, rotations_offset = 0;
    int32_t translation_count = 0, rotation_count = 0;
    int32_t data_size = 0;

    e = sf_binary_reader_read_i32(r, &frame_count);         if (e != SF_OK) goto done;
    (void)frame_count; /* derived on write; discarded on read per upstream. */
    e = sf_binary_reader_read_i32(r, &nodes_offset);        if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &node_count);          if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &translations_offset); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &rotations_offset);    if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &translation_count);   if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &rotation_count);      if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &data_size);           if (e != SF_OK) goto done;

    /* Upstream: "data size value was greater than stream size" check, then
     * AssertPattern of (length - data_size) zero bytes at the tail. */
    int64_t stream_len = sf_binary_reader_length(r);
    if ((int64_t)data_size > stream_len) { e = SF_ERR_INVALID_ARG; goto done; }
    if ((int64_t)data_size < stream_len) {
        e = sf_binary_reader_step_in(r, (int64_t)data_size);
        if (e == SF_OK) {
            e = sf_binary_reader_assert_pattern(r,
                (size_t)(stream_len - (int64_t)data_size), 0);
            sf_result_t e2 = sf_binary_reader_step_out(r);
            if (e == SF_OK) e = e2;
        }
        if (e != SF_OK) goto done;
    }

    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 1); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 1);  if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 1);  if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_pattern(r, 70, 0); if (e != SF_OK) goto done;

    if (node_count < 0 || translation_count < 0 || rotation_count < 0) {
        e = SF_ERR_INVALID_ARG; goto done;
    }

    e = sf_ani_create(&ani, alloc); if (e != SF_OK) goto done;

    if (translation_count > 0) {
        e = sf_binary_reader_step_in(r, (int64_t)translations_offset);
        if (e != SF_OK) goto done;
        for (int32_t i = 0; i < translation_count; i++) {
            sf_vec3_t v;
            e = sf_binary_reader_read_vec3(r, &v);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            e = sf_ani_add_translation(ani, v);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
        }
        e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
    }

    if (rotation_count > 0) {
        e = sf_binary_reader_step_in(r, (int64_t)rotations_offset);
        if (e != SF_OK) goto done;
        for (int32_t i = 0; i < rotation_count; i++) {
            sf_vec3_t v;
            e = ani_read_vec3_short(r, &v);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            e = sf_ani_add_rotation(ani, v);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
        }
        e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
    }

    if (node_count > 0) {
        e = sf_binary_reader_step_in(r, (int64_t)nodes_offset);
        if (e != SF_OK) goto done;
        for (int32_t i = 0; i < node_count; i++) {
            sf_ani_node_t *n = NULL;
            e = sf_ani_add_node(ani, &n);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            e = ani_read_node(r, n, i);
            if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
        }
        e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_ani_destroy(ani); return e; }
    *out = ani;
    return SF_OK;
}

/*===========================================================================
 * Write path — mirrors ANI.Write (ANI.cs:91-135).
 *===========================================================================*/

static int32_t ani_compute_key_frame_count(const sf_ani_t *a) {
    int32_t value = 0;
    for (size_t i = 0; i < a->node_count; i++) {
        const sf_ani_node_animation_t *anim = a->nodes[i]->animation;
        if (!anim) continue;
        for (size_t j = 0; j < anim->frame_count; j++) {
            int16_t kf = anim->frames[j].key_frame;
            if ((int32_t)kf > value) value = (int32_t)kf;
        }
    }
    return value;
}

static sf_result_t ani_write_vec3_short(sf_binary_writer_t *w, sf_vec3_t v) {
    TRY(sf_binary_writer_write_i16(w, (int16_t)(v.x * 1000.0f)));
    TRY(sf_binary_writer_write_i16(w, (int16_t)(v.y * 1000.0f)));
    TRY(sf_binary_writer_write_i16(w, (int16_t)(v.z * 1000.0f)));
    return SF_OK;
}

/* Mirrors NodeAnimation.Frame.Write (ANI.cs:570-600).
 *
 * NOTE: Upstream's C# write emits the keyframe as a 32-bit value
 * (`bw.WriteInt32(KeyFrame)`) while its read consumes it as 16 bits
 * (`KeyFrame = br.ReadInt16()`). That asymmetry would make any output
 * unreadable by the same library — clearly an upstream defect. We mirror
 * the read path (int16 keyframe) for both directions so round-trips
 * succeed; the resulting frame strides exactly match the documented
 * per-format byte counts (8 / 16 / 8). */
static sf_result_t ani_write_frame(sf_binary_writer_t *w, sf_ani_frame_format_t fmt,
                                   const sf_ani_frame_t *fr) {
    TRY(sf_binary_writer_write_i16(w, fr->key_frame));
    switch (fmt) {
    case SF_ANI_FRAME_FORMAT_POS_ROT_BYTES:
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->translation_index));
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->translation_in_tangent_index));
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->translation_out_tangent_index));
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->rotation_index));
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->rotation_in_tangent_index));
        TRY(sf_binary_writer_write_u8(w, (uint8_t)fr->rotation_out_tangent_index));
        return SF_OK;
    case SF_ANI_FRAME_FORMAT_POS_ROT_SHORTS:
        TRY(sf_binary_writer_write_i16(w, fr->translation_index));
        TRY(sf_binary_writer_write_i16(w, fr->translation_in_tangent_index));
        TRY(sf_binary_writer_write_i16(w, fr->translation_out_tangent_index));
        TRY(sf_binary_writer_write_i16(w, fr->rotation_index));
        TRY(sf_binary_writer_write_i16(w, fr->rotation_in_tangent_index));
        TRY(sf_binary_writer_write_i16(w, fr->rotation_out_tangent_index));
        TRY(sf_binary_writer_write_i16(w, fr->unk_index));
        return SF_OK;
    case SF_ANI_FRAME_FORMAT_ROT_SHORTS:
        TRY(sf_binary_writer_write_i16(w, fr->rotation_index));
        TRY(sf_binary_writer_write_i16(w, fr->rotation_in_tangent_index));
        TRY(sf_binary_writer_write_i16(w, fr->rotation_out_tangent_index));
        return SF_OK;
    default:
        return SF_ERR_INVALID_ARG;
    }
}

/* Mirrors NodeAnimation.Write (ANI.cs:447-458). */
static sf_result_t ani_write_animation(sf_binary_writer_t *w,
                                       const sf_ani_node_animation_t *anim) {
    int64_t pos = sf_binary_writer_position(w);
    TRY(sf_binary_writer_write_i32(w, (int32_t)(pos + ANI_ANIM_HEADER)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)anim->frame_count));
    TRY(sf_binary_writer_write_i32(w, (int32_t)anim->format));
    TRY(sf_binary_writer_write_vec3(w, anim->unk10));
    TRY(sf_binary_writer_write_vec3(w, anim->unk20));
    TRY(sf_binary_writer_write_i32(w, 0));
    for (size_t i = 0; i < anim->frame_count; i++) {
        TRY(ani_write_frame(w, anim->format, &anim->frames[i]));
    }
    return SF_OK;
}

/* Mirrors Node.Write (ANI.cs:330-349). */
static sf_result_t ani_write_node_header(sf_binary_writer_t *w, const sf_ani_node_t *n,
                                         size_t node_index) {
    char key[48];
    snprintf(key, sizeof(key), "NodeNameOffset_%zu", node_index);
    TRY(sf_binary_writer_reserve_i32(w, key));
    TRY(sf_binary_writer_write_i32(w, (int32_t)n->type));
    TRY(sf_binary_writer_write_i16(w, (int16_t)node_index));
    TRY(sf_binary_writer_write_i16(w, n->geom_index));
    TRY(sf_binary_writer_write_i16(w, n->parent_index));
    TRY(sf_binary_writer_write_i16(w, n->first_child_index));
    TRY(sf_binary_writer_write_i16(w, n->next_sibling_index));
    TRY(sf_binary_writer_write_i16(w, n->unk_index_12));
    TRY(sf_binary_writer_write_vec3(w, n->translation));
    TRY(sf_binary_writer_write_vec3(w, n->rotation));
    TRY(sf_binary_writer_write_vec3(w, n->scale));
    if (n->animation) {
        snprintf(key, sizeof(key), "AnimationOffset_%zu", node_index);
        TRY(sf_binary_writer_reserve_i32(w, key));
    } else {
        TRY(sf_binary_writer_write_i32(w, 0));
    }
    TRY(sf_binary_writer_write_pattern(w, 184, 0));
    return SF_OK;
}

sf_result_t sf_ani_write_to_memory(const sf_ani_t *a, void **out_bytes, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(a != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t       *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t         e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, true /* big_endian */, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    char key[48];

    e = sf_binary_writer_write_i32(w, ANI_MAGIC);                if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0);                        if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, ani_compute_key_frame_count(a)); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, ANI_HEADER_SIZE);          if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)a->node_count);   if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "TranslationsOffset");   if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "RotationsOffset");      if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)a->translation_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)a->rotation_count);    if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "DataSize");             if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0);                        if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 1);                        if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, 1);                         if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, 1);                         if (e != SF_OK) goto done;
    e = sf_binary_writer_write_pattern(w, 70, 0);                if (e != SF_OK) goto done;

    for (size_t i = 0; i < a->node_count; i++) {
        e = ani_write_node_header(w, a->nodes[i], i); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < a->node_count; i++) {
        const sf_ani_node_t *n = a->nodes[i];
        snprintf(key, sizeof(key), "NodeNameOffset_%zu", i);
        e = sf_binary_writer_fill_i32(w, key, (int32_t)sf_binary_writer_position(w));
        if (e != SF_OK) goto done;
        e = sf_binary_writer_write_shift_jis(w, n->name ? n->name : "", true);
        if (e != SF_OK) goto done;
        if (n->animation) {
            snprintf(key, sizeof(key), "AnimationOffset_%zu", i);
            e = sf_binary_writer_fill_i32(w, key, (int32_t)sf_binary_writer_position(w));
            if (e != SF_OK) goto done;
            e = ani_write_animation(w, n->animation);
            if (e != SF_OK) goto done;
        }
    }

    e = sf_binary_writer_fill_i32(w, "TranslationsOffset",
                                  (int32_t)sf_binary_writer_position(w));
    if (e != SF_OK) goto done;
    for (size_t i = 0; i < a->translation_count; i++) {
        e = sf_binary_writer_write_vec3(w, a->translations[i]); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_fill_i32(w, "RotationsOffset",
                                  (int32_t)sf_binary_writer_position(w));
    if (e != SF_OK) goto done;
    for (size_t i = 0; i < a->rotation_count; i++) {
        e = ani_write_vec3_short(w, a->rotations[i]); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_pad(w, 4); if (e != SF_OK) goto done;
    e = sf_binary_writer_fill_i32(w, "DataSize", (int32_t)sf_binary_writer_position(w));
    if (e != SF_OK) goto done;

    e = sf_binary_writer_pad(w, 16); if (e != SF_OK) goto done;

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
