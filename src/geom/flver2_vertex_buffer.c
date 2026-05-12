/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER2 BufferLayout / VertexBuffer.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/FLVER/FLVER2/VertexBuffer.cs
 *   SoulsFormats/Formats/FLVER/FLVER2/BufferLayout.cs
 *   SoulsFormats/Formats/FLVER/LayoutMember.cs
 */

#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static sf_result_t flver2_reserve_name(char *buf, size_t buf_size, const char *prefix,
                                       size_t index) {
    int written = snprintf(buf, buf_size, "%s%zu", prefix, index);
    return (written < 0 || (size_t)written >= buf_size) ? SF_ERR_INTERNAL : SF_OK;
}

static bool flver2_i32_product_size(int32_t a, int32_t b, size_t *out) {
    if (a < 0 || b < 0) return false;
    size_t sa = (size_t)a;
    size_t sb = (size_t)b;
    if (sa != 0 && sb > SIZE_MAX / sa) return false;
    *out = sa * sb;
    return true;
}

uint32_t sfi_flver2_buffer_layout_size(const sf_flver2_buffer_layout_t *bl) {
    if (!bl) return 0;
    uint32_t total = 0;
    for (size_t i = 0; i < bl->member_count; i++) {
        uint32_t sz = sf_flver_layout_type_size(bl->members[i].type,
                                                bl->members[i].special_modifier);
        if (sz == UINT32_MAX || total > UINT32_MAX - sz) return UINT32_MAX;
        total += sz;
    }
    return total;
}

sf_result_t sfi_flver2_vertex_buffer_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_vertex_buffer_t *out,
                                          const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && hdr != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    int32_t unk10 = 0;
    int32_t unk14 = 0;
    int32_t buffer_length = 0;
    int32_t buffer_offset = 0;
    sf_result_t r;

    r = sf_binary_reader_read_i32(br, &out->buffer_index); if (r != SF_OK) return r;
    int32_t final = out->buffer_index & ~0x60000000;
    if (final != out->buffer_index) out->buffer_index = final;
    r = sf_binary_reader_read_i32(br, &out->layout_index); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->vertex_size);  if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->vertex_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &unk10);             if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &unk14);             if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &buffer_length);     if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &buffer_offset);     if (r != SF_OK) return r;
    if (unk10 != 0 || unk14 != 0) return SF_ERR_BAD_MAGIC;
    if (buffer_offset < 0 || hdr->data_offset < 0) return SF_ERR_OUT_OF_RANGE;
    (void)buffer_length;

    if (!flver2_i32_product_size(out->vertex_size, out->vertex_count,
                                 &out->vertex_bytes_size)) {
        return SF_ERR_OUT_OF_RANGE;
    }
    if (out->vertex_bytes_size == 0) return SF_OK;

    out->vertex_bytes = (uint8_t *)sf_xalloc(a, out->vertex_bytes_size);
    if (!out->vertex_bytes) return SF_ERR_OOM;

    int64_t vertex_offset = (int64_t)hdr->data_offset + (int64_t)buffer_offset;
    r = sf_binary_reader_step_in(br, vertex_offset); if (r != SF_OK) return r;
    r = sf_binary_reader_read_bytes(br, out->vertex_bytes, out->vertex_bytes_size);
    sf_result_t step = sf_binary_reader_step_out(br);
    return (r != SF_OK) ? r : step;
}

sf_result_t sfi_flver2_vertex_buffer_write(sf_binary_writer_t *bw,
                                           const sf_flver2_header_t *hdr,
                                           const sf_flver2_vertex_buffer_t *vb,
                                           size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && vb != NULL);
    char name[48];
    sf_result_t r = flver2_reserve_name(name, sizeof(name), "VertexBufferOffset", index);
    if (r != SF_OK) return r;
    if (vb->vertex_bytes_size > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    int32_t buffer_length = (hdr->version > 0x20005u) ? (int32_t)vb->vertex_bytes_size : 0;
    r = sf_binary_writer_write_i32(bw, vb->buffer_index); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, vb->layout_index); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, vb->vertex_size);  if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, vb->vertex_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, buffer_length);    if (r != SF_OK) return r;
    return sf_binary_writer_reserve_i32(bw, name);
}

sf_result_t sfi_flver2_vertex_buffer_write_data(sf_binary_writer_t *bw,
                                                const sf_flver2_vertex_buffer_t *vb,
                                                size_t index,
                                                int32_t data_start) {
    SF_CHECK_ARG(bw != NULL && vb != NULL);
    if (vb->vertex_bytes_size > 0 && !vb->vertex_bytes) return SF_ERR_INVALID_ARG;
    char name[48];
    sf_result_t r = flver2_reserve_name(name, sizeof(name), "VertexBufferOffset", index);
    if (r != SF_OK) return r;
    int64_t pos = sf_binary_writer_position(bw);
    if (pos < data_start || pos - (int64_t)data_start > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, name, (int32_t)(pos - (int64_t)data_start)), return r);
    return sf_binary_writer_write_bytes(bw, vb->vertex_bytes, vb->vertex_bytes_size);
}

void sfi_flver2_vertex_buffer_destroy_inplace(sf_flver2_vertex_buffer_t *vb,
                                              const sf_allocator_t *a) {
    if (!vb) return;
    sf_xfree(a, vb->vertex_bytes);
    memset(vb, 0, sizeof(*vb));
}

sf_result_t sfi_flver2_buffer_layout_read(sf_binary_reader_t *br,
                                          const sf_flver2_header_t *hdr,
                                          sf_flver2_buffer_layout_t *out,
                                          const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && hdr != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    int32_t member_count = 0;
    int32_t members_offset = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &member_count); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                   if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                   if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &members_offset);           if (r != SF_OK) return r;
    if (member_count < 0 || members_offset < 0) return SF_ERR_OUT_OF_RANGE;
    if (member_count == 0) return SF_OK;
    if ((size_t)member_count > SIZE_MAX / sizeof(*out->members)) return SF_ERR_OUT_OF_RANGE;

    out->members = (sf_flver2_layout_member_t *)sf_xalloc(
        a, (size_t)member_count * sizeof(*out->members));
    if (!out->members) return SF_ERR_OOM;
    memset(out->members, 0, (size_t)member_count * sizeof(*out->members));
    out->member_count = (size_t)member_count;

    r = sf_binary_reader_step_in(br, members_offset); if (r != SF_OK) return r;
    int32_t struct_offset = 0;
    for (size_t i = 0; i < out->member_count; i++) {
        int32_t stream_raw = 0;
        r = sf_binary_reader_read_i32(br, &stream_raw); if (r != SF_OK) break;
        sf_flver2_layout_member_t *m = &out->members[i];
        bool is_speedtree = hdr->special_modifier == -32768;
        int16_t low = (int16_t)(stream_raw & 0xFFFF);
        int16_t high = (int16_t)((uint32_t)stream_raw >> 16);
        if (is_speedtree || low == -32768 || high == -32768) {
            m->stream = low;
            m->special_modifier = (high == 0 && low == -32768) ? -32768 : high;
        } else {
            m->stream = stream_raw;
            m->special_modifier = 0;
        }

        r = sf_binary_reader_read_i32(br, &m->struct_offset); if (r != SF_OK) break;
        if (!is_speedtree && low != -32768 && high != -32768 && m->struct_offset != struct_offset) {
            r = SF_ERR_BAD_MAGIC;
            break;
        }
        uint32_t type = 0;
        uint32_t semantic = 0;
        r = sf_binary_reader_read_u32(br, &type);     if (r != SF_OK) break;
        r = sf_binary_reader_read_u32(br, &semantic); if (r != SF_OK) break;
        r = sf_binary_reader_read_i32(br, &m->index); if (r != SF_OK) break;
        m->type = (sf_flver_layout_type_t)type;
        m->semantic = (sf_flver_layout_semantic_t)semantic;

        uint32_t size = sf_flver_layout_type_size(m->type, m->special_modifier);
        if (size == UINT32_MAX || struct_offset > INT32_MAX - (int32_t)size) {
            r = SF_ERR_UNSUPPORTED_VERSION;
            break;
        }
        struct_offset += (int32_t)size;
    }
    sf_result_t step = sf_binary_reader_step_out(br);
    return (r != SF_OK) ? r : step;
}

sf_result_t sfi_flver2_buffer_layout_write(sf_binary_writer_t *bw,
                                           const sf_flver2_header_t *hdr,
                                           const sf_flver2_buffer_layout_t *bl,
                                           size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && bl != NULL);
    (void)hdr;
    if (bl->member_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    char name[48];
    sf_result_t r = flver2_reserve_name(name, sizeof(name), "VertexStructLayout", index);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)bl->member_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                         if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);                         if (r != SF_OK) return r;
    return sf_binary_writer_reserve_i32(bw, name);
}

sf_result_t sfi_flver2_buffer_layout_write_members(sf_binary_writer_t *bw,
                                                   const sf_flver2_header_t *hdr,
                                                   const sf_flver2_buffer_layout_t *bl,
                                                   size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && bl != NULL);
    char name[48];
    sf_result_t r = flver2_reserve_name(name, sizeof(name), "VertexStructLayout", index);
    if (r != SF_OK) return r;
    int64_t pos = sf_binary_writer_position(bw);
    if (pos > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, name, (int32_t)pos), return r);

    int32_t struct_offset = 0;
    bool is_speedtree = hdr->special_modifier == -32768;
    for (size_t i = 0; i < bl->member_count; i++) {
        const sf_flver2_layout_member_t *m = &bl->members[i];
        if (is_speedtree || m->special_modifier != 0) {
            r = sf_binary_writer_write_i16(bw, (int16_t)m->stream); if (r != SF_OK) return r;
            r = sf_binary_writer_write_i16(bw, m->special_modifier); if (r != SF_OK) return r;
        } else {
            r = sf_binary_writer_write_i32(bw, m->stream); if (r != SF_OK) return r;
        }
        r = sf_binary_writer_write_i32(bw, is_speedtree ? m->struct_offset : struct_offset);
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_u32(bw, (uint32_t)m->type);     if (r != SF_OK) return r;
        r = sf_binary_writer_write_u32(bw, (uint32_t)m->semantic); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, m->index);              if (r != SF_OK) return r;

        uint32_t size = sf_flver_layout_type_size(m->type, m->special_modifier);
        if (size == UINT32_MAX || struct_offset > INT32_MAX - (int32_t)size) {
            return SF_ERR_UNSUPPORTED_VERSION;
        }
        struct_offset += (int32_t)size;
    }
    return SF_OK;
}

void sfi_flver2_buffer_layout_destroy_inplace(sf_flver2_buffer_layout_t *bl,
                                              const sf_allocator_t *a) {
    if (!bl) return;
    sf_xfree(a, bl->members);
    memset(bl, 0, sizeof(*bl));
}

size_t sf_flver2_buffer_layout_member_count(const sf_flver2_buffer_layout_t *bl) {
    return bl ? bl->member_count : 0;
}

uint32_t sf_flver2_buffer_layout_size(const sf_flver2_buffer_layout_t *bl) {
    return sfi_flver2_buffer_layout_size(bl);
}

static const sf_flver2_layout_member_t *flver2_layout_member(
    const sf_flver2_buffer_layout_t *bl, size_t i) {
    return (bl && i < bl->member_count) ? &bl->members[i] : NULL;
}

int32_t sf_flver2_buffer_layout_member_stream(const sf_flver2_buffer_layout_t *bl, size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->stream : 0;
}

int32_t sf_flver2_buffer_layout_member_struct_offset(const sf_flver2_buffer_layout_t *bl,
                                                     size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->struct_offset : 0;
}

sf_flver_layout_type_t sf_flver2_buffer_layout_member_type(
    const sf_flver2_buffer_layout_t *bl, size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->type : SF_FLVER_LAYOUT_TYPE_FLOAT1;
}

sf_flver_layout_semantic_t sf_flver2_buffer_layout_member_semantic(
    const sf_flver2_buffer_layout_t *bl, size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->semantic : SF_FLVER_LAYOUT_SEMANTIC_POSITION;
}

int32_t sf_flver2_buffer_layout_member_index(const sf_flver2_buffer_layout_t *bl, size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->index : -1;
}

int16_t sf_flver2_buffer_layout_member_special_modifier(const sf_flver2_buffer_layout_t *bl,
                                                        size_t i) {
    const sf_flver2_layout_member_t *m = flver2_layout_member(bl, i);
    return m ? m->special_modifier : 0;
}

int32_t sf_flver2_vertex_buffer_buffer_index(const sf_flver2_vertex_buffer_t *vb) {
    return vb ? vb->buffer_index : -1;
}

int32_t sf_flver2_vertex_buffer_layout_index(const sf_flver2_vertex_buffer_t *vb) {
    return vb ? vb->layout_index : -1;
}

int32_t sf_flver2_vertex_buffer_vertex_size(const sf_flver2_vertex_buffer_t *vb) {
    return vb ? vb->vertex_size : 0;
}

int32_t sf_flver2_vertex_buffer_vertex_count(const sf_flver2_vertex_buffer_t *vb) {
    return vb ? vb->vertex_count : 0;
}

const uint8_t *sf_flver2_vertex_buffer_bytes(const sf_flver2_vertex_buffer_t *vb,
                                            size_t *out_size) {
    if (out_size) *out_size = vb ? vb->vertex_bytes_size : 0;
    return vb ? vb->vertex_bytes : NULL;
}
