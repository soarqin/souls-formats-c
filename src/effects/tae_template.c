/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TAE Template XML reader and accessors.
 *
 * Mirrors pinned upstream:
 *   SoulsFormats/Formats/TAE/Template.cs
 */

#include "souls_formats/sf_tae_template.h"

#include "effects/tae_template_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mxml.h>

/*===========================================================================
 * Small parsing helpers
 *===========================================================================*/

static bool sfi_streq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

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

static bool sfi_parse_end(const char *end) {
    if (!end) return false;
    while (*end) {
        if (!isspace((unsigned char)*end)) return false;
        end++;
    }
    return true;
}

static bool sfi_parse_i64_base(const char *text, int base, int64_t *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(text, &end, base);
    if (errno != 0 || end == text || !sfi_parse_end(end)) return false;
    *out = (int64_t)v;
    return true;
}

static bool sfi_parse_u64_base(const char *text, int base, uint64_t *out) {
    if (!text || !out) return false;
    while (isspace((unsigned char)*text)) text++;
    if (*text == '-') return false;
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(text, &end, base);
    if (errno != 0 || end == text || !sfi_parse_end(end)) return false;
    *out = (uint64_t)v;
    return true;
}

static bool sfi_parse_i32(const char *text, int32_t *out) {
    int64_t v = 0;
    if (!sfi_parse_i64_base(text, 10, &v)) return false;
    if (v < INT32_MIN || v > INT32_MAX) return false;
    *out = (int32_t)v;
    return true;
}

static bool sfi_parse_i64(const char *text, int64_t *out) {
    return sfi_parse_i64_base(text, 10, out);
}

static bool sfi_parse_bool_value(const char *text, bool *out) {
    if (!text || !out) return false;
    if (sfi_str_iequal(text, "true") || sfi_streq(text, "1")) {
        *out = true;
        return true;
    }
    if (sfi_str_iequal(text, "false") || sfi_streq(text, "0")) {
        *out = false;
        return true;
    }
    return false;
}

static bool sfi_parse_f32(const char *text, float *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    float v = strtof(text, &end);
    if (errno != 0 || end == text || !sfi_parse_end(end)) return false;
    *out = v;
    return true;
}

static bool sfi_parse_f64(const char *text, double *out) {
    if (!text || !out) return false;
    char *end = NULL;
    errno = 0;
    double v = strtod(text, &end);
    if (errno != 0 || end == text || !sfi_parse_end(end)) return false;
    *out = v;
    return true;
}

static bool sfi_parse_tae_game(const char *text, sf_tae_format_t *out) {
    if (sfi_streq(text, "DS1")) {
        *out = SF_TAE_FORMAT_DS1;
    } else if (sfi_streq(text, "SOTFS")) {
        *out = SF_TAE_FORMAT_SOTFS;
    } else if (sfi_streq(text, "DS3") || sfi_streq(text, "BB")) {
        *out = SF_TAE_FORMAT_DS3;
    } else if (sfi_streq(text, "SDT")) {
        *out = SF_TAE_FORMAT_SDT;
    } else if (sfi_streq(text, "DES")) {
        *out = SF_TAE_FORMAT_DES;
    } else if (sfi_streq(text, "DESR")) {
        *out = SF_TAE_FORMAT_DESR;
    } else {
        return false;
    }
    return true;
}

static bool sfi_parse_param_type(const char *name, sf_tae_param_type_t *out) {
    if (sfi_streq(name, "b")) {
        *out = SF_TAE_PARAM_TYPE_B;
    } else if (sfi_streq(name, "u8")) {
        *out = SF_TAE_PARAM_TYPE_U8;
    } else if (sfi_streq(name, "x8")) {
        *out = SF_TAE_PARAM_TYPE_X8;
    } else if (sfi_streq(name, "s8")) {
        *out = SF_TAE_PARAM_TYPE_S8;
    } else if (sfi_streq(name, "u16")) {
        *out = SF_TAE_PARAM_TYPE_U16;
    } else if (sfi_streq(name, "x16")) {
        *out = SF_TAE_PARAM_TYPE_X16;
    } else if (sfi_streq(name, "s16")) {
        *out = SF_TAE_PARAM_TYPE_S16;
    } else if (sfi_streq(name, "u32")) {
        *out = SF_TAE_PARAM_TYPE_U32;
    } else if (sfi_streq(name, "x32")) {
        *out = SF_TAE_PARAM_TYPE_X32;
    } else if (sfi_streq(name, "s32")) {
        *out = SF_TAE_PARAM_TYPE_S32;
    } else if (sfi_streq(name, "u64")) {
        *out = SF_TAE_PARAM_TYPE_U64;
    } else if (sfi_streq(name, "x64")) {
        *out = SF_TAE_PARAM_TYPE_X64;
    } else if (sfi_streq(name, "s64")) {
        *out = SF_TAE_PARAM_TYPE_S64;
    } else if (sfi_streq(name, "f32")) {
        *out = SF_TAE_PARAM_TYPE_F32;
    } else if (sfi_streq(name, "f32grad")) {
        *out = SF_TAE_PARAM_TYPE_F32GRAD;
    } else if (sfi_streq(name, "f64")) {
        *out = SF_TAE_PARAM_TYPE_F64;
    } else if (sfi_streq(name, "aob")) {
        *out = SF_TAE_PARAM_TYPE_AOB;
    } else {
        return false;
    }
    return true;
}

static int32_t sfi_param_byte_count(const sf_tae_param_template_t *p) {
    if (!p) return 0;
    switch (p->type) {
    case SF_TAE_PARAM_TYPE_B:
    case SF_TAE_PARAM_TYPE_U8:
    case SF_TAE_PARAM_TYPE_X8:
    case SF_TAE_PARAM_TYPE_S8:
        return 1;
    case SF_TAE_PARAM_TYPE_U16:
    case SF_TAE_PARAM_TYPE_X16:
    case SF_TAE_PARAM_TYPE_S16:
        return 2;
    case SF_TAE_PARAM_TYPE_U32:
    case SF_TAE_PARAM_TYPE_X32:
    case SF_TAE_PARAM_TYPE_S32:
    case SF_TAE_PARAM_TYPE_F32:
        return 4;
    case SF_TAE_PARAM_TYPE_U64:
    case SF_TAE_PARAM_TYPE_X64:
    case SF_TAE_PARAM_TYPE_S64:
    case SF_TAE_PARAM_TYPE_F32GRAD:
    case SF_TAE_PARAM_TYPE_F64:
        return 8;
    case SF_TAE_PARAM_TYPE_AOB:
        return p->aob_length;
    default:
        return 0;
    }
}

static const char *sfi_node_text(mxml_node_t *node) {
    if (!node) return NULL;
    mxml_node_t *value = mxmlGetFirstChild(node);
    if (!value) return "";
    const char *opaque = mxmlGetOpaque(value);
    if (opaque) return opaque;
    bool ws = false;
    const char *text = mxmlGetText(value, &ws);
    return text ? text : "";
}

static const char *sfi_child_text(mxml_node_t *parent, const char *name) {
    if (!parent || !name) return NULL;
    for (mxml_node_t *child = mxmlGetFirstChild(parent); child;
         child = mxmlGetNextSibling(child)) {
        if (sfi_streq(mxmlGetElement(child), name)) return sfi_node_text(child);
    }
    return NULL;
}

static size_t sfi_count_direct(mxml_node_t *parent, const char *name) {
    size_t count = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(parent); child;
         child = mxmlGetNextSibling(child)) {
        const char *element = mxmlGetElement(child);
        if ((!name && element && element[0] != '#') || (name && sfi_streq(element, name))) count++;
    }
    return count;
}

/*===========================================================================
 * Little-endian value serialization for assert/default XML values
 *===========================================================================*/

static void sfi_put_u16_le(uint8_t *out, uint16_t v) {
    out[0] = (uint8_t)(v & 0xFFu);
    out[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void sfi_put_u32_le(uint8_t *out, uint32_t v) {
    out[0] = (uint8_t)(v & 0xFFu);
    out[1] = (uint8_t)((v >> 8) & 0xFFu);
    out[2] = (uint8_t)((v >> 16) & 0xFFu);
    out[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static void sfi_put_u64_le(uint8_t *out, uint64_t v) {
    for (size_t i = 0; i < 8; i++) out[i] = (uint8_t)((v >> (i * 8)) & 0xFFu);
}

static bool sfi_enum_lookup(const sf_tae_param_template_t *p, const char *text, int64_t *out) {
    if (!p || !text || !out) return false;
    for (size_t i = 0; i < p->enum_count; i++) {
        if (sfi_streq(p->enum_entries[i].name, text)) {
            *out = p->enum_entries[i].value;
            return true;
        }
    }
    return false;
}

static bool sfi_parse_aob(const char *text, uint8_t *out, int32_t byte_count) {
    if (!text || !out || byte_count < 0) return false;
    const char *p = text;
    int32_t count = 0;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (count >= byte_count) return false;
        char *end = NULL;
        errno = 0;
        unsigned long v = strtoul(p, &end, 16);
        if (errno != 0 || end == p || v > UINT8_MAX) return false;
        out[count++] = (uint8_t)v;
        p = end;
        if (*p && !isspace((unsigned char)*p)) return false;
    }
    return count == byte_count;
}

static bool sfi_parse_param_bytes(const sf_tae_param_template_t *p, const char *text,
                                  uint8_t *out) {
    if (!p || !text || !out) return false;
    int64_t enum_value = 0;
    char enum_buffer[32];
    if (sfi_enum_lookup(p, text, &enum_value)) {
        (void)snprintf(enum_buffer, sizeof(enum_buffer), "%lld", (long long)enum_value);
        text = enum_buffer;
    }

    int64_t sv = 0;
    uint64_t uv = 0;
    float f32 = 0.0f;
    double f64 = 0.0;
    switch (p->type) {
    case SF_TAE_PARAM_TYPE_B: {
        bool b = false;
        if (!sfi_parse_bool_value(text, &b)) return false;
        out[0] = b ? 1u : 0u;
        return true;
    }
    case SF_TAE_PARAM_TYPE_U8:
        if (!sfi_parse_u64_base(text, 10, &uv) || uv > UINT8_MAX) return false;
        out[0] = (uint8_t)uv;
        return true;
    case SF_TAE_PARAM_TYPE_X8:
        if (!sfi_parse_u64_base(text, 16, &uv) || uv > UINT8_MAX) return false;
        out[0] = (uint8_t)uv;
        return true;
    case SF_TAE_PARAM_TYPE_S8:
        if (!sfi_parse_i64_base(text, 10, &sv) || sv < INT8_MIN || sv > INT8_MAX) return false;
        out[0] = (uint8_t)(int8_t)sv;
        return true;
    case SF_TAE_PARAM_TYPE_U16:
        if (!sfi_parse_u64_base(text, 10, &uv) || uv > UINT16_MAX) return false;
        sfi_put_u16_le(out, (uint16_t)uv);
        return true;
    case SF_TAE_PARAM_TYPE_X16:
        if (!sfi_parse_u64_base(text, 16, &uv) || uv > UINT16_MAX) return false;
        sfi_put_u16_le(out, (uint16_t)uv);
        return true;
    case SF_TAE_PARAM_TYPE_S16:
        if (!sfi_parse_i64_base(text, 10, &sv) || sv < INT16_MIN || sv > INT16_MAX) return false;
        sfi_put_u16_le(out, (uint16_t)(int16_t)sv);
        return true;
    case SF_TAE_PARAM_TYPE_U32:
        if (!sfi_parse_u64_base(text, 10, &uv) || uv > UINT32_MAX) return false;
        sfi_put_u32_le(out, (uint32_t)uv);
        return true;
    case SF_TAE_PARAM_TYPE_X32:
        if (!sfi_parse_u64_base(text, 16, &uv) || uv > UINT32_MAX) return false;
        sfi_put_u32_le(out, (uint32_t)uv);
        return true;
    case SF_TAE_PARAM_TYPE_S32:
        if (!sfi_parse_i64_base(text, 10, &sv) || sv < INT32_MIN || sv > INT32_MAX) return false;
        sfi_put_u32_le(out, (uint32_t)(int32_t)sv);
        return true;
    case SF_TAE_PARAM_TYPE_U64:
        if (!sfi_parse_u64_base(text, 10, &uv)) return false;
        sfi_put_u64_le(out, uv);
        return true;
    case SF_TAE_PARAM_TYPE_X64:
        if (!sfi_parse_u64_base(text, 16, &uv)) return false;
        sfi_put_u64_le(out, uv);
        return true;
    case SF_TAE_PARAM_TYPE_S64:
        if (!sfi_parse_i64_base(text, 10, &sv)) return false;
        sfi_put_u64_le(out, (uint64_t)sv);
        return true;
    case SF_TAE_PARAM_TYPE_F32:
        if (!sfi_parse_f32(text, &f32)) return false;
        memcpy(&uv, &f32, sizeof(f32));
        sfi_put_u32_le(out, (uint32_t)uv);
        return true;
    case SF_TAE_PARAM_TYPE_F32GRAD: {
        const char *bar = strchr(text, '|');
        if (!bar) return false;
        char first[64];
        size_t n = (size_t)(bar - text);
        if (n == 0 || n >= sizeof(first)) return false;
        memcpy(first, text, n);
        first[n] = '\0';
        if (!sfi_parse_f32(first, &f32)) return false;
        memcpy(&uv, &f32, sizeof(f32));
        sfi_put_u32_le(out, (uint32_t)uv);
        if (!sfi_parse_f32(bar + 1, &f32)) return false;
        memcpy(&uv, &f32, sizeof(f32));
        sfi_put_u32_le(out + 4, (uint32_t)uv);
        return true;
    }
    case SF_TAE_PARAM_TYPE_F64:
        if (!sfi_parse_f64(text, &f64)) return false;
        memcpy(&uv, &f64, sizeof(f64));
        sfi_put_u64_le(out, uv);
        return true;
    case SF_TAE_PARAM_TYPE_AOB:
        return sfi_parse_aob(text, out, p->aob_length);
    default:
        return false;
    }
}

static bool sfi_parse_enum_value(const sf_tae_param_template_t *p, const char *text,
                                 int64_t *out) {
    if (!p || !text || !out) return false;
    int64_t sv = 0;
    uint64_t uv = 0;
    switch (p->type) {
    case SF_TAE_PARAM_TYPE_B: {
        bool b = false;
        if (!sfi_parse_bool_value(text, &b)) return false;
        *out = b ? 1 : 0;
        return true;
    }
    case SF_TAE_PARAM_TYPE_X8:
    case SF_TAE_PARAM_TYPE_X16:
    case SF_TAE_PARAM_TYPE_X32:
    case SF_TAE_PARAM_TYPE_X64:
        if (!sfi_parse_u64_base(text, 16, &uv) || uv > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)uv;
        return true;
    case SF_TAE_PARAM_TYPE_U8:
    case SF_TAE_PARAM_TYPE_U16:
    case SF_TAE_PARAM_TYPE_U32:
    case SF_TAE_PARAM_TYPE_U64:
        if (!sfi_parse_u64_base(text, 10, &uv) || uv > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)uv;
        return true;
    case SF_TAE_PARAM_TYPE_S8:
    case SF_TAE_PARAM_TYPE_S16:
    case SF_TAE_PARAM_TYPE_S32:
    case SF_TAE_PARAM_TYPE_S64:
        if (!sfi_parse_i64_base(text, 10, &sv)) return false;
        *out = sv;
        return true;
    default:
        return false;
    }
}

/*===========================================================================
 * Recursive allocation cleanup and cloning
 *===========================================================================*/

static void sfi_free_param(const sf_allocator_t *alloc, sf_tae_param_template_t *p) {
    if (!p) return;
    sf_xfree(alloc, p->name);
    sf_xfree(alloc, p->name_group);
    sf_xfree(alloc, p->key);
    sf_xfree(alloc, p->assert_bytes);
    sf_xfree(alloc, p->default_bytes);
    for (size_t i = 0; i < p->enum_count; i++) sf_xfree(alloc, p->enum_entries[i].name);
    sf_xfree(alloc, p->enum_entries);
    sf_xfree(alloc, p);
}

static void sfi_free_event(const sf_allocator_t *alloc, sf_tae_event_template_t *e) {
    if (!e) return;
    sf_xfree(alloc, e->name);
    for (size_t i = 0; i < e->param_count; i++) sfi_free_param(alloc, e->params[i]);
    sf_xfree(alloc, e->params);
    sf_xfree(alloc, e);
}

static void sfi_free_bank(const sf_allocator_t *alloc, sf_tae_bank_template_t *b) {
    if (!b) return;
    sf_xfree(alloc, b->name);
    for (size_t i = 0; i < b->event_count; i++) sfi_free_event(alloc, b->events[i]);
    sf_xfree(alloc, b->events);
    sf_xfree(alloc, b);
}

void sf_tae_template_destroy(sf_tae_template_t *t) {
    if (!t) return;
    const sf_allocator_t *alloc = t->alloc;
    for (size_t i = 0; i < t->bank_count; i++) sfi_free_bank(alloc, t->banks[i]);
    sf_xfree(alloc, t->banks);
    sf_xfree(alloc, t);
}

static sf_result_t sfi_clone_param(const sf_allocator_t *alloc, const sf_tae_param_template_t *src,
                                   sf_tae_param_template_t **out) {
    sf_tae_param_template_t *p = (sf_tae_param_template_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->type = src->type;
    p->aob_length = src->aob_length;
    p->has_assert = src->has_assert;
    p->has_default = src->has_default;

    int32_t byte_count = sfi_param_byte_count(src);
    p->name = sf_strdup(alloc, src->name);
    p->name_group = sf_strdup(alloc, src->name_group);
    p->key = sf_strdup(alloc, src->key);
    if ((src->name && !p->name) || (src->name_group && !p->name_group) || (src->key && !p->key)) {
        sfi_free_param(alloc, p);
        return SF_ERR_OOM;
    }
    if (src->has_assert && byte_count > 0) {
        p->assert_bytes = (uint8_t *)sf_xalloc(alloc, (size_t)byte_count);
        if (!p->assert_bytes) {
            sfi_free_param(alloc, p);
            return SF_ERR_OOM;
        }
        memcpy(p->assert_bytes, src->assert_bytes, (size_t)byte_count);
    }
    if (src->has_default && byte_count > 0) {
        p->default_bytes = (uint8_t *)sf_xalloc(alloc, (size_t)byte_count);
        if (!p->default_bytes) {
            sfi_free_param(alloc, p);
            return SF_ERR_OOM;
        }
        memcpy(p->default_bytes, src->default_bytes, (size_t)byte_count);
    }
    if (src->enum_count > 0) {
        p->enum_entries = (sf_tae_enum_entry_t *)sf_xalloc(
            alloc, src->enum_count * sizeof(*p->enum_entries));
        if (!p->enum_entries) {
            sfi_free_param(alloc, p);
            return SF_ERR_OOM;
        }
        memset(p->enum_entries, 0, src->enum_count * sizeof(*p->enum_entries));
        p->enum_count = src->enum_count;
        for (size_t i = 0; i < p->enum_count; i++) {
            p->enum_entries[i].name = sf_strdup(alloc, src->enum_entries[i].name);
            p->enum_entries[i].value = src->enum_entries[i].value;
            if (!p->enum_entries[i].name) {
                sfi_free_param(alloc, p);
                return SF_ERR_OOM;
            }
        }
    }
    *out = p;
    return SF_OK;
}

static sf_result_t sfi_clone_event(const sf_allocator_t *alloc, const sf_tae_event_template_t *src,
                                   sf_tae_event_template_t **out) {
    sf_tae_event_template_t *e = (sf_tae_event_template_t *)sf_xalloc(alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    e->id = src->id;
    e->total_byte_count = src->total_byte_count;
    e->name = sf_strdup(alloc, src->name);
    if (src->name && !e->name) {
        sfi_free_event(alloc, e);
        return SF_ERR_OOM;
    }
    if (src->param_count > 0) {
        e->params = (sf_tae_param_template_t **)sf_xalloc(alloc, src->param_count * sizeof(*e->params));
        if (!e->params) {
            sfi_free_event(alloc, e);
            return SF_ERR_OOM;
        }
        memset(e->params, 0, src->param_count * sizeof(*e->params));
        e->param_count = src->param_count;
        for (size_t i = 0; i < e->param_count; i++) {
            sf_result_t r = sfi_clone_param(alloc, src->params[i], &e->params[i]);
            if (r != SF_OK) {
                sfi_free_event(alloc, e);
                return r;
            }
        }
    }
    *out = e;
    return SF_OK;
}

/*===========================================================================
 * XML deserialization
 *===========================================================================*/

static sf_tae_bank_template_t *sfi_find_bank_mut(sf_tae_template_t *t, int64_t bank_id) {
    if (!t) return NULL;
    for (size_t i = 0; i < t->bank_count; i++) {
        if (t->banks[i]->id == bank_id) return t->banks[i];
    }
    return NULL;
}

static sf_tae_event_template_t *sfi_find_event_mut(sf_tae_bank_template_t *b, int32_t event_id) {
    if (!b) return NULL;
    for (size_t i = 0; i < b->event_count; i++) {
        if (b->events[i]->id == event_id) return b->events[i];
    }
    return NULL;
}

static sf_result_t sfi_make_key(const sf_allocator_t *alloc, sf_tae_param_template_t *p) {
    if (!p || !p->name) return SF_ERR_BAD_MAGIC;
    if (!p->name_group) {
        p->key = sf_strdup(alloc, p->name);
    } else {
        size_t n = strlen(p->name_group) + 2 + strlen(p->name) + 1;
        p->key = (char *)sf_xalloc(alloc, n);
        if (p->key) (void)snprintf(p->key, n, "%s::%s", p->name_group, p->name);
    }
    return p->key ? SF_OK : SF_ERR_OOM;
}

static sf_result_t sfi_parse_value_attr_or_child(const sf_allocator_t *alloc,
                                                 sf_tae_param_template_t *p,
                                                 mxml_node_t *node, const char *name,
                                                 bool *has_value, uint8_t **bytes) {
    const char *text = sfi_child_text(node, name);
    if (!text) text = mxmlElementGetAttr(node, name);
    *has_value = false;
    *bytes = NULL;
    if (!text) return SF_OK;

    int32_t byte_count = sfi_param_byte_count(p);
    if (byte_count <= 0) return SF_ERR_BAD_MAGIC;
    uint8_t *parsed = (uint8_t *)sf_xalloc(alloc, (size_t)byte_count);
    if (!parsed) return SF_ERR_OOM;
    if (!sfi_parse_param_bytes(p, text, parsed)) {
        sf_xfree(alloc, parsed);
        return SF_ERR_BAD_MAGIC;
    }
    *has_value = true;
    *bytes = parsed;
    return SF_OK;
}

static sf_result_t sfi_parse_param(const sf_allocator_t *alloc, int64_t bank_id,
                                   int32_t event_id, size_t param_index, mxml_node_t *node,
                                   int32_t offset, sf_tae_param_template_t **out) {
    (void)bank_id;
    (void)event_id;
    (void)param_index;
    const char *element = mxmlGetElement(node);
    sf_tae_param_template_t *p = (sf_tae_param_template_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    memset(p, 0, sizeof(*p));
    p->aob_length = -1;

    if (!sfi_parse_param_type(element, &p->type)) {
        sfi_free_param(alloc, p);
        return SF_ERR_BAD_MAGIC;
    }

    const char *group = mxmlElementGetAttr(node, "group");
    if (group) {
        p->name_group = sf_strdup(alloc, group);
        if (!p->name_group) {
            sfi_free_param(alloc, p);
            return SF_ERR_OOM;
        }
    }

    const char *name = mxmlElementGetAttr(node, "name");
    if (name) {
        p->name = sf_strdup(alloc, name);
    } else {
        char generated[32];
        (void)snprintf(generated, sizeof(generated), "Unk%02X", (unsigned int)offset);
        p->name = sf_strdup(alloc, generated);
    }
    if (!p->name) {
        sfi_free_param(alloc, p);
        return SF_ERR_OOM;
    }

    const char *length = mxmlElementGetAttr(node, "length");
    if (length) {
        int32_t parsed_length = 0;
        if (!sfi_parse_i32(length, &parsed_length) || parsed_length < 0) {
            sfi_free_param(alloc, p);
            return SF_ERR_BAD_MAGIC;
        }
        p->aob_length = parsed_length;
    } else if (p->type == SF_TAE_PARAM_TYPE_AOB) {
        sfi_free_param(alloc, p);
        return SF_ERR_BAD_MAGIC;
    }

    size_t enum_count = sfi_count_direct(node, "entry");
    if (enum_count > 0) {
        p->enum_entries = (sf_tae_enum_entry_t *)sf_xalloc(
            alloc, enum_count * sizeof(*p->enum_entries));
        if (!p->enum_entries) {
            sfi_free_param(alloc, p);
            return SF_ERR_OOM;
        }
        memset(p->enum_entries, 0, enum_count * sizeof(*p->enum_entries));
        p->enum_count = enum_count;
        size_t i = 0;
        for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
            if (!sfi_streq(mxmlGetElement(child), "entry")) continue;
            const char *entry_name = mxmlElementGetAttr(child, "name");
            const char *entry_value = mxmlElementGetAttr(child, "value");
            if (!entry_name || !entry_value || !sfi_parse_enum_value(p, entry_value,
                                                                     &p->enum_entries[i].value)) {
                sfi_free_param(alloc, p);
                return SF_ERR_BAD_MAGIC;
            }
            p->enum_entries[i].name = sf_strdup(alloc, entry_name);
            if (!p->enum_entries[i].name) {
                sfi_free_param(alloc, p);
                return SF_ERR_OOM;
            }
            i++;
        }
    }

    sf_result_t r = sfi_parse_value_attr_or_child(alloc, p, node, "assert", &p->has_assert,
                                                  &p->assert_bytes);
    if (r != SF_OK) {
        sfi_free_param(alloc, p);
        return r;
    }
    r = sfi_parse_value_attr_or_child(alloc, p, node, "default", &p->has_default,
                                      &p->default_bytes);
    if (r != SF_OK) {
        sfi_free_param(alloc, p);
        return r;
    }

    r = sfi_make_key(alloc, p);
    if (r != SF_OK) {
        sfi_free_param(alloc, p);
        return r;
    }
    *out = p;
    return SF_OK;
}

static sf_result_t sfi_parse_event(const sf_allocator_t *alloc, int64_t bank_id,
                                   mxml_node_t *node, sf_tae_event_template_t **out) {
    const char *id_text = mxmlElementGetAttr(node, "id");
    if (!id_text) return SF_ERR_BAD_MAGIC;

    sf_tae_event_template_t *e = (sf_tae_event_template_t *)sf_xalloc(alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    if (!sfi_parse_i32(id_text, &e->id)) {
        sfi_free_event(alloc, e);
        return SF_ERR_BAD_MAGIC;
    }

    const char *name = mxmlElementGetAttr(node, "name");
    if (name) {
        e->name = sf_strdup(alloc, name);
    } else {
        char generated[32];
        (void)snprintf(generated, sizeof(generated), "Event%d", e->id);
        e->name = sf_strdup(alloc, generated);
    }
    if (!e->name) {
        sfi_free_event(alloc, e);
        return SF_ERR_OOM;
    }

    size_t param_count = sfi_count_direct(node, NULL);
    for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
        const char *element = mxmlGetElement(child);
        if (sfi_streq(element, "assert") || sfi_streq(element, "default") ||
            sfi_streq(element, "entry")) {
            param_count--;
        }
    }
    if (param_count > 0) {
        e->params = (sf_tae_param_template_t **)sf_xalloc(alloc, param_count * sizeof(*e->params));
        if (!e->params) {
            sfi_free_event(alloc, e);
            return SF_ERR_OOM;
        }
        memset(e->params, 0, param_count * sizeof(*e->params));
    }

    int32_t offset = 0;
    size_t i = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
        const char *element = mxmlGetElement(child);
        if (!element || element[0] == '#' || sfi_streq(element, "assert") ||
            sfi_streq(element, "default") || sfi_streq(element, "entry")) {
            continue;
        }
        sf_result_t r = sfi_parse_param(alloc, bank_id, e->id, i, child, offset, &e->params[i]);
        if (r != SF_OK) {
            sfi_free_event(alloc, e);
            return r;
        }
        for (size_t j = 0; j < i; j++) {
            if (sfi_streq(e->params[j]->key, e->params[i]->key)) {
                sfi_free_event(alloc, e);
                return SF_ERR_BAD_MAGIC;
            }
        }
        int32_t byte_count = sfi_param_byte_count(e->params[i]);
        if (byte_count <= 0 || e->total_byte_count > INT32_MAX - byte_count) {
            sfi_free_event(alloc, e);
            return SF_ERR_BAD_MAGIC;
        }
        offset += byte_count;
        e->total_byte_count += byte_count;
        i++;
    }
    e->param_count = i;
    *out = e;
    return SF_OK;
}

static sf_result_t sfi_parse_bank(const sf_allocator_t *alloc, mxml_node_t *node,
                                  sf_tae_bank_template_t **out, int64_t *out_based_on) {
    const char *id_text = mxmlElementGetAttr(node, "id");
    const char *name = mxmlElementGetAttr(node, "name");
    if (!id_text || !name) return SF_ERR_BAD_MAGIC;

    sf_tae_bank_template_t *b = (sf_tae_bank_template_t *)sf_xalloc(alloc, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    if (!sfi_parse_i64(id_text, &b->id)) {
        sfi_free_bank(alloc, b);
        return SF_ERR_BAD_MAGIC;
    }
    b->name = sf_strdup(alloc, name);
    if (!b->name) {
        sfi_free_bank(alloc, b);
        return SF_ERR_OOM;
    }

    const char *based_on = mxmlElementGetAttr(node, "basedon");
    *out_based_on = -1;
    if (based_on && !sfi_parse_i64(based_on, out_based_on)) {
        sfi_free_bank(alloc, b);
        return SF_ERR_BAD_MAGIC;
    }

    size_t event_count = sfi_count_direct(node, "event");
    if (event_count > 0) {
        b->events = (sf_tae_event_template_t **)sf_xalloc(alloc, event_count * sizeof(*b->events));
        if (!b->events) {
            sfi_free_bank(alloc, b);
            return SF_ERR_OOM;
        }
        memset(b->events, 0, event_count * sizeof(*b->events));
    }
    size_t i = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(node); child; child = mxmlGetNextSibling(child)) {
        if (!sfi_streq(mxmlGetElement(child), "event")) continue;
        sf_result_t r = sfi_parse_event(alloc, b->id, child, &b->events[i]);
        if (r != SF_OK) {
            sfi_free_bank(alloc, b);
            return r;
        }
        for (size_t j = 0; j < i; j++) {
            if (b->events[j]->id == b->events[i]->id) {
                sfi_free_bank(alloc, b);
                return SF_ERR_BAD_MAGIC;
            }
        }
        i++;
    }
    b->event_count = i;
    *out = b;
    return SF_OK;
}

static sf_result_t sfi_resolve_based_on(sf_tae_template_t *t, const int64_t *based_on) {
    const sf_allocator_t *alloc = t->alloc;
    for (size_t i = 0; i < t->bank_count; i++) {
        if (based_on[i] == -1) continue;
        sf_tae_bank_template_t *dst = t->banks[i];
        sf_tae_bank_template_t *src = sfi_find_bank_mut(t, based_on[i]);
        if (!src) return SF_ERR_BAD_MAGIC;

        size_t missing = 0;
        for (size_t j = 0; j < src->event_count; j++) {
            if (!sfi_find_event_mut(dst, src->events[j]->id)) missing++;
        }
        if (missing == 0) continue;

        size_t old_count = dst->event_count;
        size_t new_count = old_count + missing;
        sf_tae_event_template_t **events = (sf_tae_event_template_t **)sf_xrealloc(
            alloc, dst->events, old_count * sizeof(*dst->events), new_count * sizeof(*dst->events));
        if (!events) return SF_ERR_OOM;
        dst->events = events;
        for (size_t j = old_count; j < new_count; j++) dst->events[j] = NULL;

        size_t write = old_count;
        for (size_t j = 0; j < src->event_count; j++) {
            if (sfi_find_event_mut(dst, src->events[j]->id)) continue;
            sf_result_t r = sfi_clone_event(alloc, src->events[j], &dst->events[write]);
            if (r != SF_OK) return r;
            write++;
        }
        dst->event_count = new_count;
    }
    return SF_OK;
}

static sf_result_t sfi_template_from_tree(mxml_node_t *tree, sf_tae_template_t **out,
                                          const sf_allocator_t *alloc) {
    mxml_node_t *root = NULL;
    const char *tree_name = mxmlGetElement(tree);
    if (sfi_streq(tree_name, "event_template")) {
        root = tree;
    } else {
        root = mxmlFindElement(tree, tree, "event_template", NULL, NULL, MXML_DESCEND_ALL);
    }
    if (!root) return SF_ERR_BAD_MAGIC;

    const char *game = mxmlElementGetAttr(root, "game");
    if (!game) return SF_ERR_BAD_MAGIC;

    sf_tae_template_t *t = (sf_tae_template_t *)sf_xalloc(alloc, sizeof(*t));
    if (!t) return SF_ERR_OOM;
    memset(t, 0, sizeof(*t));
    t->alloc = alloc;
    if (!sfi_parse_tae_game(game, &t->game)) {
        sf_tae_template_destroy(t);
        return SF_ERR_BAD_MAGIC;
    }

    t->bank_count = sfi_count_direct(root, "bank");
    int64_t *based_on = NULL;
    if (t->bank_count > 0) {
        t->banks = (sf_tae_bank_template_t **)sf_xalloc(alloc, t->bank_count * sizeof(*t->banks));
        based_on = (int64_t *)sf_xalloc(alloc, t->bank_count * sizeof(*based_on));
        if (!t->banks || !based_on) {
            sf_xfree(alloc, based_on);
            sf_tae_template_destroy(t);
            return SF_ERR_OOM;
        }
        memset(t->banks, 0, t->bank_count * sizeof(*t->banks));
    }

    size_t i = 0;
    for (mxml_node_t *child = mxmlGetFirstChild(root); child; child = mxmlGetNextSibling(child)) {
        if (!sfi_streq(mxmlGetElement(child), "bank")) continue;
        sf_result_t r = sfi_parse_bank(alloc, child, &t->banks[i], &based_on[i]);
        if (r != SF_OK) {
            sf_xfree(alloc, based_on);
            sf_tae_template_destroy(t);
            return r;
        }
        for (size_t j = 0; j < i; j++) {
            if (t->banks[j]->id == t->banks[i]->id) {
                sf_xfree(alloc, based_on);
                sf_tae_template_destroy(t);
                return SF_ERR_BAD_MAGIC;
            }
        }
        i++;
    }
    t->bank_count = i;

    sf_result_t r = sfi_resolve_based_on(t, based_on);
    sf_xfree(alloc, based_on);
    if (r != SF_OK) {
        sf_tae_template_destroy(t);
        return r;
    }

    *out = t;
    return SF_OK;
}

sf_result_t sf_tae_template_read_from_memory(sf_tae_template_t **out, const char *xml_text,
                                             size_t xml_len, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (xml_len == 0 || xml_text != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    char *nul_terminated = (char *)sf_xalloc(alloc, xml_len + 1);
    if (!nul_terminated) return SF_ERR_OOM;
    if (xml_len > 0) memcpy(nul_terminated, xml_text, xml_len);
    nul_terminated[xml_len] = '\0';

    mxml_options_t *options = mxmlOptionsNew();
    if (options) mxmlOptionsSetTypeValue(options, MXML_TYPE_OPAQUE);
    mxml_node_t *tree = mxmlLoadString(NULL, options, nul_terminated);
    if (options) mxmlOptionsDelete(options);
    sf_xfree(alloc, nul_terminated);
    if (!tree) return SF_ERR_INTERNAL;

    sf_result_t r = sfi_template_from_tree(tree, out, alloc);
    mxmlDelete(tree);
    return r;
}

sf_result_t sf_tae_template_read_from_file(sf_tae_template_t **out, const wchar_t *path,
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

    r = sf_tae_template_read_from_memory(out, buffer, size, alloc);
    sf_xfree(alloc, buffer);
    return r;
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

sf_tae_format_t sf_tae_template_game(const sf_tae_template_t *t) {
    return t ? t->game : SF_TAE_FORMAT_DS1;
}

size_t sf_tae_template_bank_count(const sf_tae_template_t *t) {
    return t ? t->bank_count : 0;
}

const sf_tae_bank_template_t *sf_tae_template_find_bank(const sf_tae_template_t *t,
                                                        int64_t bank_id) {
    if (!t) return NULL;
    for (size_t i = 0; i < t->bank_count; i++) {
        if (t->banks[i]->id == bank_id) return t->banks[i];
    }
    return NULL;
}

const sf_tae_bank_template_t *sf_tae_template_bank(const sf_tae_template_t *t, size_t i) {
    if (!t || i >= t->bank_count) return NULL;
    return t->banks[i];
}

int64_t sf_tae_bank_template_id(const sf_tae_bank_template_t *b) {
    return b ? b->id : 0;
}

const char *sf_tae_bank_template_name(const sf_tae_bank_template_t *b) {
    return b ? b->name : NULL;
}

size_t sf_tae_bank_template_event_count(const sf_tae_bank_template_t *b) {
    return b ? b->event_count : 0;
}

const sf_tae_event_template_t *sf_tae_bank_template_find_event(const sf_tae_bank_template_t *b,
                                                              int32_t event_id) {
    if (!b) return NULL;
    for (size_t i = 0; i < b->event_count; i++) {
        if (b->events[i]->id == event_id) return b->events[i];
    }
    return NULL;
}

const sf_tae_event_template_t *sf_tae_bank_template_event(const sf_tae_bank_template_t *b,
                                                         size_t i) {
    if (!b || i >= b->event_count) return NULL;
    return b->events[i];
}

int32_t sf_tae_event_template_id(const sf_tae_event_template_t *e) {
    return e ? e->id : 0;
}

const char *sf_tae_event_template_name(const sf_tae_event_template_t *e) {
    return e ? e->name : NULL;
}

size_t sf_tae_event_template_param_count(const sf_tae_event_template_t *e) {
    return e ? e->param_count : 0;
}

int32_t sf_tae_event_template_total_byte_count(const sf_tae_event_template_t *e) {
    return e ? e->total_byte_count : 0;
}

const sf_tae_param_template_t *sf_tae_event_template_param(const sf_tae_event_template_t *e,
                                                          size_t i) {
    if (!e || i >= e->param_count) return NULL;
    return e->params[i];
}

const sf_tae_param_template_t *sf_tae_event_template_find_param(
    const sf_tae_event_template_t *e, const char *key) {
    if (!e || !key) return NULL;
    for (size_t i = 0; i < e->param_count; i++) {
        if (sfi_streq(e->params[i]->key, key)) return e->params[i];
    }
    return NULL;
}

sf_tae_param_type_t sf_tae_param_template_type(const sf_tae_param_template_t *p) {
    return p ? p->type : (sf_tae_param_type_t)-1;
}

const char *sf_tae_param_template_name(const sf_tae_param_template_t *p) {
    return p ? p->name : NULL;
}

const char *sf_tae_param_template_name_group(const sf_tae_param_template_t *p) {
    return p ? p->name_group : NULL;
}

const char *sf_tae_param_template_key(const sf_tae_param_template_t *p) {
    return p ? p->key : NULL;
}

int32_t sf_tae_param_template_byte_count(const sf_tae_param_template_t *p) {
    return sfi_param_byte_count(p);
}

int32_t sf_tae_param_template_aob_length(const sf_tae_param_template_t *p) {
    return p ? p->aob_length : -1;
}

bool sf_tae_param_template_has_assert(const sf_tae_param_template_t *p) {
    return p ? p->has_assert : false;
}

bool sf_tae_param_template_has_default(const sf_tae_param_template_t *p) {
    return p ? p->has_default : false;
}

size_t sf_tae_param_template_enum_count(const sf_tae_param_template_t *p) {
    return p ? p->enum_count : 0;
}

const char *sf_tae_param_template_enum_name(const sf_tae_param_template_t *p, size_t i) {
    if (!p || i >= p->enum_count) return NULL;
    return p->enum_entries[i].name;
}

int64_t sf_tae_param_template_enum_value(const sf_tae_param_template_t *p, size_t i) {
    if (!p || i >= p->enum_count) return 0;
    return p->enum_entries[i].value;
}
