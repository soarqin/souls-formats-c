/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_flver0.h"

#include "internal/flver0_internal.h"
#include "internal/flver_common_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool version_supported(uint32_t v) {
    static const uint32_t versions[] = { 0x0Eu, 0x0Fu, 0x10u, 0x11u, 0x12u,
        0x13u, 0x14u, 0x15u, 0x10002u, 0x10003u };
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        if (versions[i] == v) return true;
    }
    return false;
}

static bool count_valid(int32_t count, size_t elem_size) {
    return count >= 0 && (count == 0 || (size_t)count <= SIZE_MAX / elem_size);
}

static void *alloc_array(const sf_allocator_t *a, int32_t count, size_t elem_size) {
    if (!count_valid(count, elem_size)) return NULL;
    if (count == 0) return NULL;
    void *p = sf_xalloc(a, (size_t)count * elem_size);
    if (p) memset(p, 0, (size_t)count * elem_size);
    return p;
}

static int32_t pos32(sf_binary_writer_t *bw) {
    int64_t p = sf_binary_writer_position(bw);
    return (p < 0 || p > INT32_MAX) ? -1 : (int32_t)p;
}

static sf_result_t read_header(sf_binary_reader_t *br, sf_flver0_header_t *h) {
    uint8_t magic[6];
    sf_result_t r = sf_binary_reader_read_bytes(br, magic, sizeof(magic)); if (r != SF_OK) return r;
    if (memcmp(magic, "FLVER\0", sizeof(magic)) != 0) return SF_ERR_BAD_MAGIC;
    uint8_t endian[2];
    r = sf_binary_reader_read_bytes(br, endian, sizeof(endian)); if (r != SF_OK) return r;
    if (endian[0] == 'B' && endian[1] == 0) h->big_endian = true;
    else if (endian[0] == 'L' && endian[1] == 0) h->big_endian = false;
    else return SF_ERR_BAD_MAGIC;
    sf_binary_reader_set_big_endian(br, h->big_endian);
    if ((r = sf_binary_reader_read_u32(br, &h->version)) != SF_OK) return r;
    if (!version_supported(h->version)) return SF_ERR_UNSUPPORTED_VERSION;
    if ((r = sf_binary_reader_read_i32(br, &h->data_offset)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->data_length)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->dummy_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->material_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->bone_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->mesh_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->vertex_buffer_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &h->bbox_min)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &h->bbox_max)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->face_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->total_face_count)) != SF_OK) return r;
    static const uint8_t index_sizes[] = { 16, 32 };
    if ((r = sf_binary_reader_assert_u8(br, 2, index_sizes, &h->vertex_index_size)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &h->unicode)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &h->unk4a)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &h->unk4b)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &h->unk4c)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &h->unk5c)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_u8_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_u8_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_u8_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_pattern(br, 0x20, 0)) != SF_OK) return r;
    if (!count_valid(h->dummy_count, sizeof(sf_flver_dummy_t)) ||
        !count_valid(h->material_count, sizeof(sf_flver0_material_t)) ||
        !count_valid(h->bone_count, sizeof(sf_flver_node_t)) ||
        !count_valid(h->mesh_count, sizeof(sf_flver0_mesh_t))) return SF_ERR_OUT_OF_RANGE;
    return SF_OK;
}

static sf_result_t read_layout(sf_binary_reader_t *br, sf_flver0_buffer_layout_t *bl,
                               const sf_allocator_t *a) {
    int16_t member_count = 0, struct_size = 0;
    sf_result_t r = sf_binary_reader_read_i16(br, &member_count); if (r != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &struct_size)) != SF_OK) return r;
    if (member_count < 0 || struct_size < 0) return SF_ERR_OUT_OF_RANGE;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    bl->member_count = (size_t)member_count;
    bl->size = (uint32_t)struct_size;
    bl->members = (sf_flver0_layout_member_t *)alloc_array(a, member_count, sizeof(*bl->members));
    if (member_count > 0 && !bl->members) return SF_ERR_OOM;
    int32_t struct_offset = 0;
    uint32_t computed = 0;
    for (int16_t i = 0; i < member_count; i++) {
        sfi_flver_layout_member_t tmp;
        r = sfi_flver_layout_member_read(br, struct_offset, false, &tmp); if (r != SF_OK) return r;
        if (tmp.type == SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED) return SF_ERR_UNSUPPORTED_VERSION;
        bl->members[i].stream = tmp.stream;
        bl->members[i].struct_offset = struct_offset;
        bl->members[i].type = tmp.type;
        bl->members[i].semantic = tmp.semantic;
        bl->members[i].index = tmp.index;
        bl->members[i].special_modifier = tmp.special_modifier;
        uint32_t sz = sf_flver_layout_type_size(tmp.type, tmp.special_modifier);
        if (sz == UINT32_MAX) return SF_ERR_UNSUPPORTED_VERSION;
        struct_offset += (int32_t)sz;
        computed += sz;
    }
    return computed == bl->size ? SF_OK : SF_ERR_BAD_MAGIC;
}

static sf_result_t write_layout(sf_binary_writer_t *bw, const sf_flver0_buffer_layout_t *bl) {
    if (bl->member_count > INT16_MAX || bl->size > INT16_MAX) return SF_ERR_OUT_OF_RANGE;
    sf_result_t r = sf_binary_writer_write_i16(bw, (int16_t)bl->member_count); if (r != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, (int16_t)bl->size)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    int32_t off = 0;
    for (size_t i = 0; i < bl->member_count; i++) {
        sfi_flver_layout_member_t tmp;
        tmp.stream = bl->members[i].stream;
        tmp.type = bl->members[i].type;
        tmp.semantic = bl->members[i].semantic;
        tmp.index = bl->members[i].index;
        tmp.special_modifier = bl->members[i].special_modifier;
        r = sfi_flver_layout_member_write(bw, off, false, &tmp); if (r != SF_OK) return r;
        uint32_t sz = sf_flver_layout_type_size(tmp.type, tmp.special_modifier);
        if (sz == UINT32_MAX) return SF_ERR_UNSUPPORTED_VERSION;
        off += (int32_t)sz;
    }
    return SF_OK;
}

static sf_result_t read_texture(sf_binary_reader_t *br, bool unicode, sf_flver0_texture_t *t) {
    int32_t path_off = 0, type_off = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &path_off); if (r != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &type_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    r = unicode ? sf_binary_reader_get_utf16(br, path_off, &t->path, NULL)
                : sf_binary_reader_get_shift_jis(br, path_off, &t->path, NULL);
    if (r != SF_OK) return r;
    if (type_off > 0) {
        r = unicode ? sf_binary_reader_get_utf16(br, type_off, &t->param_name, NULL)
                    : sf_binary_reader_get_shift_jis(br, type_off, &t->param_name, NULL);
    }
    return r;
}

static sf_result_t read_material(sf_binary_reader_t *br, bool unicode,
                                 sf_flver0_material_t *m, const sf_allocator_t *a) {
    int32_t name_off = 0, mtd_off = 0, textures_off = 0, layouts_off = 0;
    int32_t data_len = 0, layout_header_off = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &name_off); if (r != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &mtd_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &textures_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &layouts_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &data_len)) != SF_OK) return r;
    (void)data_len;
    if ((r = sf_binary_reader_read_i32(br, &layout_header_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    r = unicode ? sf_binary_reader_get_utf16(br, name_off, &m->name, NULL)
                : sf_binary_reader_get_shift_jis(br, name_off, &m->name, NULL);
    if (r != SF_OK) return r;
    r = unicode ? sf_binary_reader_get_utf16(br, mtd_off, &m->mtd, NULL)
                : sf_binary_reader_get_shift_jis(br, mtd_off, &m->mtd, NULL);
    if (r != SF_OK) return r;
    if ((r = sf_binary_reader_step_in(br, textures_off)) != SF_OK) return r;
    uint8_t tex_count = 0;
    if ((r = sf_binary_reader_read_u8(br, &tex_count)) == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    m->texture_count = tex_count;
    m->textures = (sf_flver0_texture_t *)alloc_array(a, tex_count, sizeof(*m->textures));
    if (r == SF_OK && tex_count > 0 && !m->textures) r = SF_ERR_OOM;
    for (uint8_t i = 0; r == SF_OK && i < tex_count; i++) r = read_texture(br, unicode, &m->textures[i]);
    sf_result_t out_r = sf_binary_reader_step_out(br);
    if (r != SF_OK) return r;
    if (out_r != SF_OK) return out_r;

    if (layout_header_off != 0) {
        if ((r = sf_binary_reader_step_in(br, layout_header_off)) != SF_OK) return r;
        int32_t layout_count = 0, offset_to_offsets = 0;
        if ((r = sf_binary_reader_read_i32(br, &layout_count)) != SF_OK) return r;
        if (!count_valid(layout_count, sizeof(sf_flver0_buffer_layout_t))) return SF_ERR_OUT_OF_RANGE;
        if ((r = sf_binary_reader_read_i32(br, &offset_to_offsets)) != SF_OK) return r;
        (void)offset_to_offsets;
        if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
        if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
        int32_t *offsets = (int32_t *)alloc_array(a, layout_count, sizeof(int32_t));
        if (layout_count > 0 && !offsets) return SF_ERR_OOM;
        r = sf_binary_reader_read_i32s(br, (size_t)layout_count, offsets);
        m->layout_count = (size_t)layout_count;
        m->layouts = (sf_flver0_buffer_layout_t *)alloc_array(a, layout_count, sizeof(*m->layouts));
        if (r == SF_OK && layout_count > 0 && !m->layouts) r = SF_ERR_OOM;
        for (int32_t i = 0; r == SF_OK && i < layout_count; i++) {
            r = sf_binary_reader_step_in(br, offsets[i]);
            if (r == SF_OK) r = read_layout(br, &m->layouts[i], a);
            sf_result_t rr = sf_binary_reader_step_out(br);
            if (r == SF_OK) r = rr;
        }
        sf_xfree(a, offsets);
        out_r = sf_binary_reader_step_out(br);
        return r != SF_OK ? r : out_r;
    }
    m->layout_count = 1;
    m->layouts = (sf_flver0_buffer_layout_t *)alloc_array(a, 1, sizeof(*m->layouts));
    if (!m->layouts) return SF_ERR_OOM;
    if ((r = sf_binary_reader_step_in(br, layouts_off)) != SF_OK) return r;
    r = read_layout(br, &m->layouts[0], a);
    out_r = sf_binary_reader_step_out(br);
    return r != SF_OK ? r : out_r;
}

static sf_result_t read_vertex_buffer_header(sf_binary_reader_t *br, int32_t off,
                                             int32_t *layout_index, int32_t *length,
                                             int32_t *buffer_off) {
    sf_result_t r = sf_binary_reader_step_in(br, off); if (r != SF_OK) return r;
    int32_t count = 0, buffers_off = 0;
    if ((r = sf_binary_reader_read_i32(br, &count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &buffers_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if (count < 1) return SF_ERR_UNSUPPORTED_VERSION;
    if ((r = sf_binary_reader_step_in(br, buffers_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, layout_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, length)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, buffer_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    sf_result_t r2 = sf_binary_reader_step_out(br);
    sf_result_t r3 = sf_binary_reader_step_out(br);
    return r != SF_OK ? r : (r2 != SF_OK ? r2 : r3);
}

static sf_result_t read_mesh(sf_binary_reader_t *br, const sf_flver0_header_t *h,
                             const sf_flver0_material_t *materials, size_t material_count,
                             sf_flver0_mesh_t *m, const sf_allocator_t *a) {
    sf_result_t r;
    if ((r = sf_binary_reader_read_u8(br, &m->dynamic)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &m->material_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_bool(br, &m->cull_backfaces)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_bool(br, &m->triangle_strip)) != SF_OK) return r;
    int32_t index_count = 0, vertex_count = 0;
    if ((r = sf_binary_reader_read_i32(br, &index_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &vertex_count)) != SF_OK) return r;
    if (index_count < 0 || vertex_count < 0) return SF_ERR_OUT_OF_RANGE;
    if ((r = sf_binary_reader_read_i16(br, &m->node_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16s(br, SF_FLVER0_MAX_BONE_COUNT, m->bone_indices)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &m->used_bone_count)) != SF_OK) return r;
    int32_t indices_len = 0, indices_off = 0, buffer_len = 0, buffer_off = 0, vb1 = 0, vb2 = 0;
    if ((r = sf_binary_reader_read_i32(br, &indices_len)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &indices_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &buffer_len)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &buffer_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &vb1)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &vb2)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    m->index_count = (size_t)index_count;
    m->index_size = h->vertex_index_size;
    m->vertex_count = (size_t)vertex_count;
    m->indices = (uint32_t *)alloc_array(a, index_count, sizeof(uint32_t));
    if (index_count > 0 && !m->indices) return SF_ERR_OOM;
    int64_t idx_abs = (int64_t)h->data_offset + indices_off;
    if (h->vertex_index_size == 16) {
        uint16_t *tmp = (uint16_t *)alloc_array(a, index_count, sizeof(uint16_t));
        if (index_count > 0 && !tmp) return SF_ERR_OOM;
        r = sf_binary_reader_get_u16s(br, idx_abs, (size_t)index_count, tmp);
        for (int32_t i = 0; r == SF_OK && i < index_count; i++) m->indices[i] = tmp[i];
        sf_xfree(a, tmp);
        if (r != SF_OK) return r;
    } else {
        r = sf_binary_reader_get_u32s(br, idx_abs, (size_t)index_count, m->indices);
        if (r != SF_OK) return r;
    }
    if (vb1 != 0) {
        r = read_vertex_buffer_header(br, vb1, &m->layout_index, &buffer_len, &buffer_off);
        if (r != SF_OK) return r;
    } else {
        m->layout_index = 0;
    }
    if (vb2 != 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (m->material_index >= material_count) return SF_ERR_OUT_OF_RANGE;
    if (m->layout_index < 0 || (size_t)m->layout_index >= materials[m->material_index].layout_count) return SF_ERR_OUT_OF_RANGE;
    (void)indices_len;
    if (buffer_len < 0) return SF_ERR_OUT_OF_RANGE;
    m->vertex_bytes_size = (size_t)buffer_len;
    if (buffer_len > 0) {
        m->vertex_bytes = (uint8_t *)sf_xalloc(a, (size_t)buffer_len);
        if (!m->vertex_bytes) return SF_ERR_OOM;
        return sf_binary_reader_get_bytes(br, (int64_t)h->data_offset + buffer_off,
                                          m->vertex_bytes, (size_t)buffer_len);
    }
    return SF_OK;
}

static void material_destroy(sf_flver0_material_t *m, const sf_allocator_t *a) {
    if (!m) return;
    sf_xfree(a, m->name); sf_xfree(a, m->mtd);
    for (size_t i = 0; i < m->texture_count; i++) {
        sf_xfree(a, m->textures[i].path); sf_xfree(a, m->textures[i].param_name);
    }
    for (size_t i = 0; i < m->layout_count; i++) sf_xfree(a, m->layouts[i].members);
    sf_xfree(a, m->textures); sf_xfree(a, m->layouts);
}

static sf_result_t populate(sf_flver0_t **out, sf_binary_reader_t *br, const sf_allocator_t *a) {
    sf_flver0_t *f = (sf_flver0_t *)sf_xalloc(a, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    memset(f, 0, sizeof(*f)); f->alloc = a;
    sf_result_t r = read_header(br, &f->header);
    if (r != SF_OK) { sf_flver0_destroy(f); return r; }
    f->dummies = (sf_flver_dummy_t *)alloc_array(a, f->header.dummy_count, sizeof(*f->dummies));
    f->materials = (sf_flver0_material_t *)alloc_array(a, f->header.material_count, sizeof(*f->materials));
    f->nodes = (sf_flver_node_t *)alloc_array(a, f->header.bone_count, sizeof(*f->nodes));
    f->meshes = (sf_flver0_mesh_t *)alloc_array(a, f->header.mesh_count, sizeof(*f->meshes));
    if ((f->header.dummy_count > 0 && !f->dummies) || (f->header.material_count > 0 && !f->materials) ||
        (f->header.bone_count > 0 && !f->nodes) || (f->header.mesh_count > 0 && !f->meshes)) {
        sf_flver0_destroy(f); return SF_ERR_OOM;
    }
    for (int32_t i = 0; i < f->header.dummy_count; i++) { r = sfi_flver_dummy_read(br, &f->dummies[i]); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < f->header.material_count; i++) { r = read_material(br, f->header.unicode != 0, &f->materials[i], a); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < f->header.bone_count; i++) { r = sfi_flver_node_read(br, f->header.unicode != 0, &f->nodes[i], a); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < f->header.mesh_count; i++) { r = read_mesh(br, &f->header, f->materials, sf_flver0_material_count(f), &f->meshes[i], a); if (r != SF_OK) goto fail; }
    *out = f; return SF_OK;
fail:
    sf_flver0_destroy(f); return r;
}

sf_result_t sf_flver0_read_from_memory(sf_flver0_t **out, const void *bytes, size_t size,
                                       const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (bytes != NULL || size == 0));
    *out = NULL; a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL; sf_result_t r = sf_istream_open_memory(&is, bytes, size, a); if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL; r = sf_binary_reader_create(&br, is, false, a);
    if (r == SF_OK) r = populate(out, br, a);
    sf_binary_reader_destroy(br); sf_istream_close(is); return r;
}

sf_result_t sf_flver0_read_from_path(sf_flver0_t **out, const wchar_t *path, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL; a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL; sf_result_t r = sf_istream_open_wfile(&is, path, a); if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL; r = sf_binary_reader_create(&br, is, false, a);
    if (r == SF_OK) r = populate(out, br, a);
    sf_binary_reader_destroy(br); sf_istream_close(is); return r;
}

static sf_result_t write_header(sf_binary_writer_t *bw, const sf_flver0_t *f) {
    const sf_flver0_header_t *h = &f->header;
    sf_result_t r = sf_binary_writer_write_bytes(bw, "FLVER\0", 6); if (r != SF_OK) return r;
    r = sf_binary_writer_write_bytes(bw, h->big_endian ? "B\0" : "L\0", 2); if (r != SF_OK) return r;
    if ((r = sf_binary_writer_write_u32(bw, h->version)) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FLVER0DataOffset"), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FLVER0DataSize"), return r);
    int32_t dc = (int32_t)sf_flver0_dummy_count(f), matc = (int32_t)sf_flver0_material_count(f);
    int32_t nc = (int32_t)sf_flver0_node_count(f), meshc = (int32_t)sf_flver0_mesh_count(f);
    if ((r = sf_binary_writer_write_i32(bw, dc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, matc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, nc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, meshc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, meshc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, h->bbox_min)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, h->bbox_max)) != SF_OK) return r;
    int32_t faces = 0, total = 0;
    for (int32_t i = 0; i < meshc; i++) {
        total += (int32_t)f->meshes[i].index_count;
        if (h->version >= 0x15u && !f->meshes[i].triangle_strip) faces += (int32_t)(f->meshes[i].index_count / 3);
        else if (f->meshes[i].index_count >= 3) faces += (int32_t)(f->meshes[i].index_count - 2);
    }
    if ((r = sf_binary_writer_write_i32(bw, faces)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, total)) != SF_OK) return r;
    uint8_t index_size = h->vertex_index_size ? h->vertex_index_size : 16;
    for (int32_t i = 0; i < meshc; i++) for (size_t j = 0; j < f->meshes[i].index_count; j++) if (f->meshes[i].indices[j] > UINT16_MAX) index_size = 32;
    if ((r = sf_binary_writer_write_u8(bw, index_size)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, h->unicode)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, h->unk4a)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, h->unk4b)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, h->unk4c)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, h->unk5c)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    return sf_binary_writer_write_pattern(bw, 0x20, 0);
}

static sf_result_t reserve_i32_indexed(sf_binary_writer_t *bw, const char *prefix, size_t i) {
    char name[64];
    snprintf(name, sizeof(name), "%s%u", prefix, (unsigned)i);
    return sf_binary_writer_reserve_i32(bw, name);
}

static sf_result_t fill_i32_indexed(sf_binary_writer_t *bw, const char *prefix, size_t i, int32_t v) {
    char name[64];
    snprintf(name, sizeof(name), "%s%u", prefix, (unsigned)i);
    return sf_binary_writer_fill_i32(bw, name, v);
}

static sf_result_t write_material_header(sf_binary_writer_t *bw, size_t i) {
    sf_result_t r;
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatName", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatMTD", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatTextures", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatLayouts", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatDataLen", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0MatLayoutHeader", i), return r);
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_mesh_header(sf_binary_writer_t *bw, const sf_flver0_mesh_t *m,
                                     const sf_flver0_material_t *mat, size_t i) {
    if (m->index_count > INT32_MAX || m->vertex_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    sf_result_t r;
    if ((r = sf_binary_writer_write_u8(bw, m->dynamic)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, m->material_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_bool(bw, m->cull_backfaces)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_bool(bw, m->triangle_strip)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)m->index_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)m->vertex_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, m->node_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16s(bw, SF_FLVER0_MAX_BONE_COUNT, m->bone_indices)) != SF_OK) return r;
    int16_t used = 0;
    for (size_t j = 0; j < SF_FLVER0_MAX_BONE_COUNT && m->bone_indices[j] >= 0; j++) used++;
    if ((r = sf_binary_writer_write_i16(bw, used)) != SF_OK) return r;
    const sf_flver0_buffer_layout_t *layout = &mat->layouts[m->layout_index];
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)(m->index_count * 2))) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0IndexOffset", i), return r);
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)(layout->size * m->vertex_count))) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0VBDataOffset", i), return r);
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0VBListOffset", i), return r);
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_material_subs(sf_binary_writer_t *bw, const sf_flver0_t *f,
                                       const sf_flver0_material_t *m, size_t i) {
    bool unicode = f->header.unicode != 0;
    sf_result_t r;
    int32_t start = pos32(bw); if (start < 0) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatName", i, start), return r);
    r = unicode ? sf_binary_writer_write_utf16(bw, m->name ? m->name : "", true)
                : sf_binary_writer_write_shift_jis(bw, m->name ? m->name : "", true);
    if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatMTD", i, pos32(bw)), return r);
    r = unicode ? sf_binary_writer_write_utf16(bw, m->mtd ? m->mtd : "", true)
                : sf_binary_writer_write_shift_jis(bw, m->mtd ? m->mtd : "", true);
    if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatTextures", i, pos32(bw)), return r);
    if (m->texture_count > UINT8_MAX) return SF_ERR_OUT_OF_RANGE;
    if ((r = sf_binary_writer_write_u8(bw, (uint8_t)m->texture_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    for (size_t t = 0; t < m->texture_count; t++) {
        char n[64]; snprintf(n, sizeof(n), "FLVER0TexPath%u_%u", (unsigned)i, (unsigned)t);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
        snprintf(n, sizeof(n), "FLVER0TexType%u_%u", (unsigned)i, (unsigned)t);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
        if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
        if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    }
    for (size_t t = 0; t < m->texture_count; t++) {
        char n[64]; snprintf(n, sizeof(n), "FLVER0TexPath%u_%u", (unsigned)i, (unsigned)t);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, pos32(bw)), return r);
        r = unicode ? sf_binary_writer_write_utf16(bw, m->textures[t].path ? m->textures[t].path : "", true)
                    : sf_binary_writer_write_shift_jis(bw, m->textures[t].path ? m->textures[t].path : "", true);
        if (r != SF_OK) return r;
        snprintf(n, sizeof(n), "FLVER0TexType%u_%u", (unsigned)i, (unsigned)t);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, pos32(bw)), return r);
        r = unicode ? sf_binary_writer_write_utf16(bw, m->textures[t].param_name ? m->textures[t].param_name : "", true)
                    : sf_binary_writer_write_shift_jis(bw, m->textures[t].param_name ? m->textures[t].param_name : "", true);
        if (r != SF_OK) return r;
    }
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatLayoutHeader", i, pos32(bw)), return r);
    if (m->layout_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)m->layout_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, pos32(bw) + 0x0C)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    for (size_t l = 0; l < m->layout_count; l++) {
        char n[64]; snprintf(n, sizeof(n), "FLVER0LayoutOffset%u_%u", (unsigned)i, (unsigned)l);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
    }
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatLayouts", i, pos32(bw)), return r);
    for (size_t l = 0; l < m->layout_count; l++) {
        char n[64]; snprintf(n, sizeof(n), "FLVER0LayoutOffset%u_%u", (unsigned)i, (unsigned)l);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, pos32(bw)), return r);
        r = write_layout(bw, &m->layouts[l]); if (r != SF_OK) return r;
    }
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0MatDataLen", i, pos32(bw) - start), return r);
    return SF_OK;
}

static sf_result_t write_node_strings(sf_binary_writer_t *bw, const sf_flver0_t *f) {
    for (int32_t i = 0; i < f->header.bone_count; i++) {
        char name[48]; snprintf(name, sizeof(name), "BoneNameOffset%u", (unsigned)i);
        sf_result_t r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, name, pos32(bw)), return r);
        const char *s = f->nodes[i].name ? f->nodes[i].name : "";
        r = f->header.unicode ? sf_binary_writer_write_utf16(bw, s, true)
                              : sf_binary_writer_write_shift_jis(bw, s, true);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t write_vb_header(sf_binary_writer_t *bw, const sf_flver0_mesh_t *m, size_t i) {
    sf_result_t r;
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0VBListOffset", i, pos32(bw)), return r);
    if ((r = sf_binary_writer_write_i32(bw, 1)) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0VBInfoOffset", i), return r);
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0VBInfoOffset", i, pos32(bw)), return r);
    if ((r = sf_binary_writer_write_i32(bw, m->layout_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, (int32_t)m->vertex_bytes_size)) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, reserve_i32_indexed(bw, "FLVER0VBOffset2", i), return r);
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t write_to_writer(const sf_flver0_t *f, sf_binary_writer_t *bw) {
    if (!version_supported(f->header.version)) return SF_ERR_UNSUPPORTED_VERSION;
    sf_binary_writer_set_big_endian(bw, f->header.big_endian);
    sf_result_t r = write_header(bw, f); if (r != SF_OK) return r;
    for (int32_t i = 0; i < f->header.dummy_count; i++) { r = sfi_flver_dummy_write(bw, &f->dummies[i]); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < f->header.material_count; i++) { r = write_material_header(bw, (size_t)i); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < f->header.bone_count; i++) { r = sfi_flver_node_write(bw, &f->nodes[i], (size_t)i); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < f->header.mesh_count; i++) {
        if (f->meshes[i].material_index >= (uint8_t)f->header.material_count) return SF_ERR_OUT_OF_RANGE;
        r = write_mesh_header(bw, &f->meshes[i], &f->materials[f->meshes[i].material_index], (size_t)i); if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.material_count; i++) { r = write_material_subs(bw, f, &f->materials[i], (size_t)i); if (r != SF_OK) return r; }
    r = write_node_strings(bw, f); if (r != SF_OK) return r;
    for (int32_t i = 0; i < f->header.mesh_count; i++) { r = write_vb_header(bw, &f->meshes[i], (size_t)i); if (r != SF_OK) return r; }
    r = sf_binary_writer_pad(bw, 0x20); if (r != SF_OK) return r;
    int32_t data_start = pos32(bw); if (data_start < 0) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, "FLVER0DataOffset", data_start), return r);
    uint8_t idx_size = f->header.vertex_index_size ? f->header.vertex_index_size : 16;
    for (int32_t i = 0; i < f->header.mesh_count; i++) {
        SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0IndexOffset", (size_t)i, pos32(bw) - data_start), return r);
        for (size_t j = 0; j < f->meshes[i].index_count; j++) {
            if (idx_size == 16) {
                if (f->meshes[i].indices[j] > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
                r = sf_binary_writer_write_u16(bw, (uint16_t)f->meshes[i].indices[j]);
            } else {
                r = sf_binary_writer_write_u32(bw, f->meshes[i].indices[j]);
            }
            if (r != SF_OK) return r;
        }
        r = sf_binary_writer_pad(bw, 0x20); if (r != SF_OK) return r;
        int32_t rel = pos32(bw) - data_start;
        SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0VBDataOffset", (size_t)i, rel), return r);
        SF_RESERVE_FILL_PAIR(r, fill_i32_indexed(bw, "FLVER0VBOffset2", (size_t)i, rel), return r);
        r = sf_binary_writer_write_bytes(bw, f->meshes[i].vertex_bytes, f->meshes[i].vertex_bytes_size); if (r != SF_OK) return r;
        r = sf_binary_writer_pad(bw, 0x20); if (r != SF_OK) return r;
    }
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, "FLVER0DataSize", pos32(bw) - data_start), return r);
    return sf_binary_writer_finish(bw);
}

sf_result_t sf_flver0_write_to_memory(const sf_flver0_t *f, void **out_bytes, size_t *out_size,
                                      const sf_allocator_t *a) {
    SF_CHECK_ARG(f != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0; a = sf_alloc_or_default(a);
    sf_ostream_t *os = NULL; sf_result_t r = sf_ostream_open_memory(&os, a); if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL; r = sf_binary_writer_create(&bw, os, f->header.big_endian, a);
    if (r == SF_OK) r = write_to_writer(f, bw);
    if (r == SF_OK) r = sf_ostream_detach_buffer(os, out_bytes, out_size);
    sf_binary_writer_destroy(bw); sf_ostream_close(os); return r;
}

sf_result_t sf_flver0_write_to_path(const sf_flver0_t *f, const wchar_t *path) {
    SF_CHECK_ARG(f != NULL && path != NULL);
    sf_ostream_t *os = NULL; sf_result_t r = sf_ostream_open_wfile(&os, path, f->alloc); if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL; r = sf_binary_writer_create(&bw, os, f->header.big_endian, f->alloc);
    if (r == SF_OK) r = write_to_writer(f, bw);
    sf_binary_writer_destroy(bw); sf_ostream_close(os); return r;
}

void sf_flver0_destroy(sf_flver0_t *f) {
    if (!f) return;
    const sf_allocator_t *a = f->alloc;
    for (int32_t i = 0; i < f->header.material_count; i++) material_destroy(&f->materials[i], a);
    for (int32_t i = 0; i < f->header.bone_count; i++) sfi_flver_node_destroy_inplace(&f->nodes[i], a);
    for (int32_t i = 0; i < f->header.mesh_count; i++) { sf_xfree(a, f->meshes[i].indices); sf_xfree(a, f->meshes[i].vertex_bytes); }
    sf_xfree(a, f->dummies); sf_xfree(a, f->materials); sf_xfree(a, f->nodes); sf_xfree(a, f->meshes); sf_xfree(a, f);
}

uint32_t sf_flver0_header_version(const sf_flver0_t *f) { return f ? f->header.version : 0; }
bool sf_flver0_header_big_endian(const sf_flver0_t *f) { return f ? f->header.big_endian : false; }
bool sf_flver0_header_unicode(const sf_flver0_t *f) { return f ? f->header.unicode != 0 : false; }
sf_vec3_t sf_flver0_header_bounding_box_min(const sf_flver0_t *f) { sf_vec3_t z = {0,0,0}; return f ? f->header.bbox_min : z; }
sf_vec3_t sf_flver0_header_bounding_box_max(const sf_flver0_t *f) { sf_vec3_t z = {0,0,0}; return f ? f->header.bbox_max : z; }
size_t sf_flver0_dummy_count(const sf_flver0_t *f) { return (f && f->header.dummy_count > 0) ? (size_t)f->header.dummy_count : 0; }
size_t sf_flver0_node_count(const sf_flver0_t *f) { return (f && f->header.bone_count > 0) ? (size_t)f->header.bone_count : 0; }
size_t sf_flver0_material_count(const sf_flver0_t *f) { return (f && f->header.material_count > 0) ? (size_t)f->header.material_count : 0; }
size_t sf_flver0_mesh_count(const sf_flver0_t *f) { return (f && f->header.mesh_count > 0) ? (size_t)f->header.mesh_count : 0; }
const sf_flver_dummy_t *sf_flver0_dummy(const sf_flver0_t *f, size_t i) { return (f && i < sf_flver0_dummy_count(f)) ? &f->dummies[i] : NULL; }
const sf_flver_node_t *sf_flver0_node(const sf_flver0_t *f, size_t i) { return (f && i < sf_flver0_node_count(f)) ? &f->nodes[i] : NULL; }
const sf_flver0_material_t *sf_flver0_material(const sf_flver0_t *f, size_t i) { return (f && i < sf_flver0_material_count(f)) ? &f->materials[i] : NULL; }
const sf_flver0_mesh_t *sf_flver0_mesh(const sf_flver0_t *f, size_t i) { return (f && i < sf_flver0_mesh_count(f)) ? &f->meshes[i] : NULL; }
const char *sf_flver0_material_name(const sf_flver0_material_t *m) { return m ? m->name : NULL; }
const char *sf_flver0_material_mtd(const sf_flver0_material_t *m) { return m ? m->mtd : NULL; }
size_t sf_flver0_material_texture_count(const sf_flver0_material_t *m) { return m ? m->texture_count : 0; }
const sf_flver0_texture_t *sf_flver0_material_texture(const sf_flver0_material_t *m, size_t i) { return (m && i < m->texture_count) ? &m->textures[i] : NULL; }
size_t sf_flver0_material_layout_count(const sf_flver0_material_t *m) { return m ? m->layout_count : 0; }
const sf_flver0_buffer_layout_t *sf_flver0_material_layout(const sf_flver0_material_t *m, size_t i) { return (m && i < m->layout_count) ? &m->layouts[i] : NULL; }
const char *sf_flver0_texture_path(const sf_flver0_texture_t *t) { return t ? t->path : NULL; }
const char *sf_flver0_texture_param_name(const sf_flver0_texture_t *t) { return t ? t->param_name : NULL; }
uint8_t sf_flver0_mesh_dynamic(const sf_flver0_mesh_t *m) { return m ? m->dynamic : 0; }
uint8_t sf_flver0_mesh_material_index(const sf_flver0_mesh_t *m) { return m ? m->material_index : 0; }
bool sf_flver0_mesh_cull_backfaces(const sf_flver0_mesh_t *m) { return m ? m->cull_backfaces : false; }
bool sf_flver0_mesh_triangle_strip(const sf_flver0_mesh_t *m) { return m ? m->triangle_strip : false; }
int16_t sf_flver0_mesh_node_index(const sf_flver0_mesh_t *m) { return m ? m->node_index : -1; }
size_t sf_flver0_mesh_index_count(const sf_flver0_mesh_t *m) { return m ? m->index_count : 0; }
uint32_t sf_flver0_mesh_index(const sf_flver0_mesh_t *m, size_t i) { return (m && i < m->index_count) ? m->indices[i] : 0; }
size_t sf_flver0_mesh_vertex_count(const sf_flver0_mesh_t *m) { return m ? m->vertex_count : 0; }
int32_t sf_flver0_mesh_layout_index(const sf_flver0_mesh_t *m) { return m ? m->layout_index : -1; }
const uint8_t *sf_flver0_mesh_vertex_bytes(const sf_flver0_mesh_t *m, size_t *out_size) { if (out_size) *out_size = m ? m->vertex_bytes_size : 0; return m ? m->vertex_bytes : NULL; }
size_t sf_flver0_buffer_layout_member_count(const sf_flver0_buffer_layout_t *bl) { return bl ? bl->member_count : 0; }
uint32_t sf_flver0_buffer_layout_size(const sf_flver0_buffer_layout_t *bl) { return bl ? bl->size : 0; }
sf_flver_layout_type_t sf_flver0_buffer_layout_member_type(const sf_flver0_buffer_layout_t *bl, size_t i) { return (bl && i < bl->member_count) ? bl->members[i].type : SF_FLVER_LAYOUT_TYPE_FLOAT1; }
sf_flver_layout_semantic_t sf_flver0_buffer_layout_member_semantic(const sf_flver0_buffer_layout_t *bl, size_t i) { return (bl && i < bl->member_count) ? bl->members[i].semantic : SF_FLVER_LAYOUT_SEMANTIC_POSITION; }
