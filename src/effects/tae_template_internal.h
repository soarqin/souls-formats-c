/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal TAE template struct definitions. Mirrors upstream:
 *   SoulsFormats/Formats/TAE/Template.cs
 *
 * NEVER include this from a public header.
 */

#ifndef SF_EFFECTS_TAE_TEMPLATE_INTERNAL_H
#define SF_EFFECTS_TAE_TEMPLATE_INTERNAL_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_tae_template.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sf_tae_enum_entry {
    char   *name;
    int64_t value;
} sf_tae_enum_entry_t;

struct sf_tae_param_template {
    sf_tae_param_type_t  type;
    char                *name;
    char                *name_group;
    char                *key;
    int32_t              aob_length;
    bool                 has_assert;
    uint8_t             *assert_bytes;
    bool                 has_default;
    uint8_t             *default_bytes;
    sf_tae_enum_entry_t *enum_entries;
    size_t               enum_count;
};

struct sf_tae_event_template {
    int32_t                   id;
    char                     *name;
    sf_tae_param_template_t **params;
    size_t                    param_count;
    int32_t                   total_byte_count;
};

struct sf_tae_bank_template {
    int64_t                    id;
    char                      *name;
    sf_tae_event_template_t  **events;
    size_t                     event_count;
};

struct sf_tae_template {
    const sf_allocator_t      *alloc;
    sf_tae_format_t            game;
    sf_tae_bank_template_t   **banks;
    size_t                     bank_count;
};

#endif /* SF_EFFECTS_TAE_TEMPLATE_INTERNAL_H */
