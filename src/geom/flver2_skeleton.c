/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER2 SkeletonSet — Sekiro-forward skeleton + control-rig block.
 *
 * Strictly mirrors upstream:
 *   SoulsFormats/Formats/FLVER/FLVER2/SkeletonSet.cs
 * at the pinned commit (see docs/api-mapping/UPSTREAM.md).
 *
 * Wire layout (only present when FLVER header version >= 0x2001A):
 *
 *   SkeletonSet header (32 bytes):
 *     i16  count1                — BaseSkeleton bone count
 *     i16  count2                — AllSkeletons bone count
 *     u32  offset1               — file-absolute BaseSkeleton offset
 *     u32  offset2               — file-absolute AllSkeletons offset
 *     5 × i32 0                  — AssertInt32(0) ×5 (20 bytes zero pad)
 *
 *   Two contiguous Bone arrays at the recorded offsets. Each Bone is
 *   exactly 16 bytes on disk:
 *     i16  ParentIndex
 *     i16  FirstChildIndex
 *     i16  NextSiblingIndex
 *     i16  PreviousSiblingIndex
 *     i32  NodeIndex
 *     i32  0                     — AssertInt32(0)
 *
 * Pre-Elden-Ring builds (Sekiro 0x20013, DS3 0x20014, …) DO NOT emit a
 * SkeletonSet block. For those, `sfi_flver2_skeleton_set_read` leaves
 * *out == NULL and returns SF_OK so the public accessor
 * `sf_flver2_skeleton_set()` correctly reports "no skeleton".
 */

#include "souls_formats/sf_flver2.h"
#include "souls_formats/sf_io.h"

#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"

#include <stdint.h>
#include <string.h>

/* First FLVER2 version that emits a SkeletonSet. Sekiro (0x20013) is below;
 * Elden Ring / Nightreign / AC6 (0x2001A and forward) all carry one. */
#define SF_FLVER2_SKELETON_MIN_VERSION 0x2001Au

/* Trailing AssertInt32(0) words at the end of the SkeletonSet header. */
#define SF_FLVER2_SKELETON_HEADER_ZERO_COUNT 5

/* Exact size of a Bone on disk. Used by post-read invariant checks. */
#define SF_FLVER2_BONE_DISK_SIZE 16

static sf_result_t flver2_skeleton_bone_read(sf_binary_reader_t *br,
                                             sf_flver2_bone_t *out) {
    sf_result_t r;
    r = sf_binary_reader_read_i16(br, &out->parent_index);           if (r != SF_OK) return r;
    r = sf_binary_reader_read_i16(br, &out->first_child_index);      if (r != SF_OK) return r;
    r = sf_binary_reader_read_i16(br, &out->next_sibling_index);     if (r != SF_OK) return r;
    r = sf_binary_reader_read_i16(br, &out->previous_sibling_index); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->node_index);             if (r != SF_OK) return r;
    return sf_binary_reader_assert_i32_one(br, 0);
}

static sf_result_t flver2_skeleton_bone_write(sf_binary_writer_t *bw,
                                              const sf_flver2_bone_t *b) {
    sf_result_t r;
    r = sf_binary_writer_write_i16(bw, b->parent_index);           if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, b->first_child_index);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, b->next_sibling_index);     if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, b->previous_sibling_index); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, b->node_index);             if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t flver2_skeleton_read_bone_array(sf_binary_reader_t *br,
                                                   uint32_t offset,
                                                   sf_flver2_bone_t *bones,
                                                   size_t count) {
    sf_result_t r = sf_binary_reader_step_in(br, (int64_t)offset);
    if (r != SF_OK) return r;

    int64_t start = sf_binary_reader_position(br);
    for (size_t i = 0; i < count; i++) {
        r = flver2_skeleton_bone_read(br, &bones[i]);
        if (r != SF_OK) {
            (void)sf_binary_reader_step_out(br);
            return r;
        }
    }
    int64_t end = sf_binary_reader_position(br);

    sf_result_t step_out_r = sf_binary_reader_step_out(br);
    if (step_out_r != SF_OK) return step_out_r;

    /* Defensive invariant: each Bone must consume exactly 16 bytes. */
    if (end - start != (int64_t)(count * SF_FLVER2_BONE_DISK_SIZE)) {
        return SF_ERR_INTERNAL;
    }
    return SF_OK;
}

sf_result_t sfi_flver2_skeleton_set_read(sf_binary_reader_t *br,
                                         const sf_flver2_header_t *hdr,
                                         sf_flver2_skeleton_set_t **out,
                                         const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && hdr != NULL && out != NULL);
    *out = NULL;

    /* Version gate: pre-Elden-Ring builds do not write a SkeletonSet block. */
    if (hdr->version < SF_FLVER2_SKELETON_MIN_VERSION) {
        return SF_OK;
    }

    int16_t  count1 = 0, count2 = 0;
    uint32_t offset1 = 0, offset2 = 0;
    sf_result_t r;
    r = sf_binary_reader_read_i16(br, &count1);   if (r != SF_OK) return r;
    r = sf_binary_reader_read_i16(br, &count2);   if (r != SF_OK) return r;
    r = sf_binary_reader_read_u32(br, &offset1);  if (r != SF_OK) return r;
    r = sf_binary_reader_read_u32(br, &offset2);  if (r != SF_OK) return r;
    for (size_t i = 0; i < SF_FLVER2_SKELETON_HEADER_ZERO_COUNT; i++) {
        r = sf_binary_reader_assert_i32_one(br, 0);
        if (r != SF_OK) return r;
    }
    if (count1 < 0 || count2 < 0) return SF_ERR_OUT_OF_RANGE;

    sf_flver2_skeleton_set_t *set =
        (sf_flver2_skeleton_set_t *)sf_xalloc(a, sizeof(*set));
    if (!set) return SF_ERR_OOM;
    memset(set, 0, sizeof(*set));

    if (count1 > 0) {
        set->base_bones = (sf_flver2_bone_t *)sf_xalloc(
            a, (size_t)count1 * sizeof(*set->base_bones));
        if (!set->base_bones) {
            sfi_flver2_skeleton_set_destroy(set, a);
            return SF_ERR_OOM;
        }
        memset(set->base_bones, 0, (size_t)count1 * sizeof(*set->base_bones));
    }
    set->base_bone_count = (size_t)count1;

    if (count2 > 0) {
        set->all_bones = (sf_flver2_bone_t *)sf_xalloc(
            a, (size_t)count2 * sizeof(*set->all_bones));
        if (!set->all_bones) {
            sfi_flver2_skeleton_set_destroy(set, a);
            return SF_ERR_OOM;
        }
        memset(set->all_bones, 0, (size_t)count2 * sizeof(*set->all_bones));
    }
    set->all_bone_count = (size_t)count2;

    r = flver2_skeleton_read_bone_array(br, offset1, set->base_bones,
                                        set->base_bone_count);
    if (r != SF_OK) {
        sfi_flver2_skeleton_set_destroy(set, a);
        return r;
    }
    r = flver2_skeleton_read_bone_array(br, offset2, set->all_bones,
                                        set->all_bone_count);
    if (r != SF_OK) {
        sfi_flver2_skeleton_set_destroy(set, a);
        return r;
    }

    *out = set;
    return SF_OK;
}

sf_result_t sfi_flver2_skeleton_set_write(sf_binary_writer_t *bw,
                                          const sf_flver2_header_t *hdr,
                                          const sf_flver2_skeleton_set_t *set) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL);

    /* Older versions never emit the block, even if the caller hands us one. */
    if (hdr->version < SF_FLVER2_SKELETON_MIN_VERSION) {
        return SF_OK;
    }

    size_t base_count = set ? set->base_bone_count : 0;
    size_t all_count  = set ? set->all_bone_count  : 0;
    if (base_count > (size_t)INT16_MAX || all_count > (size_t)INT16_MAX) {
        return SF_ERR_OUT_OF_RANGE;
    }

    sf_result_t r;
    r = sf_binary_writer_write_i16(bw, (int16_t)base_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, (int16_t)all_count);  if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_u32(bw, "BaseSkeletonOffset");    if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_u32(bw, "ControlSkeletonOffset"); if (r != SF_OK) return r;
    for (size_t i = 0; i < SF_FLVER2_SKELETON_HEADER_ZERO_COUNT; i++) {
        r = sf_binary_writer_write_i32(bw, 0);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_fill_u32(bw, "BaseSkeletonOffset",
                                  (uint32_t)sf_binary_writer_position(bw));
    if (r != SF_OK) return r;
    for (size_t i = 0; i < base_count; i++) {
        r = flver2_skeleton_bone_write(bw, &set->base_bones[i]);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_fill_u32(bw, "ControlSkeletonOffset",
                                  (uint32_t)sf_binary_writer_position(bw));
    if (r != SF_OK) return r;
    for (size_t i = 0; i < all_count; i++) {
        r = flver2_skeleton_bone_write(bw, &set->all_bones[i]);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

void sfi_flver2_skeleton_set_destroy(sf_flver2_skeleton_set_t *set,
                                     const sf_allocator_t *a) {
    if (!set) return;
    sf_xfree(a, set->base_bones);
    sf_xfree(a, set->all_bones);
    sf_xfree(a, set);
}
