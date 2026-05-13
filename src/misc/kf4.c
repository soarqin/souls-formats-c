/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_kf4.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static const uint8_t SF_KF4_DAT_MAGIC[4] = {0x00, 0x80, 0x04, 0x1E};

#define SF_KF4_DAT_NAME_LEN 0x34u
#define SF_KF4_DAT_HEADER_SIZE 0x40u
#define SF_KF4_DAT_ENTRY_SIZE 0x40u

struct sf_kf4_dat_file {
    char *name;
    const uint8_t *bytes;
    size_t size;
};

struct sf_kf4_dat {
    const sf_allocator_t *alloc;
    struct sf_kf4_dat_file *files;
    size_t file_count;
};

struct sf_kf4_om2 {
    const sf_allocator_t *alloc;
    int32_t file_size;
};

bool sf_kf4_dat_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_KF4_DAT_HEADER_SIZE) return false;
    return memcmp(bytes, SF_KF4_DAT_MAGIC, 4) == 0;
}

static void kf4_dat_free_contents(sf_kf4_dat_t *dat) {
    if (!dat || !dat->files) return;
    for (size_t i = 0; i < dat->file_count; i++) {
        sf_xfree(dat->alloc, dat->files[i].name);
    }
    sf_xfree(dat->alloc, dat->files);
    dat->files = NULL;
    dat->file_count = 0;
}

void sf_kf4_dat_destroy(sf_kf4_dat_t *dat) {
    if (!dat) return;
    kf4_dat_free_contents(dat);
    sf_xfree(dat->alloc, dat);
}

size_t sf_kf4_dat_file_count(const sf_kf4_dat_t *dat) {
    return dat ? dat->file_count : 0u;
}

sf_result_t sf_kf4_dat_get_file_name(const sf_kf4_dat_t *dat, size_t index,
                                     const char **out_name) {
    SF_CHECK_ARG(dat != NULL && out_name != NULL);
    if (index >= dat->file_count) return SF_ERR_OUT_OF_RANGE;
    *out_name = dat->files[index].name;
    return SF_OK;
}

sf_result_t sf_kf4_dat_get_file_data(const sf_kf4_dat_t *dat, size_t index,
                                     const uint8_t **out_bytes, size_t *out_size) {
    SF_CHECK_ARG(dat != NULL && out_bytes != NULL && out_size != NULL);
    if (index >= dat->file_count) return SF_ERR_OUT_OF_RANGE;
    *out_bytes = dat->files[index].bytes;
    *out_size = dat->files[index].size;
    return SF_OK;
}

sf_result_t sf_kf4_dat_read_from_memory(sf_kf4_dat_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_KF4_DAT_HEADER_SIZE) return SF_ERR_TRUNCATED;

    const uint8_t *raw = (const uint8_t *)bytes;
    if (memcmp(raw, SF_KF4_DAT_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_kf4_dat_t *dat = NULL;
    sf_result_t e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_u8_one(r, 0x00); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 0x80); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 0x04); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 0x1E); if (e != SF_OK) goto done;

    int32_t file_count = 0;
    e = sf_binary_reader_read_i32(r, &file_count); if (e != SF_OK) goto done;
    if (file_count < 0) { e = SF_ERR_TRUNCATED; goto done; }

    e = sf_binary_reader_assert_pattern(r, 0x38u, 0); if (e != SF_OK) goto done;

    if ((uint64_t)file_count * (uint64_t)SF_KF4_DAT_ENTRY_SIZE >
        (uint64_t)(size - SF_KF4_DAT_HEADER_SIZE)) {
        e = SF_ERR_TRUNCATED; goto done;
    }

    dat = (sf_kf4_dat_t *)sf_xalloc(alloc, sizeof(*dat));
    if (!dat) { e = SF_ERR_OOM; goto done; }
    memset(dat, 0, sizeof(*dat));
    dat->alloc = alloc;
    dat->file_count = (size_t)file_count;

    if (file_count > 0) {
        dat->files = (struct sf_kf4_dat_file *)sf_xalloc(
            alloc, (size_t)file_count * sizeof(*dat->files));
        if (!dat->files) { e = SF_ERR_OOM; goto done; }
        memset(dat->files, 0, (size_t)file_count * sizeof(*dat->files));
    }

    for (int32_t i = 0; i < file_count; i++) {
        char *name = NULL;
        e = sf_binary_reader_read_fix_str(r, SF_KF4_DAT_NAME_LEN, &name, NULL);
        if (e != SF_OK) goto done;
        dat->files[i].name = name;

        int32_t fsize = 0, padded = 0, offset = 0;
        e = sf_binary_reader_read_i32(r, &fsize);  if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &padded); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &offset); if (e != SF_OK) goto done;
        (void)padded;

        if (fsize < 0 || offset < 0) { e = SF_ERR_TRUNCATED; goto done; }
        if ((uint64_t)offset + (uint64_t)fsize > (uint64_t)size) {
            e = SF_ERR_TRUNCATED; goto done;
        }
        dat->files[i].bytes = raw + (size_t)offset;
        dat->files[i].size = (size_t)fsize;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_kf4_dat_destroy(dat); return e; }
    *out = dat;
    return SF_OK;
}

void sf_kf4_om2_destroy(sf_kf4_om2_t *om2) {
    if (!om2) return;
    sf_xfree(om2->alloc, om2);
}

int32_t sf_kf4_om2_file_size(const sf_kf4_om2_t *om2) {
    return om2 ? om2->file_size : 0;
}

sf_result_t sf_kf4_om2_read_from_memory(sf_kf4_om2_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < 4) return SF_ERR_TRUNCATED;

    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_kf4_om2_t *om2 = NULL;
    sf_result_t e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    int32_t file_size = 0;
    e = sf_binary_reader_read_i32(r, &file_size);
    if (e != SF_OK) goto done;

    om2 = (sf_kf4_om2_t *)sf_xalloc(alloc, sizeof(*om2));
    if (!om2) { e = SF_ERR_OOM; goto done; }
    memset(om2, 0, sizeof(*om2));
    om2->alloc = alloc;
    om2->file_size = file_size;

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_kf4_om2_destroy(om2); return e; }
    *out = om2;
    return SF_OK;
}
