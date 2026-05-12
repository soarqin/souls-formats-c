/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mdl4.h"

#include "internal/mdl4_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static bool version_supported(uint32_t v) { return v == 0x40001u || v == 0x40002u; }

static size_t vertex_size(uint32_t version, uint8_t fmt) {
    if (version == 0x40001u) {
        if (fmt == 0) return 0x40u;
        if (fmt == 1) return 0x54u;
        if (fmt == 2) return 0x3Cu;
    } else if (version == 0x40002u) {
        if (fmt == 0) return 0x28u;
        if (fmt == 1) return 0x34u;
    }
    return 0u;
}

static sf_result_t read_header(sf_binary_reader_t *br, sf_mdl4_header_t *h) {
    sf_binary_reader_set_big_endian(br, true);
    sf_result_t r = sf_binary_reader_assert_ascii(br, "MDL4"); if (r != SF_OK) return r;
    r = sf_binary_reader_read_u32(br, &h->version); if (r != SF_OK) return r;
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
    r = sf_binary_reader_assert_pattern(br, 0x3C, 0); if (r != SF_OK) return r;
    if (!count_valid(h->dummy_count, sizeof(sf_mdl4_dummy_t)) ||
        !count_valid(h->material_count, sizeof(sf_mdl4_material_t)) ||
        !count_valid(h->bone_count, sizeof(sf_mdl4_node_t)) ||
        !count_valid(h->mesh_count, sizeof(sf_mdl4_mesh_t))) return SF_ERR_OUT_OF_RANGE;
    return SF_OK;
}

static sf_result_t read_dummy(sf_binary_reader_t *br, sf_mdl4_dummy_t *d) {
    sf_result_t r;
    if ((r = sf_binary_reader_read_vec3(br, &d->position)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &d->forward)) != SF_OK) return r;
    uint8_t c[4];
    if ((r = sf_binary_reader_read_bytes(br, c, 4)) != SF_OK) return r;
    d->color = ((uint32_t)c[0] << 24) | ((uint32_t)c[1] << 16) | ((uint32_t)c[2] << 8) | c[3];
    if ((r = sf_binary_reader_read_i16(br, &d->reference_id)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &d->parent_bone_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &d->attach_bone_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &d->unk22)) != SF_OK) return r;
    return sf_binary_reader_assert_pattern(br, 0x0C, 0);
}

static sf_result_t write_dummy(sf_binary_writer_t *bw, const sf_mdl4_dummy_t *d) {
    sf_result_t r;
    if ((r = sf_binary_writer_write_vec3(bw, d->position)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, d->forward)) != SF_OK) return r;
    uint8_t c[4] = { (uint8_t)(d->color >> 24), (uint8_t)(d->color >> 16),
                     (uint8_t)(d->color >> 8), (uint8_t)d->color };
    if ((r = sf_binary_writer_write_bytes(bw, c, sizeof(c))) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, d->reference_id)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, d->parent_bone_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, d->attach_bone_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, d->unk22)) != SF_OK) return r;
    return sf_binary_writer_write_pattern(bw, 0x0C, 0);
}

static sf_result_t read_param(sf_binary_reader_t *br, sf_mdl4_material_param_t *p) {
    int64_t start = sf_binary_reader_position(br);
    uint8_t t = 0;
    sf_result_t r = sf_binary_reader_read_u8(br, &t); if (r != SF_OK) return r;
    if (!(t == 0 || t == 1 || t == 4 || t == 5)) return SF_ERR_UNSUPPORTED_VERSION;
    p->type = (sf_mdl4_param_type_t)t;
    if ((r = sf_binary_reader_read_fix_str(br, 0x1F, &p->name, NULL)) != SF_OK) return r;
    switch (p->type) {
        case SF_MDL4_PARAM_TYPE_INT: r = sf_binary_reader_read_i32(br, &p->int_value); break;
        case SF_MDL4_PARAM_TYPE_FLOAT: r = sf_binary_reader_read_f32(br, &p->float_value); break;
        case SF_MDL4_PARAM_TYPE_FLOAT4: r = sf_binary_reader_read_f32s(br, 4, p->float4_value); break;
        case SF_MDL4_PARAM_TYPE_STRING: r = sf_binary_reader_read_shift_jis(br, &p->string_value, NULL); break;
    }
    if (r != SF_OK) return r;
    return sf_istream_seek(sf_binary_reader_stream(br), start + 0x40);
}

static sf_result_t write_param(sf_binary_writer_t *bw, const sf_mdl4_material_param_t *p) {
    int64_t start = sf_binary_writer_position(bw);
    sf_result_t r = sf_binary_writer_write_u8(bw, (uint8_t)p->type); if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, p->name ? p->name : "", 0x1F, 0); if (r != SF_OK) return r;
    switch (p->type) {
        case SF_MDL4_PARAM_TYPE_INT: r = sf_binary_writer_write_i32(bw, p->int_value); break;
        case SF_MDL4_PARAM_TYPE_FLOAT: r = sf_binary_writer_write_f32(bw, p->float_value); break;
        case SF_MDL4_PARAM_TYPE_FLOAT4: r = sf_binary_writer_write_f32s(bw, 4, p->float4_value); break;
        case SF_MDL4_PARAM_TYPE_STRING: r = sf_binary_writer_write_shift_jis(bw, p->string_value ? p->string_value : "", true); break;
    }
    if (r != SF_OK) return r;
    int64_t pos = sf_binary_writer_position(bw);
    if (pos > start + 0x40) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_write_pattern(bw, (size_t)(start + 0x40 - pos), 0);
}

static sf_result_t read_material(sf_binary_reader_t *br, sf_mdl4_material_t *m,
                                 const sf_allocator_t *a) {
    sf_result_t r = sf_binary_reader_read_fix_str(br, 0x1F, &m->name, NULL); if (r != SF_OK) return r;
    if ((r = sf_binary_reader_read_fix_str(br, 0x1D, &m->shader, NULL)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &m->unk3c)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &m->unk3d)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &m->unk3e)) != SF_OK) return r;
    uint8_t param_count = 0;
    if ((r = sf_binary_reader_read_u8(br, &param_count)) != SF_OK) return r;
    int64_t params_offset = sf_binary_reader_position(br);
    m->param_count = param_count;
    m->params = (sf_mdl4_material_param_t *)alloc_array(a, param_count, sizeof(*m->params));
    if (param_count > 0 && !m->params) return SF_ERR_OOM;
    for (uint8_t i = 0; i < param_count; i++) {
        r = read_param(br, &m->params[i]); if (r != SF_OK) return r;
    }
    return sf_istream_seek(sf_binary_reader_stream(br), params_offset + 0x800);
}

static sf_result_t write_material(sf_binary_writer_t *bw, const sf_mdl4_material_t *m) {
    if (m->param_count > UINT8_MAX) return SF_ERR_OUT_OF_RANGE;
    sf_result_t r = sf_binary_writer_write_fix_str(bw, m->name ? m->name : "", 0x1F, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_fix_str(bw, m->shader ? m->shader : "", 0x1D, 0); if (r != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, m->unk3c)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, m->unk3d)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, m->unk3e)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, (uint8_t)m->param_count)) != SF_OK) return r;
    int64_t params_offset = sf_binary_writer_position(bw);
    for (size_t i = 0; i < m->param_count; i++) {
        r = write_param(bw, &m->params[i]); if (r != SF_OK) return r;
    }
    int64_t pos = sf_binary_writer_position(bw);
    if (pos > params_offset + 0x800) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_write_pattern(bw, (size_t)(params_offset + 0x800 - pos), 0);
}

static sf_result_t read_node(sf_binary_reader_t *br, sf_mdl4_node_t *n) {
    sf_result_t r = sf_binary_reader_read_fix_str(br, 0x20, &n->name, NULL); if (r != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &n->translation)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &n->rotation)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &n->scale)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &n->bbox_min)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec3(br, &n->bbox_max)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &n->parent_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &n->first_child_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &n->next_sibling_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16(br, &n->previous_sibling_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0)) != SF_OK) return r;
    return sf_binary_reader_read_i16s(br, 16, n->unk_indices);
}

static sf_result_t write_node(sf_binary_writer_t *bw, const sf_mdl4_node_t *n) {
    sf_result_t r = sf_binary_writer_write_fix_str(bw, n->name ? n->name : "", 0x20, 0); if (r != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, n->translation)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, n->rotation)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, n->scale)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, n->bbox_min)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, n->bbox_max)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, n->parent_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, n->first_child_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, n->next_sibling_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, n->previous_sibling_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, 0)) != SF_OK) return r;
    return sf_binary_writer_write_i16s(bw, 16, n->unk_indices);
}

static sf_result_t read_mesh(sf_binary_reader_t *br, const sf_mdl4_header_t *h,
                             sf_mdl4_mesh_t *m, const sf_allocator_t *a) {
    sf_result_t r;
    if ((r = sf_binary_reader_read_u8(br, &m->vertex_format)) != SF_OK) return r;
    if (!(m->vertex_format == 0 || m->vertex_format == 1 || m->vertex_format == 2)) return SF_ERR_UNSUPPORTED_VERSION;
    if ((r = sf_binary_reader_read_u8(br, &m->material_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_bool(br, &m->unk02)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_bool(br, &m->unk03)) != SF_OK) return r;
    uint16_t index_count = 0;
    if ((r = sf_binary_reader_read_u16(br, &index_count)) != SF_OK) return r;
    m->index_count = index_count;
    if ((r = sf_binary_reader_read_i16(br, &m->unk08)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i16s(br, 28, m->bone_indices)) != SF_OK) return r;
    int32_t index_len = 0, index_off = 0, buffer_len = 0, buffer_off = 0;
    if ((r = sf_binary_reader_read_i32(br, &index_len)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &index_off)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &buffer_len)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &buffer_off)) != SF_OK) return r;
    if (index_len < 0 || buffer_len < 0) return SF_ERR_OUT_OF_RANGE;
    if (m->vertex_format == 2) {
        for (size_t i = 0; i < 16; i++) {
            int32_t len = 0, off = 0;
            if ((r = sf_binary_reader_read_i32(br, &len)) != SF_OK) return r;
            if ((r = sf_binary_reader_read_i32(br, &off)) != SF_OK) return r;
            if (len < 0 || off < 0) return SF_ERR_OUT_OF_RANGE;
            m->unk_block_sizes[i] = (size_t)len;
            if (len > 0) {
                m->unk_blocks[i] = (uint8_t *)sf_xalloc(a, (size_t)len);
                if (!m->unk_blocks[i]) return SF_ERR_OOM;
                if ((r = sf_binary_reader_get_bytes(br, h->data_offset + off, m->unk_blocks[i], (size_t)len)) != SF_OK) return r;
            }
        }
    }
    m->indices = (uint16_t *)alloc_array(a, (int32_t)m->index_count, sizeof(uint16_t));
    if (m->index_count > 0 && !m->indices) return SF_ERR_OOM;
    if (m->index_count > 0 && (r = sf_binary_reader_get_u16s(br, h->data_offset + index_off,
            m->index_count, m->indices)) != SF_OK) return r;
    size_t vs = vertex_size(h->version, m->vertex_format);
    if (vs == 0) return SF_ERR_UNSUPPORTED_VERSION;
    m->vertex_bytes_size = (size_t)buffer_len;
    m->vertex_count = m->vertex_bytes_size / vs;
    if (buffer_len > 0) {
        m->vertex_bytes = (uint8_t *)sf_xalloc(a, (size_t)buffer_len);
        if (!m->vertex_bytes) return SF_ERR_OOM;
        if ((r = sf_binary_reader_get_bytes(br, h->data_offset + buffer_off, m->vertex_bytes, (size_t)buffer_len)) != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t write_mesh_header(sf_binary_writer_t *bw, const sf_mdl4_mesh_t *m, size_t index) {
    if (m->index_count > UINT16_MAX) return SF_ERR_OUT_OF_RANGE;
    char n[64];
    sf_result_t r;
    if ((r = sf_binary_writer_write_u8(bw, m->vertex_format)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, m->material_index)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_bool(bw, m->unk02)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_bool(bw, m->unk03)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_u16(bw, (uint16_t)m->index_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16(bw, m->unk08)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i16s(bw, 28, m->bone_indices)) != SF_OK) return r;
    snprintf(n, sizeof(n), "MDL4IndexLength%u", (unsigned)index);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
    snprintf(n, sizeof(n), "MDL4IndexOffset%u", (unsigned)index);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
    snprintf(n, sizeof(n), "MDL4BufferLength%u", (unsigned)index);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
    snprintf(n, sizeof(n), "MDL4BufferOffset%u", (unsigned)index);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
    if (m->vertex_format == 2) {
        for (size_t i = 0; i < 16; i++) {
            snprintf(n, sizeof(n), "MDL4UnkBlockLength%u_%u", (unsigned)index, (unsigned)i);
            SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
            snprintf(n, sizeof(n), "MDL4UnkBlockOffset%u_%u", (unsigned)index, (unsigned)i);
            SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, n), return r);
        }
    }
    return SF_OK;
}

static int32_t pos32(sf_binary_writer_t *bw) {
    int64_t p = sf_binary_writer_position(bw);
    return (p < 0 || p > INT32_MAX) ? -1 : (int32_t)p;
}

static sf_result_t populate(sf_mdl4_t **out, sf_binary_reader_t *br, const sf_allocator_t *a) {
    sf_mdl4_t *m = (sf_mdl4_t *)sf_xalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    memset(m, 0, sizeof(*m));
    m->alloc = a;
    sf_result_t r = read_header(br, &m->header);
    if (r != SF_OK) { sf_mdl4_destroy(m); return r; }
    m->dummies = (sf_mdl4_dummy_t *)alloc_array(a, m->header.dummy_count, sizeof(*m->dummies));
    m->materials = (sf_mdl4_material_t *)alloc_array(a, m->header.material_count, sizeof(*m->materials));
    m->nodes = (sf_mdl4_node_t *)alloc_array(a, m->header.bone_count, sizeof(*m->nodes));
    m->meshes = (sf_mdl4_mesh_t *)alloc_array(a, m->header.mesh_count, sizeof(*m->meshes));
    if ((m->header.dummy_count > 0 && !m->dummies) || (m->header.material_count > 0 && !m->materials) ||
        (m->header.bone_count > 0 && !m->nodes) || (m->header.mesh_count > 0 && !m->meshes)) {
        sf_mdl4_destroy(m); return SF_ERR_OOM;
    }
    for (int32_t i = 0; i < m->header.dummy_count; i++) { r = read_dummy(br, &m->dummies[i]); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < m->header.material_count; i++) { r = read_material(br, &m->materials[i], a); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < m->header.bone_count; i++) { r = read_node(br, &m->nodes[i]); if (r != SF_OK) goto fail; }
    for (int32_t i = 0; i < m->header.mesh_count; i++) { r = read_mesh(br, &m->header, &m->meshes[i], a); if (r != SF_OK) goto fail; }
    *out = m;
    return SF_OK;
fail:
    sf_mdl4_destroy(m);
    return r;
}

sf_result_t sf_mdl4_read_from_memory(sf_mdl4_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (bytes != NULL || size == 0));
    *out = NULL; a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL; sf_result_t r = sf_istream_open_memory(&is, bytes, size, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL; r = sf_binary_reader_create(&br, is, true, a);
    if (r == SF_OK) r = populate(out, br, a);
    sf_binary_reader_destroy(br); sf_istream_close(is); return r;
}

sf_result_t sf_mdl4_read_from_path(sf_mdl4_t **out, const wchar_t *path, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL; a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL; sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL; r = sf_binary_reader_create(&br, is, true, a);
    if (r == SF_OK) r = populate(out, br, a);
    sf_binary_reader_destroy(br); sf_istream_close(is); return r;
}

static sf_result_t write_to_writer(const sf_mdl4_t *m, sf_binary_writer_t *bw) {
    if (!version_supported(m->header.version)) return SF_ERR_UNSUPPORTED_VERSION;
    sf_binary_writer_set_big_endian(bw, true);
    sf_result_t r = sf_binary_writer_write_bytes(bw, "MDL4", 4); if (r != SF_OK) return r;
    if ((r = sf_binary_writer_write_u32(bw, m->header.version)) != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "MDL4DataOffset"), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "MDL4DataLength"), return r);
    int32_t dc = (int32_t)sf_mdl4_dummy_count(m), mc = (int32_t)sf_mdl4_material_count(m);
    int32_t nc = (int32_t)sf_mdl4_node_count(m), meshc = (int32_t)sf_mdl4_mesh_count(m);
    if ((r = sf_binary_writer_write_i32(bw, dc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, mc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, nc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, meshc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, meshc)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, m->header.bbox_min)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_vec3(bw, m->header.bbox_max)) != SF_OK) return r;
    int32_t face_count = 0;
    for (int32_t i = 0; i < meshc; i++) if (m->meshes[i].index_count >= 3) face_count += (int32_t)(m->meshes[i].index_count - 2);
    if ((r = sf_binary_writer_write_i32(bw, face_count)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_i32(bw, face_count * 3)) != SF_OK) return r;
    if ((r = sf_binary_writer_write_pattern(bw, 0x3C, 0)) != SF_OK) return r;
    for (int32_t i = 0; i < dc; i++) { r = write_dummy(bw, &m->dummies[i]); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < mc; i++) { r = write_material(bw, &m->materials[i]); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < nc; i++) { r = write_node(bw, &m->nodes[i]); if (r != SF_OK) return r; }
    for (int32_t i = 0; i < meshc; i++) { r = write_mesh_header(bw, &m->meshes[i], (size_t)i); if (r != SF_OK) return r; }
    int32_t data_start = pos32(bw); if (data_start < 0) return SF_ERR_OUT_OF_RANGE;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, "MDL4DataOffset", data_start), return r);
    char n[64];
    for (int32_t i = 0; i < meshc; i++) {
        int32_t rel = pos32(bw) - data_start;
        snprintf(n, sizeof(n), "MDL4IndexOffset%u", (unsigned)i);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, rel), return r);
        r = sf_binary_writer_write_u16s(bw, m->meshes[i].index_count, m->meshes[i].indices); if (r != SF_OK) return r;
        int32_t end_rel = pos32(bw) - data_start;
        snprintf(n, sizeof(n), "MDL4IndexLength%u", (unsigned)i);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, end_rel - rel), return r);
        rel = pos32(bw) - data_start;
        snprintf(n, sizeof(n), "MDL4BufferOffset%u", (unsigned)i);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, rel), return r);
        r = sf_binary_writer_write_bytes(bw, m->meshes[i].vertex_bytes, m->meshes[i].vertex_bytes_size); if (r != SF_OK) return r;
        end_rel = pos32(bw) - data_start;
        snprintf(n, sizeof(n), "MDL4BufferLength%u", (unsigned)i);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, end_rel - rel), return r);
        if (m->meshes[i].vertex_format == 2) {
            for (size_t j = 0; j < 16; j++) {
                rel = pos32(bw) - data_start;
                snprintf(n, sizeof(n), "MDL4UnkBlockOffset%u_%u", (unsigned)i, (unsigned)j);
                SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, rel), return r);
                r = sf_binary_writer_write_bytes(bw, m->meshes[i].unk_blocks[j], m->meshes[i].unk_block_sizes[j]); if (r != SF_OK) return r;
                snprintf(n, sizeof(n), "MDL4UnkBlockLength%u_%u", (unsigned)i, (unsigned)j);
                SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, n, (int32_t)m->meshes[i].unk_block_sizes[j]), return r);
            }
        }
    }
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, "MDL4DataLength", pos32(bw) - data_start), return r);
    return sf_binary_writer_finish(bw);
}

sf_result_t sf_mdl4_write_to_memory(const sf_mdl4_t *m, void **out_bytes, size_t *out_size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(m != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0; a = sf_alloc_or_default(a);
    sf_ostream_t *os = NULL; sf_result_t r = sf_ostream_open_memory(&os, a); if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL; r = sf_binary_writer_create(&bw, os, true, a);
    if (r == SF_OK) r = write_to_writer(m, bw);
    if (r == SF_OK) r = sf_ostream_detach_buffer(os, out_bytes, out_size);
    sf_binary_writer_destroy(bw); sf_ostream_close(os); return r;
}

sf_result_t sf_mdl4_write_to_path(const sf_mdl4_t *m, const wchar_t *path) {
    SF_CHECK_ARG(m != NULL && path != NULL);
    sf_ostream_t *os = NULL; sf_result_t r = sf_ostream_open_wfile(&os, path, m->alloc); if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL; r = sf_binary_writer_create(&bw, os, true, m->alloc);
    if (r == SF_OK) r = write_to_writer(m, bw);
    sf_binary_writer_destroy(bw); sf_ostream_close(os); return r;
}

void sf_mdl4_destroy(sf_mdl4_t *m) {
    if (!m) return;
    const sf_allocator_t *a = m->alloc;
    for (int32_t i = 0; i < m->header.material_count; i++) {
        sf_xfree(a, m->materials[i].name); sf_xfree(a, m->materials[i].shader);
        for (size_t j = 0; j < m->materials[i].param_count; j++) {
            sf_xfree(a, m->materials[i].params[j].name);
            sf_xfree(a, m->materials[i].params[j].string_value);
        }
        sf_xfree(a, m->materials[i].params);
    }
    for (int32_t i = 0; i < m->header.bone_count; i++) sf_xfree(a, m->nodes[i].name);
    for (int32_t i = 0; i < m->header.mesh_count; i++) {
        sf_xfree(a, m->meshes[i].indices); sf_xfree(a, m->meshes[i].vertex_bytes);
        for (size_t j = 0; j < 16; j++) sf_xfree(a, m->meshes[i].unk_blocks[j]);
    }
    sf_xfree(a, m->dummies); sf_xfree(a, m->materials); sf_xfree(a, m->nodes); sf_xfree(a, m->meshes); sf_xfree(a, m);
}

uint32_t sf_mdl4_header_version(const sf_mdl4_t *m) { return m ? m->header.version : 0; }
sf_vec3_t sf_mdl4_header_bounding_box_min(const sf_mdl4_t *m) { sf_vec3_t z = {0,0,0}; return m ? m->header.bbox_min : z; }
sf_vec3_t sf_mdl4_header_bounding_box_max(const sf_mdl4_t *m) { sf_vec3_t z = {0,0,0}; return m ? m->header.bbox_max : z; }
size_t sf_mdl4_dummy_count(const sf_mdl4_t *m) { return (m && m->header.dummy_count > 0) ? (size_t)m->header.dummy_count : 0; }
size_t sf_mdl4_material_count(const sf_mdl4_t *m) { return (m && m->header.material_count > 0) ? (size_t)m->header.material_count : 0; }
size_t sf_mdl4_node_count(const sf_mdl4_t *m) { return (m && m->header.bone_count > 0) ? (size_t)m->header.bone_count : 0; }
size_t sf_mdl4_mesh_count(const sf_mdl4_t *m) { return (m && m->header.mesh_count > 0) ? (size_t)m->header.mesh_count : 0; }
const sf_mdl4_material_t *sf_mdl4_material(const sf_mdl4_t *m, size_t i) { return (m && i < sf_mdl4_material_count(m)) ? &m->materials[i] : NULL; }
const sf_mdl4_node_t *sf_mdl4_node(const sf_mdl4_t *m, size_t i) { return (m && i < sf_mdl4_node_count(m)) ? &m->nodes[i] : NULL; }
const sf_mdl4_mesh_t *sf_mdl4_mesh(const sf_mdl4_t *m, size_t i) { return (m && i < sf_mdl4_mesh_count(m)) ? &m->meshes[i] : NULL; }
const char *sf_mdl4_material_name(const sf_mdl4_material_t *m) { return m ? m->name : NULL; }
const char *sf_mdl4_material_shader(const sf_mdl4_material_t *m) { return m ? m->shader : NULL; }
size_t sf_mdl4_material_param_count(const sf_mdl4_material_t *m) { return m ? m->param_count : 0; }
const sf_mdl4_material_param_t *sf_mdl4_material_param(const sf_mdl4_material_t *m, size_t i) { return (m && i < m->param_count) ? &m->params[i] : NULL; }
sf_mdl4_param_type_t sf_mdl4_material_param_type(const sf_mdl4_material_param_t *p) { return p ? p->type : SF_MDL4_PARAM_TYPE_INT; }
const char *sf_mdl4_material_param_name(const sf_mdl4_material_param_t *p) { return p ? p->name : NULL; }
const char *sf_mdl4_node_name(const sf_mdl4_node_t *n) { return n ? n->name : NULL; }
uint8_t sf_mdl4_mesh_vertex_format(const sf_mdl4_mesh_t *m) { return m ? m->vertex_format : 0; }
uint8_t sf_mdl4_mesh_material_index(const sf_mdl4_mesh_t *m) { return m ? m->material_index : 0; }
size_t sf_mdl4_mesh_index_count(const sf_mdl4_mesh_t *m) { return m ? m->index_count : 0; }
uint16_t sf_mdl4_mesh_index(const sf_mdl4_mesh_t *m, size_t i) { return (m && i < m->index_count) ? m->indices[i] : 0; }
size_t sf_mdl4_mesh_vertex_count(const sf_mdl4_mesh_t *m) { return m ? m->vertex_count : 0; }
const uint8_t *sf_mdl4_mesh_vertex_bytes(const sf_mdl4_mesh_t *m, size_t *out_size) { if (out_size) *out_size = m ? m->vertex_bytes_size : 0; return m ? m->vertex_bytes : NULL; }
