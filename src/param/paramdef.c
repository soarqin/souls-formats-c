/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDEF binary reader.
 *
 * Mirrors pinned upstream:
 *   SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs:85-145
 *   SoulsFormats/Formats/PARAM/PARAMDEF/Field.cs:239-405
 */

#include "souls_formats/sf_paramdef.h"

#include "internal/sf_internal.h"
#include "param/paramdef_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void field_free(sf_paramdef_field_t *f, const sf_allocator_t *a) {
    if (!f) return;
    sf_xfree(a, f->display_name);
    sf_xfree(a, f->internal_type);
    sf_xfree(a, f->internal_name);
    sf_xfree(a, f->description);
    sf_xfree(a, f->display_format);
    memset(f, 0, sizeof(*f));
}

static void field_layout_free(sf_paramdef_field_layout_t *layout,
                              const sf_allocator_t *a) {
    if (!layout) return;
    sf_xfree(a, layout->entries);
    sf_xfree(a, layout);
}

void sf_paramdef_destroy(sf_paramdef_t *paramdef) {
    if (!paramdef) return;
    const sf_allocator_t *a = paramdef->alloc;
    field_layout_free(paramdef->layout_cache, a);
    for (size_t i = 0; i < paramdef->field_count; i++) field_free(&paramdef->fields[i], a);
    sf_xfree(a, paramdef->fields);
    sf_xfree(a, paramdef->param_type);
    sf_xfree(a, paramdef);
}

static void trim_ascii_in_place(char *s) {
    if (!s) return;
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        s[--n] = '\0';
    }
}

static bool parse_def_type(const char *s, sf_paramdef_def_type_t *out) {
    if (!s || !out) return false;
    if (strcmp(s, "s8") == 0) *out = SF_PARAMDEF_DEF_TYPE_S8;
    else if (strcmp(s, "u8") == 0) *out = SF_PARAMDEF_DEF_TYPE_U8;
    else if (strcmp(s, "s16") == 0) *out = SF_PARAMDEF_DEF_TYPE_S16;
    else if (strcmp(s, "u16") == 0) *out = SF_PARAMDEF_DEF_TYPE_U16;
    else if (strcmp(s, "s32") == 0) *out = SF_PARAMDEF_DEF_TYPE_S32;
    else if (strcmp(s, "u32") == 0) *out = SF_PARAMDEF_DEF_TYPE_U32;
    else if (strcmp(s, "s64") == 0) *out = SF_PARAMDEF_DEF_TYPE_S64;
    else if (strcmp(s, "u64") == 0) *out = SF_PARAMDEF_DEF_TYPE_U64;
    else if (strcmp(s, "b32") == 0) *out = SF_PARAMDEF_DEF_TYPE_B32;
    else if (strcmp(s, "f32") == 0) *out = SF_PARAMDEF_DEF_TYPE_F32;
    else if (strcmp(s, "angle32") == 0) *out = SF_PARAMDEF_DEF_TYPE_ANGLE32;
    else if (strcmp(s, "f64") == 0) *out = SF_PARAMDEF_DEF_TYPE_F64;
    else if (strcmp(s, "dummy8") == 0) *out = SF_PARAMDEF_DEF_TYPE_DUMMY8;
    else if (strcmp(s, "fixstr") == 0) *out = SF_PARAMDEF_DEF_TYPE_FIXSTR;
    else if (strcmp(s, "fixstrW") == 0) *out = SF_PARAMDEF_DEF_TYPE_FIXSTR_W;
    else return false;
    return true;
}

static bool is_array_type(sf_paramdef_def_type_t type) {
    return type == SF_PARAMDEF_DEF_TYPE_U8 || type == SF_PARAMDEF_DEF_TYPE_DUMMY8 ||
           type == SF_PARAMDEF_DEF_TYPE_FIXSTR || type == SF_PARAMDEF_DEF_TYPE_FIXSTR_W;
}

size_t sf_param_util_get_value_size(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        return 1;
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return 2;
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_B32:
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        return 4;
    case SF_PARAMDEF_DEF_TYPE_F64:
    case SF_PARAMDEF_DEF_TYPE_S64:
    case SF_PARAMDEF_DEF_TYPE_U64:
        return 8;
    default:
        return 0;
    }
}

bool sf_param_util_is_bit_type(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        return true;
    default:
        return false;
    }
}

int sf_param_util_get_bit_limit(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
        return 8;
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
        return 16;
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
        return 32;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        return 8;
    default:
        return 0;
    }
}

static bool is_supported_version(int16_t version) {
    switch (version) {
    case 0:
    case 101:
    case 102:
    case 103:
    case 104:
    case 106:
    case 201:
    case 202:
    case 203:
        return true;
    default:
        return false;
    }
}

static int16_t expected_field_size(int16_t version) {
    switch (version) {
    case 0: return 0x68;
    case 101: return 0x8C;
    case 102: return 0xAC;
    case 103: return 0x6C; /* Upstream deliberately preserves this wrong value. */
    case 104: return 0xB0;
    case 106: return 0x48;
    case 201: return 0xD0;
    case 202: return 0x68;
    case 203: return 0x88;
    default: return -1;
    }
}

static bool is_writable_version(int16_t version) {
    switch (version) {
    case 104:
    case 106:
    case 201:
    case 202:
    case 203:
        return true;
    default:
        return false;
    }
}

static const char *def_type_name(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8: return "s8";
    case SF_PARAMDEF_DEF_TYPE_U8: return "u8";
    case SF_PARAMDEF_DEF_TYPE_S16: return "s16";
    case SF_PARAMDEF_DEF_TYPE_U16: return "u16";
    case SF_PARAMDEF_DEF_TYPE_S32: return "s32";
    case SF_PARAMDEF_DEF_TYPE_U32: return "u32";
    case SF_PARAMDEF_DEF_TYPE_S64: return "s64";
    case SF_PARAMDEF_DEF_TYPE_U64: return "u64";
    case SF_PARAMDEF_DEF_TYPE_B32: return "b32";
    case SF_PARAMDEF_DEF_TYPE_F32: return "f32";
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: return "angle32";
    case SF_PARAMDEF_DEF_TYPE_F64: return "f64";
    case SF_PARAMDEF_DEF_TYPE_DUMMY8: return "dummy8";
    case SF_PARAMDEF_DEF_TYPE_FIXSTR: return "fixstr";
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W: return "fixstrW";
    default: return NULL;
    }
}

static sf_result_t reserve_name(char out[64], const char *base, size_t index) {
    int n = snprintf(out, 64, "%s%zu", base, index);
    if (n < 0 || n >= 64) return SF_ERR_OUT_OF_RANGE;
    return SF_OK;
}

static sf_result_t make_internal_name(const sf_paramdef_field_t *field, char **out,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(field != NULL && out != NULL);
    *out = NULL;
    const char *name = field->internal_name ? field->internal_name : "";
    int needed = 0;
    if (field->bit_size != -1) {
        needed = snprintf(NULL, 0, "%s:%" PRId32, name, field->bit_size);
    } else if (is_array_type(field->display_type)) {
        needed = snprintf(NULL, 0, "%s[%" PRId32 "]", name, field->array_length);
    } else {
        *out = sf_strdup(alloc, name);
        return *out ? SF_OK : SF_ERR_OOM;
    }
    if (needed < 0) return SF_ERR_INTERNAL;
    char *s = (char *)sf_xalloc(alloc, (size_t)needed + 1);
    if (!s) return SF_ERR_OOM;
    if (field->bit_size != -1) {
        (void)snprintf(s, (size_t)needed + 1, "%s:%" PRId32, name, field->bit_size);
    } else {
        (void)snprintf(s, (size_t)needed + 1, "%s[%" PRId32 "]", name, field->array_length);
    }
    *out = s;
    return SF_OK;
}

static float default_value_to_f32(sf_paramdef_default_value_t value) {
    switch (value.type) {
    case SF_PARAMDEF_DEF_TYPE_S8: return (float)value.v.s8;
    case SF_PARAMDEF_DEF_TYPE_U8: return (float)value.v.u8;
    case SF_PARAMDEF_DEF_TYPE_S16: return (float)value.v.s16;
    case SF_PARAMDEF_DEF_TYPE_U16: return (float)value.v.u16;
    case SF_PARAMDEF_DEF_TYPE_S32: return (float)value.v.s32;
    case SF_PARAMDEF_DEF_TYPE_U32: return (float)value.v.u32;
    case SF_PARAMDEF_DEF_TYPE_S64: return (float)value.v.s64;
    case SF_PARAMDEF_DEF_TYPE_U64: return (float)value.v.u64;
    case SF_PARAMDEF_DEF_TYPE_B32: return (float)value.v.b32;
    case SF_PARAMDEF_DEF_TYPE_F32: return value.v.f32;
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: return value.v.angle32;
    case SF_PARAMDEF_DEF_TYPE_F64: return (float)value.v.f64;
    default: return 0.0f;
    }
}

static int32_t default_value_to_i32(sf_paramdef_default_value_t value) {
    switch (value.type) {
    case SF_PARAMDEF_DEF_TYPE_S8: return value.v.s8;
    case SF_PARAMDEF_DEF_TYPE_U8: return value.v.u8;
    case SF_PARAMDEF_DEF_TYPE_S16: return value.v.s16;
    case SF_PARAMDEF_DEF_TYPE_U16: return value.v.u16;
    case SF_PARAMDEF_DEF_TYPE_S32: return value.v.s32;
    case SF_PARAMDEF_DEF_TYPE_U32: return (int32_t)value.v.u32;
    case SF_PARAMDEF_DEF_TYPE_S64: return (int32_t)value.v.s64;
    case SF_PARAMDEF_DEF_TYPE_U64: return (int32_t)value.v.u64;
    case SF_PARAMDEF_DEF_TYPE_B32: return (int32_t)value.v.b32;
    case SF_PARAMDEF_DEF_TYPE_F32: return (int32_t)value.v.f32;
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: return (int32_t)value.v.angle32;
    case SF_PARAMDEF_DEF_TYPE_F64: return (int32_t)value.v.f64;
    default: return 0;
    }
}

static sf_result_t read_var_offset(sf_binary_reader_t *br, int64_t *out) {
    return sf_binary_reader_read_varint(br, out);
}

static sf_result_t read_nonzero_string_at(sf_binary_reader_t *br, int64_t off, bool utf16,
                                          char **out) {
    *out = NULL;
    if (off == 0) return SF_OK;
    return utf16 ? sf_binary_reader_get_utf16(br, off, out, NULL)
                 : sf_binary_reader_get_shift_jis(br, off, out, NULL);
}

static void set_default_f32(sf_paramdef_default_value_t *out, float value) {
    memset(out, 0, sizeof(*out));
    out->type = SF_PARAMDEF_DEF_TYPE_F32;
    out->v.f32 = value;
}

static sf_result_t read_variable_value(sf_binary_reader_t *br, sf_paramdef_def_type_t type,
                                       sf_paramdef_default_value_t *out) {
    memset(out, 0, sizeof(*out));
    out->type = type;

    sf_result_t r;
    int32_t i32 = 0;
    uint32_t u32 = 0;
    int64_t i64 = 0;
    uint64_t u64 = 0;
    float f32 = 0.0f;
    double f64 = 0.0;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
        r = sf_binary_reader_read_i32(br, &i32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        out->v.s8 = (int8_t)i32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U8:
        r = sf_binary_reader_read_i32(br, &i32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        out->v.u8 = (uint8_t)i32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_S16:
        r = sf_binary_reader_read_i32(br, &i32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        out->v.s16 = (int16_t)i32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U16:
        r = sf_binary_reader_read_i32(br, &i32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        out->v.u16 = (uint16_t)i32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_S32:
        r = sf_binary_reader_read_i32(br, &i32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        out->v.s32 = i32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_B32:
        r = sf_binary_reader_read_u32(br, &u32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        if (type == SF_PARAMDEF_DEF_TYPE_U32) out->v.u32 = u32;
        else out->v.b32 = u32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_S64:
        r = sf_binary_reader_read_i64(br, &i64); if (r != SF_OK) return r;
        out->v.s64 = i64;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_U64:
        r = sf_binary_reader_read_u64(br, &u64); if (r != SF_OK) return r;
        out->v.u64 = u64;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        r = sf_binary_reader_read_f32(br, &f32); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        if (type == SF_PARAMDEF_DEF_TYPE_F32) out->v.f32 = f32;
        else out->v.angle32 = f32;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_F64:
        r = sf_binary_reader_read_f64(br, &f64); if (r != SF_OK) return r;
        out->v.f64 = f64;
        return SF_OK;
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return sf_binary_reader_assert_i64_one(br, 0);
    default:
        return SF_ERR_BAD_MAGIC;
    }
}

static void parse_internal_name(sf_paramdef_field_t *field) {
    field->bit_size = -1;
    if (!field->internal_name) return;

    char *colon = strrchr(field->internal_name, ':');
    if (colon != NULL && colon[1] != '\0') {
        char *end = NULL;
        long bits = strtol(colon + 1, &end, 10);
        if (end != colon + 1 && *end == '\0' && bits >= 0 && bits <= INT32_MAX) {
            *colon = '\0';
            field->bit_size = (int32_t)bits;
            return;
        }
    }

    if (!is_array_type(field->display_type)) return;
    size_t len = strlen(field->internal_name);
    if (len < 3 || field->internal_name[len - 1] != ']') return;
    char *open = strrchr(field->internal_name, '[');
    if (!open || open[1] == '\0') return;
    char *end = NULL;
    long named_len = strtol(open + 1, &end, 10);
    if (end != open + 1 && end == field->internal_name + len - 1 && named_len > 0 &&
        named_len <= INT32_MAX) {
        *open = '\0';
        if (named_len != field->array_length && field->display_type != SF_PARAMDEF_DEF_TYPE_U8 &&
            field->display_type != SF_PARAMDEF_DEF_TYPE_DUMMY8) {
            field->array_length = (int32_t)named_len;
        } else if (named_len < field->array_length) {
            field->array_length = (int32_t)named_len;
        }
    }
}

static sf_result_t read_field(sf_binary_reader_t *br, const sf_paramdef_t *def,
                              sf_paramdef_field_t *field) {
    memset(field, 0, sizeof(*field));
    field->array_length = 1;
    field->bit_size = -1;
    field->sort_id = 0;

    sf_result_t r;
    if (def->format_version >= 202 ||
        (def->format_version >= 106 && def->format_version < 200)) {
        int64_t off = 0;
        r = read_var_offset(br, &off); if (r != SF_OK) return r;
        r = sf_binary_reader_get_utf16(br, off, &field->display_name, NULL); if (r != SF_OK) return r;
    } else if (def->unicode) {
        r = sf_binary_reader_read_fix_str_w(br, 0x40, &field->display_name, NULL);
        if (r != SF_OK) return r;
    } else {
        r = sf_binary_reader_read_fix_str(br, 0x40, &field->display_name, NULL);
        if (r != SF_OK) return r;
    }

    char *type_name = NULL;
    r = sf_binary_reader_read_fix_str(br, 8, &type_name, NULL); if (r != SF_OK) return r;
    trim_ascii_in_place(type_name);
    bool type_ok = parse_def_type(type_name, &field->display_type);
    sf_xfree(def->alloc, type_name);
    if (!type_ok) return SF_ERR_BAD_MAGIC;

    r = sf_binary_reader_read_fix_str(br, 8, &field->display_format, NULL);
    if (r != SF_OK) return r;
    trim_ascii_in_place(field->display_format);

    if (def->format_version >= 203) {
        r = sf_binary_reader_assert_pattern(br, 0x10, 0); if (r != SF_OK) return r;
    } else {
        float value = 0.0f;
        r = sf_binary_reader_read_f32(br, &value); if (r != SF_OK) return r;
        set_default_f32(&field->default_value, value);
        r = sf_binary_reader_read_f32(br, &value); if (r != SF_OK) return r;
        set_default_f32(&field->minimum, value);
        r = sf_binary_reader_read_f32(br, &value); if (r != SF_OK) return r;
        set_default_f32(&field->maximum, value);
        r = sf_binary_reader_read_f32(br, &value); if (r != SF_OK) return r;
        set_default_f32(&field->increment, value);
    }

    int32_t edit_flags = 0;
    r = sf_binary_reader_read_i32(br, &edit_flags); if (r != SF_OK) return r;
    field->edit_flags = (sf_paramdef_edit_flags_t)edit_flags;
    r = sf_binary_reader_read_i32(br, &field->byte_count); if (r != SF_OK) return r;

    size_t value_size = sf_param_util_get_value_size(field->display_type);
    if (value_size == 0 || field->byte_count < 0) return SF_ERR_BAD_MAGIC;
    if (!is_array_type(field->display_type)) {
        if (field->byte_count != (int32_t)value_size) return SF_ERR_BAD_MAGIC;
    } else if ((size_t)field->byte_count % value_size != 0) {
        return SF_ERR_BAD_MAGIC;
    }
    field->array_length = (int32_t)((size_t)field->byte_count / value_size);

    if (def->basic_fields) {
        field->internal_type = sf_strdup(def->alloc, "");
        field->internal_name = sf_strdup(def->alloc, "");
        field->description = sf_strdup(def->alloc, "");
        if (!field->internal_type || !field->internal_name || !field->description) return SF_ERR_OOM;
        field->bit_size = -1;
        return SF_OK;
    }

    int64_t description_offset = 0;
    r = read_var_offset(br, &description_offset); if (r != SF_OK) return r;
    r = read_nonzero_string_at(br, description_offset, def->unicode, &field->description);
    if (r != SF_OK) return r;
    if (!field->description) {
        field->description = sf_strdup(def->alloc, "");
        if (!field->description) return SF_ERR_OOM;
    }

    if (def->format_version >= 202 ||
        (def->format_version >= 106 && def->format_version < 200)) {
        int64_t off = 0;
        r = read_var_offset(br, &off); if (r != SF_OK) return r;
        r = off == 0 ? SF_OK : sf_binary_reader_get_ascii(br, off, &field->internal_type, NULL);
    } else {
        r = sf_binary_reader_read_fix_str(br, 0x20, &field->internal_type, NULL);
    }
    if (r != SF_OK) return r;
    if (!field->internal_type) field->internal_type = sf_strdup(def->alloc, "");
    if (!field->internal_type) return SF_ERR_OOM;
    trim_ascii_in_place(field->internal_type);

    if (def->format_version >= 102) {
        if (def->format_version >= 202 ||
            (def->format_version >= 106 && def->format_version < 200)) {
            int64_t off = 0;
            r = read_var_offset(br, &off); if (r != SF_OK) return r;
            r = off == 0 ? SF_OK : sf_binary_reader_get_ascii(br, off, &field->internal_name, NULL);
        } else {
            r = sf_binary_reader_read_fix_str(br, 0x20, &field->internal_name, NULL);
        }
        if (r != SF_OK) return r;
        if (!field->internal_name) field->internal_name = sf_strdup(def->alloc, "");
        if (!field->internal_name) return SF_ERR_OOM;
        trim_ascii_in_place(field->internal_name);
        parse_internal_name(field);
    } else {
        field->internal_name = sf_strdup(def->alloc, "");
        if (!field->internal_name) return SF_ERR_OOM;
    }

    if (def->format_version >= 104) {
        r = sf_binary_reader_read_i32(br, &field->sort_id); if (r != SF_OK) return r;
    }

    if (def->format_version >= 200) {
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        int64_t ignored = 0;
        r = read_var_offset(br, &ignored); if (r != SF_OK) return r;
        r = read_var_offset(br, &ignored); if (r != SF_OK) return r;
        r = read_var_offset(br, &ignored); if (r != SF_OK) return r;
    } else if (def->format_version >= 106) {
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    }

    if (def->format_version >= 203) {
        r = read_variable_value(br, field->display_type, &field->default_value); if (r != SF_OK) return r;
        r = read_variable_value(br, field->display_type, &field->minimum); if (r != SF_OK) return r;
        r = read_variable_value(br, field->display_type, &field->maximum); if (r != SF_OK) return r;
        r = read_variable_value(br, field->display_type, &field->increment); if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t read_param_type(sf_binary_reader_t *br, sf_paramdef_t *def) {
    sf_result_t r;
    if (def->format_version >= 202) {
        r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
        int64_t off = 0;
        r = sf_binary_reader_read_i64(br, &off); if (r != SF_OK) return r;
        r = sf_binary_reader_get_shift_jis(br, off, &def->param_type, NULL); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
        return sf_binary_reader_assert_i32_one(br, 0);
    }

    if (def->format_version >= 106 && def->format_version < 200) {
        int32_t off32 = 0;
        r = sf_binary_reader_read_i32(br, &off32); if (r != SF_OK) return r;
        r = sf_binary_reader_get_shift_jis(br, off32, &def->param_type, NULL); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i64_one(br, 0); if (r != SF_OK) return r;
        return sf_binary_reader_assert_i32_one(br, 0);
    }

    return sf_binary_reader_read_fix_str(br, 0x20, &def->param_type, NULL);
}

static sf_result_t paramdef_read(sf_binary_reader_t *br, sf_paramdef_t **out,
                                 const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    uint8_t endian_byte = 0;
    sf_result_t r = sf_binary_reader_get_u8(br, 0x2C, &endian_byte); if (r != SF_OK) return r;
    bool big_endian = endian_byte == 0x01;
    if (endian_byte != 0x00 && endian_byte != 0x01 && endian_byte != 0xFF) return SF_ERR_BAD_MAGIC;
    sf_binary_reader_set_big_endian(br, big_endian);

    int16_t format_version = 0;
    r = sf_binary_reader_get_i16(br, 0x2E, &format_version); if (r != SF_OK) return r;
    if (!is_supported_version(format_version)) return SF_ERR_UNSUPPORTED_VERSION;
    sf_binary_reader_set_varint_long(br, format_version >= 200);

    r = sf_istream_seek(sf_binary_reader_stream(br), 0); if (r != SF_OK) return r;

    sf_paramdef_t *def = (sf_paramdef_t *)sf_xalloc(alloc, sizeof(*def));
    if (!def) return SF_ERR_OOM;
    memset(def, 0, sizeof(*def));
    def->alloc = alloc;
    def->format_version = format_version;
    def->big_endian = big_endian;
    def->index = -1;

    int32_t file_size = 0;
    int16_t header_size = 0;
    int16_t field_count = 0;
    int16_t field_size = 0;
    r = sf_binary_reader_read_i32(br, &file_size); if (r != SF_OK) goto fail;
    (void)file_size;
    r = sf_binary_reader_read_i16(br, &header_size); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i16(br, &def->data_version); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i16(br, &field_count); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i16(br, &field_size); if (r != SF_OK) goto fail;
    if (field_count < 0) { r = SF_ERR_BAD_MAGIC; goto fail; }
    if ((format_version < 200 && header_size != 0x30) ||
        (format_version >= 200 && header_size != 0xFF)) {
        r = SF_ERR_BAD_MAGIC;
        goto fail;
    }
    if (field_size != expected_field_size(format_version)) {
        r = SF_ERR_BAD_MAGIC;
        goto fail;
    }
    def->basic_fields = format_version == 0 && field_size == 0x68;

    r = read_param_type(br, def); if (r != SF_OK) goto fail;
    if (!def->param_type) {
        def->param_type = sf_strdup(alloc, "");
        if (!def->param_type) { r = SF_ERR_OOM; goto fail; }
    }

    uint8_t header_endian = 0;
    r = sf_binary_reader_read_u8(br, &header_endian); if (r != SF_OK) goto fail;
    if (header_endian != endian_byte) { r = SF_ERR_BAD_MAGIC; goto fail; }
    r = sf_binary_reader_read_bool(br, &def->unicode); if (r != SF_OK) goto fail;
    int16_t version_in_header = 0;
    r = sf_binary_reader_read_i16(br, &version_in_header); if (r != SF_OK) goto fail;
    if (version_in_header != format_version) { r = SF_ERR_UNSUPPORTED_VERSION; goto fail; }
    if (format_version >= 200) {
        r = sf_binary_reader_assert_i64_one(br, 0x38); if (r != SF_OK) goto fail;
    }

    def->field_count = (size_t)field_count;
    if (def->field_count > 0) {
        def->fields = (sf_paramdef_field_t *)sf_xalloc(alloc, def->field_count * sizeof(*def->fields));
        if (!def->fields) { r = SF_ERR_OOM; goto fail; }
        memset(def->fields, 0, def->field_count * sizeof(*def->fields));
    }

    for (size_t i = 0; i < def->field_count; i++) {
        r = read_field(br, def, &def->fields[i]);
        if (r != SF_OK) goto fail;
    }
    def->row_size = sf_paramdef_internal_compute_row_size(def);

    *out = def;
    return SF_OK;

fail:
    sf_paramdef_destroy(def);
    return r;
}

sf_result_t sf_paramdef_read_from_stream(sf_paramdef_t **out, sf_istream_t *stream,
                                         const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;
    r = paramdef_read(br, out, alloc);
    sf_binary_reader_destroy(br);
    return r;
}

sf_result_t sf_paramdef_read_from_memory(sf_paramdef_t **out, const uint8_t *data,
                                         size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_paramdef_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_paramdef_read_from_path(sf_paramdef_t **out, const wchar_t *path,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_paramdef_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

static sf_result_t paramdef_validate_for_write(const sf_paramdef_t *def) {
    SF_CHECK_ARG(def != NULL);
    if (!def->param_type || def->param_type[0] == '\0') return SF_ERR_INVALID_ARG;
    if (def->field_count > 0 && !def->fields) return SF_ERR_INVALID_ARG;
    if (def->field_count > (size_t)INT16_MAX) return SF_ERR_OUT_OF_RANGE;

    for (size_t i = 0; i < def->field_count; i++) {
        const sf_paramdef_field_t *field = &def->fields[i];
        if (!field->display_name) return SF_ERR_INVALID_ARG;
        if (!field->display_format) return SF_ERR_INVALID_ARG;
        if (def->format_version >= 102 && !field->internal_type) return SF_ERR_INVALID_ARG;
        if (!field->internal_name) return SF_ERR_INVALID_ARG;
        if (!def_type_name(field->display_type)) return SF_ERR_INVALID_ARG;
    }
    return SF_OK;
}

static sf_result_t write_variable_value(sf_binary_writer_t *bw, sf_paramdef_def_type_t type,
                                        sf_paramdef_default_value_t value) {
    sf_result_t r;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_B32:
        r = sf_binary_writer_write_i32(bw, default_value_to_i32(value)); if (r != SF_OK) return r;
        return sf_binary_writer_write_i32(bw, 0);
    case SF_PARAMDEF_DEF_TYPE_S64:
        return sf_binary_writer_write_i64(bw, value.type == SF_PARAMDEF_DEF_TYPE_S64
                                                  ? value.v.s64
                                                  : (int64_t)default_value_to_i32(value));
    case SF_PARAMDEF_DEF_TYPE_U64:
        return sf_binary_writer_write_u64(bw, value.type == SF_PARAMDEF_DEF_TYPE_U64
                                                  ? value.v.u64
                                                  : (uint64_t)(uint32_t)default_value_to_i32(value));
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        r = sf_binary_writer_write_f32(bw, default_value_to_f32(value)); if (r != SF_OK) return r;
        return sf_binary_writer_write_i32(bw, 0);
    case SF_PARAMDEF_DEF_TYPE_F64:
        return sf_binary_writer_write_f64(bw, value.type == SF_PARAMDEF_DEF_TYPE_F64
                                                  ? value.v.f64
                                                  : (double)default_value_to_f32(value));
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return sf_binary_writer_write_i64(bw, 0);
    default:
        return SF_ERR_INVALID_ARG;
    }
}

static sf_result_t write_field_record(sf_binary_writer_t *bw, const sf_paramdef_t *def,
                                      size_t index) {
    const sf_paramdef_field_t *field = &def->fields[index];
    const int16_t version = def->format_version;
    const bool indirect_strings = version >= 202 || (version >= 106 && version < 200);
    sf_result_t r;
    char name[64];

    if (indirect_strings) {
        r = reserve_name(name, "DisplayNameOffset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    } else if (def->unicode) {
        r = sf_binary_writer_write_fix_str_w(bw, field->display_name, 0x40,
                                             (uint8_t)(version >= 104 ? 0x00 : 0x20));
        if (r != SF_OK) return r;
    } else {
        r = sf_binary_writer_write_fix_str(bw, field->display_name, 0x40,
                                           (uint8_t)(version >= 104 ? 0x00 : 0x20));
        if (r != SF_OK) return r;
    }

    const uint8_t padding = (uint8_t)(version >= 106 ? 0x00 : 0x20);
    const char *type_name = def_type_name(field->display_type);
    if (!type_name) return SF_ERR_INVALID_ARG;
    r = sf_binary_writer_write_fix_str(bw, type_name, 8, padding); if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, field->display_format, 8, padding);
    if (r != SF_OK) return r;

    if (version >= 203) {
        r = sf_binary_writer_write_pattern(bw, 0x10, 0x00); if (r != SF_OK) return r;
    } else {
        r = sf_binary_writer_write_f32(bw, default_value_to_f32(field->default_value));
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_f32(bw, default_value_to_f32(field->minimum));
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_f32(bw, default_value_to_f32(field->maximum));
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_f32(bw, default_value_to_f32(field->increment));
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_write_i32(bw, (int32_t)field->edit_flags); if (r != SF_OK) return r;
    size_t value_size = sf_param_util_get_value_size(field->display_type);
    if (value_size == 0) return SF_ERR_INVALID_ARG;
    int32_t value_count = is_array_type(field->display_type) ? field->array_length : 1;
    if (value_count <= 0 || value_size > (size_t)INT32_MAX / (size_t)value_count) {
        return SF_ERR_OUT_OF_RANGE;
    }
    r = sf_binary_writer_write_i32(bw, (int32_t)(value_size * (size_t)value_count));
    if (r != SF_OK) return r;

    if (def->basic_fields) return SF_OK;

    r = reserve_name(name, "DescriptionOffset", index); if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);

    if (indirect_strings) {
        r = reserve_name(name, "InternalTypeOffset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    } else {
        r = sf_binary_writer_write_fix_str(bw, field->internal_type, 0x20, padding);
        if (r != SF_OK) return r;
    }

    if (indirect_strings) {
        r = reserve_name(name, "InternalNameOffset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    } else if (version >= 102) {
        char *internal_name = NULL;
        r = make_internal_name(field, &internal_name, def->alloc); if (r != SF_OK) return r;
        r = sf_binary_writer_write_fix_str(bw, internal_name, 0x20, padding);
        sf_xfree(def->alloc, internal_name);
        if (r != SF_OK) return r;
    }

    if (version >= 104) {
        r = sf_binary_writer_write_i32(bw, field->sort_id); if (r != SF_OK) return r;
    }

    if (version >= 200) {
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = reserve_name(name, "UnkB8Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, name), return r);
        r = reserve_name(name, "UnkC0Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, name), return r);
        r = reserve_name(name, "UnkC8Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i64(bw, name), return r);
    } else if (version >= 106) {
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    }

    if (version >= 203) {
        r = write_variable_value(bw, field->display_type, field->default_value); if (r != SF_OK) return r;
        r = write_variable_value(bw, field->display_type, field->minimum); if (r != SF_OK) return r;
        r = write_variable_value(bw, field->display_type, field->maximum); if (r != SF_OK) return r;
        r = write_variable_value(bw, field->display_type, field->increment); if (r != SF_OK) return r;
    }

    return SF_OK;
}

static sf_result_t write_field_strings(sf_binary_writer_t *bw, const sf_paramdef_t *def,
                                       size_t index) {
    const sf_paramdef_field_t *field = &def->fields[index];
    const int16_t version = def->format_version;
    const bool indirect_strings = version >= 202 || (version >= 106 && version < 200);
    sf_result_t r;
    char name[64];

    if (indirect_strings) {
        r = reserve_name(name, "DisplayNameOffset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_utf16(bw, field->display_name, true); if (r != SF_OK) return r;
    }

    if (def->basic_fields) return SF_OK;

    int64_t description_offset = 0;
    if (field->description && field->description[0] != '\0') {
        description_offset = sf_binary_writer_position(bw);
        r = def->unicode ? sf_binary_writer_write_utf16(bw, field->description, true)
                         : sf_binary_writer_write_shift_jis(bw, field->description, true);
        if (r != SF_OK) return r;
    }
    r = reserve_name(name, "DescriptionOffset", index); if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, description_offset), return r);

    if (indirect_strings) {
        r = reserve_name(name, "InternalTypeOffset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_ascii(bw, field->internal_type, true); if (r != SF_OK) return r;

        char *internal_name = NULL;
        r = make_internal_name(field, &internal_name, def->alloc); if (r != SF_OK) return r;
        r = reserve_name(name, "InternalNameOffset", index);
        if (r == SF_OK) r = sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw));
        if (r == SF_OK) r = sf_binary_writer_write_ascii(bw, internal_name, true);
        sf_xfree(def->alloc, internal_name);
        if (r != SF_OK) return r;
    }

    if (version >= 200) {
        r = reserve_name(name, "UnkB8Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, name, 0), return r);
        r = reserve_name(name, "UnkC0Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, name, 0), return r);
        r = reserve_name(name, "UnkC8Offset", index); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i64(bw, name, 0), return r);
    }

    return SF_OK;
}

static sf_result_t paramdef_write_to_writer(const sf_paramdef_t *def, sf_binary_writer_t *bw) {
    sf_result_t r = paramdef_validate_for_write(def);
    if (r != SF_OK) return r;
    if (def->version_aware) return SF_ERR_INVALID_ARG;
    if (!is_writable_version(def->format_version)) return SF_ERR_UNSUPPORTED_VERSION;

    sf_binary_writer_set_big_endian(bw, def->big_endian);
    sf_binary_writer_set_varint_long(bw, def->format_version >= 200);

    const int16_t version = def->format_version;
    const bool indirect_param_type = version >= 202 || (version >= 106 && version < 200);

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FileSize"), return r);
    r = sf_binary_writer_write_i16(bw, (int16_t)(version >= 200 ? 0xFF : 0x30));
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, def->data_version); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, (int16_t)def->field_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, expected_field_size(version)); if (r != SF_OK) return r;

    if (version >= 202) {
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "ParamTypeOffset"), return r);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    } else if (version >= 106 && version < 200) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "ParamTypeOffset"), return r);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    } else {
        r = sf_binary_writer_write_fix_str(bw, def->param_type, 0x20,
                                           (uint8_t)(version >= 200 ? 0x00 : 0x20));
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_write_i8(bw, (int8_t)(def->big_endian ? -1 : 0)); if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, def->unicode); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, version); if (r != SF_OK) return r;
    if (version >= 200) {
        r = sf_binary_writer_write_i64(bw, 0x38); if (r != SF_OK) return r;
    }

    for (size_t i = 0; i < def->field_count; i++) {
        r = write_field_record(bw, def, i); if (r != SF_OK) return r;
    }

    if (indirect_param_type) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "ParamTypeOffset", sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_shift_jis(bw, def->param_type, true); if (r != SF_OK) return r;
    }

    int64_t field_strings_start = sf_binary_writer_position(bw);
    for (size_t i = 0; i < def->field_count; i++) {
        r = write_field_strings(bw, def, i); if (r != SF_OK) return r;
    }

    if (version == 104 || version == 201) {
        int64_t field_strings_length = sf_binary_writer_position(bw) - field_strings_start;
        if ((field_strings_length % 0x10) != 0) {
            r = sf_binary_writer_write_pattern(bw, (size_t)(0x10 - (field_strings_length % 0x10)), 0);
            if (r != SF_OK) return r;
        }
    } else {
        if (version >= 202 && (sf_binary_writer_position(bw) % 0x10) == 0) {
            r = sf_binary_writer_write_pattern(bw, 0x10, 0); if (r != SF_OK) return r;
        }
        r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
    }

    return sf_binary_writer_fill_i32(bw, "FileSize", (int32_t)sf_binary_writer_position(bw));
}

sf_result_t sf_paramdef_write_to_memory(const sf_paramdef_t *paramdef, uint8_t **out,
                                        size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(paramdef != NULL && out != NULL && out_size != NULL);
    *out = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    r = paramdef_write_to_writer(paramdef, bw);
    if (r == SF_OK) {
        r = sf_binary_writer_finish_bytes(bw, out, out_size);
    } else {
        sf_binary_writer_destroy(bw);
    }
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_paramdef_write_to_stream(const sf_paramdef_t *paramdef, sf_ostream_t *stream,
                                        const sf_allocator_t *alloc) {
    SF_CHECK_ARG(paramdef != NULL && stream != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) return r;

    r = paramdef_write_to_writer(paramdef, bw);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    else sf_binary_writer_destroy(bw);
    return r;
}

sf_result_t sf_paramdef_write_to_path(const sf_paramdef_t *paramdef, const wchar_t *path,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(paramdef != NULL && path != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;

    r = sf_paramdef_write_to_stream(paramdef, stream, alloc);
    sf_ostream_close(stream);
    return r;
}

int16_t sf_paramdef_get_data_version(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->data_version : 0;
}

const char *sf_paramdef_get_param_type(const sf_paramdef_t *paramdef) {
    return (paramdef && paramdef->param_type) ? paramdef->param_type : "";
}

bool sf_paramdef_is_big_endian(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->big_endian : false;
}

bool sf_paramdef_is_unicode(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->unicode : false;
}

int16_t sf_paramdef_get_format_version(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->format_version : 0;
}

bool sf_paramdef_is_version_aware(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->version_aware : false;
}

size_t sf_paramdef_get_field_count(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->field_count : 0;
}

const sf_paramdef_field_t *sf_paramdef_get_field(const sf_paramdef_t *paramdef, size_t index) {
    if (!paramdef || index >= paramdef->field_count) return NULL;
    return &paramdef->fields[index];
}

int32_t sf_paramdef_get_row_size(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->row_size : 0;
}

int32_t sf_paramdef_internal_compute_row_size(const sf_paramdef_t *def) {
    if (!def || def->field_count == 0) return 0;

    int32_t size = 0;
    size_t field_count = def->field_count;
    for (size_t i = 0; i < field_count; i++) {
        const sf_paramdef_field_t *field = &def->fields[i];

        /* Mirrors C# Field.IsValidForRegulationVersion(ulong.MaxValue):
         * for version-aware defs, a field with a non-zero
         * removed_regulation_version has already been removed and must not
         * contribute to the row size. */
        if (def->version_aware && field->removed_regulation_version != 0) continue;

        sf_paramdef_def_type_t type = field->display_type;
        size_t value_size = sf_param_util_get_value_size(type);
        if (is_array_type(type)) {
            int32_t len = field->array_length > 0 ? field->array_length : 0;
            size += (int32_t)value_size * len;
        } else {
            size += (int32_t)value_size;
        }

        /* If this is a sized bit field, fold subsequent bit fields that pack
         * into the same byte window (same bit-limit type, total <= bit limit).
         * They share the byte we just counted, so skip them.
         *
         * Crucially the C# upstream XmlDeserialize defaults `versionAware=false`,
         * which drops every RemovedVersion field at parse time. The C port
         * retains the full field list for round-trip fidelity, so the fold
         * walk has to skip those removed slots inline; otherwise a removed
         * bitfield would consume bit budget and force the next valid bitfield
         * into a new byte, over-counting the row size (EquipParamWeapon 665 vs
         * upstream 664, EquipParamGoods 177 vs upstream 176). */
        if (sf_param_util_is_bit_type(type) && field->bit_size != -1) {
            int bit_offset = field->bit_size;
            int bit_limit = sf_param_util_get_bit_limit(type);

            for (;;) {
                size_t next_idx = i + 1;
                while (next_idx < field_count) {
                    const sf_paramdef_field_t *probe = &def->fields[next_idx];
                    if (!(def->version_aware && probe->removed_regulation_version != 0)) break;
                    next_idx++;
                }
                if (next_idx >= field_count) break;

                const sf_paramdef_field_t *next = &def->fields[next_idx];
                sf_paramdef_def_type_t next_type = next->display_type;
                if (!sf_param_util_is_bit_type(next_type) ||
                    next->bit_size == -1 ||
                    sf_param_util_get_bit_limit(next_type) != bit_limit ||
                    bit_offset + next->bit_size > bit_limit) {
                    break;
                }
                bit_offset += next->bit_size;
                i = next_idx; /* advance outer i past the consumed field */
            }
        }
    }
    return size;
}

int32_t sf_paramdef_get_index(const sf_paramdef_t *paramdef) {
    return paramdef ? paramdef->index : -1;
}

const char *sf_paramdef_field_get_display_name(const sf_paramdef_field_t *field) {
    return (field && field->display_name) ? field->display_name : "";
}

const char *sf_paramdef_field_get_internal_name(const sf_paramdef_field_t *field) {
    return (field && field->internal_name) ? field->internal_name : "";
}

const char *sf_paramdef_field_get_description(const sf_paramdef_field_t *field) {
    return (field && field->description) ? field->description : "";
}

sf_paramdef_def_type_t sf_paramdef_field_get_display_type(const sf_paramdef_field_t *field) {
    return field ? field->display_type : SF_PARAMDEF_DEF_TYPE_S8;
}

const char *sf_paramdef_field_get_display_format(const sf_paramdef_field_t *field) {
    return (field && field->display_format) ? field->display_format : "";
}

sf_paramdef_default_value_t sf_paramdef_field_get_default_value(const sf_paramdef_field_t *field) {
    sf_paramdef_default_value_t zero;
    memset(&zero, 0, sizeof(zero));
    return field ? field->default_value : zero;
}

sf_paramdef_default_value_t sf_paramdef_field_get_minimum(const sf_paramdef_field_t *field) {
    sf_paramdef_default_value_t zero;
    memset(&zero, 0, sizeof(zero));
    return field ? field->minimum : zero;
}

sf_paramdef_default_value_t sf_paramdef_field_get_maximum(const sf_paramdef_field_t *field) {
    sf_paramdef_default_value_t zero;
    memset(&zero, 0, sizeof(zero));
    return field ? field->maximum : zero;
}

sf_paramdef_default_value_t sf_paramdef_field_get_increment(const sf_paramdef_field_t *field) {
    sf_paramdef_default_value_t zero;
    memset(&zero, 0, sizeof(zero));
    return field ? field->increment : zero;
}

sf_paramdef_edit_flags_t sf_paramdef_field_get_edit_flags(const sf_paramdef_field_t *field) {
    return field ? field->edit_flags : SF_PARAMDEF_EDIT_FLAGS_NONE;
}

int32_t sf_paramdef_field_get_byte_count(const sf_paramdef_field_t *field) {
    return field ? field->byte_count : 0;
}

int32_t sf_paramdef_field_get_bit_size(const sf_paramdef_field_t *field) {
    return field ? field->bit_size : 0;
}

int32_t sf_paramdef_field_get_array_length(const sf_paramdef_field_t *field) {
    return field ? field->array_length : 0;
}

int32_t sf_paramdef_field_get_sort_id(const sf_paramdef_field_t *field) {
    return field ? field->sort_id : 0;
}

uint64_t sf_paramdef_field_get_first_regulation_version(const sf_paramdef_field_t *field) {
    return field ? field->first_regulation_version : 0;
}

uint64_t sf_paramdef_field_get_removed_regulation_version(const sf_paramdef_field_t *field) {
    return field ? field->removed_regulation_version : 0;
}
