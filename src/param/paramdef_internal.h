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
#include "souls_formats/sf_param.h"

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

typedef struct sf_paramdef_field_layout_entry {
    const sf_paramdef_field_t *field;
    size_t byte_offset;
    size_t byte_count;
    size_t bit_offset;
    size_t bit_size;
    size_t bit_limit;
    sf_paramdef_def_type_t display_type;
    sf_param_cell_kind_t cell_kind;
    int32_t array_length;
    int32_t declared_byte_count;
    int32_t declared_bit_size;
    bool is_bit_field;
    bool check_orphaned_bits_after;
} sf_paramdef_field_layout_entry_t;

typedef struct sf_paramdef_field_layout {
    sf_paramdef_field_layout_entry_t *entries;
    size_t entry_count;
    size_t row_data_size;
} sf_paramdef_field_layout_t;

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
    sf_paramdef_field_layout_t *layout_cache;
};

/* Compute paramdef row size with the same semantics as
 * SoulsFormatsNEXT/PARAMDEF.cs `GetFieldsSize(field_count, ulong.MaxValue)`:
 *  - For version-aware defs, skip fields whose removed_regulation_version != 0
 *    (a field that has been removed is invalid when reading with MaxValue).
 *  - Fold runs of consecutive sized bit fields that share a byte window so the
 *    second/third/... bit field in the same byte does not double-count.
 * The previous implementation summed every field's `byte_count`, which
 * over-counted bitfields and produced row sizes that disagreed with both the
 * binary PARAM's detected row stride and the upstream C# reference.
 * Defined in paramdef.c; declared here so both binary and XML readers can
 * call it after parsing all fields. */
int32_t sf_paramdef_internal_compute_row_size(const sf_paramdef_t *def);

#endif /* SF_PARAMDEF_INTERNAL_H */
