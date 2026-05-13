/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Simplified read-only port of SoulsFormats/Formats/DRB/DRB.cs.
 * Parses the file envelope, STR string table, and TEXI texture list.
 * Every other block is header-validated then skipped so callers can
 * detect and inventory DRB files without modelling the full UI graph.
 */

#include "souls_formats/sf_drb.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) goto done; } while (0)

static sf_result_t drb_jump_to(sf_binary_reader_t *r, int64_t target) {
    int64_t cur = sf_binary_reader_position(r);
    if (cur > target) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_skip(r, target - cur);
}

struct sf_drb {
    const sf_allocator_t *alloc;
    sf_drb_version_t version;
    bool big_endian;
    char **texture_names;
    size_t texture_count;
    char **dlg_names;
    size_t dlg_count;
};

/* DRB blocks encode their magic as a little-endian int32 via FourCCToInt,
 * meaning bytes in the file are reversed on big-endian saves. */
static int32_t drb_fourcc(const char *name) {
    return (int32_t)((uint8_t)name[0] | ((uint8_t)name[1] << 8) |
                     ((uint8_t)name[2] << 16) | ((uint8_t)name[3] << 24));
}

static sf_result_t drb_assert_magic(sf_binary_reader_t *r, const char *name) {
    int32_t got = 0;
    sf_result_t e = sf_binary_reader_read_i32(r, &got);
    if (e != SF_OK) return e;
    return got == drb_fourcc(name) ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t drb_read_null_block(sf_binary_reader_t *r, const char *name) {
    sf_result_t e = drb_assert_magic(r, name);
    if (e != SF_OK) return e;
    for (int i = 0; i < 3; i++) {
        int32_t z = 0;
        e = sf_binary_reader_read_i32(r, &z);
        if (e != SF_OK) return e;
        if (z != 0) return SF_ERR_BAD_MAGIC;
    }
    return SF_OK;
}

static sf_result_t drb_read_block_header(sf_binary_reader_t *r, const char *name,
                                         int32_t *out_size, int32_t *out_count) {
    sf_result_t e = drb_assert_magic(r, name);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_i32(r, out_size);  if (e != SF_OK) return e;
    e = sf_binary_reader_read_i32(r, out_count); if (e != SF_OK) return e;
    int32_t z = 0;
    e = sf_binary_reader_read_i32(r, &z);        if (e != SF_OK) return e;
    if (z != 0) return SF_ERR_BAD_MAGIC;
    return SF_OK;
}

static sf_result_t drb_read_blob_block(sf_binary_reader_t *r, const char *name,
                                       int32_t *out_size) {
    sf_result_t e = drb_assert_magic(r, name);
    if (e != SF_OK) return e;
    e = sf_binary_reader_read_i32(r, out_size); if (e != SF_OK) return e;
    int32_t one = 0;
    e = sf_binary_reader_read_i32(r, &one);     if (e != SF_OK) return e;
    if (one != 1) return SF_ERR_BAD_MAGIC;
    int32_t z = 0;
    e = sf_binary_reader_read_i32(r, &z);       if (e != SF_OK) return e;
    if (z != 0) return SF_ERR_BAD_MAGIC;
    return SF_OK;
}

static sf_result_t drb_skip_block(sf_binary_reader_t *r, const char *name) {
    int32_t size = 0, count = 0;
    sf_result_t e = drb_read_block_header(r, name, &size, &count);
    if (e != SF_OK) return e;
    if (size < 0) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_skip(r, (int64_t)size);
}

static sf_result_t drb_skip_blob(sf_binary_reader_t *r, const char *name) {
    int32_t size = 0;
    sf_result_t e = drb_read_blob_block(r, name, &size);
    if (e != SF_OK) return e;
    if (size < 0) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_skip(r, (int64_t)size);
}

static sf_result_t drb_grow_str_array(const sf_allocator_t *a, char ***arr,
                                       size_t count, size_t new_count) {
    char **np = (char **)sf_xalloc(a, new_count * sizeof(char *));
    if (!np) return SF_ERR_OOM;
    memset(np, 0, new_count * sizeof(char *));
    if (*arr) {
        memcpy(np, *arr, count * sizeof(char *));
        sf_xfree(a, *arr);
    }
    *arr = np;
    return SF_OK;
}

static void drb_free_str_array(const sf_allocator_t *a, char **arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) sf_xfree(a, arr[i]);
    sf_xfree(a, arr);
}

sf_result_t sf_drb_create(sf_drb_t **out, sf_drb_version_t version, bool big_endian,
                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_drb_t *d = (sf_drb_t *)sf_xalloc(alloc, sizeof(*d));
    if (!d) return SF_ERR_OOM;
    memset(d, 0, sizeof(*d));
    d->alloc = alloc;
    d->version = version;
    d->big_endian = big_endian;
    *out = d;
    return SF_OK;
}

void sf_drb_destroy(sf_drb_t *d) {
    if (!d) return;
    drb_free_str_array(d->alloc, d->texture_names, d->texture_count);
    drb_free_str_array(d->alloc, d->dlg_names, d->dlg_count);
    sf_xfree(d->alloc, d);
}

bool sf_drb_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "DRB\0", 4) == 0 || memcmp(bytes, "\0BRD", 4) == 0;
}

sf_drb_version_t sf_drb_version(const sf_drb_t *d) {
    return d ? d->version : SF_DRB_VERSION_ARMORED_CORE_FOR_ANSWER;
}

bool sf_drb_big_endian(const sf_drb_t *d) { return d ? d->big_endian : false; }

size_t sf_drb_texture_count(const sf_drb_t *d) { return d ? d->texture_count : 0u; }

sf_result_t sf_drb_get_texture_name(const sf_drb_t *d, size_t index, const char **out_name) {
    SF_CHECK_ARG(d != NULL && out_name != NULL);
    if (index >= d->texture_count) return SF_ERR_OUT_OF_RANGE;
    *out_name = d->texture_names[index];
    return SF_OK;
}

size_t sf_drb_dlg_count(const sf_drb_t *d) { return d ? d->dlg_count : 0u; }

sf_result_t sf_drb_get_dlg_name(const sf_drb_t *d, size_t index, const char **out_name) {
    SF_CHECK_ARG(d != NULL && out_name != NULL);
    if (index >= d->dlg_count) return SF_ERR_OUT_OF_RANGE;
    *out_name = d->dlg_names[index];
    return SF_OK;
}

/* Read TEXI entries: each is { i32 nameOffset, i32 pathOffset, i32 0, i32 0 } per
 * upstream Texture.cs. Texture names are fetched from the STR data starting at
 * `str_data_start`. */
static sf_result_t drb_read_texi(sf_binary_reader_t *r, sf_drb_t *d, int64_t str_data_start) {
    int32_t size = 0, count = 0;
    sf_result_t e = drb_read_block_header(r, "TEXI", &size, &count);
    if (e != SF_OK) return e;
    if (count < 0 || size < 0) return SF_ERR_BAD_MAGIC;

    int64_t block_data_start = sf_binary_reader_position(r);

    if (count > 0) {
        e = drb_grow_str_array(d->alloc, &d->texture_names, 0, (size_t)count);
        if (e != SF_OK) return e;
        d->texture_count = (size_t)count;
    }

    for (int32_t i = 0; i < count; i++) {
        int32_t name_off = 0, path_off = 0, z1 = 0, z2 = 0;
        e = sf_binary_reader_read_i32(r, &name_off); if (e != SF_OK) return e;
        e = sf_binary_reader_read_i32(r, &path_off); if (e != SF_OK) return e;
        e = sf_binary_reader_read_i32(r, &z1);       if (e != SF_OK) return e;
        e = sf_binary_reader_read_i32(r, &z2);       if (e != SF_OK) return e;
        (void)path_off; (void)z1; (void)z2;
        if (name_off < 0) return SF_ERR_BAD_MAGIC;

        char *name = NULL;
        size_t name_len = 0;
        e = sf_binary_reader_get_utf16(r, str_data_start + name_off, &name, &name_len);
        if (e != SF_OK) return e;
        d->texture_names[i] = name;
    }

    return drb_jump_to(r, block_data_start + (int64_t)size);
}

sf_result_t sf_drb_read_from_memory(sf_drb_t **out, const void *bytes, size_t size,
                                    sf_drb_version_t version, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    if (size < 4) return SF_ERR_BAD_MAGIC;

    bool big_endian;
    if (memcmp(bytes, "DRB\0", 4) == 0) {
        big_endian = false;
    } else if (memcmp(bytes, "\0BRD", 4) == 0) {
        big_endian = true;
    } else {
        return SF_ERR_BAD_MAGIC;
    }

    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_drb_t *d = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, big_endian, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_drb_create(&d, version, big_endian, alloc);
    if (e != SF_OK) goto done;

    TRY(drb_read_null_block(r, "DRB\0"));

    int32_t str_size = 0, str_count = 0;
    TRY(drb_read_block_header(r, "STR\0", &str_size, &str_count));
    int64_t str_data_start = sf_binary_reader_position(r);
    if (str_size < 0) { e = SF_ERR_BAD_MAGIC; goto done; }
    TRY(sf_binary_reader_skip(r, (int64_t)str_size));

    TRY(drb_read_texi(r, d, str_data_start));

    TRY(drb_skip_blob(r, "SHPR"));
    TRY(drb_skip_blob(r, "CTPR"));
    TRY(drb_skip_blob(r, "ANIP"));
    TRY(drb_skip_blob(r, "INTP"));
    TRY(drb_skip_blob(r, "SCDP"));

    TRY(drb_skip_block(r, "SHAP"));
    TRY(drb_skip_block(r, "CTRL"));
    TRY(drb_skip_block(r, "ANIK"));
    TRY(drb_skip_block(r, "ANIO"));
    TRY(drb_skip_block(r, "ANIM"));
    TRY(drb_skip_block(r, "SCDK"));
    TRY(drb_skip_block(r, "SCDO"));
    TRY(drb_skip_block(r, "SCDL"));
    TRY(drb_skip_block(r, "DLGO"));

    /* DLG entries vary in size: each is a Dlgo (32 bytes) plus 32 bytes of Dlg
     * extension fields. We harvest the leading nameOffset (i32) of each entry
     * then jump to the next, using size/count to derive the stride so future
     * upstream growth doesn't break the loop. */
    int32_t dlg_size = 0, dlg_count = 0;
    TRY(drb_read_block_header(r, "DLG\0", &dlg_size, &dlg_count));
    int64_t dlg_data_start = sf_binary_reader_position(r);
    if (dlg_size < 0 || dlg_count < 0) { e = SF_ERR_BAD_MAGIC; goto done; }

    if (dlg_count > 0) {
        if (dlg_size == 0) { e = SF_ERR_BAD_MAGIC; goto done; }
        int64_t stride = (int64_t)dlg_size / (int64_t)dlg_count;
        if (stride < 4) { e = SF_ERR_BAD_MAGIC; goto done; }

        e = drb_grow_str_array(d->alloc, &d->dlg_names, 0, (size_t)dlg_count);
        if (e != SF_OK) goto done;
        d->dlg_count = (size_t)dlg_count;

        for (int32_t i = 0; i < dlg_count; i++) {
            int32_t name_off = 0;
            TRY(sf_binary_reader_read_i32(r, &name_off));
            if (name_off < 0) { e = SF_ERR_BAD_MAGIC; goto done; }

            char *name = NULL;
            size_t name_len = 0;
            e = sf_binary_reader_get_utf16(r, str_data_start + name_off, &name, &name_len);
            if (e != SF_OK) goto done;
            d->dlg_names[i] = name;

            if (i + 1 < dlg_count) {
                TRY(sf_binary_reader_skip(r, stride - 4));
            }
        }
    }

    TRY(drb_jump_to(r, dlg_data_start + (int64_t)dlg_size));

    TRY(drb_read_null_block(r, "END\0"));

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_drb_destroy(d); return e; }
    *out = d;
    return SF_OK;
}
