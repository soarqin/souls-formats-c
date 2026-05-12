/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MATBIN material binary reader + writer.
 *
 * Mirrors pinned upstream (commit 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a):
 *   SoulsFormats/Formats/MATBIN.cs
 *     - MATBIN.Read / Write
 *     - MATBIN.Param.Read / Write / WriteData
 *     - MATBIN.Sampler.Read / Write / WriteData
 */

#include "souls_formats/sf_matbin.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct sf_matbin_param {
    char                  *name;   /* UTF-8, heap-owned */
    sf_matbin_param_type_t type;
    union {
        bool     b;
        int32_t  i;
        int32_t  i2[2];
        float    f;
        float    f2[2];
        float    f3[3];
        float    f4[4];
        float    f5[5];
    } value;
    uint32_t key;
} sf_matbin_param_t;

typedef struct sf_matbin_sampler {
    char      *type;   /* UTF-8, heap-owned */
    char      *path;   /* UTF-8, heap-owned */
    uint32_t   key;
    sf_vec2_t  unk14;
} sf_matbin_sampler_t;

typedef struct sf_matbin {
    const sf_allocator_t  *alloc;
    char                  *shader_path;  /* UTF-8, heap-owned */
    char                  *source_path;  /* UTF-8, heap-owned */
    uint32_t               key;
    sf_matbin_param_t     *params;
    size_t                 param_count;
    sf_matbin_sampler_t   *samplers;
    size_t                 sampler_count;
} sf_matbin_t;

static bool matbin_param_type_known(uint32_t type) {
    switch ((sf_matbin_param_type_t)type) {
        case SF_MATBIN_PARAM_TYPE_BOOL:
        case SF_MATBIN_PARAM_TYPE_INT:
        case SF_MATBIN_PARAM_TYPE_INT2:
        case SF_MATBIN_PARAM_TYPE_FLOAT:
        case SF_MATBIN_PARAM_TYPE_FLOAT2:
        case SF_MATBIN_PARAM_TYPE_FLOAT3:
        case SF_MATBIN_PARAM_TYPE_FLOAT4:
        case SF_MATBIN_PARAM_TYPE_FLOAT5:
            return true;
    }
    return false;
}

static sf_result_t matbin_create_empty(sf_matbin_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;

    alloc = sf_alloc_or_default(alloc);
    sf_matbin_t *m = (sf_matbin_t *)sf_xalloc(alloc, sizeof(*m));
    if (!m) return SF_ERR_OOM;
    memset(m, 0, sizeof(*m));

    m->alloc = alloc;
    *out = m;
    return SF_OK;
}

void sf_matbin_destroy(sf_matbin_t *m) {
    if (!m) return;
    const sf_allocator_t *alloc = m->alloc;

    sf_xfree(alloc, m->shader_path);
    sf_xfree(alloc, m->source_path);

    if (m->params) {
        for (size_t i = 0; i < m->param_count; i++) {
            sf_xfree(alloc, m->params[i].name);
        }
        sf_xfree(alloc, m->params);
    }

    if (m->samplers) {
        for (size_t i = 0; i < m->sampler_count; i++) {
            sf_xfree(alloc, m->samplers[i].type);
            sf_xfree(alloc, m->samplers[i].path);
        }
        sf_xfree(alloc, m->samplers);
    }

    sf_xfree(alloc, m);
}

static sf_result_t matbin_read_param_value(sf_binary_reader_t *br, sf_matbin_param_t *p,
                                           int64_t value_offset) {
    sf_result_t r = sf_binary_reader_step_in(br, value_offset);
    if (r != SF_OK) return r;

    switch (p->type) {
        case SF_MATBIN_PARAM_TYPE_BOOL: {
            uint8_t b = 0;
            r = sf_binary_reader_read_u8(br, &b);
            p->value.b = (b != 0);
            break;
        }
        case SF_MATBIN_PARAM_TYPE_INT:
            r = sf_binary_reader_read_i32(br, &p->value.i);
            break;
        case SF_MATBIN_PARAM_TYPE_INT2:
            r = sf_binary_reader_read_i32s(br, 2, p->value.i2);
            break;
        case SF_MATBIN_PARAM_TYPE_FLOAT:
            r = sf_binary_reader_read_f32(br, &p->value.f);
            break;
        case SF_MATBIN_PARAM_TYPE_FLOAT2:
            r = sf_binary_reader_read_f32s(br, 2, p->value.f2);
            break;
        case SF_MATBIN_PARAM_TYPE_FLOAT3:
            r = sf_binary_reader_read_f32s(br, 3, p->value.f3);
            break;
        case SF_MATBIN_PARAM_TYPE_FLOAT4:
            r = sf_binary_reader_read_f32s(br, 4, p->value.f4);
            break;
        case SF_MATBIN_PARAM_TYPE_FLOAT5:
            r = sf_binary_reader_read_f32s(br, 5, p->value.f5);
            break;
    }

    sf_result_t out_r = sf_binary_reader_step_out(br);
    return (r != SF_OK) ? r : out_r;
}

static sf_result_t matbin_read_from_reader(sf_binary_reader_t *br, sf_matbin_t **out,
                                           const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    *out = NULL;

    alloc = sf_alloc_or_default(alloc);
    sf_binary_reader_set_big_endian(br, false);

    sf_matbin_t *m = NULL;
    sf_result_t r = matbin_create_empty(&m, alloc);
    if (r != SF_OK) return r;

    r = sf_binary_reader_assert_ascii(br, "MAB"); if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_u8_one(br, 0);     if (r != SF_OK) goto fail;
    r = sf_binary_reader_assert_i32_one(br, 2);    if (r != SF_OK) goto fail;

    int64_t shader_path_offset = 0;
    int64_t source_path_offset = 0;
    int32_t param_count = 0;
    int32_t sampler_count = 0;

    r = sf_binary_reader_read_i64(br, &shader_path_offset); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i64(br, &source_path_offset); if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_u32(br, &m->key);             if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i32(br, &param_count);        if (r != SF_OK) goto fail;
    r = sf_binary_reader_read_i32(br, &sampler_count);      if (r != SF_OK) goto fail;
    if (param_count < 0 || sampler_count < 0) { r = SF_ERR_OUT_OF_RANGE; goto fail; }
    r = sf_binary_reader_assert_pattern(br, 0x14, 0x00);    if (r != SF_OK) goto fail;

    m->param_count = (size_t)param_count;
    if (m->param_count > 0) {
        if (m->param_count > SIZE_MAX / sizeof(*m->params)) {
            r = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }
        m->params = (sf_matbin_param_t *)sf_xalloc(alloc, m->param_count * sizeof(*m->params));
        if (!m->params) { r = SF_ERR_OOM; goto fail; }
        memset(m->params, 0, m->param_count * sizeof(*m->params));

        for (size_t i = 0; i < m->param_count; i++) {
            sf_matbin_param_t *p = &m->params[i];
            int64_t name_offset = 0;
            int64_t value_offset = 0;
            uint32_t type = 0;

            r = sf_binary_reader_read_i64(br, &name_offset);  if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_i64(br, &value_offset); if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_u32(br, &p->key);       if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_u32(br, &type);         if (r != SF_OK) goto fail;
            if (!matbin_param_type_known(type)) { r = SF_ERR_UNSUPPORTED_VERSION; goto fail; }
            p->type = (sf_matbin_param_type_t)type;
            r = sf_binary_reader_assert_pattern(br, 0x10, 0x00); if (r != SF_OK) goto fail;

            r = sf_binary_reader_get_utf16(br, name_offset, &p->name, NULL);
            if (r != SF_OK) goto fail;
            r = matbin_read_param_value(br, p, value_offset);
            if (r != SF_OK) goto fail;
        }
    }

    m->sampler_count = (size_t)sampler_count;
    if (m->sampler_count > 0) {
        if (m->sampler_count > SIZE_MAX / sizeof(*m->samplers)) {
            r = SF_ERR_OUT_OF_RANGE;
            goto fail;
        }
        m->samplers = (sf_matbin_sampler_t *)sf_xalloc(alloc,
                                                       m->sampler_count * sizeof(*m->samplers));
        if (!m->samplers) { r = SF_ERR_OOM; goto fail; }
        memset(m->samplers, 0, m->sampler_count * sizeof(*m->samplers));

        for (size_t i = 0; i < m->sampler_count; i++) {
            sf_matbin_sampler_t *s = &m->samplers[i];
            int64_t type_offset = 0;
            int64_t path_offset = 0;

            r = sf_binary_reader_read_i64(br, &type_offset);       if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_i64(br, &path_offset);       if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_u32(br, &s->key);            if (r != SF_OK) goto fail;
            r = sf_binary_reader_read_vec2(br, &s->unk14);         if (r != SF_OK) goto fail;
            r = sf_binary_reader_assert_pattern(br, 0x14, 0x00);   if (r != SF_OK) goto fail;

            r = sf_binary_reader_get_utf16(br, type_offset, &s->type, NULL);
            if (r != SF_OK) goto fail;
            r = sf_binary_reader_get_utf16(br, path_offset, &s->path, NULL);
            if (r != SF_OK) goto fail;
        }
    }

    r = sf_binary_reader_get_utf16(br, shader_path_offset, &m->shader_path, NULL);
    if (r != SF_OK) goto fail;
    r = sf_binary_reader_get_utf16(br, source_path_offset, &m->source_path, NULL);
    if (r != SF_OK) goto fail;

    *out = m;
    return SF_OK;

fail:
    sf_matbin_destroy(m);
    return r;
}

sf_result_t sf_matbin_read_from_memory(sf_matbin_t **out, const void *bytes, size_t size,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bytes != NULL || size == 0);
    *out = NULL;

    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, bytes, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, alloc);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    r = matbin_read_from_reader(br, out, alloc);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_matbin_read_from_path(sf_matbin_t **out, const wchar_t *path,
                                     const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;

    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, alloc);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    r = matbin_read_from_reader(br, out, alloc);
    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

static sf_result_t matbin_write_i64_at(sf_binary_writer_t *bw, int64_t slot, int64_t value) {
    sf_result_t r = sf_binary_writer_step_in(bw, slot);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i64(bw, value);
    sf_result_t out_r = sf_binary_writer_step_out(bw);
    return (r != SF_OK) ? r : out_r;
}

static sf_result_t matbin_write_param_value(sf_binary_writer_t *bw,
                                            const sf_matbin_param_t *p) {
    switch (p->type) {
        case SF_MATBIN_PARAM_TYPE_BOOL:
            return sf_binary_writer_write_bool(bw, p->value.b);
        case SF_MATBIN_PARAM_TYPE_INT:
            return sf_binary_writer_write_i32(bw, p->value.i);
        case SF_MATBIN_PARAM_TYPE_INT2:
            return sf_binary_writer_write_i32s(bw, 2, p->value.i2);
        case SF_MATBIN_PARAM_TYPE_FLOAT:
            return sf_binary_writer_write_f32(bw, p->value.f);
        case SF_MATBIN_PARAM_TYPE_FLOAT2:
            return sf_binary_writer_write_f32s(bw, 2, p->value.f2);
        case SF_MATBIN_PARAM_TYPE_FLOAT3: {
            sf_result_t r = sf_binary_writer_write_f32s(bw, 3, p->value.f3);
            if (r != SF_OK) return r;
            r = sf_binary_writer_write_f32(bw, 1.0f);
            if (r != SF_OK) return r;
            return sf_binary_writer_write_f32(bw, 1.0f);
        }
        case SF_MATBIN_PARAM_TYPE_FLOAT4:
            return sf_binary_writer_write_f32s(bw, 4, p->value.f4);
        case SF_MATBIN_PARAM_TYPE_FLOAT5:
            return sf_binary_writer_write_f32s(bw, 5, p->value.f5);
    }
    return SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t matbin_write_to_writer(const sf_matbin_t *m, sf_binary_writer_t *bw,
                                          const sf_allocator_t *scratch_alloc) {
    SF_CHECK_ARG(m != NULL && bw != NULL);
    if (m->param_count > (size_t)INT32_MAX || m->sampler_count > (size_t)INT32_MAX) {
        return SF_ERR_OUT_OF_RANGE;
    }
    if ((m->param_count > 0 && m->params == NULL) ||
        (m->sampler_count > 0 && m->samplers == NULL)) {
        return SF_ERR_INVALID_ARG;
    }

    scratch_alloc = sf_alloc_or_default(scratch_alloc);
    sf_binary_writer_set_big_endian(bw, false);

    int64_t *param_name_slots = NULL;
    int64_t *param_value_slots = NULL;
    int64_t *sampler_type_slots = NULL;
    int64_t *sampler_path_slots = NULL;

    if (m->param_count > 0) {
        if (m->param_count > SIZE_MAX / sizeof(int64_t)) return SF_ERR_OUT_OF_RANGE;
        param_name_slots = (int64_t *)sf_xalloc(scratch_alloc, m->param_count * sizeof(int64_t));
        param_value_slots = (int64_t *)sf_xalloc(scratch_alloc, m->param_count * sizeof(int64_t));
        if (!param_name_slots || !param_value_slots) {
            sf_xfree(scratch_alloc, param_name_slots);
            sf_xfree(scratch_alloc, param_value_slots);
            return SF_ERR_OOM;
        }
    }
    if (m->sampler_count > 0) {
        if (m->sampler_count > SIZE_MAX / sizeof(int64_t)) {
            sf_xfree(scratch_alloc, param_name_slots);
            sf_xfree(scratch_alloc, param_value_slots);
            return SF_ERR_OUT_OF_RANGE;
        }
        sampler_type_slots = (int64_t *)sf_xalloc(scratch_alloc,
                                                  m->sampler_count * sizeof(int64_t));
        sampler_path_slots = (int64_t *)sf_xalloc(scratch_alloc,
                                                  m->sampler_count * sizeof(int64_t));
        if (!sampler_type_slots || !sampler_path_slots) {
            sf_xfree(scratch_alloc, param_name_slots);
            sf_xfree(scratch_alloc, param_value_slots);
            sf_xfree(scratch_alloc, sampler_type_slots);
            sf_xfree(scratch_alloc, sampler_path_slots);
            return SF_ERR_OOM;
        }
    }

    static const uint8_t magic[4] = { 'M', 'A', 'B', 0 };
    sf_result_t r = sf_binary_writer_write_bytes(bw, magic, sizeof(magic));
    if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, 2); if (r != SF_OK) goto done;

    int64_t shader_path_slot = sf_binary_writer_position(bw);
    r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
    int64_t source_path_slot = sf_binary_writer_position(bw);
    r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u32(bw, m->key); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, (int32_t)m->param_count); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_i32(bw, (int32_t)m->sampler_count); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_pattern(bw, 0x14, 0x00); if (r != SF_OK) goto done;

    for (size_t i = 0; i < m->param_count; i++) {
        const sf_matbin_param_t *p = &m->params[i];
        if (!matbin_param_type_known((uint32_t)p->type)) {
            r = SF_ERR_UNSUPPORTED_VERSION;
            goto done;
        }

        param_name_slots[i] = sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
        param_value_slots[i] = sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_u32(bw, p->key); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_u32(bw, (uint32_t)p->type); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_pattern(bw, 0x10, 0x00); if (r != SF_OK) goto done;
    }

    for (size_t i = 0; i < m->sampler_count; i++) {
        const sf_matbin_sampler_t *s = &m->samplers[i];
        sampler_type_slots[i] = sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
        sampler_path_slots[i] = sf_binary_writer_position(bw);
        r = sf_binary_writer_write_i64(bw, 0); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_u32(bw, s->key); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_vec2(bw, s->unk14); if (r != SF_OK) goto done;
        r = sf_binary_writer_write_pattern(bw, 0x14, 0x00); if (r != SF_OK) goto done;
    }

    for (size_t i = 0; i < m->param_count; i++) {
        const sf_matbin_param_t *p = &m->params[i];
        r = matbin_write_i64_at(bw, param_name_slots[i], sf_binary_writer_position(bw));
        if (r != SF_OK) goto done;
        r = sf_binary_writer_write_utf16(bw, p->name ? p->name : "", true);
        if (r != SF_OK) goto done;

        r = matbin_write_i64_at(bw, param_value_slots[i], sf_binary_writer_position(bw));
        if (r != SF_OK) goto done;
        r = matbin_write_param_value(bw, p);
        if (r != SF_OK) goto done;
    }

    for (size_t i = 0; i < m->sampler_count; i++) {
        const sf_matbin_sampler_t *s = &m->samplers[i];
        r = matbin_write_i64_at(bw, sampler_type_slots[i], sf_binary_writer_position(bw));
        if (r != SF_OK) goto done;
        r = sf_binary_writer_write_utf16(bw, s->type ? s->type : "", true);
        if (r != SF_OK) goto done;

        r = matbin_write_i64_at(bw, sampler_path_slots[i], sf_binary_writer_position(bw));
        if (r != SF_OK) goto done;
        r = sf_binary_writer_write_utf16(bw, s->path ? s->path : "", true);
        if (r != SF_OK) goto done;
    }

    r = matbin_write_i64_at(bw, shader_path_slot, sf_binary_writer_position(bw));
    if (r != SF_OK) goto done;
    r = sf_binary_writer_write_utf16(bw, m->shader_path ? m->shader_path : "", true);
    if (r != SF_OK) goto done;

    r = matbin_write_i64_at(bw, source_path_slot, sf_binary_writer_position(bw));
    if (r != SF_OK) goto done;
    r = sf_binary_writer_write_utf16(bw, m->source_path ? m->source_path : "", true);

done:
    sf_xfree(scratch_alloc, param_name_slots);
    sf_xfree(scratch_alloc, param_value_slots);
    sf_xfree(scratch_alloc, sampler_type_slots);
    sf_xfree(scratch_alloc, sampler_path_slots);
    return r;
}

sf_result_t sf_matbin_write_to_memory(const sf_matbin_t *m, void **out_bytes, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(m != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;

    alloc = sf_alloc_or_default(alloc);
    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = matbin_write_to_writer(m, bw, alloc);
    if (r == SF_OK) {
        uint8_t *bytes = NULL;
        r = sf_binary_writer_finish_bytes(bw, &bytes, out_size);
        if (r == SF_OK) *out_bytes = bytes;
    }

    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_matbin_write_to_path(const sf_matbin_t *m, const wchar_t *path,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(m != NULL && path != NULL);
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = matbin_write_to_writer(m, bw, alloc);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);

    sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}

const char *sf_matbin_shader_path(const sf_matbin_t *m) {
    return m ? m->shader_path : NULL;
}

const char *sf_matbin_source_path(const sf_matbin_t *m) {
    return m ? m->source_path : NULL;
}

uint32_t sf_matbin_key(const sf_matbin_t *m) {
    return m ? m->key : 0;
}

size_t sf_matbin_param_count(const sf_matbin_t *m) {
    return m ? m->param_count : 0;
}

const sf_matbin_param_t *sf_matbin_param(const sf_matbin_t *m, size_t i) {
    if (!m || i >= m->param_count) return NULL;
    return &m->params[i];
}

const char *sf_matbin_param_name(const sf_matbin_param_t *p) {
    return p ? p->name : NULL;
}

sf_matbin_param_type_t sf_matbin_param_type(const sf_matbin_param_t *p) {
    return p ? p->type : SF_MATBIN_PARAM_TYPE_BOOL;
}

uint32_t sf_matbin_param_key(const sf_matbin_param_t *p) {
    return p ? p->key : 0;
}

#define MATBIN_VALUE_CHECK(param, out_ptr, expected) \
    do { \
        if (!(param) || !(out_ptr) || (param)->type != (expected)) return SF_ERR_INVALID_ARG; \
    } while (0)

sf_result_t sf_matbin_param_value_bool(const sf_matbin_param_t *p, bool *out) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_BOOL);
    *out = p->value.b;
    return SF_OK;
}

sf_result_t sf_matbin_param_value_int(const sf_matbin_param_t *p, int32_t *out) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_INT);
    *out = p->value.i;
    return SF_OK;
}

sf_result_t sf_matbin_param_value_int2(const sf_matbin_param_t *p, int32_t out[2]) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_INT2);
    memcpy(out, p->value.i2, sizeof(p->value.i2));
    return SF_OK;
}

sf_result_t sf_matbin_param_value_float(const sf_matbin_param_t *p, float *out) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_FLOAT);
    *out = p->value.f;
    return SF_OK;
}

sf_result_t sf_matbin_param_value_float2(const sf_matbin_param_t *p, float out[2]) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_FLOAT2);
    memcpy(out, p->value.f2, sizeof(p->value.f2));
    return SF_OK;
}

sf_result_t sf_matbin_param_value_float3(const sf_matbin_param_t *p, float out[3]) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_FLOAT3);
    memcpy(out, p->value.f3, sizeof(p->value.f3));
    return SF_OK;
}

sf_result_t sf_matbin_param_value_float4(const sf_matbin_param_t *p, float out[4]) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_FLOAT4);
    memcpy(out, p->value.f4, sizeof(p->value.f4));
    return SF_OK;
}

sf_result_t sf_matbin_param_value_float5(const sf_matbin_param_t *p, float out[5]) {
    MATBIN_VALUE_CHECK(p, out, SF_MATBIN_PARAM_TYPE_FLOAT5);
    memcpy(out, p->value.f5, sizeof(p->value.f5));
    return SF_OK;
}

size_t sf_matbin_sampler_count(const sf_matbin_t *m) {
    return m ? m->sampler_count : 0;
}

const sf_matbin_sampler_t *sf_matbin_sampler(const sf_matbin_t *m, size_t i) {
    if (!m || i >= m->sampler_count) return NULL;
    return &m->samplers[i];
}

const char *sf_matbin_sampler_type(const sf_matbin_sampler_t *s) {
    return s ? s->type : NULL;
}

const char *sf_matbin_sampler_path(const sf_matbin_sampler_t *s) {
    return s ? s->path : NULL;
}

uint32_t sf_matbin_sampler_key(const sf_matbin_sampler_t *s) {
    return s ? s->key : 0;
}

sf_result_t sf_matbin_sampler_unk14(const sf_matbin_sampler_t *s, sf_vec2_t *out) {
    SF_CHECK_ARG(s != NULL && out != NULL);
    *out = s->unk14;
    return SF_OK;
}
