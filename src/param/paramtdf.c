/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PARAMTDF text reader.
 *
 * Mirrors pinned upstream (commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/PARAM/PARAMTDF.cs:62-92  (string ctor parsing)
 *   SoulsFormats/Formats/PARAM/PARAMTDF.cs:25-29  (allowed type set: s8/u8/s16/u16/s32/u32)
 *
 * Strict naive Trim('"') parsing only — no escape sequences, no BOM, no
 * comments, no quoted-comma handling. Lines split on '\r' and '\n' with
 * empty entries removed (StringSplitOptions.RemoveEmptyEntries semantics).
 *
 * Note on error codes: upstream throws ArgumentException for bad type names
 * and FormatException/OverflowException for unparseable values. Mapping:
 *   - bad type name        → SF_ERR_INVALID_ARG
 *   - unparseable value    → SF_ERR_OUT_OF_RANGE (matches existing codebase
 *                            usage for "value cannot fit / cannot be parsed")
 *   - too few lines        → SF_ERR_TRUNCATED
 *   - missing comma on row → SF_ERR_INVALID_ARG (would IndexOutOfRange in C#)
 */

#include "souls_formats/sf_paramtdf.h"

#include "internal/sf_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct sf_paramtdf_entry {
    char *name; /* NULL when upstream produced a null-named entry. */
    int64_t value;
};

struct sf_paramtdf {
    const sf_allocator_t *alloc;
    char *name;
    sf_paramtdf_type_t type;
    sf_paramtdf_entry_t *entries;
    size_t entry_count;
};

/*===========================================================================
 * Helpers
 *===========================================================================*/

/*  Mirrors C#'s `string.Trim('"')`: strip ALL leading and trailing '"'.
 *  Returns a pointer into [s, s+s_len) and writes the trimmed length to
 *  *out_len. Note: the returned slice is NOT NUL-terminated. */
static const char *trim_quotes(const char *s, size_t s_len, size_t *out_len) {
    size_t start = 0;
    while (start < s_len && s[start] == '"') start++;
    size_t end = s_len;
    while (end > start && s[end - 1] == '"') end--;
    *out_len = end - start;
    return s + start;
}

/*  Allocate and copy a slice [s, s+len) as a NUL-terminated string. */
static char *dup_slice(const sf_allocator_t *a, const char *s, size_t len) {
    char *p = (char *)sf_xalloc(a, len + 1);
    if (!p) return NULL;
    if (len > 0) memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

static bool slice_eq(const char *s, size_t len, const char *literal) {
    size_t lit_len = strlen(literal);
    return len == lit_len && memcmp(s, literal, len) == 0;
}

/*  Parse the type-name slice (mirrors PARAMTDF.cs:66 + the 6-type check at
 *  PARAMTDF.cs:25-29). Returns false for any name outside {s8,u8,s16,u16,
 *  s32,u32}. */
static bool parse_type(const char *s, size_t len, sf_paramtdf_type_t *out) {
    if (slice_eq(s, len, "s8"))  { *out = SF_PARAMTDF_TYPE_S8;  return true; }
    if (slice_eq(s, len, "u8"))  { *out = SF_PARAMTDF_TYPE_U8;  return true; }
    if (slice_eq(s, len, "s16")) { *out = SF_PARAMTDF_TYPE_S16; return true; }
    if (slice_eq(s, len, "u16")) { *out = SF_PARAMTDF_TYPE_U16; return true; }
    if (slice_eq(s, len, "s32")) { *out = SF_PARAMTDF_TYPE_S32; return true; }
    if (slice_eq(s, len, "u32")) { *out = SF_PARAMTDF_TYPE_U32; return true; }
    return false;
}

/*  Range-check a parsed integer against the declared TDF type. */
static bool value_in_range(sf_paramtdf_type_t type, int64_t value) {
    switch (type) {
    case SF_PARAMTDF_TYPE_S8:  return value >= INT8_MIN  && value <= INT8_MAX;
    case SF_PARAMTDF_TYPE_U8:  return value >= 0         && value <= UINT8_MAX;
    case SF_PARAMTDF_TYPE_S16: return value >= INT16_MIN && value <= INT16_MAX;
    case SF_PARAMTDF_TYPE_U16: return value >= 0         && value <= UINT16_MAX;
    case SF_PARAMTDF_TYPE_S32: return value >= INT32_MIN && value <= INT32_MAX;
    case SF_PARAMTDF_TYPE_U32: return value >= 0         && value <= (int64_t)UINT32_MAX;
    }
    return false;
}

/*  Parse a NUL-terminated value string with strtol/strtoul depending on
 *  whether the type is signed or unsigned. Mirrors upstream's per-type
 *  Parse calls (PARAMTDF.cs:76-81). */
static bool parse_value(const char *s, sf_paramtdf_type_t type, int64_t *out) {
    if (!s || *s == '\0') return false;

    char *end = NULL;
    errno = 0;
    int64_t result;

    switch (type) {
    case SF_PARAMTDF_TYPE_S8:
    case SF_PARAMTDF_TYPE_S16:
    case SF_PARAMTDF_TYPE_S32: {
        long v = strtol(s, &end, 10);
        if (end == s || *end != '\0' || errno == ERANGE) return false;
        result = (int64_t)v;
        break;
    }
    case SF_PARAMTDF_TYPE_U8:
    case SF_PARAMTDF_TYPE_U16:
    case SF_PARAMTDF_TYPE_U32: {
        /*  Upstream byte/ushort/uint.Parse rejects leading '-'. */
        const char *scan = s;
        while (*scan == ' ' || *scan == '\t') scan++;
        if (*scan == '-') return false;
        unsigned long v = strtoul(s, &end, 10);
        if (end == s || *end != '\0' || errno == ERANGE) return false;
        result = (int64_t)v;
        break;
    }
    default:
        return false;
    }

    if (!value_in_range(type, result)) return false;
    *out = result;
    return true;
}

/*  Find the next line in [data, data+size). Sets *line_start / *line_len to
 *  the line's bounds (excluding the terminator), and returns the offset
 *  of the next byte to scan. Empty runs are skipped per
 *  StringSplitOptions.RemoveEmptyEntries. Returns false when no further
 *  non-empty line exists. */
static bool next_nonempty_line(const char *data, size_t size, size_t *cursor,
                               const char **line_start, size_t *line_len) {
    while (*cursor < size) {
        /*  Skip leading run of '\r' / '\n'. */
        while (*cursor < size && (data[*cursor] == '\r' || data[*cursor] == '\n')) {
            (*cursor)++;
        }
        if (*cursor >= size) return false;

        size_t start = *cursor;
        while (*cursor < size && data[*cursor] != '\r' && data[*cursor] != '\n') {
            (*cursor)++;
        }
        size_t len = *cursor - start;
        if (len > 0) {
            *line_start = data + start;
            *line_len = len;
            return true;
        }
    }
    return false;
}

/*  Count non-empty lines in the input so the entry-list can be sized
 *  exactly. Mirrors upstream `new List<Entry>(lines.Length - 2)`. */
static size_t count_nonempty_lines(const char *data, size_t size) {
    size_t count = 0;
    size_t cursor = 0;
    const char *line = NULL;
    size_t line_len = 0;
    while (next_nonempty_line(data, size, &cursor, &line, &line_len)) count++;
    return count;
}

/*===========================================================================
 * Public read path
 *===========================================================================*/

void sf_paramtdf_destroy(sf_paramtdf_t *tdf, const sf_allocator_t *alloc) {
    if (!tdf) return;
    /*  Honour the allocator the object was created with; the parameter is
     *  accepted for symmetry with other modules but ignored when set. */
    const sf_allocator_t *a = tdf->alloc;
    (void)alloc;

    for (size_t i = 0; i < tdf->entry_count; i++) {
        sf_xfree(a, tdf->entries[i].name);
    }
    sf_xfree(a, tdf->entries);
    sf_xfree(a, tdf->name);
    sf_xfree(a, tdf);
}

sf_result_t sf_paramtdf_read_from_text(const char *utf8_text, size_t size,
                                       sf_paramtdf_t **out,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || utf8_text != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    /*  Upstream requires at least 2 lines (name + type). */
    size_t total_lines = count_nonempty_lines(utf8_text, size);
    if (total_lines < 2) return SF_ERR_TRUNCATED;

    sf_paramtdf_t *tdf = (sf_paramtdf_t *)sf_xalloc(alloc, sizeof(*tdf));
    if (!tdf) return SF_ERR_OOM;
    memset(tdf, 0, sizeof(*tdf));
    tdf->alloc = alloc;

    sf_result_t r;
    size_t cursor = 0;
    const char *line = NULL;
    size_t line_len = 0;

    /*  Line 0 — TDF name. */
    if (!next_nonempty_line(utf8_text, size, &cursor, &line, &line_len)) {
        r = SF_ERR_TRUNCATED;
        goto fail;
    }
    {
        size_t trimmed_len = 0;
        const char *trimmed = trim_quotes(line, line_len, &trimmed_len);
        tdf->name = dup_slice(alloc, trimmed, trimmed_len);
        if (!tdf->name) { r = SF_ERR_OOM; goto fail; }
    }

    /*  Line 1 — type name; restricted to {s8,u8,s16,u16,s32,u32}. */
    if (!next_nonempty_line(utf8_text, size, &cursor, &line, &line_len)) {
        r = SF_ERR_TRUNCATED;
        goto fail;
    }
    {
        size_t trimmed_len = 0;
        const char *trimmed = trim_quotes(line, line_len, &trimmed_len);
        if (!parse_type(trimmed, trimmed_len, &tdf->type)) {
            r = SF_ERR_INVALID_ARG;
            goto fail;
        }
    }

    /*  Lines 2..N — entries. Allocate slot for every remaining line. */
    size_t expected_entries = total_lines - 2;
    if (expected_entries > 0) {
        tdf->entries = (sf_paramtdf_entry_t *)sf_xalloc(
            alloc, expected_entries * sizeof(*tdf->entries));
        if (!tdf->entries) { r = SF_ERR_OOM; goto fail; }
        memset(tdf->entries, 0, expected_entries * sizeof(*tdf->entries));
    }

    while (next_nonempty_line(utf8_text, size, &cursor, &line, &line_len)) {
        if (tdf->entry_count >= expected_entries) {
            /*  Should not happen — count_nonempty_lines is the source of
             *  truth, but guard defensively. */
            r = SF_ERR_INTERNAL;
            goto fail;
        }

        /*  Split on the first ',' (upstream uses Split(','); the second
         *  token is at index 1; any subsequent commas would be appended to
         *  later indices upstream — but value strings never contain commas
         *  in practice). We follow the literal upstream and read indices
         *  [0] and [1] only; reject lines without a comma. */
        const char *comma = (const char *)memchr(line, ',', line_len);
        if (!comma) {
            r = SF_ERR_INVALID_ARG;
            goto fail;
        }
        size_t name_len = (size_t)(comma - line);
        const char *value_raw = comma + 1;
        size_t value_raw_len = line_len - (name_len + 1);

        /*  Upstream: `if (elements[0] == "") Entries.Add(new Entry(null,
         *  value)); else Entries.Add(new Entry(elements[0].Trim('"'),
         *  value));`. Note the empty-check happens BEFORE Trim('"'), so a
         *  literal `""` parses as the empty string, NOT NULL. */
        sf_paramtdf_entry_t *entry = &tdf->entries[tdf->entry_count];
        if (name_len == 0) {
            entry->name = NULL;
        } else {
            size_t trimmed_len = 0;
            const char *trimmed = trim_quotes(line, name_len, &trimmed_len);
            entry->name = dup_slice(alloc, trimmed, trimmed_len);
            if (!entry->name) { r = SF_ERR_OOM; goto fail; }
        }

        size_t value_len = 0;
        const char *value_trim = trim_quotes(value_raw, value_raw_len, &value_len);
        char *value_cstr = dup_slice(alloc, value_trim, value_len);
        if (!value_cstr) { r = SF_ERR_OOM; goto fail; }

        bool ok = parse_value(value_cstr, tdf->type, &entry->value);
        sf_xfree(alloc, value_cstr);
        if (!ok) {
            r = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }

        tdf->entry_count++;
    }

    *out = tdf;
    return SF_OK;

fail:
    sf_paramtdf_destroy(tdf, alloc);
    return r;
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

const char *sf_paramtdf_get_name(const sf_paramtdf_t *tdf) {
    return (tdf && tdf->name) ? tdf->name : "";
}

sf_paramtdf_type_t sf_paramtdf_get_type(const sf_paramtdf_t *tdf) {
    return tdf ? tdf->type : SF_PARAMTDF_TYPE_S8;
}

size_t sf_paramtdf_get_entry_count(const sf_paramtdf_t *tdf) {
    return tdf ? tdf->entry_count : 0;
}

const sf_paramtdf_entry_t *sf_paramtdf_get_entry(const sf_paramtdf_t *tdf,
                                                 size_t index) {
    if (!tdf || index >= tdf->entry_count) return NULL;
    return &tdf->entries[index];
}

const char *sf_paramtdf_entry_get_name(const sf_paramtdf_entry_t *entry) {
    return entry ? entry->name : NULL;
}

int64_t sf_paramtdf_entry_get_value(const sf_paramtdf_entry_t *entry) {
    return entry ? entry->value : 0;
}

/*===========================================================================
 * Write path — implemented in T3.5; stub returns SF_ERR_INTERNAL.
 *===========================================================================*/

static const char *type_to_name(sf_paramtdf_type_t type) {
    switch (type) {
    case SF_PARAMTDF_TYPE_S8:  return "s8";
    case SF_PARAMTDF_TYPE_U8:  return "u8";
    case SF_PARAMTDF_TYPE_S16: return "s16";
    case SF_PARAMTDF_TYPE_U16: return "u16";
    case SF_PARAMTDF_TYPE_S32: return "s32";
    case SF_PARAMTDF_TYPE_U32: return "u32";
    }
    return NULL;
}

static sf_result_t format_value(sf_paramtdf_type_t type, int64_t value,
                                char *buf, size_t buf_size, size_t *out_len) {
    int n;
    switch (type) {
    case SF_PARAMTDF_TYPE_S8:
    case SF_PARAMTDF_TYPE_S16:
    case SF_PARAMTDF_TYPE_S32:
        n = snprintf(buf, buf_size, "%lld", (long long)value);
        break;
    case SF_PARAMTDF_TYPE_U8:
    case SF_PARAMTDF_TYPE_U16:
    case SF_PARAMTDF_TYPE_U32:
        n = snprintf(buf, buf_size, "%llu", (unsigned long long)value);
        break;
    default:
        return SF_ERR_INVALID_ARG;
    }

    if (n < 0 || (size_t)n >= buf_size) return SF_ERR_INTERNAL;
    *out_len = (size_t)n;
    return SF_OK;
}

sf_result_t sf_paramtdf_write_to_text(const sf_paramtdf_t *tdf, char **out_text,
                                      size_t *out_size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(tdf != NULL);
    SF_CHECK_ARG(out_text != NULL);
    SF_CHECK_ARG(out_size != NULL);

    *out_text = NULL;
    *out_size = 0;

    const char *name = tdf->name ? tdf->name : "";
    const char *type_name = type_to_name(tdf->type);
    SF_CHECK_ARG(type_name != NULL);

    char value_buf[32];
    size_t total = 0;

    size_t name_len = strlen(name);
    size_t type_len = strlen(type_name);
    if (name_len > SIZE_MAX - 4 || type_len > SIZE_MAX - 4) return SF_ERR_OOM;
    total += name_len + 4; /* "name"\r\n */
    total += type_len + 4; /* "type"\r\n */

    for (size_t i = 0; i < tdf->entry_count; i++) {
        const sf_paramtdf_entry_t *entry = &tdf->entries[i];
        size_t value_len = 0;
        sf_result_t r = format_value(tdf->type, entry->value, value_buf,
                                      sizeof(value_buf), &value_len);
        if (r != SF_OK) return r;

        if (entry->name != NULL) {
            size_t entry_name_len = strlen(entry->name);
            if (entry_name_len > SIZE_MAX - total - 7 - value_len) return SF_ERR_OOM;
            total += entry_name_len + value_len + 7;
        } else {
            if (value_len > SIZE_MAX - total - 5) return SF_ERR_OOM;
            total += value_len + 5;
        }
    }

    char *text = (char *)sf_xalloc(alloc, total + 1);
    if (!text) return SF_ERR_OOM;

    char *dst = text;
    *dst++ = '"';
    memcpy(dst, name, name_len);
    dst += name_len;
    *dst++ = '"';
    *dst++ = '\r';
    *dst++ = '\n';

    *dst++ = '"';
    memcpy(dst, type_name, type_len);
    dst += type_len;
    *dst++ = '"';
    *dst++ = '\r';
    *dst++ = '\n';

    for (size_t i = 0; i < tdf->entry_count; i++) {
        const sf_paramtdf_entry_t *entry = &tdf->entries[i];
        size_t value_len = 0;
        sf_result_t r = format_value(tdf->type, entry->value, value_buf,
                                      sizeof(value_buf), &value_len);
        if (r != SF_OK) {
            sf_xfree(alloc, text);
            return r;
        }

        if (entry->name != NULL) {
            size_t entry_name_len = strlen(entry->name);
            *dst++ = '"';
            memcpy(dst, entry->name, entry_name_len);
            dst += entry_name_len;
            *dst++ = '"';
        }

        *dst++ = ',';
        *dst++ = '"';
        memcpy(dst, value_buf, value_len);
        dst += value_len;
        *dst++ = '"';
        *dst++ = '\r';
        *dst++ = '\n';
    }

    *dst = '\0';
    *out_text = text;
    *out_size = total;
    return SF_OK;
}
