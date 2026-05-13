/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fxr1.h"
#include "internal/sf_internal.h"

#include <string.h>

#define FXR1_MAGIC_SIZE 4u
#define FXR1_CHECK_LITTLE 0x00010000u
#define FXR1_CHECK_BIG    0x00000100u
#define FXR1_WIDE_GARBAGE 0xCDCDCDCDu

struct sf_fxr1 {
    const sf_allocator_t *alloc;
    bool big_endian;
    bool wide;
    int32_t unk1;
    int32_t unk2;
    uint8_t *node_blob;
    size_t node_blob_size;
    uint8_t *metadata_blob;
    size_t metadata_blob_size;
    int32_t pointer_table_count;
    int32_t function_table_count;
};

static size_t fxr1_header_size(bool wide) {
    return wide ? 48u : 32u;
}

static uint32_t fxr1_read_u32_at(const uint8_t *p, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t fxr1_read_i32_at(const uint8_t *p, bool big_endian) {
    return (int32_t)fxr1_read_u32_at(p, big_endian);
}

static void fxr1_write_u32_at(uint8_t *p, uint32_t v, bool big_endian) {
    if (big_endian) {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    } else {
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        p[3] = (uint8_t)(v >> 24);
    }
}

static void fxr1_write_i32_at(uint8_t *p, int32_t v, bool big_endian) {
    fxr1_write_u32_at(p, (uint32_t)v, big_endian);
}

static void fxr1_clear_blobs(sf_fxr1_t *fxr) {
    if (!fxr) return;
    sf_xfree(fxr->alloc, fxr->node_blob);
    sf_xfree(fxr->alloc, fxr->metadata_blob);
    fxr->node_blob = NULL;
    fxr->metadata_blob = NULL;
    fxr->node_blob_size = 0;
    fxr->metadata_blob_size = 0;
}

static sf_result_t fxr1_copy_blob(const sf_allocator_t *alloc, const uint8_t *src,
                                  size_t size, uint8_t **out) {
    *out = NULL;
    if (size == 0) return SF_OK;
    uint8_t *dst = (uint8_t *)sf_xalloc(alloc, size);
    if (!dst) return SF_ERR_OOM;
    memcpy(dst, src, size);
    *out = dst;
    return SF_OK;
}

sf_result_t sf_fxr1_create(sf_fxr1_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_fxr1_t *fxr = (sf_fxr1_t *)sf_xalloc(alloc, sizeof(*fxr));
    if (!fxr) return SF_ERR_OOM;
    memset(fxr, 0, sizeof(*fxr));
    fxr->alloc = alloc;
    *out = fxr;
    return SF_OK;
}

void sf_fxr1_destroy(sf_fxr1_t *fxr) {
    if (!fxr) return;
    fxr1_clear_blobs(fxr);
    sf_xfree(fxr->alloc, fxr);
}

bool sf_fxr1_is(const void *bytes, size_t size) {
    if (!bytes || size < 8) return false;
    const uint8_t *p = (const uint8_t *)bytes;
    if (memcmp(p, "FXR\0", FXR1_MAGIC_SIZE) != 0) return false;
    uint32_t check = fxr1_read_u32_at(p + 4, false);
    return check == FXR1_CHECK_LITTLE || check == FXR1_CHECK_BIG;
}

sf_result_t sf_fxr1_read_from_memory(const void *bytes, size_t size,
                                     sf_fxr1_t **out,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(bytes != NULL && out != NULL);
    *out = NULL;
    if (!sf_fxr1_is(bytes, size)) return SF_ERR_BAD_MAGIC;

    const uint8_t *p = (const uint8_t *)bytes;
    uint32_t check = fxr1_read_u32_at(p + 4, false);
    bool big_endian = check == FXR1_CHECK_BIG;

    if (size < 16) return SF_ERR_TRUNCATED;
    uint32_t long_check = fxr1_read_u32_at(p + 12, big_endian);
    bool wide = long_check == 0 || long_check == FXR1_WIDE_GARBAGE;
    size_t header_size = fxr1_header_size(wide);
    if (size < header_size) return SF_ERR_TRUNCATED;

    size_t cursor = 8;
    int32_t main_data_offset = fxr1_read_i32_at(p + cursor, big_endian);
    cursor += 4;
    if (wide) cursor += 4;
    int32_t metadata_table_offset = fxr1_read_i32_at(p + cursor, big_endian);
    cursor += 4;
    int32_t pointer_table_count = fxr1_read_i32_at(p + cursor, big_endian);
    cursor += 4;
    int32_t function_table_count = fxr1_read_i32_at(p + cursor, big_endian);
    cursor += 4;
    int32_t unk1 = fxr1_read_i32_at(p + cursor, big_endian);
    cursor += 4;
    int32_t unk2 = fxr1_read_i32_at(p + cursor, big_endian);

    if (main_data_offset < 0 || metadata_table_offset < 0) return SF_ERR_OUT_OF_RANGE;
    size_t main_off = (size_t)main_data_offset;
    size_t meta_off = (size_t)metadata_table_offset;
    if (main_off > size || meta_off > size || meta_off < main_off) return SF_ERR_TRUNCATED;
    if (main_off < header_size) return SF_ERR_OUT_OF_RANGE;

    sf_fxr1_t *fxr = NULL;
    sf_result_t r = sf_fxr1_create(&fxr, alloc);
    if (r != SF_OK) return r;

    fxr->big_endian = big_endian;
    fxr->wide = wide;
    fxr->unk1 = unk1;
    fxr->unk2 = unk2;
    fxr->pointer_table_count = pointer_table_count;
    fxr->function_table_count = function_table_count;

    r = fxr1_copy_blob(fxr->alloc, p + main_off, meta_off - main_off, &fxr->node_blob);
    if (r != SF_OK) {
        sf_fxr1_destroy(fxr);
        return r;
    }
    fxr->node_blob_size = meta_off - main_off;

    r = fxr1_copy_blob(fxr->alloc, p + meta_off, size - meta_off, &fxr->metadata_blob);
    if (r != SF_OK) {
        sf_fxr1_destroy(fxr);
        return r;
    }
    fxr->metadata_blob_size = size - meta_off;

    *out = fxr;
    return SF_OK;
}

sf_result_t sf_fxr1_write_to_memory(const sf_fxr1_t *fxr,
                                    void **out_bytes,
                                    size_t *out_size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(fxr != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    size_t header_size = fxr1_header_size(fxr->wide);
    if (fxr->node_blob_size > SIZE_MAX - header_size) return SF_ERR_OUT_OF_RANGE;
    size_t metadata_offset = header_size + fxr->node_blob_size;
    if (fxr->metadata_blob_size > SIZE_MAX - metadata_offset) return SF_ERR_OUT_OF_RANGE;
    size_t total_size = metadata_offset + fxr->metadata_blob_size;
    if (metadata_offset > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, total_size ? total_size : header_size);
    if (!buf) return SF_ERR_OOM;
    memset(buf, 0, total_size ? total_size : header_size);

    memcpy(buf, "FXR\0", FXR1_MAGIC_SIZE);
    fxr1_write_u32_at(buf + 4, fxr->big_endian ? FXR1_CHECK_BIG : FXR1_CHECK_LITTLE, false);

    size_t cursor = 8;
    fxr1_write_i32_at(buf + cursor, (int32_t)header_size, fxr->big_endian);
    cursor += 4;
    if (fxr->wide) {
        fxr1_write_u32_at(buf + cursor, 0, fxr->big_endian);
        cursor += 4;
    }
    fxr1_write_i32_at(buf + cursor, (int32_t)metadata_offset, fxr->big_endian);
    cursor += 4;
    fxr1_write_i32_at(buf + cursor, fxr->pointer_table_count, fxr->big_endian);
    cursor += 4;
    fxr1_write_i32_at(buf + cursor, fxr->function_table_count, fxr->big_endian);
    cursor += 4;
    fxr1_write_i32_at(buf + cursor, fxr->unk1, fxr->big_endian);
    cursor += 4;
    fxr1_write_i32_at(buf + cursor, fxr->unk2, fxr->big_endian);

    if (fxr->node_blob_size > 0) memcpy(buf + header_size, fxr->node_blob, fxr->node_blob_size);
    if (fxr->metadata_blob_size > 0) {
        memcpy(buf + metadata_offset, fxr->metadata_blob, fxr->metadata_blob_size);
    }

    *out_bytes = buf;
    *out_size = total_size;
    return SF_OK;
}

bool sf_fxr1_big_endian(const sf_fxr1_t *fxr) { return fxr ? fxr->big_endian : false; }
bool sf_fxr1_wide(const sf_fxr1_t *fxr) { return fxr ? fxr->wide : false; }
int32_t sf_fxr1_unk1(const sf_fxr1_t *fxr) { return fxr ? fxr->unk1 : 0; }
int32_t sf_fxr1_unk2(const sf_fxr1_t *fxr) { return fxr ? fxr->unk2 : 0; }

void sf_fxr1_set_big_endian(sf_fxr1_t *fxr, bool big_endian) {
    if (fxr) fxr->big_endian = big_endian;
}

void sf_fxr1_set_wide(sf_fxr1_t *fxr, bool wide) {
    if (fxr) fxr->wide = wide;
}

void sf_fxr1_set_unk1(sf_fxr1_t *fxr, int32_t unk1) {
    if (fxr) fxr->unk1 = unk1;
}

void sf_fxr1_set_unk2(sf_fxr1_t *fxr, int32_t unk2) {
    if (fxr) fxr->unk2 = unk2;
}
