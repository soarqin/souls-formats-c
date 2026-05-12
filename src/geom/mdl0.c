/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mdl.h"

#include "internal/mdl_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MDL0_NODE_SIZE 0x50u
#define MDL0_VERTEX_A_SIZE 0x24u
#define MDL0_VERTEX_B_SIZE 0x28u
#define MDL0_VERTEX_C_SIZE 0x30u
#define MDL0_STRUCT6_SIZE 0x20u
#define MDL0_MATERIAL_SIZE 0x70u

static bool mdl0_count_ok(int32_t count, size_t elem_size) {
    return count >= 0 && (count == 0 || (size_t)count <= SIZE_MAX / elem_size);
}

static void *mdl0_zalloc_array(const sf_allocator_t *a, int32_t count, size_t elem_size) {
    if (!mdl0_count_ok(count, elem_size)) return NULL;
    if (count == 0) return NULL;
    void *p = sf_xalloc(a, (size_t)count * elem_size);
    if (p) memset(p, 0, (size_t)count * elem_size);
    return p;
}

static sf_result_t mdl0_read_blob(sf_binary_reader_t *br, int32_t offset, int32_t count,
                                  size_t elem_size, sf_mdl_blob_t *out,
                                  const sf_allocator_t *a) {
    if (!mdl0_count_ok(count, elem_size) || offset < 0) return SF_ERR_OUT_OF_RANGE;
    out->count = (size_t)count;
    out->size = (size_t)count * elem_size;
    out->bytes = NULL;
    if (out->size == 0) return SF_OK;
    out->bytes = (uint8_t *)sf_xalloc(a, out->size);
    if (!out->bytes) return SF_ERR_OOM;
    return sf_binary_reader_get_bytes(br, offset, out->bytes, out->size);
}

static sf_result_t mdl0_read_textures(sf_binary_reader_t *br, int32_t offset, int32_t count,
                                      char ***out, const sf_allocator_t *a) {
    if (!mdl0_count_ok(count, sizeof(char *)) || offset < 0) return SF_ERR_OUT_OF_RANGE;
    *out = (char **)mdl0_zalloc_array(a, count, sizeof(char *));
    if (count > 0 && !*out) return SF_ERR_OOM;
    sf_result_t r = sf_binary_reader_step_in(br, offset);
    if (r != SF_OK) return r;
    for (int32_t i = 0; i < count; i++) {
        r = sf_binary_reader_read_shift_jis(br, &(*out)[i], NULL);
        if (r != SF_OK) break;
    }
    sf_result_t out_r = sf_binary_reader_step_out(br);
    return r != SF_OK ? r : out_r;
}

static void mdl0_free_blob(sf_mdl_blob_t *b, const sf_allocator_t *a) {
    if (!b) return;
    sf_xfree(a, b->bytes);
    memset(b, 0, sizeof(*b));
}

static sf_result_t mdl0_populate(sf_mdl0_t **out, sf_binary_reader_t *br,
                                 const sf_allocator_t *a) {
    int32_t file_size = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &file_size); if (r != SF_OK) return r;
    (void)file_size;
    sf_mdl0_t *m = (sf_mdl0_t *)sf_xalloc(a, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    memset(m, 0, sizeof(*m));
    m->alloc = a;

    if ((r = sf_binary_reader_read_i32(br, &m->unk04)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->unk08)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->face_count)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->node_count)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->index_count)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->vertex_count_a)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->vertex_count_b)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->vertex_count_c)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->count6)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->material_count)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &m->texture_count)) != SF_OK) goto fail;
    if (!mdl0_count_ok(m->index_count, sizeof(uint16_t))) { r = SF_ERR_OUT_OF_RANGE; goto fail; }

    int32_t bones_offset = 0, indices_offset = 0, va_offset = 0, vb_offset = 0;
    int32_t vc_offset = 0, s6_offset = 0, mat_offset = 0, tex_offset = 0;
    if ((r = sf_binary_reader_read_i32(br, &bones_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &indices_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &va_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &vb_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &vc_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &s6_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &mat_offset)) != SF_OK) goto fail;
    if ((r = sf_binary_reader_read_i32(br, &tex_offset)) != SF_OK) goto fail;

    m->indices = (uint16_t *)mdl0_zalloc_array(a, m->index_count, sizeof(uint16_t));
    if (m->index_count > 0 && !m->indices) { r = SF_ERR_OOM; goto fail; }
    if (m->index_count > 0 && (r = sf_binary_reader_get_u16s(br, indices_offset,
            (size_t)m->index_count, m->indices)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, bones_offset, m->node_count, MDL0_NODE_SIZE, &m->nodes, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, va_offset, m->vertex_count_a, MDL0_VERTEX_A_SIZE, &m->vertices_a, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, vb_offset, m->vertex_count_b, MDL0_VERTEX_B_SIZE, &m->vertices_b, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, vc_offset, m->vertex_count_c, MDL0_VERTEX_C_SIZE, &m->vertices_c, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, s6_offset, m->count6, MDL0_STRUCT6_SIZE, &m->struct6s, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_blob(br, mat_offset, m->material_count, MDL0_MATERIAL_SIZE, &m->materials, a)) != SF_OK) goto fail;
    if ((r = mdl0_read_textures(br, tex_offset, m->texture_count, &m->textures, a)) != SF_OK) goto fail;
    *out = m;
    return SF_OK;
fail:
    sf_mdl0_destroy(m);
    return r;
}

sf_result_t sf_mdl0_read_from_memory(sf_mdl0_t **out, const void *bytes, size_t size,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (bytes != NULL || size == 0));
    *out = NULL;
    a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, bytes, size, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r == SF_OK) r = mdl0_populate(out, br, a);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_mdl0_read_from_path(sf_mdl0_t **out, const wchar_t *path, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    a = sf_alloc_or_default(a);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r == SF_OK) r = mdl0_populate(out, br, a);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

void sf_mdl0_destroy(sf_mdl0_t *m) {
    if (!m) return;
    const sf_allocator_t *a = m->alloc;
    for (int32_t i = 0; i < m->texture_count; i++) sf_xfree(a, m->textures[i]);
    sf_xfree(a, m->textures);
    sf_xfree(a, m->indices);
    mdl0_free_blob(&m->nodes, a);
    mdl0_free_blob(&m->vertices_a, a);
    mdl0_free_blob(&m->vertices_b, a);
    mdl0_free_blob(&m->vertices_c, a);
    mdl0_free_blob(&m->struct6s, a);
    mdl0_free_blob(&m->materials, a);
    sf_xfree(a, m);
}

int32_t sf_mdl0_unk04(const sf_mdl0_t *m) { return m ? m->unk04 : 0; }
int32_t sf_mdl0_unk08(const sf_mdl0_t *m) { return m ? m->unk08 : 0; }
size_t sf_mdl0_node_count(const sf_mdl0_t *m) { return (m && m->node_count > 0) ? (size_t)m->node_count : 0; }
size_t sf_mdl0_index_count(const sf_mdl0_t *m) { return (m && m->index_count > 0) ? (size_t)m->index_count : 0; }
size_t sf_mdl0_vertex_count_a(const sf_mdl0_t *m) { return (m && m->vertex_count_a > 0) ? (size_t)m->vertex_count_a : 0; }
size_t sf_mdl0_vertex_count_b(const sf_mdl0_t *m) { return (m && m->vertex_count_b > 0) ? (size_t)m->vertex_count_b : 0; }
size_t sf_mdl0_vertex_count_c(const sf_mdl0_t *m) { return (m && m->vertex_count_c > 0) ? (size_t)m->vertex_count_c : 0; }
size_t sf_mdl0_material_count(const sf_mdl0_t *m) { return (m && m->material_count > 0) ? (size_t)m->material_count : 0; }
size_t sf_mdl0_texture_count(const sf_mdl0_t *m) { return (m && m->texture_count > 0) ? (size_t)m->texture_count : 0; }
