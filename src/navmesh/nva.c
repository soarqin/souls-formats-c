/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NVA navmesh area (BB/DS3/Sekiro).
 *
 * Mirrors:
 *   SoulsFormats/Formats/NVA.cs
 *
 * Section structure: file header (NVMA + version + size + section_count),
 * then `section_count` sections each prefixed by [index(i32), version(i32),
 * length(i32), count(i32)] followed by `length - 16` bytes of entry payload
 * (padded to 16-byte boundary with 0xFF).
 */

#include "souls_formats/sf_nva.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sf_nva_section {
    int32_t  index;
    int32_t  version;
    uint8_t *entries;
    size_t   entry_size;
    size_t   entry_count;
    size_t   entry_capacity;
};

struct sf_nva {
    const sf_allocator_t *alloc;
    sf_nva_version_t      version;
    sf_nva_section_t     *sections;
    size_t                section_count;
    size_t                section_capacity;
};

static void section_release(sf_nva_section_t *s, const sf_allocator_t *a) {
    if (!s) return;
    sf_xfree(a, s->entries);
    memset(s, 0, sizeof(*s));
}

void sf_nva_destroy(sf_nva_t *nva) {
    if (!nva) return;
    const sf_allocator_t *a = nva->alloc;
    for (size_t i = 0; i < nva->section_count; i++) section_release(&nva->sections[i], a);
    sf_xfree(a, nva->sections);
    sf_xfree(a, nva);
}

static sf_result_t reserve_sections(sf_nva_t *nva, size_t needed) {
    if (needed <= nva->section_capacity) return SF_OK;
    size_t cap = nva->section_capacity ? nva->section_capacity * 2 : 8;
    if (cap < needed) cap = needed;
    sf_nva_section_t *arr = (sf_nva_section_t *)sf_xrealloc(
        nva->alloc, nva->sections,
        nva->section_capacity * sizeof(sf_nva_section_t),
        cap * sizeof(sf_nva_section_t));
    if (!arr) return SF_ERR_OOM;
    nva->sections = arr;
    nva->section_capacity = cap;
    return SF_OK;
}

sf_result_t sf_nva_create_empty(sf_nva_t **out, sf_nva_version_t version,
                                const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_nva_t *nva = (sf_nva_t *)sf_xalloc(alloc, sizeof(*nva));
    if (!nva) return SF_ERR_OOM;
    memset(nva, 0, sizeof(*nva));
    nva->alloc = alloc;
    nva->version = version;

    size_t section_count = (version == SF_NVA_VERSION_OLD_BLOODBORNE) ? 8 : 9;
    sf_result_t r = reserve_sections(nva, section_count);
    if (r != SF_OK) { sf_xfree(alloc, nva); return r; }
    for (size_t i = 0; i < section_count; i++) {
        sf_nva_section_t *s = &nva->sections[i];
        memset(s, 0, sizeof(*s));
        s->index = (int32_t)i;
        if (i == 0)
            s->version = (version == SF_NVA_VERSION_OLD_BLOODBORNE) ? 2 :
                         (version == SF_NVA_VERSION_DARK_SOULS_3) ? 2 : 4;
        else if (i == 8)
            s->version = (version == SF_NVA_VERSION_SEKIRO) ? 2 : 1;
        else
            s->version = 1;
    }
    nva->section_count = section_count;
    *out = nva;
    return SF_OK;
}

sf_nva_version_t sf_nva_version(const sf_nva_t *n) {
    return n ? n->version : SF_NVA_VERSION_DARK_SOULS_3;
}
size_t sf_nva_section_count(const sf_nva_t *n) { return n ? n->section_count : 0; }
const sf_nva_section_t *sf_nva_section(const sf_nva_t *n, size_t i) {
    if (!n || i >= n->section_count) return NULL;
    return &n->sections[i];
}
sf_nva_section_t *sf_nva_section_mut(sf_nva_t *n, size_t i) {
    if (!n || i >= n->section_count) return NULL;
    return &n->sections[i];
}

int32_t       sf_nva_section_index(const sf_nva_section_t *s) { return s ? s->index : 0; }
int32_t       sf_nva_section_version(const sf_nva_section_t *s) { return s ? s->version : 0; }
size_t        sf_nva_section_entry_count(const sf_nva_section_t *s) { return s ? s->entry_count : 0; }
size_t        sf_nva_section_entry_size (const sf_nva_section_t *s) { return s ? s->entry_size : 0; }
const uint8_t *sf_nva_section_entries   (const sf_nva_section_t *s) { return s ? s->entries : NULL; }

void sf_nva_section_set_version(sf_nva_section_t *s, int32_t v) { if (s) s->version = v; }

sf_result_t sf_nva_section_append_entry(sf_nva_section_t *s, const void *data,
                                        size_t entry_size_bytes) {
    SF_CHECK_ARG(s != NULL);
    SF_CHECK_ARG(data != NULL || entry_size_bytes == 0);
    if (entry_size_bytes == 0) return SF_OK;
    if (s->entry_size == 0) {
        s->entry_size = entry_size_bytes;
    } else if (s->entry_size != entry_size_bytes) {
        return SF_ERR_INVALID_ARG;
    }
    if (s->entry_count + 1 > s->entry_capacity) {
        size_t cap = s->entry_capacity ? s->entry_capacity * 2 : 4;
        if (cap < s->entry_count + 1) cap = s->entry_count + 1;
        size_t old_bytes = s->entry_capacity * s->entry_size;
        size_t new_bytes = cap * s->entry_size;
        uint8_t *arr = (uint8_t *)sf_xrealloc(NULL, s->entries, old_bytes, new_bytes);
        if (!arr) return SF_ERR_OOM;
        s->entries = arr;
        s->entry_capacity = cap;
    }
    memcpy(s->entries + s->entry_count * s->entry_size, data, entry_size_bytes);
    s->entry_count++;
    return SF_OK;
}

/*===========================================================================
 * Default entry sizes per section type
 *===========================================================================*/

static size_t default_entry_size(int32_t section_index, sf_nva_version_t version) {
    if (section_index == 0) {
        if (version == SF_NVA_VERSION_OLD_BLOODBORNE) return 64;
        if (version == SF_NVA_VERSION_DARK_SOULS_3)   return 64;
        return 144;
    }
    if (section_index == 3) return 0;
    if (section_index == 4) return 32;
    if (section_index == 8) {
        if (version == SF_NVA_VERSION_SEKIRO) return 36;
        return 16;
    }
    return 16;
}

/*===========================================================================
 * Read
 *===========================================================================*/

sf_result_t sf_nva_read_from_memory(sf_nva_t **out, const void *data, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) { sf_istream_close(stream); return r; }

    sf_nva_t *nva = NULL;

    r = sf_binary_reader_assert_ascii(br, "NVMA");
    if (r != SF_OK) goto cleanup;

    uint32_t version_u = 0;
    r = sf_binary_reader_read_u32(br, &version_u);
    if (r != SF_OK) goto cleanup;
    if (version_u != SF_NVA_VERSION_OLD_BLOODBORNE &&
        version_u != SF_NVA_VERSION_DARK_SOULS_3 &&
        version_u != SF_NVA_VERSION_SEKIRO) {
        r = SF_ERR_UNSUPPORTED_VERSION; goto cleanup;
    }
    uint32_t file_size = 0;
    r = sf_binary_reader_read_u32(br, &file_size);
    if (r != SF_OK) goto cleanup;
    int32_t section_count = 0;
    r = sf_binary_reader_read_i32(br, &section_count);
    if (r != SF_OK) goto cleanup;
    int32_t expected = (version_u == SF_NVA_VERSION_OLD_BLOODBORNE) ? 8 : 9;
    if (section_count != expected) { r = SF_ERR_BAD_MAGIC; goto cleanup; }

    nva = (sf_nva_t *)sf_xalloc(alloc, sizeof(*nva));
    if (!nva) { r = SF_ERR_OOM; goto cleanup; }
    memset(nva, 0, sizeof(*nva));
    nva->alloc = alloc;
    nva->version = (sf_nva_version_t)version_u;

    r = reserve_sections(nva, (size_t)section_count);
    if (r != SF_OK) goto cleanup;

    for (int32_t i = 0; i < section_count; i++) {
        sf_nva_section_t *s = &nva->sections[nva->section_count++];
        memset(s, 0, sizeof(*s));

        r = sf_binary_reader_read_i32(br, &s->index);
        if (r == SF_OK) r = sf_binary_reader_read_i32(br, &s->version);
        int32_t length = 0;
        int32_t count = 0;
        if (r == SF_OK) r = sf_binary_reader_read_i32(br, &length);
        if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count);
        if (r != SF_OK) goto cleanup;
        if (s->index != i) { r = SF_ERR_BAD_MAGIC; goto cleanup; }
        if (length < 16 || count < 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }

        int64_t section_start = sf_binary_reader_position(br);
        int64_t payload_bytes = length - 16;

        if (count > 0 && payload_bytes > 0) {
            size_t entry_size_total = (size_t)payload_bytes;
            size_t entry_size = entry_size_total / (size_t)count;
            if (entry_size * (size_t)count > entry_size_total) {
                r = SF_ERR_OUT_OF_RANGE; goto cleanup;
            }
            s->entry_size = entry_size;
            s->entry_count = (size_t)count;
            s->entry_capacity = (size_t)count;
            s->entries = (uint8_t *)sf_xalloc(alloc, entry_size * (size_t)count);
            if (!s->entries) { r = SF_ERR_OOM; goto cleanup; }
            r = sf_binary_reader_read_bytes(br, s->entries, entry_size * (size_t)count);
            if (r != SF_OK) goto cleanup;
        }

        int64_t section_end = section_start + payload_bytes;
        r = sf_binary_reader_skip(br, section_end - sf_binary_reader_position(br));
        if (r != SF_OK) goto cleanup;
    }

    *out = nva;
    nva = NULL;

cleanup:
    if (nva) sf_nva_destroy(nva);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

sf_result_t sf_nva_write_to_memory(const sf_nva_t *nva, void **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(nva != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) { sf_ostream_close(stream); return r; }

    r = sf_binary_writer_write_ascii(bw, "NVMA", false);
    if (r == SF_OK) r = sf_binary_writer_write_u32(bw, (uint32_t)nva->version);
    if (r == SF_OK) r = sf_binary_writer_reserve_u32(bw, "FileSize");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw,
        (nva->version == SF_NVA_VERSION_OLD_BLOODBORNE) ? 8 : 9);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < nva->section_count; i++) {
        const sf_nva_section_t *s = &nva->sections[i];
        if (s->entry_count > (size_t)INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        size_t entry_size = s->entry_size;
        if (entry_size == 0)
            entry_size = default_entry_size(s->index, nva->version);
        size_t payload_bytes = entry_size * s->entry_count;

        r = sf_binary_writer_write_i32(bw, s->index);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, s->version);
        char rname[24];
        size_t n = (size_t)snprintf(rname, sizeof(rname), "NvaSecLen_%zu", i);
        if (n >= sizeof(rname)) { r = SF_ERR_INTERNAL; goto cleanup; }
        if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, rname);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)s->entry_count);
        if (r != SF_OK) goto cleanup;

        int64_t section_start = sf_binary_writer_position(bw);
        if (payload_bytes > 0 && s->entries != NULL) {
            r = sf_binary_writer_write_bytes(bw, s->entries, payload_bytes);
            if (r != SF_OK) goto cleanup;
        }
        int64_t pos = sf_binary_writer_position(bw);
        int64_t pad = 0;
        if ((pos % 0x10) != 0) pad = 0x10 - (pos % 0x10);
        for (int64_t k = 0; k < pad; k++) {
            r = sf_binary_writer_write_u8(bw, 0xFF);
            if (r != SF_OK) goto cleanup;
        }

        int64_t length = (sf_binary_writer_position(bw) - section_start) + 16;
        if (length > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        r = sf_binary_writer_fill_i32(bw, rname, (int32_t)length);
        if (r != SF_OK) goto cleanup;
    }

    int64_t fsize = sf_binary_writer_position(bw);
    if (fsize > UINT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_u32(bw, "FileSize", (uint32_t)fsize);
    if (r != SF_OK) goto cleanup;

    uint8_t *bytes = NULL;
    size_t   bytes_len = 0;
    r = sf_binary_writer_finish_bytes(bw, &bytes, &bytes_len);
    bw = NULL;
    if (r != SF_OK) goto cleanup;
    *out_data = bytes;
    *out_size = bytes_len;

cleanup:
    if (bw) sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
