/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMDEF XML reader (mxml DOM walk).
 *
 * Mirrors pinned upstream:
 *   SoulsFormats/Formats/PARAM/PARAMDEF/XmlSerializer.cs:18-175
 *
 * Notes on divergence from upstream:
 *   - SF_ERR_BAD_DATA is spelled SF_ERR_BAD_MAGIC in this project: any input
 *     that fails structural validation reuses the binary-side error name.
 *   - mxml parse failures (malformed XML, IO errors) map to SF_ERR_INTERNAL.
 *   - The Def attribute parser is a hand-written state machine; the upstream
 *     Regex relies on full PCRE semantics not present in C99.
 */

#include "souls_formats/sf_paramdef.h"

#include "internal/sf_internal.h"
#include "param/paramdef_internal.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mxml.h>

/*===========================================================================
 * Internal helpers
 *===========================================================================*/

static bool sfi_str_iequal(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static const char *sfi_child_text(mxml_node_t *parent, const char *name) {
    if (!parent || !name) return NULL;
    mxml_node_t *child = mxmlFindElement(parent, parent, name, NULL, NULL,
                                         MXML_DESCEND_FIRST);
    if (!child) return NULL;
    mxml_node_t *value = mxmlGetFirstChild(child);
    if (!value) return "";
    const char *opaque = mxmlGetOpaque(value);
    if (opaque) return opaque;
    bool ws = false;
    const char *text = mxmlGetText(value, &ws);
    return text ? text : "";
}

static bool sfi_parse_bool(const char *text, bool *out) {
    if (!text || !out) return false;
    if (sfi_str_iequal(text, "true") || strcmp(text, "1") == 0) {
        *out = true;
        return true;
    }
    if (sfi_str_iequal(text, "false") || strcmp(text, "0") == 0) {
        *out = false;
        return true;
    }
    return false;
}

static bool sfi_parse_i16(const char *text, int16_t *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 10);
    if (errno != 0 || end == text) return false;
    if (v < INT16_MIN || v > INT16_MAX) return false;
    *out = (int16_t)v;
    return true;
}

static bool sfi_parse_i32(const char *text, int32_t *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 10);
    if (errno != 0 || end == text) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    *out = (int32_t)v;
    return true;
}

static bool sfi_parse_f32(const char *text, float *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    float v = strtof(text, &end);
    if (errno != 0 || end == text) return false;
    *out = v;
    return true;
}

static bool sfi_parse_f64(const char *text, double *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    double v = strtod(text, &end);
    if (errno != 0 || end == text) return false;
    *out = v;
    return true;
}

static bool sfi_parse_def_type(const char *name, size_t len,
                               sf_paramdef_def_type_t *out) {
    if (!name || !out) return false;
    struct { const char *name; size_t len; sf_paramdef_def_type_t type; } table[] = {
        { "s8",      2, SF_PARAMDEF_DEF_TYPE_S8 },
        { "u8",      2, SF_PARAMDEF_DEF_TYPE_U8 },
        { "s16",     3, SF_PARAMDEF_DEF_TYPE_S16 },
        { "u16",     3, SF_PARAMDEF_DEF_TYPE_U16 },
        { "s32",     3, SF_PARAMDEF_DEF_TYPE_S32 },
        { "u32",     3, SF_PARAMDEF_DEF_TYPE_U32 },
        { "s64",     3, SF_PARAMDEF_DEF_TYPE_S64 },
        { "u64",     3, SF_PARAMDEF_DEF_TYPE_U64 },
        { "b32",     3, SF_PARAMDEF_DEF_TYPE_B32 },
        { "f32",     3, SF_PARAMDEF_DEF_TYPE_F32 },
        { "angle32", 7, SF_PARAMDEF_DEF_TYPE_ANGLE32 },
        { "f64",     3, SF_PARAMDEF_DEF_TYPE_F64 },
        { "dummy8",  6, SF_PARAMDEF_DEF_TYPE_DUMMY8 },
        { "fixstr",  6, SF_PARAMDEF_DEF_TYPE_FIXSTR },
        { "fixstrW", 7, SF_PARAMDEF_DEF_TYPE_FIXSTR_W },
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].len == len && strncmp(name, table[i].name, len) == 0) {
            *out = table[i].type;
            return true;
        }
    }
    return false;
}

static bool sfi_is_array_type(sf_paramdef_def_type_t type) {
    return type == SF_PARAMDEF_DEF_TYPE_U8 ||
           type == SF_PARAMDEF_DEF_TYPE_DUMMY8 ||
           type == SF_PARAMDEF_DEF_TYPE_FIXSTR ||
           type == SF_PARAMDEF_DEF_TYPE_FIXSTR_W;
}

/*===========================================================================
 * Default value helpers (mirrors ParamUtil.cs:67-199)
 *===========================================================================*/

static sf_paramdef_default_value_t sfi_default_default(sf_paramdef_def_type_t type,
                                                       bool variable) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    (void)variable;
    return v;
}

static sf_paramdef_default_value_t sfi_default_minimum(sf_paramdef_def_type_t type,
                                                       bool variable) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:  v.v.s8  = INT8_MIN; break;
    case SF_PARAMDEF_DEF_TYPE_U8:  v.v.u8  = 0; break;
    case SF_PARAMDEF_DEF_TYPE_S16: v.v.s16 = INT16_MIN; break;
    case SF_PARAMDEF_DEF_TYPE_U16: v.v.u16 = 0; break;
    case SF_PARAMDEF_DEF_TYPE_S32:
        v.v.s32 = variable ? INT32_MIN : -2147483520;
        break;
    case SF_PARAMDEF_DEF_TYPE_U32: v.v.u32 = 0; break;
    case SF_PARAMDEF_DEF_TYPE_B32: v.v.b32 = 0; break;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        v.v.f32 = -3.402823466e+38F;
        break;
    case SF_PARAMDEF_DEF_TYPE_F64:
        v.v.f64 = variable ? -1.7976931348623157e+308 : (double)-3.402823466e+38F;
        break;
    default: break;
    }
    return v;
}

static sf_paramdef_default_value_t sfi_default_maximum(sf_paramdef_def_type_t type,
                                                       bool variable) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:  v.v.s8  = INT8_MAX; break;
    case SF_PARAMDEF_DEF_TYPE_U8:  v.v.u8  = UINT8_MAX; break;
    case SF_PARAMDEF_DEF_TYPE_S16: v.v.s16 = INT16_MAX; break;
    case SF_PARAMDEF_DEF_TYPE_U16: v.v.u16 = UINT16_MAX; break;
    case SF_PARAMDEF_DEF_TYPE_S32:
        v.v.s32 = variable ? INT32_MAX : 2147483520;
        break;
    case SF_PARAMDEF_DEF_TYPE_U32:
        v.v.u32 = variable ? (uint32_t)INT32_MAX : 4294967040u;
        break;
    case SF_PARAMDEF_DEF_TYPE_B32: v.v.b32 = 1; break;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        v.v.f32 = 3.402823466e+38F;
        break;
    case SF_PARAMDEF_DEF_TYPE_F64:
        v.v.f64 = variable ? 1.7976931348623157e+308 : (double)3.402823466e+38F;
        break;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        v.v.s32 = variable ? 0 : 1000000000;
        break;
    default: break;
    }
    return v;
}

static sf_paramdef_default_value_t sfi_default_increment(sf_paramdef_def_type_t type,
                                                         bool variable) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    (void)variable;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_S64:
    case SF_PARAMDEF_DEF_TYPE_U64:
    case SF_PARAMDEF_DEF_TYPE_B32:
        v.v.s32 = 1; break;
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
        v.v.f32 = 0.01f; break;
    case SF_PARAMDEF_DEF_TYPE_F64:
        v.v.f64 = 0.01; break;
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        v.v.s32 = variable ? 0 : 1;
        break;
    default: break;
    }
    return v;
}

static const char *sfi_default_format(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:
    case SF_PARAMDEF_DEF_TYPE_U8:
    case SF_PARAMDEF_DEF_TYPE_S16:
    case SF_PARAMDEF_DEF_TYPE_U16:
    case SF_PARAMDEF_DEF_TYPE_S32:
    case SF_PARAMDEF_DEF_TYPE_U32:
    case SF_PARAMDEF_DEF_TYPE_B32:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
        return "%d";
    case SF_PARAMDEF_DEF_TYPE_F32:
    case SF_PARAMDEF_DEF_TYPE_ANGLE32:
    case SF_PARAMDEF_DEF_TYPE_F64:
        return "%f";
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:
    default:
        return "";
    }
}

static const char *sfi_def_type_name(sf_paramdef_def_type_t type) {
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:      return "s8";
    case SF_PARAMDEF_DEF_TYPE_U8:      return "u8";
    case SF_PARAMDEF_DEF_TYPE_S16:     return "s16";
    case SF_PARAMDEF_DEF_TYPE_U16:     return "u16";
    case SF_PARAMDEF_DEF_TYPE_S32:     return "s32";
    case SF_PARAMDEF_DEF_TYPE_U32:     return "u32";
    case SF_PARAMDEF_DEF_TYPE_S64:     return "s64";
    case SF_PARAMDEF_DEF_TYPE_U64:     return "u64";
    case SF_PARAMDEF_DEF_TYPE_B32:     return "b32";
    case SF_PARAMDEF_DEF_TYPE_F32:     return "f32";
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: return "angle32";
    case SF_PARAMDEF_DEF_TYPE_F64:     return "f64";
    case SF_PARAMDEF_DEF_TYPE_DUMMY8:  return "dummy8";
    case SF_PARAMDEF_DEF_TYPE_FIXSTR:  return "fixstr";
    case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:return "fixstrW";
    default: return NULL;
    }
}

static sf_paramdef_edit_flags_t sfi_default_edit_flags(sf_paramdef_def_type_t type) {
    if (type == SF_PARAMDEF_DEF_TYPE_DUMMY8) return SF_PARAMDEF_EDIT_FLAGS_NONE;
    return SF_PARAMDEF_EDIT_FLAGS_WRAP;
}

static bool sfi_parse_edit_flags(const char *text, sf_paramdef_edit_flags_t *out) {
    if (!text || !out) return false;
    sf_paramdef_edit_flags_t flags = SF_PARAMDEF_EDIT_FLAGS_NONE;
    const char *p = text;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\t') p++;
        size_t len = (size_t)(p - start);
        if (len == 4 && strncmp(start, "None", 4) == 0) {
            /* flags |= 0 */
        } else if (len == 4 && strncmp(start, "Wrap", 4) == 0) {
            flags = (sf_paramdef_edit_flags_t)(flags | SF_PARAMDEF_EDIT_FLAGS_WRAP);
        } else if (len == 4 && strncmp(start, "Lock", 4) == 0) {
            flags = (sf_paramdef_edit_flags_t)(flags | SF_PARAMDEF_EDIT_FLAGS_LOCK);
        } else {
            return false;
        }
    }
    *out = flags;
    return true;
}

/*===========================================================================
 * Variable-value reading per-DefType (mirrors XmlSerializer.cs:214-249)
 *===========================================================================*/

static sf_paramdef_default_value_t sfi_make_value_i32(sf_paramdef_def_type_t type,
                                                     int32_t value) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:  v.v.s8  = (int8_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U8:  v.v.u8  = (uint8_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S16: v.v.s16 = (int16_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U16: v.v.u16 = (uint16_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S32: v.v.s32 = value; break;
    case SF_PARAMDEF_DEF_TYPE_U32: v.v.u32 = (uint32_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S64: v.v.s64 = value; break;
    case SF_PARAMDEF_DEF_TYPE_U64: v.v.u64 = (uint32_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_B32: v.v.b32 = (uint32_t)value; break;
    default: v.v.s32 = value; break;
    }
    return v;
}

static sf_paramdef_default_value_t sfi_make_value_f32(sf_paramdef_def_type_t type,
                                                     float value) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    if (type == SF_PARAMDEF_DEF_TYPE_ANGLE32) {
        v.v.angle32 = value;
    } else {
        v.v.f32 = value;
    }
    return v;
}

static sf_paramdef_default_value_t sfi_make_value_f64(sf_paramdef_def_type_t type,
                                                     double value) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    v.v.f64 = value;
    return v;
}

static sf_paramdef_default_value_t sfi_make_value_float_fallback(sf_paramdef_def_type_t type,
                                                                 float value) {
    sf_paramdef_default_value_t v;
    memset(&v, 0, sizeof(v));
    v.type = type;
    switch (type) {
    case SF_PARAMDEF_DEF_TYPE_S8:  v.v.s8  = (int8_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U8:  v.v.u8  = (uint8_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S16: v.v.s16 = (int16_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U16: v.v.u16 = (uint16_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S32: v.v.s32 = (int32_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U32: v.v.u32 = (uint32_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_S64: v.v.s64 = (int64_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_U64: v.v.u64 = (uint64_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_B32: v.v.b32 = (uint32_t)value; break;
    case SF_PARAMDEF_DEF_TYPE_F32: v.v.f32 = value; break;
    case SF_PARAMDEF_DEF_TYPE_ANGLE32: v.v.angle32 = value; break;
    case SF_PARAMDEF_DEF_TYPE_F64: v.v.f64 = (double)value; break;
    default: break;
    }
    return v;
}

static bool sfi_parse_typed_value(const char *text, sf_paramdef_def_type_t type,
                                  bool variable, sf_paramdef_default_value_t *out) {
    if (!text || !out) return false;
    if (variable) {
        switch (type) {
        case SF_PARAMDEF_DEF_TYPE_S8:
        case SF_PARAMDEF_DEF_TYPE_U8:
        case SF_PARAMDEF_DEF_TYPE_S16:
        case SF_PARAMDEF_DEF_TYPE_U16:
        case SF_PARAMDEF_DEF_TYPE_S32:
        case SF_PARAMDEF_DEF_TYPE_U32:
        case SF_PARAMDEF_DEF_TYPE_S64:
        case SF_PARAMDEF_DEF_TYPE_U64:
        case SF_PARAMDEF_DEF_TYPE_B32: {
            int32_t v = 0;
            if (!sfi_parse_i32(text, &v)) return false;
            *out = sfi_make_value_i32(type, v);
            return true;
        }
        case SF_PARAMDEF_DEF_TYPE_F32:
        case SF_PARAMDEF_DEF_TYPE_ANGLE32: {
            float v = 0;
            if (!sfi_parse_f32(text, &v)) return false;
            *out = sfi_make_value_f32(type, v);
            return true;
        }
        case SF_PARAMDEF_DEF_TYPE_F64: {
            double v = 0;
            if (!sfi_parse_f64(text, &v)) return false;
            *out = sfi_make_value_f64(type, v);
            return true;
        }
        case SF_PARAMDEF_DEF_TYPE_DUMMY8:
        case SF_PARAMDEF_DEF_TYPE_FIXSTR:
        case SF_PARAMDEF_DEF_TYPE_FIXSTR_W:
            memset(out, 0, sizeof(*out));
            out->type = type;
            return true;
        default: return false;
        }
    } else {
        float v = 0;
        if (!sfi_parse_f32(text, &v)) return false;
        *out = sfi_make_value_float_fallback(type, v);
        return true;
    }
}

static sf_paramdef_default_value_t sfi_read_typed_value_or_default(
    mxml_node_t *parent, const char *name, sf_paramdef_def_type_t type, bool variable,
    sf_paramdef_default_value_t default_value) {
    const char *text = sfi_child_text(parent, name);
    if (!text || text[0] == '\0') return default_value;
    sf_paramdef_default_value_t v;
    if (!sfi_parse_typed_value(text, type, variable, &v)) return default_value;
    return v;
}

/*===========================================================================
 * Def attribute parsing
 *
 * Recognised forms (mirrors XmlSerializer.cs:83-117):
 *   "type name"
 *   "type name = default"
 *   "type name:bits"        (bit field)
 *   "type name:bits = default"
 *   "type name[length]"      (array)
 *   "type name[length] = default"
 *===========================================================================*/

typedef struct sfi_def_parts {
    const char *type_start;
    size_t type_len;
    char *name;
    bool has_default;
    const char *default_start;
    size_t default_len;
    bool has_bit;
    int32_t bit_size;
    bool has_array;
    int32_t array_length;
} sfi_def_parts_t;

static void sfi_def_parts_free(sfi_def_parts_t *parts, const sf_allocator_t *alloc) {
    if (!parts) return;
    sf_xfree(alloc, parts->name);
    memset(parts, 0, sizeof(*parts));
}

static const char *sfi_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    return p;
}

static const char *sfi_skip_ws_back(const char *p, const char *start) {
    while (p > start && (p[-1] == ' ' || p[-1] == '\t')) p--;
    return p;
}

static bool sfi_parse_int32_substr(const char *s, size_t n, int32_t *out) {
    if (!s || n == 0 || !out) return false;
    char buf[16];
    if (n >= sizeof(buf)) return false;
    memcpy(buf, s, n);
    buf[n] = '\0';
    char *end = NULL;
    errno = 0;
    long v = strtol(buf, &end, 10);
    if (errno != 0 || end != buf + n) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    *out = (int32_t)v;
    return true;
}

static sf_result_t sfi_parse_def_attr(const char *def, const sf_allocator_t *alloc,
                                      sfi_def_parts_t *parts) {
    SF_CHECK_ARG(def != NULL && parts != NULL);
    memset(parts, 0, sizeof(*parts));
    parts->bit_size = -1;
    parts->array_length = 1;

    const char *p = def;
    const char *end = def + strlen(def);
    p = sfi_skip_ws(p, end);
    end = sfi_skip_ws_back(end, p);
    if (p >= end) return SF_ERR_BAD_MAGIC;

    parts->type_start = p;
    while (p < end && *p != ' ' && *p != '\t') p++;
    parts->type_len = (size_t)(p - parts->type_start);
    if (parts->type_len == 0) return SF_ERR_BAD_MAGIC;

    p = sfi_skip_ws(p, end);
    if (p >= end) return SF_ERR_BAD_MAGIC;

    const char *eq = NULL;
    for (const char *q = end; q > p; q--) {
        if (q[-1] == '=') { eq = q - 1; break; }
    }
    const char *name_end = end;
    if (eq != NULL) {
        parts->has_default = true;
        const char *default_start = sfi_skip_ws(eq + 1, end);
        const char *default_end = sfi_skip_ws_back(end, default_start);
        if (default_start >= default_end) return SF_ERR_BAD_MAGIC;
        parts->default_start = default_start;
        parts->default_len = (size_t)(default_end - default_start);
        name_end = sfi_skip_ws_back(eq, p);
    }

    if (name_end > p && name_end[-1] == ']') {
        const char *open = NULL;
        for (const char *q = name_end - 1; q > p; q--) {
            if (*q == '[') { open = q; break; }
        }
        if (open != NULL) {
            const char *num_start = sfi_skip_ws(open + 1, name_end - 1);
            const char *num_end = sfi_skip_ws_back(name_end - 1, num_start);
            int32_t length = 0;
            if (sfi_parse_int32_substr(num_start, (size_t)(num_end - num_start), &length)) {
                parts->has_array = true;
                parts->array_length = length;
                name_end = sfi_skip_ws_back(open, p);
            }
        }
    } else {
        const char *colon = NULL;
        for (const char *q = name_end - 1; q >= p; q--) {
            if (*q == ':') { colon = q; break; }
        }
        if (colon != NULL && colon + 1 < name_end) {
            const char *num_start = sfi_skip_ws(colon + 1, name_end);
            const char *num_end = sfi_skip_ws_back(name_end, num_start);
            int32_t bits = 0;
            if (sfi_parse_int32_substr(num_start, (size_t)(num_end - num_start), &bits)) {
                parts->has_bit = true;
                parts->bit_size = bits;
                name_end = sfi_skip_ws_back(colon, p);
            }
        }
    }

    if (name_end <= p) return SF_ERR_BAD_MAGIC;

    size_t name_len = (size_t)(name_end - p);
    parts->name = (char *)sf_xalloc(alloc, name_len + 1);
    if (!parts->name) return SF_ERR_OOM;
    memcpy(parts->name, p, name_len);
    parts->name[name_len] = '\0';
    return SF_OK;
}

/*===========================================================================
 * Field deserialization (mirrors XmlSerializer.cs:87-175)
 *===========================================================================*/

static sf_result_t sfi_read_field(sf_paramdef_t *def, mxml_node_t *node,
                                  sf_paramdef_field_t *field) {
    const sf_allocator_t *alloc = def->alloc;
    sf_result_t r;
    memset(field, 0, sizeof(*field));
    field->bit_size = -1;
    field->array_length = 1;

    const char *first_version = mxmlElementGetAttr(node, "FirstVersion");
    const char *removed_version = mxmlElementGetAttr(node, "RemovedVersion");
    if (first_version) {
        char *end = NULL;
        errno = 0;
        unsigned long long v = strtoull(first_version, &end, 10);
        if (errno != 0 || end == first_version) return SF_ERR_BAD_MAGIC;
        field->first_regulation_version = (uint64_t)v;
    }
    if (removed_version) {
        char *end = NULL;
        errno = 0;
        unsigned long long v = strtoull(removed_version, &end, 10);
        if (errno != 0 || end == removed_version) return SF_ERR_BAD_MAGIC;
        field->removed_regulation_version = (uint64_t)v;
    }

    const char *def_attr = mxmlElementGetAttr(node, "Def");
    if (!def_attr) return SF_ERR_BAD_MAGIC;

    sfi_def_parts_t parts;
    r = sfi_parse_def_attr(def_attr, alloc, &parts);
    if (r != SF_OK) return r;

    if (!sfi_parse_def_type(parts.type_start, parts.type_len, &field->display_type)) {
        sfi_def_parts_free(&parts, alloc);
        return SF_ERR_BAD_MAGIC;
    }

    bool variable = def->format_version >= 203;

    if (parts.has_default) {
        char buf[64];
        if (parts.default_len >= sizeof(buf)) {
            sfi_def_parts_free(&parts, alloc);
            return SF_ERR_BAD_MAGIC;
        }
        memcpy(buf, parts.default_start, parts.default_len);
        buf[parts.default_len] = '\0';
        if (!sfi_parse_typed_value(buf, field->display_type, variable,
                                   &field->default_value)) {
            sfi_def_parts_free(&parts, alloc);
            return SF_ERR_BAD_MAGIC;
        }
    } else {
        field->default_value = sfi_default_default(field->display_type, variable);
    }

    if (sf_param_util_is_bit_type(field->display_type) && parts.has_bit) {
        field->bit_size = parts.bit_size;
    } else if (sfi_is_array_type(field->display_type) && parts.has_array) {
        field->array_length = parts.array_length;
    }

    char *internal_name = sf_strdup(alloc, parts.name);
    sfi_def_parts_free(&parts, alloc);
    if (!internal_name) return SF_ERR_OOM;
    if (def->format_version < 102 && strcmp(internal_name, "unnamed") == 0) {
        internal_name[0] = '\0';
    }
    field->internal_name = internal_name;

    const char *display_name = sfi_child_text(node, "DisplayName");
    field->display_name = sf_strdup(alloc, (display_name && display_name[0])
                                               ? display_name
                                               : field->internal_name);
    if (!field->display_name) return SF_ERR_OOM;

    const char *internal_type = sfi_child_text(node, "Enum");
    const char *type_name = sfi_def_type_name(field->display_type);
    field->internal_type = sf_strdup(alloc, (internal_type && internal_type[0])
                                                ? internal_type
                                                : (type_name ? type_name : ""));
    if (!field->internal_type) return SF_ERR_OOM;

    const char *description = sfi_child_text(node, "Description");
    field->description = sf_strdup(alloc, description ? description : "");
    if (!field->description) return SF_ERR_OOM;

    const char *display_format = sfi_child_text(node, "DisplayFormat");
    field->display_format = sf_strdup(alloc, (display_format && display_format[0])
                                                 ? display_format
                                                 : sfi_default_format(field->display_type));
    if (!field->display_format) return SF_ERR_OOM;

    const char *edit_flags_text = sfi_child_text(node, "EditFlags");
    if (edit_flags_text && edit_flags_text[0]) {
        if (!sfi_parse_edit_flags(edit_flags_text, &field->edit_flags)) {
            return SF_ERR_BAD_MAGIC;
        }
    } else {
        field->edit_flags = sfi_default_edit_flags(field->display_type);
    }

    field->minimum = sfi_read_typed_value_or_default(
        node, "Minimum", field->display_type, variable,
        sfi_default_minimum(field->display_type, variable));
    field->maximum = sfi_read_typed_value_or_default(
        node, "Maximum", field->display_type, variable,
        sfi_default_maximum(field->display_type, variable));
    field->increment = sfi_read_typed_value_or_default(
        node, "Increment", field->display_type, variable,
        sfi_default_increment(field->display_type, variable));

    const char *sort_text = sfi_child_text(node, "SortID");
    if (sort_text && sort_text[0]) {
        if (!sfi_parse_i32(sort_text, &field->sort_id)) return SF_ERR_BAD_MAGIC;
    }

    size_t value_size = sf_param_util_get_value_size(field->display_type);
    if (value_size == 0) return SF_ERR_BAD_MAGIC;
    int32_t count = sfi_is_array_type(field->display_type) ? field->array_length : 1;
    if (count <= 0) return SF_ERR_BAD_MAGIC;
    field->byte_count = (int32_t)(value_size * (size_t)count);
    return SF_OK;
}

/*===========================================================================
 * Document-level deserialization (mirrors XmlSerializer.cs:18-42)
 *===========================================================================*/

static void sfi_paramdef_free(sf_paramdef_t *def) {
    if (!def) return;
    const sf_allocator_t *alloc = def->alloc;
    for (size_t i = 0; i < def->field_count; i++) {
        sf_paramdef_field_t *f = &def->fields[i];
        sf_xfree(alloc, f->display_name);
        sf_xfree(alloc, f->internal_type);
        sf_xfree(alloc, f->internal_name);
        sf_xfree(alloc, f->description);
        sf_xfree(alloc, f->display_format);
    }
    sf_xfree(alloc, def->fields);
    sf_xfree(alloc, def->param_type);
    sf_xfree(alloc, def);
}

static sf_result_t sfi_count_fields(mxml_node_t *fields_root, size_t *out) {
    SF_CHECK_ARG(fields_root != NULL && out != NULL);
    size_t count = 0;
    for (mxml_node_t *child = mxmlFindElement(fields_root, fields_root, "Field",
                                              NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, fields_root, "Field", NULL, NULL,
                                 MXML_DESCEND_NONE)) {
        count++;
    }
    *out = count;
    return SF_OK;
}

static sf_result_t sfi_paramdef_from_tree(mxml_node_t *tree, sf_paramdef_t **out,
                                          const sf_allocator_t *alloc) {
    mxml_node_t *root = NULL;
    const char *tree_name = mxmlGetElement(tree);
    if (tree_name && strcmp(tree_name, "PARAMDEF") == 0) {
        root = tree;
    } else {
        root = mxmlFindElement(tree, tree, "PARAMDEF", NULL, NULL, MXML_DESCEND_ALL);
    }
    if (!root) return SF_ERR_BAD_MAGIC;

    const char *param_type = sfi_child_text(root, "ParamType");
    if (!param_type || param_type[0] == '\0') return SF_ERR_BAD_MAGIC;

    mxml_node_t *fields_node = mxmlFindElement(root, root, "Fields", NULL, NULL,
                                               MXML_DESCEND_FIRST);
    if (!fields_node) return SF_ERR_BAD_MAGIC;

    sf_paramdef_t *def = (sf_paramdef_t *)sf_xalloc(alloc, sizeof(*def));
    if (!def) return SF_ERR_OOM;
    memset(def, 0, sizeof(*def));
    def->alloc = alloc;
    def->index = -1;

    def->param_type = sf_strdup(alloc, param_type);
    if (!def->param_type) {
        sfi_paramdef_free(def);
        return SF_ERR_OOM;
    }

    const char *data_version_text = sfi_child_text(root, "DataVersion");
    if (!data_version_text) data_version_text = sfi_child_text(root, "Unk06");
    if (data_version_text && data_version_text[0]) {
        if (!sfi_parse_i16(data_version_text, &def->data_version)) {
            sfi_paramdef_free(def);
            return SF_ERR_BAD_MAGIC;
        }
    }

    const char *big_endian_text = sfi_child_text(root, "BigEndian");
    if (big_endian_text && big_endian_text[0]) {
        if (!sfi_parse_bool(big_endian_text, &def->big_endian)) {
            sfi_paramdef_free(def);
            return SF_ERR_BAD_MAGIC;
        }
    }

    const char *unicode_text = sfi_child_text(root, "Unicode");
    if (unicode_text && unicode_text[0]) {
        if (!sfi_parse_bool(unicode_text, &def->unicode)) {
            sfi_paramdef_free(def);
            return SF_ERR_BAD_MAGIC;
        }
    }

    const char *format_text = sfi_child_text(root, "FormatVersion");
    if (!format_text) format_text = sfi_child_text(root, "Version");
    if (format_text && format_text[0]) {
        if (!sfi_parse_i16(format_text, &def->format_version)) {
            sfi_paramdef_free(def);
            return SF_ERR_BAD_MAGIC;
        }
    }

    const char *index_text = sfi_child_text(root, "Index");
    if (index_text && index_text[0]) {
        int32_t idx = 0;
        if (!sfi_parse_i32(index_text, &idx)) {
            sfi_paramdef_free(def);
            return SF_ERR_BAD_MAGIC;
        }
        def->index = idx;
    }

    size_t field_count = 0;
    sf_result_t r = sfi_count_fields(fields_node, &field_count);
    if (r != SF_OK) {
        sfi_paramdef_free(def);
        return r;
    }

    if (field_count > 0) {
        def->fields = (sf_paramdef_field_t *)sf_xalloc(
            alloc, field_count * sizeof(*def->fields));
        if (!def->fields) {
            sfi_paramdef_free(def);
            return SF_ERR_OOM;
        }
        memset(def->fields, 0, field_count * sizeof(*def->fields));
    }
    def->field_count = field_count;

    size_t i = 0;
    for (mxml_node_t *child = mxmlFindElement(fields_node, fields_node, "Field",
                                              NULL, NULL, MXML_DESCEND_FIRST);
         child != NULL;
         child = mxmlFindElement(child, fields_node, "Field", NULL, NULL,
                                 MXML_DESCEND_NONE)) {
        r = sfi_read_field(def, child, &def->fields[i]);
        if (r != SF_OK) {
            sfi_paramdef_free(def);
            return r;
        }
        def->row_size += def->fields[i].byte_count;
        i++;
    }

    *out = def;
    return SF_OK;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

sf_result_t sf_paramdef_read_xml_from_memory(sf_paramdef_t **out, const char *xml,
                                             size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || xml != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    char *nul_terminated = (char *)sf_xalloc(alloc, size + 1);
    if (!nul_terminated) return SF_ERR_OOM;
    if (size > 0) memcpy(nul_terminated, xml, size);
    nul_terminated[size] = '\0';

    mxml_options_t *options = mxmlOptionsNew();
    if (options) {
        mxmlOptionsSetTypeValue(options, MXML_TYPE_OPAQUE);
    }
    mxml_node_t *tree = mxmlLoadString(NULL, options, nul_terminated);
    if (options) mxmlOptionsDelete(options);
    sf_xfree(alloc, nul_terminated);
    if (!tree) return SF_ERR_INTERNAL;

    sf_result_t r = sfi_paramdef_from_tree(tree, out, alloc);
    mxmlDelete(tree);
    return r;
}

sf_result_t sf_paramdef_read_xml_from_path(sf_paramdef_t **out, const wchar_t *path,
                                           const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;

    int64_t length = sf_istream_length(stream);
    if (length < 0 || (uint64_t)length > (uint64_t)SIZE_MAX) {
        sf_istream_close(stream);
        return SF_ERR_OUT_OF_RANGE;
    }

    size_t size = (size_t)length;
    char *buffer = (char *)sf_xalloc(alloc, size > 0 ? size : 1);
    if (!buffer) {
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    if (size > 0) {
        r = sf_istream_read(stream, buffer, size);
        if (r != SF_OK) {
            sf_xfree(alloc, buffer);
            sf_istream_close(stream);
            return r;
        }
    }
    sf_istream_close(stream);

    r = sf_paramdef_read_xml_from_memory(out, buffer, size, alloc);
    sf_xfree(alloc, buffer);
    return r;
}
