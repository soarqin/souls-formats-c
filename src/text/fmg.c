/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — FMG string container reader.
 *
 * Mirrors pinned upstream (commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/FMG.cs:68-139  (Read)
 *
 * The FMG layout has three width-sensitive regions: header, group table,
 * and string-offset table. The "wide" mode (DarkSouls3 / Bloodborne / DS3 /
 * ER) uses 8-byte varints and 16-byte group entries; "narrow" (DemonsSouls
 * and DarkSouls1/2) uses 4-byte varints and 12-byte group entries.
 *
 * Optional 16-byte MD5 prefix (Gundam Unicorn) is detected by peeking byte
 * 0: if non-zero, prefix is present and is skipped (NOT verified, mirroring
 * upstream limitation).
 *
 * Write path lives in T3.6 and currently returns SF_ERR_UNSUPPORTED_VERSION.
 */

#include "souls_formats/sf_fmg.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Internal data layout
 *===========================================================================*/

struct sf_fmg_entry {
    int32_t  id;
    char    *text_utf8; /* NULL = deleted (tombstone). "" = valid empty. */
};

struct sf_fmg {
    const sf_allocator_t *alloc;

    sf_fmg_version_t version;
    bool             big_endian;
    bool             unicode;
    bool             has_md5;
    bool             reuse_offsets;

    sf_fmg_entry_t *entries;
    size_t          entry_count;
    size_t          entry_capacity;
};

/*===========================================================================
 * Helpers
 *===========================================================================*/

static void entry_clear(sf_fmg_entry_t *e, const sf_allocator_t *alloc) {
    if (!e) return;
    sf_xfree(alloc, e->text_utf8);
    e->text_utf8 = NULL;
    e->id = 0;
}

static sf_result_t entries_reserve(sf_fmg_t *fmg, size_t need) {
    if (need <= fmg->entry_capacity) return SF_OK;
    size_t new_cap = fmg->entry_capacity ? fmg->entry_capacity * 2 : 16;
    while (new_cap < need) {
        if (new_cap > SIZE_MAX / 2) return SF_ERR_OUT_OF_RANGE;
        new_cap *= 2;
    }
    size_t old_bytes = fmg->entry_capacity * sizeof(sf_fmg_entry_t);
    size_t new_bytes = new_cap * sizeof(sf_fmg_entry_t);
    sf_fmg_entry_t *p = (sf_fmg_entry_t *)sf_xrealloc(fmg->alloc, fmg->entries,
                                                      old_bytes, new_bytes);
    if (!p) return SF_ERR_OOM;
    /* Zero new tail to keep destroy safe on partial fill. */
    memset((char *)p + old_bytes, 0, new_bytes - old_bytes);
    fmg->entries = p;
    fmg->entry_capacity = new_cap;
    return SF_OK;
}

static sf_result_t entries_append(sf_fmg_t *fmg, int32_t id, char *text_utf8) {
    sf_result_t r = entries_reserve(fmg, fmg->entry_count + 1);
    if (r != SF_OK) return r;
    fmg->entries[fmg->entry_count].id = id;
    fmg->entries[fmg->entry_count].text_utf8 = text_utf8;
    fmg->entry_count++;
    return SF_OK;
}

static bool version_is_known(uint8_t v) {
    return v == SF_FMG_VERSION_DEMONS_SOULS ||
           v == SF_FMG_VERSION_DARK_SOULS_1 ||
           v == SF_FMG_VERSION_DARK_SOULS_3;
}

static sf_result_t fmg_alloc_blank(const sf_allocator_t *alloc, sf_fmg_t **out) {
    sf_fmg_t *fmg = (sf_fmg_t *)sf_xalloc(alloc, sizeof(*fmg));
    if (!fmg) return SF_ERR_OOM;
    memset(fmg, 0, sizeof(*fmg));
    fmg->alloc = alloc;
    fmg->version = SF_FMG_VERSION_DARK_SOULS_1;
    fmg->big_endian = false;
    fmg->unicode = true;
    fmg->has_md5 = false;
    fmg->reuse_offsets = false;
    *out = fmg;
    return SF_OK;
}

/*===========================================================================
 * Read path — mirrors FMG.cs:68-139
 *===========================================================================*/

static sf_result_t fmg_read(sf_binary_reader_t *br, sf_fmg_t **out,
                            const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_result_t r;
    sf_fmg_t *fmg = NULL;
    r = fmg_alloc_blank(alloc, &fmg);
    if (r != SF_OK) return r;

    /*  MD5 prefix detection (FMG.cs:70-74). */
    uint8_t first_byte = 0;
    r = sf_binary_reader_get_u8(br, 0, &first_byte);
    if (r != SF_OK) goto fail;
    if (first_byte != 0) {
        fmg->has_md5 = true;
        r = sf_binary_reader_skip(br, 16);
        if (r != SF_OK) goto fail;
    }

    /*  Header (FMG.cs:76-99). */
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    bool big_endian = false;
    r = sf_binary_reader_read_bool(br, &big_endian); if (r != SF_OK) goto fail;
    fmg->big_endian = big_endian;
    sf_binary_reader_set_big_endian(br, big_endian);

    uint8_t version_byte = 0;
    r = sf_binary_reader_read_u8(br, &version_byte); if (r != SF_OK) goto fail;
    if (!version_is_known(version_byte)) {
        r = SF_ERR_UNSUPPORTED_VERSION;
        goto fail;
    }
    fmg->version = (sf_fmg_version_t)version_byte;

    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    bool wide = (fmg->version == SF_FMG_VERSION_DARK_SOULS_3);
    sf_binary_reader_set_varint_long(br, wide);

    int32_t file_size = 0;
    r = sf_binary_reader_read_i32(br, &file_size); if (r != SF_OK) goto fail;
    (void)file_size; /* upstream reads but does not validate */

    bool unicode = false;
    r = sf_binary_reader_read_bool(br, &unicode); if (r != SF_OK) goto fail;
    fmg->unicode = unicode;

    /*  Aux byte: DemonsSouls writes 0xFF, others 0x00 (FMG.cs:85). Stored
     *  implicitly by version; we just enforce the upstream invariant. */
    uint8_t expected_aux =
        (fmg->version == SF_FMG_VERSION_DEMONS_SOULS) ? 0xFFu : 0x00u;
    r = sf_binary_reader_assert_u8_one(br, expected_aux); if (r != SF_OK) goto fail;

    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) goto fail;

    int32_t group_count = 0;
    r = sf_binary_reader_read_i32(br, &group_count); if (r != SF_OK) goto fail;
    if (group_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

    int32_t string_count = 0;
    r = sf_binary_reader_read_i32(br, &string_count); if (r != SF_OK) goto fail;
    (void)string_count; /* upstream reads but does not validate */

    if (wide) {
        r = sf_binary_reader_assert_i32_one(br, (int32_t)0xFF);
        if (r != SF_OK) goto fail;
    }

    int64_t string_offsets_offset = 0;
    r = sf_binary_reader_read_varint(br, &string_offsets_offset);
    if (r != SF_OK) goto fail;
    if (fmg->has_md5) string_offsets_offset += 16;

    r = sf_binary_reader_assert_varint_one(br, 0); if (r != SF_OK) goto fail;

    /*  Group table (FMG.cs:101-138). */
    const int64_t per_offset = wide ? 8 : 4;

    for (int32_t gi = 0; gi < group_count; gi++) {
        int32_t offset_index = 0;
        int32_t first_id     = 0;
        int32_t last_id      = 0;

        r = sf_binary_reader_read_i32(br, &offset_index); if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &first_id);     if (r != SF_OK) goto fail;
        r = sf_binary_reader_read_i32(br, &last_id);      if (r != SF_OK) goto fail;

        if (wide) {
            r = sf_binary_reader_assert_i32_one(br, 0);
            if (r != SF_OK) goto fail;
        }

        if (last_id < first_id) { r = SF_ERR_OUT_OF_RANGE; goto fail; }
        if (offset_index < 0)   { r = SF_ERR_OUT_OF_RANGE; goto fail; }

        /*  Validate group span won't overflow when computing the table
         *  position; mirrors the StepIn target in FMG.cs:113. */
        int64_t span = (int64_t)last_id - (int64_t)first_id + 1;
        if (span <= 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

        int64_t table_pos =
            string_offsets_offset + (int64_t)offset_index * per_offset;

        r = sf_binary_reader_step_in(br, table_pos);
        if (r != SF_OK) goto fail;

        sf_result_t inner = SF_OK;
        for (int64_t j = 0; j < span; j++) {
            int64_t string_offset = 0;
            inner = sf_binary_reader_read_varint(br, &string_offset);
            if (inner != SF_OK) break;
            if (fmg->has_md5 && string_offset > 0) string_offset += 16;

            char *text_utf8 = NULL;
            if (string_offset > 0) {
                if (fmg->unicode) {
                    inner = sf_binary_reader_get_utf16(br, string_offset,
                                                       &text_utf8, NULL);
                } else {
                    inner = sf_binary_reader_get_shift_jis(br, string_offset,
                                                           &text_utf8, NULL);
                }
                if (inner != SF_OK) break;
            }

            int32_t id = first_id + (int32_t)j;
            inner = entries_append(fmg, id, text_utf8);
            if (inner != SF_OK) {
                sf_xfree(alloc, text_utf8);
                break;
            }
        }

        sf_result_t out_r = sf_binary_reader_step_out(br);
        if (inner != SF_OK) { r = inner; goto fail; }
        if (out_r != SF_OK) { r = out_r; goto fail; }
    }

    *out = fmg;
    return SF_OK;

fail:
    sf_fmg_destroy(fmg, alloc);
    return r;
}

/*===========================================================================
 * Public read entry points
 *===========================================================================*/

sf_result_t sf_fmg_read_from_stream(sf_fmg_t **out, sf_istream_t *stream,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;
    r = fmg_read(br, out, alloc);
    sf_binary_reader_destroy(br);
    return r;
}

sf_result_t sf_fmg_read_from_memory(sf_fmg_t **out, const uint8_t *data,
                                    size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_fmg_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_fmg_read_from_path(sf_fmg_t **out, const char *utf8_path,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && utf8_path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_file(&stream, utf8_path, alloc);
    if (r != SF_OK) return r;
    r = sf_fmg_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Public construction / destruction
 *===========================================================================*/

sf_result_t sf_fmg_create(const sf_allocator_t *alloc, sf_fmg_version_t version,
                          sf_fmg_t **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (!version_is_known((uint8_t)version)) return SF_ERR_INVALID_ARG;
    alloc = sf_alloc_or_default(alloc);

    sf_fmg_t *fmg = NULL;
    sf_result_t r = fmg_alloc_blank(alloc, &fmg);
    if (r != SF_OK) return r;
    fmg->version = version;
    /*  Upstream: BigEndian = (version == DemonsSouls) when constructing
     *  via FMG(version) ctor (FMG.cs:57-63). */
    fmg->big_endian = (version == SF_FMG_VERSION_DEMONS_SOULS);
    fmg->unicode = true;
    *out = fmg;
    return SF_OK;
}

void sf_fmg_destroy(sf_fmg_t *fmg, const sf_allocator_t *alloc) {
    if (!fmg) return;
    /*  Honour the allocator the object was created with; the parameter is
     *  accepted for symmetry with other modules but ignored. */
    (void)alloc;
    const sf_allocator_t *a = fmg->alloc;
    for (size_t i = 0; i < fmg->entry_count; i++) {
        entry_clear(&fmg->entries[i], a);
    }
    sf_xfree(a, fmg->entries);
    sf_xfree(a, fmg);
}

/*===========================================================================
 * Write path stubs — implemented in T3.6
 *===========================================================================*/

sf_result_t sf_fmg_write_to_memory(const sf_fmg_t *fmg, uint8_t **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    (void)fmg; (void)alloc;
    SF_CHECK_ARG(out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    return SF_ERR_UNSUPPORTED_VERSION;
}

sf_result_t sf_fmg_write_to_stream(const sf_fmg_t *fmg, sf_ostream_t *stream,
                                   const sf_allocator_t *alloc) {
    (void)fmg; (void)alloc;
    SF_CHECK_ARG(stream != NULL);
    return SF_ERR_UNSUPPORTED_VERSION;
}

sf_result_t sf_fmg_write_to_path(const sf_fmg_t *fmg, const char *utf8_path,
                                 const sf_allocator_t *alloc) {
    (void)fmg; (void)alloc;
    SF_CHECK_ARG(utf8_path != NULL);
    return SF_ERR_UNSUPPORTED_VERSION;
}

/*===========================================================================
 * Public getters
 *===========================================================================*/

sf_fmg_version_t sf_fmg_get_version(const sf_fmg_t *fmg) {
    return fmg ? fmg->version : SF_FMG_VERSION_DARK_SOULS_1;
}

bool sf_fmg_is_big_endian(const sf_fmg_t *fmg) {
    return fmg ? fmg->big_endian : false;
}

bool sf_fmg_is_unicode(const sf_fmg_t *fmg) {
    return fmg ? fmg->unicode : false;
}

bool sf_fmg_has_md5(const sf_fmg_t *fmg) {
    return fmg ? fmg->has_md5 : false;
}

bool sf_fmg_get_reuse_offsets(const sf_fmg_t *fmg) {
    return fmg ? fmg->reuse_offsets : false;
}

size_t sf_fmg_get_entry_count(const sf_fmg_t *fmg) {
    return fmg ? fmg->entry_count : 0;
}

const sf_fmg_entry_t *sf_fmg_get_entry(const sf_fmg_t *fmg, size_t index) {
    if (!fmg || index >= fmg->entry_count) return NULL;
    return &fmg->entries[index];
}

const sf_fmg_entry_t *sf_fmg_find_entry_by_id(const sf_fmg_t *fmg, int32_t id) {
    if (!fmg) return NULL;
    /*  Mirrors FMG.cs:283: `Entries.Find(entry => entry.ID == id)` — first
     *  match by linear scan. */
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) return &fmg->entries[i];
    }
    return NULL;
}

/*===========================================================================
 * Public setters
 *===========================================================================*/

void sf_fmg_set_big_endian(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->big_endian = value;
}

void sf_fmg_set_unicode(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->unicode = value;
}

void sf_fmg_set_md5(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->has_md5 = value;
}

void sf_fmg_set_reuse_offsets(sf_fmg_t *fmg, bool value) {
    if (fmg) fmg->reuse_offsets = value;
}

/*===========================================================================
 * Entry mutation
 *===========================================================================*/

static char *dup_text_or_null(const sf_allocator_t *a, const char *text_utf8) {
    /*  NULL stays NULL (deleted tombstone). Empty string "" round-trips. */
    if (!text_utf8) return NULL;
    size_t n = strlen(text_utf8) + 1;
    char *p = (char *)sf_xalloc(a, n);
    if (!p) return NULL;
    memcpy(p, text_utf8, n);
    return p;
}

sf_result_t sf_fmg_add_entry(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                             const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL);
    /*  alloc is accepted for API symmetry; the FMG owns its allocator. */
    (void)alloc;
    char *dup = NULL;
    if (text_utf8) {
        dup = dup_text_or_null(fmg->alloc, text_utf8);
        if (!dup) return SF_ERR_OOM;
    }
    sf_result_t r = entries_append(fmg, id, dup);
    if (r != SF_OK) sf_xfree(fmg->alloc, dup);
    return r;
}

sf_result_t sf_fmg_remove_entry(sf_fmg_t *fmg, int32_t id) {
    SF_CHECK_ARG(fmg != NULL);
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) {
            entry_clear(&fmg->entries[i], fmg->alloc);
            /*  Shift down; preserve insertion order. */
            for (size_t j = i + 1; j < fmg->entry_count; j++) {
                fmg->entries[j - 1] = fmg->entries[j];
            }
            fmg->entry_count--;
            memset(&fmg->entries[fmg->entry_count], 0, sizeof(sf_fmg_entry_t));
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

sf_result_t sf_fmg_set_entry_text(sf_fmg_t *fmg, int32_t id, const char *text_utf8,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fmg != NULL);
    (void)alloc;
    /*  Mirrors FMG.cs:285-294 indexer setter: if entry exists, update text;
     *  otherwise add a new entry. */
    for (size_t i = 0; i < fmg->entry_count; i++) {
        if (fmg->entries[i].id == id) {
            char *dup = NULL;
            if (text_utf8) {
                dup = dup_text_or_null(fmg->alloc, text_utf8);
                if (!dup) return SF_ERR_OOM;
            }
            sf_xfree(fmg->alloc, fmg->entries[i].text_utf8);
            fmg->entries[i].text_utf8 = dup;
            return SF_OK;
        }
    }
    return sf_fmg_add_entry(fmg, id, text_utf8, alloc);
}

/*===========================================================================
 * Entry accessors
 *===========================================================================*/

int32_t sf_fmg_entry_get_id(const sf_fmg_entry_t *entry) {
    return entry ? entry->id : 0;
}

const char *sf_fmg_entry_get_text(const sf_fmg_entry_t *entry) {
    return entry ? entry->text_utf8 : NULL;
}
