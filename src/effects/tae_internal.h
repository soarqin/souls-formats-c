/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal TAE struct definitions. Mirrors upstream:
 *   SoulsFormats/Formats/TAE/TAE.cs
 *   SoulsFormats/Formats/TAE/Animation.cs
 *   SoulsFormats/Formats/TAE/Event.cs
 *   SoulsFormats/Formats/TAE/EventGroup.cs
 *
 * NEVER include this from a public header. Tests in tests/anim/ include it
 * to construct synthetic TAE objects without going through the binary wire
 * format, mirroring the FLVER2/EMEVD synthetic-test pattern.
 */

#ifndef SF_EFFECTS_TAE_INTERNAL_H
#define SF_EFFECTS_TAE_INTERNAL_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_tae.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sf_tae_event_group {
    int32_t  group_type;
    int32_t *members;
    size_t   member_count;
};

struct sf_tae_event {
    float    start_time;
    float    end_time;
    int32_t  type;
    int32_t  unk04;
    uint8_t *parameters;
    size_t   parameters_size;
};

struct sf_tae_animation {
    int64_t                    id;
    sf_tae_anim_mini_header_t  mini_header;
    sf_tae_event_t           **events;
    size_t                     event_count;
    sf_tae_event_group_t     **event_groups;
    size_t                     event_group_count;
    char                      *anim_file_name;
};

struct sf_tae {
    const sf_allocator_t  *alloc;
    sf_tae_format_t        format;
    int32_t                id;
    uint8_t                flags[8];
    char                  *skeleton_name;
    char                  *sib_name;
    int64_t                event_bank;
    sf_tae_animation_t   **animations;
    size_t                 animation_count;
};

#endif /* SF_EFFECTS_TAE_INTERNAL_H */
