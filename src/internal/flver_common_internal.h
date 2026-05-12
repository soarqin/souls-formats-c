/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal helpers for FLVER common types (Dummy / Node / LayoutMember).
 * Mirrors upstream Read/Write code paths in:
 *   - SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Dummy.cs
 *   - SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/Node.cs
 *   - SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/LayoutMember.cs
 *
 * NEVER include this from a public header.
 */

#ifndef SF_FLVER_COMMON_INTERNAL_H
#define SF_FLVER_COMMON_INTERNAL_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stddef.h>

/*===========================================================================
 * Dummy — 64-byte fixed-size POD record
 *
 * Layout (upstream Dummy.cs:Read at version != 0x20010):
 *   12 B  Position           Vec3
 *    4 B  Color (ARGB)       u32
 *   12 B  Forward            Vec3
 *    2 B  ReferenceID        i16
 *    2 B  ParentBoneIndex    i16
 *   12 B  Upward             Vec3
 *    2 B  AttachBoneIndex    i16
 *    1 B  Flag1              bool
 *    1 B  UseUpwardVector    bool
 *    4 B  Unk30              i32
 *    4 B  Unk34              i32
 *    4 B  AssertInt32(0)     padding
 *    4 B  AssertInt32(0)     padding
 *  ---
 *   64 B
 *===========================================================================*/

#define SFI_FLVER_DUMMY_SIZE 64u

sf_result_t sfi_flver_dummy_read (sf_binary_reader_t *br, sf_flver_dummy_t *out);
sf_result_t sfi_flver_dummy_write(sf_binary_writer_t *bw, const sf_flver_dummy_t *d);

/*===========================================================================
 * Node — 128-byte fixed-size POD record + string pool
 *
 * Layout (upstream Node.cs:Read):
 *   12 B  Translation        Vec3
 *    4 B  NameOffset         i32   (pointer into string pool)
 *   12 B  Rotation           Vec3
 *    2 B  ParentIndex        i16
 *    2 B  FirstChildIndex    i16
 *   12 B  Scale              Vec3
 *    2 B  NextSiblingIndex   i16
 *    2 B  PreviousSiblingIdx i16
 *   12 B  BoundingBoxMin     Vec3
 *    4 B  Flags              i32 / NodeFlags
 *   12 B  BoundingBoxMax     Vec3
 *   52 B  AssertPattern(0x34, 0x00)
 *  ---
 *  128 B
 *
 * sfi_flver_node_read reads the 128-byte record AND resolves the string at
 * `nameOffset` via Get* (no cursor movement). The string is allocated via
 * `a` and the caller (or sfi_flver_node_destroy) is responsible for freeing
 * it.
 *
 * sfi_flver_node_write writes the 128-byte record using a Reserve_i32 named
 * "BoneNameOffset{index}" — the caller must subsequently invoke a matching
 * sf_binary_writer_fill_i32 once the string section has been positioned.
 * For round-trip tests we also expose a `name_offset_override` variant via
 * `sfi_flver_node_write_with_offset` that bypasses the reservation pattern.
 *===========================================================================*/

#define SFI_FLVER_NODE_SIZE 128u

sf_result_t sfi_flver_node_read (sf_binary_reader_t *br, bool unicode,
                                 sf_flver_node_t *out, const sf_allocator_t *a);

sf_result_t sfi_flver_node_write(sf_binary_writer_t *bw, const sf_flver_node_t *n,
                                 size_t index);

/* Test-only variant that writes a literal `name_offset` instead of a
 * reservation. Lets unit tests round-trip a Node without managing a string
 * section. Production code must use sfi_flver_node_write + Reserve/Fill. */
sf_result_t sfi_flver_node_write_with_offset(sf_binary_writer_t *bw,
                                              const sf_flver_node_t *n,
                                              int32_t name_offset);

/* Destroy ONLY the heap members of a Node (currently `name`). The struct
 * itself is caller-owned. NULL-safe. */
void sfi_flver_node_destroy_inplace(sf_flver_node_t *n, const sf_allocator_t *a);

/*===========================================================================
 * LayoutMember — 4×i32 record (non-SpeedTree path)
 *
 * Layout (upstream LayoutMember.cs:Read):
 *    4 B  Stream             i32
 *    4 B  AssertInt32(structOffset)
 *    4 B  Type               u32  (LayoutType enum)
 *    4 B  Semantic           u32  (LayoutSemantic enum)
 *    4 B  Index              i32
 *  ---
 *   20 B
 *
 * The SpeedTree variant (Stream=i16, SpecialModifier=i16, localOffset=i32)
 * is identified by an outer-context flag; we forward it as an explicit
 * `is_speedtree` parameter.
 *===========================================================================*/

typedef struct sfi_flver_layout_member {
    int32_t                     stream;
    int16_t                     special_modifier; /* SpeedTree only; 0 otherwise */
    sf_flver_layout_type_t      type;
    sf_flver_layout_semantic_t  semantic;
    int32_t                     index;
} sfi_flver_layout_member_t;

#define SFI_FLVER_LAYOUT_MEMBER_SIZE 20u

sf_result_t sfi_flver_layout_member_read (sf_binary_reader_t *br,
                                           int32_t struct_offset, bool is_speedtree,
                                           sfi_flver_layout_member_t *out);

sf_result_t sfi_flver_layout_member_write(sf_binary_writer_t *bw,
                                           int32_t struct_offset, bool is_speedtree,
                                           const sfi_flver_layout_member_t *m);

#endif /* SF_FLVER_COMMON_INTERNAL_H */
