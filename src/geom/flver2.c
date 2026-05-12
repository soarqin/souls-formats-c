/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FLVER2 top-level reader/writer dispatch.
 *
 * Strictly mirrors the outer Read/Write flow in upstream
 * SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs. Sub-record parsers are
 * intentionally skeletal until Phase 6 T13-T18 land.
 */

#include "souls_formats/sf_flver2.h"

#include "internal/flver2_internal.h"
#include "internal/flver_common_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool flver2_version_supported(uint32_t version) {
    static const uint32_t versions[] = {
        0x20005u, 0x20007u, 0x20009u, 0x2000Bu, 0x2000Cu, 0x2000Du,
        0x2000Eu, 0x2000Fu, 0x20010u, 0x20013u, 0x20014u, 0x20016u,
        0x20017u, 0x2001Au, 0x2001Bu, 0x20021u,
    };
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        if (versions[i] == version) return true;
    }
    return false;
}

static bool flver2_count_valid(int32_t count, size_t elem_size) {
    return count >= 0 && (count == 0 || (size_t)count <= SIZE_MAX / elem_size);
}

static void *flver2_alloc_array(const sf_allocator_t *a, int32_t count, size_t elem_size) {
    if (count == 0) return NULL;
    if (!flver2_count_valid(count, elem_size)) return NULL;
    void *p = sf_xalloc(a, (size_t)count * elem_size);
    if (p) memset(p, 0, (size_t)count * elem_size);
    return p;
}

static sf_result_t flver2_read_header(sf_binary_reader_t *br, sf_flver2_header_t *h) {
    memset(h, 0, sizeof(*h));

    uint8_t magic[6];
    sf_result_t r = sf_binary_reader_read_bytes(br, magic, sizeof(magic));
    if (r != SF_OK) return r;
    if (memcmp(magic, "FLVER\0", sizeof(magic)) != 0) return SF_ERR_BAD_MAGIC;

    uint8_t endian[2];
    r = sf_binary_reader_read_bytes(br, endian, sizeof(endian));
    if (r != SF_OK) return r;
    if (endian[0] == 'B' && endian[1] == 0) return SF_ERR_UNSUPPORTED_VERSION;
    if (!(endian[0] == 'L' && endian[1] == 0)) return SF_ERR_BAD_MAGIC;
    sf_binary_reader_set_big_endian(br, false);

    r = sf_binary_reader_read_u32(br, &h->version); if (r != SF_OK) return r;
    if (!flver2_version_supported(h->version)) return SF_ERR_UNSUPPORTED_VERSION;

    r = sf_binary_reader_read_i32(br, &h->data_offset);         if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->data_length);         if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->dummy_count);         if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->material_count);      if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->bone_count);          if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->mesh_count);          if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->vertex_buffer_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_vec3(br, &h->bbox_min);           if (r != SF_OK) return r;
    r = sf_binary_reader_read_vec3(br, &h->bbox_max);           if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->face_count);          if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->unk_face_count);      if (r != SF_OK) return r;

    uint8_t vertex_indices_size = 0;
    static const uint8_t index_size_options[] = { 0, 8, 16, 32 };
    r = sf_binary_reader_assert_u8(br, 4, index_size_options, &vertex_indices_size);
    if (r != SF_OK) return r;
    h->vertex_indices_size = (int32_t)vertex_indices_size;
    r = sf_binary_reader_read_u8(br, &h->unicode); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &h->unk4a);   if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &h->unk4b);   if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->unk4c);  if (r != SF_OK) return r;

    r = sf_binary_reader_read_i32(br, &h->face_set_count);      if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->buffer_layout_count); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &h->texture_count);       if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &h->unk5c);                if (r != SF_OK) return r;
    r = sf_binary_reader_read_u8(br, &h->unk5d);                if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);                  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0);                  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                 if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);                 if (r != SF_OK) return r;

    if (h->version >= 0x20014u) {
        int16_t unk68 = 0;
        static const int16_t unk68_options[] = { 0, 1, 2, 3, 4, 5 };
        static const int16_t special_options[] = { 0, -32768 };
        r = sf_binary_reader_assert_i16(br, 6, unk68_options, &unk68);
        if (r != SF_OK) return r;
        h->unk68 = unk68;
        r = sf_binary_reader_assert_i16(br, 2, special_options, &h->special_modifier);
        if (r != SF_OK) return r;
    } else {
        static const int32_t unk68_options[] = { 0, 1, 2, 3, 4, 5 };
        r = sf_binary_reader_assert_i32(br, 6, unk68_options, &h->unk68);
        if (r != SF_OK) return r;
        h->special_modifier = 0;
    }

    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    static const int32_t unk74_options[] = { 0, 0x10 };
    r = sf_binary_reader_assert_i32(br, 2, unk74_options, &h->unk74);
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0); if (r != SF_OK) return r;

    if (!flver2_count_valid(h->dummy_count, sizeof(sf_flver_dummy_t)) ||
        !flver2_count_valid(h->material_count, sizeof(sf_flver2_material_t)) ||
        !flver2_count_valid(h->bone_count, sizeof(sf_flver_node_t)) ||
        !flver2_count_valid(h->mesh_count, sizeof(sf_flver2_mesh_t)) ||
        !flver2_count_valid(h->face_set_count, sizeof(sf_flver2_face_set_t)) ||
        !flver2_count_valid(h->vertex_buffer_count, sizeof(sf_flver2_vertex_buffer_t)) ||
        !flver2_count_valid(h->buffer_layout_count, sizeof(sf_flver2_buffer_layout_t)) ||
        !flver2_count_valid(h->texture_count, sizeof(sf_flver2_texture_t))) {
        return SF_ERR_OUT_OF_RANGE;
    }
    return SF_OK;
}

static sf_result_t flver2_alloc_sections(sf_flver2_t *f) {
    const sf_allocator_t *a = f->alloc;
    f->dummies = (sf_flver_dummy_t *)flver2_alloc_array(a, f->header.dummy_count,
                                                        sizeof(*f->dummies));
    f->materials = (sf_flver2_material_t *)flver2_alloc_array(a, f->header.material_count,
                                                             sizeof(*f->materials));
    f->nodes = (sf_flver_node_t *)flver2_alloc_array(a, f->header.bone_count,
                                                     sizeof(*f->nodes));
    f->meshes = (sf_flver2_mesh_t *)flver2_alloc_array(a, f->header.mesh_count,
                                                       sizeof(*f->meshes));
    f->face_sets = (sf_flver2_face_set_t *)flver2_alloc_array(a, f->header.face_set_count,
                                                              sizeof(*f->face_sets));
    f->vertex_buffers = (sf_flver2_vertex_buffer_t *)flver2_alloc_array(
        a, f->header.vertex_buffer_count, sizeof(*f->vertex_buffers));
    f->buffer_layouts = (sf_flver2_buffer_layout_t *)flver2_alloc_array(
        a, f->header.buffer_layout_count, sizeof(*f->buffer_layouts));
    f->textures = (sf_flver2_texture_t *)flver2_alloc_array(a, f->header.texture_count,
                                                            sizeof(*f->textures));

    if ((f->header.dummy_count > 0 && !f->dummies) ||
        (f->header.material_count > 0 && !f->materials) ||
        (f->header.bone_count > 0 && !f->nodes) ||
        (f->header.mesh_count > 0 && !f->meshes) ||
        (f->header.face_set_count > 0 && !f->face_sets) ||
        (f->header.vertex_buffer_count > 0 && !f->vertex_buffers) ||
        (f->header.buffer_layout_count > 0 && !f->buffer_layouts) ||
        (f->header.texture_count > 0 && !f->textures)) {
        return SF_ERR_OOM;
    }
    return SF_OK;
}

static sf_result_t flver2_read_sections(sf_flver2_t *f, sf_binary_reader_t *br) {
    sf_result_t r = flver2_alloc_sections(f);
    if (r != SF_OK) return r;

    for (int32_t i = 0; i < f->header.dummy_count; i++) {
        r = sfi_flver_dummy_read(br, &f->dummies[i]); if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.material_count; i++) {
        r = sfi_flver2_material_read(br, f, &f->materials[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.bone_count; i++) {
        r = sfi_flver_node_read(br, f->header.unicode != 0, &f->nodes[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.mesh_count; i++) {
        r = sfi_flver2_mesh_read(br, &f->header, &f->meshes[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.face_set_count; i++) {
        r = sfi_flver2_face_set_read(br, &f->header, f->header.vertex_indices_size,
                                     f->header.data_offset, &f->face_sets[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.vertex_buffer_count; i++) {
        r = sfi_flver2_vertex_buffer_read(br, &f->header, &f->vertex_buffers[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.buffer_layout_count; i++) {
        r = sfi_flver2_buffer_layout_read(br, &f->header, &f->buffer_layouts[i], f->alloc);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.texture_count; i++) {
        r = sfi_flver2_texture_read(br, &f->header, &f->textures[i], f->alloc);
        if (r != SF_OK) return r;
    }
    if (f->header.version >= 0x2001Au) {
        r = sfi_flver2_skeleton_set_read(br, &f->header, &f->skeleton_set, f->alloc);
        if (r != SF_OK) return r;
    }

    r = sfi_flver2_take_textures(f);
    if (r != SF_OK) return r;
    return SF_OK;
}

static sf_result_t flver2_populate_from_reader(sf_flver2_t **out, sf_binary_reader_t *br,
                                               const sf_allocator_t *a) {
    sf_flver2_t *f = (sf_flver2_t *)sf_xalloc(a, sizeof(*f));
    if (!f) return SF_ERR_OOM;
    memset(f, 0, sizeof(*f));
    f->alloc = a;

    sf_result_t r = flver2_read_header(br, &f->header);
    if (r == SF_OK) r = flver2_read_sections(f, br);
    if (r != SF_OK) {
        sf_flver2_destroy(f);
        return r;
    }
    *out = f;
    return SF_OK;
}

sf_result_t sf_flver2_read_from_memory(sf_flver2_t **out, const void *bytes, size_t size,
                                       const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bytes != NULL || size == 0);
    *out = NULL;
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, bytes, size, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    r = flver2_populate_from_reader(out, br, a);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_flver2_read_from_path(sf_flver2_t **out, const wchar_t *path,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    r = flver2_populate_from_reader(out, br, a);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

static sf_result_t flver2_write_header(sf_binary_writer_t *bw, const sf_flver2_t *f) {
    const sf_flver2_header_t *h = &f->header;
    sf_result_t r;
    r = sf_binary_writer_write_bytes(bw, "FLVER\0", 6); if (r != SF_OK) return r;
    r = sf_binary_writer_write_bytes(bw, "L\0", 2);     if (r != SF_OK) return r;
    r = sf_binary_writer_write_u32(bw, h->version);      if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "DataOffset"); if (r != SF_OK) return r;
    r = sf_binary_writer_reserve_i32(bw, "DataSize");   if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->dummy_count);         if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->material_count);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->bone_count);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->mesh_count);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->vertex_buffer_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_vec3(bw, h->bbox_min);           if (r != SF_OK) return r;
    r = sf_binary_writer_write_vec3(bw, h->bbox_max);           if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->face_count);          if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->unk_face_count);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, (uint8_t)h->vertex_indices_size); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, h->unicode); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, h->unk4a);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, h->unk4b);   if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->unk4c);  if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->face_set_count);      if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->buffer_layout_count); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->texture_count);       if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, h->unk5c); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, h->unk5d); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);       if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);       if (r != SF_OK) return r;
    if (h->version >= 0x20014u) {
        r = sf_binary_writer_write_i16(bw, (int16_t)h->unk68); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i16(bw, h->special_modifier); if (r != SF_OK) return r;
    } else {
        r = sf_binary_writer_write_i32(bw, h->unk68); if (r != SF_OK) return r;
    }
    r = sf_binary_writer_write_i32(bw, 0);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);        if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, h->unk74); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);        if (r != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

static sf_result_t flver2_write_node_strings(sf_binary_writer_t *bw, const sf_flver2_t *f) {
    for (int32_t i = 0; i < f->header.bone_count; i++) {
        char reserve[32];
        int written = snprintf(reserve, sizeof(reserve), "BoneNameOffset%zu", (size_t)i);
        if (written < 0 || (size_t)written >= sizeof(reserve)) return SF_ERR_INTERNAL;
        sf_result_t r = sf_binary_writer_fill_i32(bw, reserve,
                                                  (int32_t)sf_binary_writer_position(bw));
        if (r != SF_OK) return r;
        const char *name = f->nodes[i].name ? f->nodes[i].name : "";
        r = (f->header.unicode != 0) ? sf_binary_writer_write_utf16(bw, name, true)
                                    : sf_binary_writer_write_shift_jis(bw, name, true);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t flver2_write_to_writer(const sf_flver2_t *f, sf_binary_writer_t *bw) {
    SF_CHECK_ARG(f != NULL && bw != NULL);
    if (!flver2_version_supported(f->header.version)) return SF_ERR_UNSUPPORTED_VERSION;
    sf_binary_writer_set_big_endian(bw, false);

    sf_result_t r = flver2_write_header(bw, f); if (r != SF_OK) return r;
    for (int32_t i = 0; i < f->header.dummy_count; i++) {
        r = sfi_flver_dummy_write(bw, &f->dummies[i]); if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.material_count; i++) {
        r = sfi_flver2_material_write(bw, &f->header, &f->materials[i], (size_t)i);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.bone_count; i++) {
        r = sfi_flver_node_write(bw, &f->nodes[i], (size_t)i); if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.mesh_count; i++) {
        r = sfi_flver2_mesh_write(bw, &f->header, &f->meshes[i], (size_t)i);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.face_set_count; i++) {
        r = sfi_flver2_face_set_write(bw, &f->header, &f->face_sets[i],
                                      f->header.vertex_indices_size, (size_t)i);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.vertex_buffer_count; i++) {
        r = sfi_flver2_vertex_buffer_write(bw, &f->header, &f->vertex_buffers[i], (size_t)i);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.buffer_layout_count; i++) {
        r = sfi_flver2_buffer_layout_write(bw, &f->header, &f->buffer_layouts[i], (size_t)i);
        if (r != SF_OK) return r;
    }
    {
        size_t texture_index = 0;
        for (int32_t i = 0; i < f->header.material_count; i++) {
            r = sfi_flver2_material_write_textures(bw, &f->header, &f->materials[i],
                                                   (size_t)i, texture_index);
            if (r != SF_OK) return r;
            texture_index += f->materials[i].texture_count;
        }
    }
    if (f->header.version >= 0x2001Au) {
        r = sfi_flver2_skeleton_set_write(bw, &f->header, f->skeleton_set);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
    for (int32_t i = 0; i < f->header.buffer_layout_count; i++) {
        r = sfi_flver2_buffer_layout_write_members(bw, &f->header, &f->buffer_layouts[i],
                                                   (size_t)i);
        if (r != SF_OK) return r;
    }

    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
    int32_t *gx_offsets = NULL;
    if (f->gx_list_count > 0) {
        gx_offsets = (int32_t *)sf_xalloc(f->alloc, f->gx_list_count * sizeof(int32_t));
        if (!gx_offsets) return SF_ERR_OOM;
    }
    for (size_t i = 0; i < f->gx_list_count; i++) {
        gx_offsets[i] = (int32_t)sf_binary_writer_position(bw);
        for (size_t j = 0; j < f->gx_lists[i].count; j++) {
            const sf_flver2_gx_item_t *item = &f->gx_lists[i].items[j];
            r = sf_binary_writer_write_u32(bw, item->id);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            r = sf_binary_writer_write_u32(bw, item->unk04);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            r = sf_binary_writer_write_i32(bw, (int32_t)(item->data_size + 0x0C));
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            r = sf_binary_writer_write_bytes(bw, item->data, item->data_size);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
        }
        if (f->header.version >= 0x20010u) {
            r = sf_binary_writer_write_i32(bw, f->gx_lists[i].terminator_id);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            r = sf_binary_writer_write_i32(bw, 100);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            int32_t length_subtraction = (f->header.unk68 == 5) ? 0 : 0x0C;
            r = sf_binary_writer_write_i32(bw,
                f->gx_lists[i].terminator_length + length_subtraction);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
            r = sf_binary_writer_write_pattern(bw,
                (size_t)f->gx_lists[i].terminator_length, 0);
            if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
        }
    }
    for (int32_t i = 0; i < f->header.material_count; i++) {
        r = sfi_flver2_material_fill_gx_offset(bw, (size_t)i, f->materials[i].gx_index,
                                               gx_offsets, f->gx_list_count);
        if (r != SF_OK) { sf_xfree(f->alloc, gx_offsets); return r; }
    }
    sf_xfree(f->alloc, gx_offsets);

    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
    {
        size_t texture_index = 0;
        for (int32_t i = 0; i < f->header.material_count; i++) {
            r = sfi_flver2_material_write_strings(bw, &f->header, &f->materials[i],
                                                  (size_t)i, texture_index);
            if (r != SF_OK) return r;
            texture_index += f->materials[i].texture_count;
        }
    }

    r = sf_binary_writer_pad(bw, 0x10); if (r != SF_OK) return r;
    r = flver2_write_node_strings(bw, f); if (r != SF_OK) return r;

    int align = (f->header.version <= 0x2000Eu) ? 0x20 : 0x10;
    r = sf_binary_writer_pad(bw, align); if (r != SF_OK) return r;
    if (f->header.version == 0x2000Fu || f->header.version == 0x20010u) {
        r = sf_binary_writer_pad(bw, 0x20); if (r != SF_OK) return r;
    }

    int32_t data_start = (int32_t)sf_binary_writer_position(bw);
    r = sf_binary_writer_fill_i32(bw, "DataOffset", data_start); if (r != SF_OK) return r;
    for (int32_t i = 0; i < f->header.face_set_count; i++) {
        r = sfi_flver2_face_set_write_indices(bw, &f->face_sets[i], (size_t)i, data_start);
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < f->header.vertex_buffer_count; i++) {
        r = sf_binary_writer_pad(bw, align); if (r != SF_OK) return r;
        r = sfi_flver2_vertex_buffer_write_data(bw, &f->vertex_buffers[i], (size_t)i,
                                                data_start);
        if (r != SF_OK) return r;
    }
    r = sf_binary_writer_pad(bw, align); if (r != SF_OK) return r;
    r = sf_binary_writer_fill_i32(bw, "DataSize",
                                  (int32_t)sf_binary_writer_position(bw) - data_start);
    if (r != SF_OK) return r;
    if (f->header.version == 0x2000Fu || f->header.version == 0x20010u) {
        r = sf_binary_writer_pad(bw, 0x20); if (r != SF_OK) return r;
    }
    return sf_binary_writer_finish(bw);
}

sf_result_t sf_flver2_write_to_memory(const sf_flver2_t *f, void **out_bytes,
                                      size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG(f != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    a = sf_alloc_or_default(a);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, a);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, a);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = flver2_write_to_writer(f, bw);
    if (r == SF_OK) r = sf_ostream_detach_buffer(os, out_bytes, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_flver2_write_to_path(const sf_flver2_t *f, const wchar_t *path) {
    SF_CHECK_ARG(f != NULL && path != NULL);
    const sf_allocator_t *a = f->alloc;
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, a);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, a);
    if (r != SF_OK) { sf_ostream_close(os); return r; }
    r = flver2_write_to_writer(f, bw);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

void sf_flver2_destroy(sf_flver2_t *f) {
    if (!f) return;
    const sf_allocator_t *a = f->alloc;
    if (f->nodes) {
        for (int32_t i = 0; i < f->header.bone_count; i++) {
            sfi_flver_node_destroy_inplace(&f->nodes[i], a);
        }
    }
    if (f->materials) {
        for (int32_t i = 0; i < f->header.material_count; i++) {
            sfi_flver2_material_destroy_inplace(&f->materials[i], a);
        }
    }
    if (f->meshes) {
        for (int32_t i = 0; i < f->header.mesh_count; i++) {
            sfi_flver2_mesh_destroy_inplace(&f->meshes[i], a);
        }
    }
    if (f->face_sets) {
        for (int32_t i = 0; i < f->header.face_set_count; i++) {
            sfi_flver2_face_set_destroy_inplace(&f->face_sets[i], a);
        }
    }
    if (f->textures) {
        for (int32_t i = 0; i < f->header.texture_count; i++) {
            sfi_flver2_texture_destroy_inplace(&f->textures[i], a);
        }
    }
    if (f->vertex_buffers) {
        for (int32_t i = 0; i < f->header.vertex_buffer_count; i++) {
            sfi_flver2_vertex_buffer_destroy_inplace(&f->vertex_buffers[i], a);
        }
    }
    if (f->buffer_layouts) {
        for (int32_t i = 0; i < f->header.buffer_layout_count; i++) {
            sfi_flver2_buffer_layout_destroy_inplace(&f->buffer_layouts[i], a);
        }
    }
    for (size_t i = 0; i < f->gx_list_count; i++) {
        for (size_t j = 0; j < f->gx_lists[i].count; j++) {
            sf_xfree(a, f->gx_lists[i].items[j].data);
        }
        sf_xfree(a, f->gx_lists[i].items);
    }
    sfi_flver2_skeleton_set_destroy(f->skeleton_set, a);
    sf_xfree(a, f->dummies);
    sf_xfree(a, f->materials);
    sf_xfree(a, f->nodes);
    sf_xfree(a, f->meshes);
    sf_xfree(a, f->face_sets);
    sf_xfree(a, f->vertex_buffers);
    sf_xfree(a, f->buffer_layouts);
    sf_xfree(a, f->textures);
    sf_xfree(a, f->gx_lists);
    sf_xfree(a, f->gx_offsets_internal);
    sf_xfree(a, f);
}

uint32_t sf_flver2_header_version(const sf_flver2_t *f) { return f ? f->header.version : 0; }
bool sf_flver2_header_unicode(const sf_flver2_t *f) { return f ? f->header.unicode != 0 : false; }
sf_vec3_t sf_flver2_header_bounding_box_min(const sf_flver2_t *f) {
    sf_vec3_t zero = { 0, 0, 0 };
    return f ? f->header.bbox_min : zero;
}
sf_vec3_t sf_flver2_header_bounding_box_max(const sf_flver2_t *f) {
    sf_vec3_t zero = { 0, 0, 0 };
    return f ? f->header.bbox_max : zero;
}

size_t sf_flver2_dummy_count(const sf_flver2_t *f) {
    return (f && f->header.dummy_count > 0) ? (size_t)f->header.dummy_count : 0;
}
size_t sf_flver2_node_count(const sf_flver2_t *f) {
    return (f && f->header.bone_count > 0) ? (size_t)f->header.bone_count : 0;
}
size_t sf_flver2_material_count(const sf_flver2_t *f) {
    return (f && f->header.material_count > 0) ? (size_t)f->header.material_count : 0;
}
size_t sf_flver2_mesh_count(const sf_flver2_t *f) {
    return (f && f->header.mesh_count > 0) ? (size_t)f->header.mesh_count : 0;
}
size_t sf_flver2_buffer_layout_count(const sf_flver2_t *f) {
    return (f && f->header.buffer_layout_count > 0) ? (size_t)f->header.buffer_layout_count : 0;
}
size_t sf_flver2_vertex_buffer_count(const sf_flver2_t *f) {
    return (f && f->header.vertex_buffer_count > 0) ? (size_t)f->header.vertex_buffer_count : 0;
}
size_t sf_flver2_face_set_count(const sf_flver2_t *f) {
    return (f && f->header.face_set_count > 0) ? (size_t)f->header.face_set_count : 0;
}
size_t sf_flver2_texture_count(const sf_flver2_t *f) {
    return (f && f->header.texture_count > 0) ? (size_t)f->header.texture_count : 0;
}

const sf_flver_dummy_t *sf_flver2_dummy(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_dummy_count(f)) ? &f->dummies[i] : NULL;
}
const sf_flver_node_t *sf_flver2_node(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_node_count(f)) ? &f->nodes[i] : NULL;
}
const sf_flver2_material_t *sf_flver2_material(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_material_count(f)) ? &f->materials[i] : NULL;
}
const sf_flver2_mesh_t *sf_flver2_mesh(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_mesh_count(f)) ? &f->meshes[i] : NULL;
}
const sf_flver2_buffer_layout_t *sf_flver2_buffer_layout(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_buffer_layout_count(f)) ? &f->buffer_layouts[i] : NULL;
}
const sf_flver2_vertex_buffer_t *sf_flver2_vertex_buffer(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_vertex_buffer_count(f)) ? &f->vertex_buffers[i] : NULL;
}
const sf_flver2_face_set_t *sf_flver2_face_set(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_face_set_count(f)) ? &f->face_sets[i] : NULL;
}
const sf_flver2_texture_t *sf_flver2_texture(const sf_flver2_t *f, size_t i) {
    return (f && i < sf_flver2_texture_count(f)) ? &f->textures[i] : NULL;
}

const sf_flver2_gx_list_t *sf_flver2_gx_list(const sf_flver2_t *f, size_t mesh_index) {
    if (!f || mesh_index >= sf_flver2_mesh_count(f)) return NULL;
    int32_t material_index = f->meshes[mesh_index].material_index;
    if (material_index < 0 || (size_t)material_index >= sf_flver2_material_count(f)) return NULL;
    int32_t gx_index = f->materials[material_index].gx_index;
    return (gx_index >= 0 && (size_t)gx_index < f->gx_list_count) ? &f->gx_lists[gx_index] : NULL;
}
size_t sf_flver2_gx_list_item_count(const sf_flver2_gx_list_t *gx) { return gx ? gx->count : 0; }
const sf_flver2_gx_item_t *sf_flver2_gx_item(const sf_flver2_gx_list_t *gx, size_t i) {
    return (gx && i < gx->count) ? &gx->items[i] : NULL;
}
uint32_t sf_flver2_gx_item_id(const sf_flver2_gx_item_t *item) { return item ? item->id : 0; }
uint32_t sf_flver2_gx_item_unk04(const sf_flver2_gx_item_t *item) { return item ? item->unk04 : 0; }
const uint8_t *sf_flver2_gx_item_data(const sf_flver2_gx_item_t *item, size_t *out_size) {
    if (out_size) *out_size = item ? item->data_size : 0;
    return item ? item->data : NULL;
}

const sf_flver2_skeleton_set_t *sf_flver2_skeleton_set(const sf_flver2_t *f) {
    return f ? f->skeleton_set : NULL;
}
size_t sf_flver2_skeleton_set_base_count(const sf_flver2_skeleton_set_t *set) {
    return set ? set->base_bone_count : 0;
}
const sf_flver2_bone_t *sf_flver2_skeleton_set_base_bone(const sf_flver2_skeleton_set_t *set,
                                                         size_t i) {
    return (set && i < set->base_bone_count) ? &set->base_bones[i] : NULL;
}
size_t sf_flver2_skeleton_set_all_count(const sf_flver2_skeleton_set_t *set) {
    return set ? set->all_bone_count : 0;
}
const sf_flver2_bone_t *sf_flver2_skeleton_set_all_bone(const sf_flver2_skeleton_set_t *set,
                                                        size_t i) {
    return (set && i < set->all_bone_count) ? &set->all_bones[i] : NULL;
}
int16_t sf_flver2_bone_parent_index(const sf_flver2_bone_t *b) { return b ? b->parent_index : -1; }
int16_t sf_flver2_bone_first_child_index(const sf_flver2_bone_t *b) {
    return b ? b->first_child_index : -1;
}
int16_t sf_flver2_bone_next_sibling_index(const sf_flver2_bone_t *b) {
    return b ? b->next_sibling_index : -1;
}
int16_t sf_flver2_bone_previous_sibling_index(const sf_flver2_bone_t *b) {
    return b ? b->previous_sibling_index : -1;
}
int32_t sf_flver2_bone_node_index(const sf_flver2_bone_t *b) { return b ? b->node_index : -1; }

sf_result_t sf_flver2_decode_mesh(const sf_flver2_t *f, size_t mesh_index,
                                  sf_flver2_decoded_mesh_t *out,
                                  const sf_allocator_t *a) {
    (void)f; (void)mesh_index; (void)out; (void)a;
    return SF_ERR_UNSUPPORTED_VERSION;
}
void sf_flver2_decoded_mesh_free(sf_flver2_decoded_mesh_t *m, const sf_allocator_t *a) {
    if (!m) return;
    sf_xfree(a, m->positions);
    sf_xfree(a, m->normals);
    sf_xfree(a, m->tangents);
    sf_xfree(a, m->bitangents);
    for (size_t i = 0; i < sizeof(m->uvs) / sizeof(m->uvs[0]); i++) sf_xfree(a, m->uvs[i]);
    for (size_t i = 0; i < sizeof(m->colors) / sizeof(m->colors[0]); i++) sf_xfree(a, m->colors[i]);
    sf_xfree(a, m->bone_indices);
    sf_xfree(a, m->bone_weights);
    sf_xfree(a, m->indices);
    memset(m, 0, sizeof(*m));
}

/* Material / Texture / TakeTextures — implemented in src/geom/flver2_material.c (T13). */
/* SkeletonSet / Bone — implemented in src/geom/flver2_skeleton.c (T18). */

/* Mesh — implemented in src/geom/flver2_mesh.c (T14). */
