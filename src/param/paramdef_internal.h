/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — internal PARAMDEF structures shared by PARAMDEF modules.
 *
 * Both the binary reader/writer (paramdef.c) and the XML reader
 * (paramdef_xml_read.c) need to mutate paramdef state directly. This
 * header centralises the layout. It is internal-only — never included
 * from public headers.
 */

#ifndef SF_PARAMDEF_INTERNAL_H
#define SF_PARAMDEF_INTERNAL_H

#include "souls_formats/sf_paramdef.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sf_paramdef_field {
    char *display_name;
    char *internal_type;
    char *internal_name;
    char *description;
    char *display_format;

    sf_paramdef_def_type_t display_type;
    sf_paramdef_default_value_t default_value;
    sf_paramdef_default_value_t minimum;
    sf_paramdef_default_value_t maximum;
    sf_paramdef_default_value_t increment;
    sf_paramdef_edit_flags_t edit_flags;
    int32_t byte_count;
    int32_t bit_size;
    int32_t array_length;
    int32_t sort_id;
    uint64_t first_regulation_version;
    uint64_t removed_regulation_version;
};

struct sf_paramdef {
    const sf_allocator_t *alloc;
    sf_paramdef_field_t *fields;
    size_t field_count;

    char *param_type;
    int16_t data_version;
    int16_t format_version;
    int32_t row_size;
    int32_t index;
    bool big_endian;
    bool unicode;
    bool version_aware;
    bool basic_fields;
};

#endif /* SF_PARAMDEF_INTERNAL_H */
