/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER2 FaceSet records. Mirrors upstream
 * SoulsFormats/Formats/FLVER/FLVER2/FaceSet.cs for the v1-supported
 * uncompressed 16/32-bit index paths.
 */

#include "souls_formats/sf_flver2.h"

#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static sf_result_t faceset_index_label(char *buf, size_t buf_size, size_t index) {
    int written = snprintf(buf, buf_size, "FaceSetIndices%zu", index);
    return (written < 0 || (size_t)written >= buf_size) ? SF_ERR_INTERNAL : SF_OK;
}

static sf_result_t faceset_read_indices(sf_binary_reader_t *br,
                                        sf_flver2_face_set_t *out,
                                        int64_t absolute_offset,
                                        const sf_allocator_t *a) {
    if (out->index_count == 0) return SF_OK;
    if (out->index_count > SIZE_MAX / sizeof(*out->indices)) return SF_ERR_OUT_OF_RANGE;

    out->indices = (uint32_t *)sf_xalloc(a, out->index_count * sizeof(*out->indices));
    if (!out->indices) return SF_ERR_OOM;

    sf_result_t r = sf_binary_reader_step_in(br, absolute_offset);
    if (r != SF_OK) return r;

    if (out->index_size == 16) {
        for (size_t i = 0; i < out->index_count; i++) {
            uint16_t value = 0;
            r = sf_binary_reader_read_u16(br, &value);
            if (r != SF_OK) break;
            out->indices[i] = value;
        }
    } else if (out->index_size == 32) {
        for (size_t i = 0; i < out->index_count; i++) {
            r = sf_binary_reader_read_u32(br, &out->indices[i]);
            if (r != SF_OK) break;
        }
    } else {
        r = SF_ERR_UNSUPPORTED_VERSION;
    }

    sf_result_t step = sf_binary_reader_step_out(br);
    return (r != SF_OK) ? r : step;
}

sf_result_t sfi_flver2_face_set_read(sf_binary_reader_t *br,
                                     const sf_flver2_header_t *hdr,
                                     int32_t vertex_indices_size,
                                     int32_t data_offset,
                                     sf_flver2_face_set_t *out,
                                     const sf_allocator_t *a) {
    (void)hdr;
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r = sf_binary_reader_read_u32(br, &out->flags);
    if (r != SF_OK) return r;
    if ((out->flags & SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED) != 0) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    r = sf_binary_reader_read_bool(br, &out->triangle_strip); if (r != SF_OK) return r;
    r = sf_binary_reader_read_bool(br, &out->cull_backfaces); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &out->unk06);            if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &out->unk07);            if (r != SF_OK) return r;

    int32_t index_count = 0;
    int32_t index_offset = 0;
    r = sf_binary_reader_read_i32(br, &index_count);  if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &index_offset); if (r != SF_OK) return r;
    if (index_count < 0 || index_offset < 0) return SF_ERR_OUT_OF_RANGE;
    out->index_count = (size_t)index_count;

    r = sf_binary_reader_read_u8(br, &out->index_size); if (r != SF_OK) return r;
    if (out->index_size == 0 && (vertex_indices_size == 16 || vertex_indices_size == 32)) {
        out->index_size = (uint8_t)vertex_indices_size;
    }
    if (out->index_size != 16 && out->index_size != 32) return SF_ERR_UNSUPPORTED_VERSION;

    r = sf_binary_reader_assert_u8_one(br, 0);  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i16_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;

    return faceset_read_indices(br, out, (int64_t)data_offset + index_offset, a);
}

sf_result_t sfi_flver2_face_set_write(sf_binary_writer_t *bw,
                                      const sf_flver2_header_t *hdr,
                                      const sf_flver2_face_set_t *fs,
                                      int32_t vertex_indices_size,
                                      size_t index) {
    (void)hdr;
    SF_CHECK_ARG(bw != NULL && fs != NULL);
    if ((fs->flags & SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED) != 0) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    uint8_t index_size = fs->index_size;
    if (index_size == 0 && (vertex_indices_size == 16 || vertex_indices_size == 32)) {
        index_size = (uint8_t)vertex_indices_size;
    }
    if (index_size != 16 && index_size != 32) return SF_ERR_UNSUPPORTED_VERSION;
    if (fs->index_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    char label[48];
    sf_result_t r = faceset_index_label(label, sizeof(label), index);
    if (r != SF_OK) return r;

    r = sf_binary_writer_write_u32(bw, fs->flags);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, fs->triangle_strip);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_bool(bw, fs->cull_backfaces);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, fs->unk06);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, fs->unk07);                 if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)fs->index_count); if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, label);                  if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, index_size);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);                         if (r != SF_OK) return r;
    r = sf_binary_writer_write_i16(bw, 0);                        if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

sf_result_t sfi_flver2_face_set_write_indices(sf_binary_writer_t *bw,
                                              const sf_flver2_face_set_t *fs,
                                              size_t index,
                                              int32_t data_start) {
    SF_CHECK_ARG(bw != NULL && fs != NULL);
    if ((fs->flags & SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED) != 0) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    if (fs->index_count > 0 && fs->indices == NULL) return SF_ERR_INVALID_ARG;

    int64_t pos = sf_binary_writer_position(bw);
    if (pos < data_start || pos - data_start > INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    char label[48];
    sf_result_t r = faceset_index_label(label, sizeof(label), index);
    if (r != SF_OK) return r;
    r = sf_binary_writer_fill_i32(bw, label, (int32_t)(pos - data_start));
    if (r != SF_OK) return r;

    if (fs->index_size == 16) {
        for (size_t i = 0; i < fs->index_count; i++) {
            if (fs->indices[i] > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
            r = sf_binary_writer_write_u16(bw, (uint16_t)fs->indices[i]);
            if (r != SF_OK) return r;
        }
        return SF_OK;
    }
    if (fs->index_size == 32) {
        return sf_binary_writer_write_u32s(bw, fs->index_count, fs->indices);
    }
    return SF_ERR_UNSUPPORTED_VERSION;
}

static bool faceset_is_degenerate(uint32_t a, uint32_t b, uint32_t c) {
    return a == b || b == c || c == a;
}

static size_t faceset_strip_triangle_count(const sf_flver2_face_set_t *fs,
                                           bool filter_degenerate) {
    size_t total = 0;
    size_t segment_start = 0;
    for (size_t i = 0; i <= fs->index_count; i++) {
        bool restart = fs->index_size == 16 && i < fs->index_count && fs->indices[i] == 0xFFFFu;
        if (i == fs->index_count || restart) {
            size_t segment_len = i - segment_start;
            for (size_t j = 0; j + 2 < segment_len; j++) {
                uint32_t a = fs->indices[segment_start + j];
                uint32_t b = fs->indices[segment_start + j + 1];
                uint32_t c = fs->indices[segment_start + j + 2];
                if (!filter_degenerate || !faceset_is_degenerate(a, b, c)) total++;
            }
            segment_start = i + 1;
        }
    }
    return total;
}

sf_result_t sfi_flver2_face_set_triangulate(const sf_flver2_face_set_t *fs,
                                            bool filter_degenerate,
                                            uint32_t **out_indices,
                                            size_t *out_count,
                                            const sf_allocator_t *a) {
    SF_CHECK_ARG(fs != NULL && out_indices != NULL && out_count != NULL);
    *out_indices = NULL;
    *out_count = 0;
    if (fs->index_count > 0 && fs->indices == NULL) return SF_ERR_INVALID_ARG;

    if (!fs->triangle_strip) {
        if (fs->index_count == 0) return SF_OK;
        if (fs->index_count > SIZE_MAX / sizeof(**out_indices)) return SF_ERR_OUT_OF_RANGE;
        uint32_t *copy = (uint32_t *)sf_xalloc(a, fs->index_count * sizeof(*copy));
        if (!copy) return SF_ERR_OOM;
        memcpy(copy, fs->indices, fs->index_count * sizeof(*copy));
        *out_indices = copy;
        *out_count = fs->index_count;
        return SF_OK;
    }

    size_t tri_count = faceset_strip_triangle_count(fs, filter_degenerate);
    if (tri_count == 0) return SF_OK;
    if (tri_count > SIZE_MAX / (3u * sizeof(**out_indices))) return SF_ERR_OUT_OF_RANGE;
    uint32_t *triangles = (uint32_t *)sf_xalloc(a, tri_count * 3u * sizeof(*triangles));
    if (!triangles) return SF_ERR_OOM;

    size_t out = 0;
    size_t segment_start = 0;
    for (size_t i = 0; i <= fs->index_count; i++) {
        bool restart = fs->index_size == 16 && i < fs->index_count && fs->indices[i] == 0xFFFFu;
        if (i == fs->index_count || restart) {
            size_t segment_len = i - segment_start;
            for (size_t j = 0; j + 2 < segment_len; j++) {
                uint32_t i0 = fs->indices[segment_start + j];
                uint32_t i1 = fs->indices[segment_start + j + 1];
                uint32_t i2 = fs->indices[segment_start + j + 2];
                if (filter_degenerate && faceset_is_degenerate(i0, i1, i2)) continue;
                if ((j & 1u) == 0) {
                    triangles[out++] = i0;
                    triangles[out++] = i1;
                    triangles[out++] = i2;
                } else {
                    triangles[out++] = i1;
                    triangles[out++] = i0;
                    triangles[out++] = i2;
                }
            }
            segment_start = i + 1;
        }
    }

    *out_indices = triangles;
    *out_count = out;
    return SF_OK;
}

void sfi_flver2_face_set_destroy_inplace(sf_flver2_face_set_t *fs,
                                         const sf_allocator_t *a) {
    if (!fs) return;
    sf_xfree(a, fs->indices);
    memset(fs, 0, sizeof(*fs));
}

sf_flver2_fs_flags_t sf_flver2_face_set_flags(const sf_flver2_face_set_t *fs) {
    return fs ? fs->flags : SF_FLVER2_FS_FLAGS_NONE;
}
bool sf_flver2_face_set_triangle_strip(const sf_flver2_face_set_t *fs) {
    return fs ? fs->triangle_strip : false;
}
bool sf_flver2_face_set_cull_backfaces(const sf_flver2_face_set_t *fs) {
    return fs ? fs->cull_backfaces : false;
}
uint8_t sf_flver2_face_set_unk06(const sf_flver2_face_set_t *fs) {
    return fs ? fs->unk06 : 0;
}
uint8_t sf_flver2_face_set_unk07(const sf_flver2_face_set_t *fs) {
    return fs ? fs->unk07 : 0;
}
uint8_t sf_flver2_face_set_index_size(const sf_flver2_face_set_t *fs) {
    return fs ? fs->index_size : 0;
}
size_t sf_flver2_face_set_index_count(const sf_flver2_face_set_t *fs) {
    return fs ? fs->index_count : 0;
}
uint32_t sf_flver2_face_set_index(const sf_flver2_face_set_t *fs, size_t i) {
    return (fs && i < fs->index_count) ? fs->indices[i] : 0;
}
