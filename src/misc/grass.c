/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_grass.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

struct sf_grass {
    const sf_allocator_t *alloc;
    sf_grass_volume_t *volumes;
    size_t volume_count;
    size_t volume_cap;
    sf_grass_vertex_t *vertices;
    size_t vertex_count;
    size_t vertex_cap;
    sf_grass_face_t *faces;
    size_t face_count;
    size_t face_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t grass_push_volume(sf_grass_t *g, sf_grass_volume_t v) {
    if (g->volume_count >= g->volume_cap) {
        size_t new_cap = g->volume_cap == 0 ? 8u : g->volume_cap * 2u;
        sf_grass_volume_t *nv = (sf_grass_volume_t *)sf_xalloc(g->alloc, new_cap * sizeof(*nv));
        if (!nv) return SF_ERR_OOM;
        if (g->volumes) {
            memcpy(nv, g->volumes, g->volume_count * sizeof(*nv));
            sf_xfree(g->alloc, g->volumes);
        }
        g->volumes = nv;
        g->volume_cap = new_cap;
    }
    g->volumes[g->volume_count++] = v;
    return SF_OK;
}

static sf_result_t grass_push_vertex(sf_grass_t *g, sf_grass_vertex_t v) {
    if (g->vertex_count >= g->vertex_cap) {
        size_t new_cap = g->vertex_cap == 0 ? 16u : g->vertex_cap * 2u;
        sf_grass_vertex_t *nv = (sf_grass_vertex_t *)sf_xalloc(g->alloc, new_cap * sizeof(*nv));
        if (!nv) return SF_ERR_OOM;
        if (g->vertices) {
            memcpy(nv, g->vertices, g->vertex_count * sizeof(*nv));
            sf_xfree(g->alloc, g->vertices);
        }
        g->vertices = nv;
        g->vertex_cap = new_cap;
    }
    g->vertices[g->vertex_count++] = v;
    return SF_OK;
}

static sf_result_t grass_push_face(sf_grass_t *g, sf_grass_face_t f) {
    if (g->face_count >= g->face_cap) {
        size_t new_cap = g->face_cap == 0 ? 16u : g->face_cap * 2u;
        sf_grass_face_t *nf = (sf_grass_face_t *)sf_xalloc(g->alloc, new_cap * sizeof(*nf));
        if (!nf) return SF_ERR_OOM;
        if (g->faces) {
            memcpy(nf, g->faces, g->face_count * sizeof(*nf));
            sf_xfree(g->alloc, g->faces);
        }
        g->faces = nf;
        g->face_cap = new_cap;
    }
    g->faces[g->face_count++] = f;
    return SF_OK;
}

sf_result_t sf_grass_create(sf_grass_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_grass_t *g = (sf_grass_t *)sf_xalloc(alloc, sizeof(*g));
    if (!g) return SF_ERR_OOM;
    memset(g, 0, sizeof(*g));
    g->alloc = alloc;
    *out = g;
    return SF_OK;
}

void sf_grass_destroy(sf_grass_t *g) {
    if (!g) return;
    sf_xfree(g->alloc, g->volumes);
    sf_xfree(g->alloc, g->vertices);
    sf_xfree(g->alloc, g->faces);
    sf_xfree(g->alloc, g);
}

bool sf_grass_is(const void *bytes, size_t size) {
    if (!bytes || size < 0x28) return false;
    const uint8_t *p = (const uint8_t *)bytes;
    int32_t version, header_size, volume_size, vertex_size, face_size, bb_size;
    memcpy(&version,     p + 0x00, 4);
    memcpy(&header_size, p + 0x04, 4);
    memcpy(&volume_size, p + 0x08, 4);
    memcpy(&vertex_size, p + 0x10, 4);
    memcpy(&face_size,   p + 0x18, 4);
    memcpy(&bb_size,     p + 0x20, 4);
    return version == 1 && header_size == 0x28
        && volume_size == 0x14 && vertex_size == 0x24
        && face_size == 0x18 && bb_size == 0x18;
}

size_t sf_grass_volume_count(const sf_grass_t *g) { return g ? g->volume_count : 0u; }
size_t sf_grass_vertex_count(const sf_grass_t *g) { return g ? g->vertex_count : 0u; }
size_t sf_grass_face_count(const sf_grass_t *g)   { return g ? g->face_count   : 0u; }

sf_result_t sf_grass_get_volume(const sf_grass_t *g, size_t index, sf_grass_volume_t *out) {
    SF_CHECK_ARG(g != NULL && out != NULL);
    if (index >= g->volume_count) return SF_ERR_OUT_OF_RANGE;
    *out = g->volumes[index];
    return SF_OK;
}
sf_result_t sf_grass_add_volume(sf_grass_t *g, sf_grass_volume_t v) {
    SF_CHECK_ARG(g != NULL);
    return grass_push_volume(g, v);
}

sf_result_t sf_grass_get_vertex(const sf_grass_t *g, size_t index, sf_grass_vertex_t *out) {
    SF_CHECK_ARG(g != NULL && out != NULL);
    if (index >= g->vertex_count) return SF_ERR_OUT_OF_RANGE;
    *out = g->vertices[index];
    return SF_OK;
}
sf_result_t sf_grass_add_vertex(sf_grass_t *g, sf_grass_vertex_t v) {
    SF_CHECK_ARG(g != NULL);
    return grass_push_vertex(g, v);
}

sf_result_t sf_grass_get_face(const sf_grass_t *g, size_t index, sf_grass_face_t *out) {
    SF_CHECK_ARG(g != NULL && out != NULL);
    if (index >= g->face_count) return SF_ERR_OUT_OF_RANGE;
    *out = g->faces[index];
    return SF_OK;
}
sf_result_t sf_grass_add_face(sf_grass_t *g, sf_grass_face_t f) {
    SF_CHECK_ARG(g != NULL);
    return grass_push_face(g, f);
}

sf_result_t sf_grass_read_from_memory(sf_grass_t **out, const void *bytes, size_t size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_grass_t *g = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    int32_t volume_count = 0, vertex_count = 0, face_count = 0;

    e = sf_binary_reader_assert_i32_one(r, 1);         if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x28);      if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x14);      if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &volume_count);   if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x24);      if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &vertex_count);   if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x18);      if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &face_count);     if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x18);      if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, volume_count); if (e != SF_OK) goto done;

    if (volume_count < 0 || vertex_count < 0 || face_count < 0) {
        e = SF_ERR_BAD_MAGIC; goto done;
    }

    e = sf_grass_create(&g, alloc); if (e != SF_OK) goto done;

    for (int32_t i = 0; i < volume_count; i++) {
        sf_grass_volume_t v;
        memset(&v, 0, sizeof(v));
        e = sf_binary_reader_read_i32(r, &v.start_child_index); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &v.end_child_index);   if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &v.start_face_index);  if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &v.end_face_index);    if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &v.unk10);             if (e != SF_OK) goto done;
        e = grass_push_volume(g, v); if (e != SF_OK) goto done;
    }

    for (int32_t i = 0; i < vertex_count; i++) {
        sf_grass_vertex_t v;
        memset(&v, 0, sizeof(v));
        e = sf_binary_reader_read_f32(r, &v.x); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v.y); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v.z); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32s(r, 6, v.grass_densities); if (e != SF_OK) goto done;
        e = grass_push_vertex(g, v); if (e != SF_OK) goto done;
    }

    for (int32_t i = 0; i < face_count; i++) {
        sf_grass_face_t f;
        memset(&f, 0, sizeof(f));
        e = sf_binary_reader_read_f32(r, &f.normal_x); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &f.normal_y); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &f.normal_z); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &f.vertex_index_a); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &f.vertex_index_b); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &f.vertex_index_c); if (e != SF_OK) goto done;
        e = grass_push_face(g, f); if (e != SF_OK) goto done;
    }

    for (int32_t i = 0; i < volume_count; i++) {
        sf_grass_volume_t *v = &g->volumes[i];
        e = sf_binary_reader_read_f32(r, &v->bb_min_x); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v->bb_min_y); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v->bb_min_z); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v->bb_max_x); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v->bb_max_y); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_f32(r, &v->bb_max_z); if (e != SF_OK) goto done;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_grass_destroy(g); return e; }
    *out = g;
    return SF_OK;
}

sf_result_t sf_grass_write_to_memory(const sf_grass_t *g, void **out_bytes,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(g != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, false, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_i32(w, 1);    if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x28); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x14); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)g->volume_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x24); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)g->vertex_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x18); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)g->face_count);   if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x18); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, (int32_t)g->volume_count); if (e != SF_OK) goto done;

    for (size_t i = 0; i < g->volume_count; i++) {
        const sf_grass_volume_t *v = &g->volumes[i];
        e = sf_binary_writer_write_i32(w, v->start_child_index); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, v->end_child_index);   if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, v->start_face_index);  if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, v->end_face_index);    if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, v->unk10);             if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < g->vertex_count; i++) {
        const sf_grass_vertex_t *v = &g->vertices[i];
        e = sf_binary_writer_write_f32(w, v->x); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->y); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->z); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32s(w, 6, v->grass_densities); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < g->face_count; i++) {
        const sf_grass_face_t *f = &g->faces[i];
        e = sf_binary_writer_write_f32(w, f->normal_x); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, f->normal_y); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, f->normal_z); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, f->vertex_index_a); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, f->vertex_index_b); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, f->vertex_index_c); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < g->volume_count; i++) {
        const sf_grass_volume_t *v = &g->volumes[i];
        e = sf_binary_writer_write_f32(w, v->bb_min_x); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->bb_min_y); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->bb_min_z); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->bb_max_x); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->bb_max_y); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_f32(w, v->bb_max_z); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
