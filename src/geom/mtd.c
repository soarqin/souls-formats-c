/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MTD material definition.
 *
 * Mirrors:
 *   SoulsFormats/Formats/MTD.cs
 */

#include "souls_formats/sf_mtd.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_encoding.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sf_mtd_param {
    char               *name;
    sf_mtd_param_type_t type;
    union {
        int32_t i;
        int32_t i2[2];
        float   f;
        float   f4[4];
        bool    b;
    } value;
};

struct sf_mtd_texture {
    char   *type;
    int32_t uv_number;
    int32_t shader_data_index;
    bool    has_extended;
    char   *path;
    float  *unk_floats;
    size_t  unk_float_count;
};

struct sf_mtd {
    const sf_allocator_t *alloc;
    char                 *shader_path;
    char                 *description;
    sf_mtd_param_t       *params;
    size_t                param_count;
    sf_mtd_texture_t     *textures;
    size_t                texture_count;
};

typedef struct mtd_block {
    int64_t  start;
    uint32_t length;
    int32_t  type;
    int32_t  version;
    uint8_t  marker;
} mtd_block_t;

static void *mtd_alloc_array(const sf_allocator_t *alloc, size_t count, size_t elem_size) {
    if (count == 0) return NULL;
    if (elem_size != 0 && count > SIZE_MAX / elem_size) return NULL;
    return sf_xalloc(alloc, count * elem_size);
}

static void mtd_param_free(sf_mtd_param_t *param, const sf_allocator_t *alloc) {
    if (!param) return;
    sf_xfree(alloc, param->name);
    memset(param, 0, sizeof(*param));
}

static void mtd_texture_free(sf_mtd_texture_t *texture, const sf_allocator_t *alloc) {
    if (!texture) return;
    sf_xfree(alloc, texture->type);
    sf_xfree(alloc, texture->path);
    sf_xfree(alloc, texture->unk_floats);
    memset(texture, 0, sizeof(*texture));
}

void sf_mtd_destroy(sf_mtd_t *mtd) {
    if (!mtd) return;
    const sf_allocator_t *alloc = mtd->alloc;
    sf_xfree(alloc, mtd->shader_path);
    sf_xfree(alloc, mtd->description);
    for (size_t i = 0; i < mtd->param_count; i++) mtd_param_free(&mtd->params[i], alloc);
    for (size_t i = 0; i < mtd->texture_count; i++) mtd_texture_free(&mtd->textures[i], alloc);
    sf_xfree(alloc, mtd->params);
    sf_xfree(alloc, mtd->textures);
    sf_xfree(alloc, mtd);
}

static sf_result_t mtd_create_empty(sf_mtd_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_mtd_t *mtd = (sf_mtd_t *)sf_xalloc(alloc, sizeof(*mtd));
    if (!mtd) return SF_ERR_OOM;
    memset(mtd, 0, sizeof(*mtd));
    mtd->alloc = alloc;

    mtd->shader_path = sf_strdup(alloc, "");
    mtd->description = sf_strdup(alloc, "");
    if (!mtd->shader_path || !mtd->description) {
        sf_mtd_destroy(mtd);
        return SF_ERR_OOM;
    }

    *out = mtd;
    return SF_OK;
}

static sf_result_t mtd_read_marker(sf_binary_reader_t *br, uint8_t *out_marker) {
    sf_result_t r = sf_binary_reader_read_u8(br, out_marker);
    if (r != SF_OK) return r;
    return sf_binary_reader_pad(br, 4);
}

static sf_result_t mtd_assert_marker(sf_binary_reader_t *br, uint8_t marker) {
    sf_result_t r = sf_binary_reader_assert_u8_one(br, marker);
    if (r != SF_OK) return r;
    return sf_binary_reader_pad(br, 4);
}

static sf_result_t mtd_write_marker(sf_binary_writer_t *bw, uint8_t marker) {
    sf_result_t r = sf_binary_writer_write_u8(bw, marker);
    if (r != SF_OK) return r;
    return sf_binary_writer_pad(bw, 4);
}

static sf_result_t mtd_read_marked_string(sf_binary_reader_t *br, uint8_t marker,
                                          char **out_string) {
    SF_CHECK_ARG(out_string != NULL);
    *out_string = NULL;

    int32_t length = 0;
    sf_result_t r = sf_binary_reader_read_i32(br, &length);
    if (r != SF_OK) return r;
    if (length < 0) return SF_ERR_OUT_OF_RANGE;

    r = sf_binary_reader_read_shift_jis_n(br, (size_t)length, out_string, NULL);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, marker);
    if (r != SF_OK) {
        sf_free(NULL, *out_string);
        *out_string = NULL;
    }
    return r;
}

static sf_result_t mtd_assert_marked_string(sf_binary_reader_t *br, uint8_t marker,
                                            const char *expected) {
    char *actual = NULL;
    sf_result_t r = mtd_read_marked_string(br, marker, &actual);
    if (r != SF_OK) return r;
    if (strcmp(actual ? actual : "", expected) != 0) r = SF_ERR_BAD_MAGIC;
    sf_free(NULL, actual);
    return r;
}

static sf_result_t mtd_write_marked_string(sf_binary_writer_t *bw, uint8_t marker,
                                           const char *string) {
    void *bytes = NULL;
    size_t byte_count = 0;
    sf_result_t r = sf_utf8_to_shift_jis(string ? string : "", false, &bytes, &byte_count, NULL);
    if (r != SF_OK) return r;
    if (byte_count > (size_t)INT32_MAX) {
        sf_free(NULL, bytes);
        return SF_ERR_OUT_OF_RANGE;
    }

    r = sf_binary_writer_write_i32(bw, (int32_t)byte_count);
    if (r == SF_OK) r = sf_binary_writer_write_bytes(bw, bytes, byte_count);
    sf_free(NULL, bytes);
    if (r != SF_OK) return r;
    return mtd_write_marker(bw, marker);
}

static sf_result_t mtd_block_read(sf_binary_reader_t *br, const int32_t *assert_type,
                                  const int32_t *assert_version, const uint8_t *assert_marker,
                                  mtd_block_t *out) {
    SF_CHECK_ARG(out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;
    r = sf_binary_reader_read_u32(br, &out->length);
    if (r != SF_OK) return r;
    out->start = sf_binary_reader_position(br);

    if (assert_type) {
        r = sf_binary_reader_assert_i32_one(br, *assert_type);
        out->type = *assert_type;
    } else {
        r = sf_binary_reader_read_i32(br, &out->type);
    }
    if (r != SF_OK) return r;

    if (assert_version) {
        r = sf_binary_reader_assert_i32_one(br, *assert_version);
        out->version = *assert_version;
    } else {
        r = sf_binary_reader_read_i32(br, &out->version);
    }
    if (r != SF_OK) return r;

    if (assert_marker) {
        r = mtd_assert_marker(br, *assert_marker);
        out->marker = *assert_marker;
    } else {
        r = mtd_read_marker(br, &out->marker);
    }
    return r;
}

static sf_result_t mtd_block_write(sf_binary_writer_t *bw, int32_t type, int32_t version,
                                   uint8_t marker, mtd_block_t *out) {
    SF_CHECK_ARG(out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;

    out->start = sf_binary_writer_position(bw) + 4;
    out->type = type;
    out->version = version;
    out->marker = marker;

    char name[32];
    (void)snprintf(name, sizeof(name), "Block%llX", (unsigned long long)out->start);
    r = sf_binary_writer_reserve_u32(bw, name);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, type);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, version);
    if (r != SF_OK) return r;
    return mtd_write_marker(bw, marker);
}

static sf_result_t mtd_block_finish(sf_binary_writer_t *bw, mtd_block_t *block) {
    int64_t current = sf_binary_writer_position(bw);
    if (current < block->start) return SF_ERR_INTERNAL;
    uint64_t length64 = (uint64_t)(current - block->start);
    if (length64 > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;

    block->length = (uint32_t)length64;
    char name[32];
    (void)snprintf(name, sizeof(name), "Block%llX", (unsigned long long)block->start);
    return sf_binary_writer_fill_u32(bw, name, block->length);
}

static sf_result_t mtd_parse_param_type(const char *string, sf_mtd_param_type_t *out_type) {
    if (strcmp(string, "bool") == 0) *out_type = SF_MTD_PARAM_TYPE_BOOL;
    else if (strcmp(string, "int") == 0) *out_type = SF_MTD_PARAM_TYPE_INT;
    else if (strcmp(string, "int2") == 0) *out_type = SF_MTD_PARAM_TYPE_INT2;
    else if (strcmp(string, "float") == 0) *out_type = SF_MTD_PARAM_TYPE_FLOAT;
    else if (strcmp(string, "float2") == 0) *out_type = SF_MTD_PARAM_TYPE_FLOAT2;
    else if (strcmp(string, "float3") == 0) *out_type = SF_MTD_PARAM_TYPE_FLOAT3;
    else if (strcmp(string, "float4") == 0) *out_type = SF_MTD_PARAM_TYPE_FLOAT4;
    else return SF_ERR_BAD_MAGIC;
    return SF_OK;
}

static const char *mtd_param_type_string(sf_mtd_param_type_t type) {
    switch (type) {
        case SF_MTD_PARAM_TYPE_BOOL:   return "bool";
        case SF_MTD_PARAM_TYPE_INT:    return "int";
        case SF_MTD_PARAM_TYPE_INT2:   return "int2";
        case SF_MTD_PARAM_TYPE_FLOAT:  return "float";
        case SF_MTD_PARAM_TYPE_FLOAT2: return "float2";
        case SF_MTD_PARAM_TYPE_FLOAT3: return "float3";
        case SF_MTD_PARAM_TYPE_FLOAT4: return "float4";
    }
    return NULL;
}

static int32_t mtd_param_value_count(sf_mtd_param_type_t type) {
    switch (type) {
        case SF_MTD_PARAM_TYPE_BOOL:
        case SF_MTD_PARAM_TYPE_INT:
        case SF_MTD_PARAM_TYPE_FLOAT:  return 1;
        case SF_MTD_PARAM_TYPE_INT2:
        case SF_MTD_PARAM_TYPE_FLOAT2: return 2;
        case SF_MTD_PARAM_TYPE_FLOAT3: return 3;
        case SF_MTD_PARAM_TYPE_FLOAT4: return 4;
    }
    return -1;
}

static sf_result_t mtd_read_param_value(sf_binary_reader_t *br, sf_mtd_param_t *param) {
    mtd_block_t value_block;
    const int32_t version = 1;
    sf_result_t r = mtd_block_read(br, NULL, &version, NULL, &value_block);
    if (r != SF_OK) return r;

    int32_t value_count = 0;
    r = sf_binary_reader_read_i32(br, &value_count);
    if (r != SF_OK) return r;
    (void)value_count;

    switch (param->type) {
        case SF_MTD_PARAM_TYPE_BOOL:   return sf_binary_reader_read_bool(br, &param->value.b);
        case SF_MTD_PARAM_TYPE_INT:    return sf_binary_reader_read_i32(br, &param->value.i);
        case SF_MTD_PARAM_TYPE_INT2:   return sf_binary_reader_read_i32s(br, 2, param->value.i2);
        case SF_MTD_PARAM_TYPE_FLOAT:  return sf_binary_reader_read_f32(br, &param->value.f);
        case SF_MTD_PARAM_TYPE_FLOAT2: return sf_binary_reader_read_f32s(br, 2, param->value.f4);
        case SF_MTD_PARAM_TYPE_FLOAT3: return sf_binary_reader_read_f32s(br, 3, param->value.f4);
        case SF_MTD_PARAM_TYPE_FLOAT4: return sf_binary_reader_read_f32s(br, 4, param->value.f4);
    }
    return SF_ERR_BAD_MAGIC;
}

static sf_result_t mtd_read_param(sf_binary_reader_t *br, sf_mtd_param_t *param) {
    const int32_t type = 4;
    const int32_t version = 4;
    const uint8_t marker = 0xA3;
    mtd_block_t param_block;
    sf_result_t r = mtd_block_read(br, &type, &version, &marker, &param_block);
    if (r != SF_OK) return r;

    r = mtd_read_marked_string(br, 0xA3, &param->name);
    if (r != SF_OK) return r;

    char *type_string = NULL;
    r = mtd_read_marked_string(br, 0x04, &type_string);
    if (r != SF_OK) return r;
    r = mtd_parse_param_type(type_string, &param->type);
    sf_free(NULL, type_string);
    if (r != SF_OK) return r;

    r = sf_binary_reader_assert_i32_one(br, 1);
    if (r != SF_OK) return r;
    r = mtd_read_param_value(br, param);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x04);
    if (r != SF_OK) return r;
    return sf_binary_reader_assert_i32_one(br, 0);
}

static sf_result_t mtd_read_texture(sf_binary_reader_t *br, sf_mtd_texture_t *texture,
                                    const sf_allocator_t *alloc) {
    const int32_t type = 0x2000;
    const uint8_t marker = 0xA3;
    mtd_block_t texture_block;
    sf_result_t r = mtd_block_read(br, &type, NULL, &marker, &texture_block);
    if (r != SF_OK) return r;

    if (texture_block.version == 3) texture->has_extended = false;
    else if (texture_block.version == 5) texture->has_extended = true;
    else return SF_ERR_UNSUPPORTED_VERSION;

    r = mtd_read_marked_string(br, 0x35, &texture->type);
    if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &texture->uv_number);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x35);
    if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &texture->shader_data_index);
    if (r != SF_OK) return r;

    if (!texture->has_extended) {
        texture->path = sf_strdup(alloc, "");
        return texture->path ? SF_OK : SF_ERR_OOM;
    }

    r = sf_binary_reader_assert_i32_one(br, 0xA3);
    if (r != SF_OK) return r;
    r = mtd_read_marked_string(br, 0xBA, &texture->path);
    if (r != SF_OK) return r;

    int32_t float_count = 0;
    r = sf_binary_reader_read_i32(br, &float_count);
    if (r != SF_OK) return r;
    if (float_count < 0) return SF_ERR_OUT_OF_RANGE;
    texture->unk_float_count = (size_t)float_count;
    if (texture->unk_float_count == 0) return SF_OK;

    texture->unk_floats = (float *)mtd_alloc_array(alloc, texture->unk_float_count, sizeof(float));
    if (!texture->unk_floats) return SF_ERR_OOM;
    return sf_binary_reader_read_f32s(br, texture->unk_float_count, texture->unk_floats);
}

static sf_result_t mtd_read_to_object(sf_binary_reader_t *br, sf_mtd_t *mtd) {
    sf_binary_reader_set_big_endian(br, false);

    const int32_t file_type = 0;
    const int32_t file_version = 3;
    const uint8_t file_marker = 0x01;
    mtd_block_t file_block;
    sf_result_t r = mtd_block_read(br, &file_type, &file_version, &file_marker, &file_block);
    if (r != SF_OK) return r;

    const int32_t header_type = 1;
    const int32_t header_version = 2;
    const uint8_t header_marker = 0xB0;
    mtd_block_t header_block;
    r = mtd_block_read(br, &header_type, &header_version, &header_marker, &header_block);
    if (r != SF_OK) return r;
    r = mtd_assert_marked_string(br, 0x34, "MTD ");
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 1000);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x01);
    if (r != SF_OK) return r;

    const int32_t data_type = 2;
    const int32_t data_version = 4;
    const uint8_t data_marker = 0xA3;
    mtd_block_t data_block;
    r = mtd_block_read(br, &data_type, &data_version, &data_marker, &data_block);
    if (r != SF_OK) return r;

    sf_xfree(mtd->alloc, mtd->shader_path);
    sf_xfree(mtd->alloc, mtd->description);
    mtd->shader_path = NULL;
    mtd->description = NULL;
    r = mtd_read_marked_string(br, 0xA3, &mtd->shader_path);
    if (r != SF_OK) return r;
    r = mtd_read_marked_string(br, 0x03, &mtd->description);
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 1);
    if (r != SF_OK) return r;

    const int32_t lists_type = 3;
    const int32_t lists_version = 4;
    const uint8_t lists_marker = 0xA3;
    mtd_block_t lists_block;
    r = mtd_block_read(br, &lists_type, &lists_version, &lists_marker, &lists_block);
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x03);
    if (r != SF_OK) return r;

    int32_t param_count = 0;
    r = sf_binary_reader_read_i32(br, &param_count);
    if (r != SF_OK) return r;
    if (param_count < 0) return SF_ERR_OUT_OF_RANGE;
    mtd->param_count = (size_t)param_count;
    if (mtd->param_count > 0) {
        mtd->params = (sf_mtd_param_t *)mtd_alloc_array(mtd->alloc, mtd->param_count,
                                                        sizeof(*mtd->params));
        if (!mtd->params) return SF_ERR_OOM;
        memset(mtd->params, 0, mtd->param_count * sizeof(*mtd->params));
    }
    for (size_t i = 0; i < mtd->param_count; i++) {
        r = mtd_read_param(br, &mtd->params[i]);
        if (r != SF_OK) return r;
    }

    r = mtd_assert_marker(br, 0x03);
    if (r != SF_OK) return r;
    int32_t texture_count = 0;
    r = sf_binary_reader_read_i32(br, &texture_count);
    if (r != SF_OK) return r;
    if (texture_count < 0) return SF_ERR_OUT_OF_RANGE;
    mtd->texture_count = (size_t)texture_count;
    if (mtd->texture_count > 0) {
        mtd->textures = (sf_mtd_texture_t *)mtd_alloc_array(mtd->alloc, mtd->texture_count,
                                                            sizeof(*mtd->textures));
        if (!mtd->textures) return SF_ERR_OOM;
        memset(mtd->textures, 0, mtd->texture_count * sizeof(*mtd->textures));
    }
    for (size_t i = 0; i < mtd->texture_count; i++) {
        r = mtd_read_texture(br, &mtd->textures[i], mtd->alloc);
        if (r != SF_OK) return r;
    }

    r = mtd_assert_marker(br, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;
    r = mtd_assert_marker(br, 0x04);
    if (r != SF_OK) return r;
    return sf_binary_reader_assert_i32_one(br, 0);
}

sf_result_t sf_mtd_read_from_stream(sf_mtd_t **out, sf_istream_t *stream,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && stream != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_binary_reader_t *br = NULL;
    sf_result_t r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) return r;

    sf_mtd_t *mtd = NULL;
    r = mtd_create_empty(&mtd, alloc);
    if (r == SF_OK) r = mtd_read_to_object(br, mtd);
    sf_binary_reader_destroy(br);

    if (r != SF_OK) {
        sf_mtd_destroy(mtd);
        return r;
    }
    *out = mtd;
    return SF_OK;
}

sf_result_t sf_mtd_read_from_memory(sf_mtd_t **out, const uint8_t *data, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL || size == 0);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    r = sf_mtd_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_mtd_read_from_path(sf_mtd_t **out, const wchar_t *path,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && path != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_mtd_read_from_stream(out, stream, alloc);
    sf_istream_close(stream);
    return r;
}

static sf_result_t mtd_write_param_value(sf_binary_writer_t *bw, const sf_mtd_param_t *param) {
    int32_t value_type = -1;
    uint8_t value_marker = 0xFF;
    switch (param->type) {
        case SF_MTD_PARAM_TYPE_BOOL:   value_type = 0x1000; value_marker = 0xC0; break;
        case SF_MTD_PARAM_TYPE_INT:
        case SF_MTD_PARAM_TYPE_INT2:   value_type = 0x1001; value_marker = 0xC5; break;
        case SF_MTD_PARAM_TYPE_FLOAT:
        case SF_MTD_PARAM_TYPE_FLOAT2:
        case SF_MTD_PARAM_TYPE_FLOAT3:
        case SF_MTD_PARAM_TYPE_FLOAT4: value_type = 0x1002; value_marker = 0xCA; break;
    }

    mtd_block_t value_block;
    sf_result_t r = mtd_block_write(bw, value_type, 1, value_marker, &value_block);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, mtd_param_value_count(param->type));
    if (r != SF_OK) return r;

    switch (param->type) {
        case SF_MTD_PARAM_TYPE_BOOL:   r = sf_binary_writer_write_bool(bw, param->value.b); break;
        case SF_MTD_PARAM_TYPE_INT:    r = sf_binary_writer_write_i32(bw, param->value.i); break;
        case SF_MTD_PARAM_TYPE_INT2:   r = sf_binary_writer_write_i32s(bw, 2, param->value.i2); break;
        case SF_MTD_PARAM_TYPE_FLOAT:  r = sf_binary_writer_write_f32(bw, param->value.f); break;
        case SF_MTD_PARAM_TYPE_FLOAT2: r = sf_binary_writer_write_f32s(bw, 2, param->value.f4); break;
        case SF_MTD_PARAM_TYPE_FLOAT3: r = sf_binary_writer_write_f32s(bw, 3, param->value.f4); break;
        case SF_MTD_PARAM_TYPE_FLOAT4: r = sf_binary_writer_write_f32s(bw, 4, param->value.f4); break;
    }
    if (r != SF_OK) return r;
    return mtd_block_finish(bw, &value_block);
}

static sf_result_t mtd_write_param(sf_binary_writer_t *bw, const sf_mtd_param_t *param) {
    const char *type_string = mtd_param_type_string(param->type);
    if (!type_string) return SF_ERR_INVALID_ARG;

    mtd_block_t param_block;
    sf_result_t r = mtd_block_write(bw, 4, 4, 0xA3, &param_block);
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0xA3, param->name ? param->name : "");
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0x04, type_string);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 1);
    if (r != SF_OK) return r;
    r = mtd_write_param_value(bw, param);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    return mtd_block_finish(bw, &param_block);
}

static sf_result_t mtd_write_texture(sf_binary_writer_t *bw, const sf_mtd_texture_t *texture) {
    mtd_block_t texture_block;
    sf_result_t r = mtd_block_write(bw, 0x2000, texture->has_extended ? 5 : 3, 0xA3,
                                    &texture_block);
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0x35, texture->type ? texture->type : "");
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, texture->uv_number);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x35);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, texture->shader_data_index);
    if (r != SF_OK) return r;

    if (texture->has_extended) {
        if (texture->unk_float_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_write_i32(bw, 0xA3);
        if (r != SF_OK) return r;
        r = mtd_write_marked_string(bw, 0xBA, texture->path ? texture->path : "");
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, (int32_t)texture->unk_float_count);
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_f32s(bw, texture->unk_float_count, texture->unk_floats);
        if (r != SF_OK) return r;
    }
    return mtd_block_finish(bw, &texture_block);
}

static sf_result_t mtd_write_to_writer(const sf_mtd_t *mtd, sf_binary_writer_t *bw) {
    if (mtd->param_count > (size_t)INT32_MAX || mtd->texture_count > (size_t)INT32_MAX) {
        return SF_ERR_OUT_OF_RANGE;
    }

    sf_binary_writer_set_big_endian(bw, false);

    mtd_block_t file_block;
    sf_result_t r = mtd_block_write(bw, 0, 3, 0x01, &file_block);
    if (r != SF_OK) return r;
    mtd_block_t header_block;
    r = mtd_block_write(bw, 1, 2, 0xB0, &header_block);
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0x34, "MTD ");
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 1000);
    if (r != SF_OK) return r;
    r = mtd_block_finish(bw, &header_block);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x01);
    if (r != SF_OK) return r;

    mtd_block_t data_block;
    r = mtd_block_write(bw, 2, 4, 0xA3, &data_block);
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0xA3, mtd->shader_path ? mtd->shader_path : "");
    if (r != SF_OK) return r;
    r = mtd_write_marked_string(bw, 0x03, mtd->description ? mtd->description : "");
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 1);
    if (r != SF_OK) return r;

    mtd_block_t lists_block;
    r = mtd_block_write(bw, 3, 4, 0xA3, &lists_block);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x03);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)mtd->param_count);
    if (r != SF_OK) return r;
    for (size_t i = 0; i < mtd->param_count; i++) {
        r = mtd_write_param(bw, &mtd->params[i]);
        if (r != SF_OK) return r;
    }
    r = mtd_write_marker(bw, 0x03);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, (int32_t)mtd->texture_count);
    if (r != SF_OK) return r;
    for (size_t i = 0; i < mtd->texture_count; i++) {
        r = mtd_write_texture(bw, &mtd->textures[i]);
        if (r != SF_OK) return r;
    }
    r = mtd_write_marker(bw, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    r = mtd_block_finish(bw, &lists_block);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    r = mtd_block_finish(bw, &data_block);
    if (r != SF_OK) return r;
    r = mtd_write_marker(bw, 0x04);
    if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) return r;
    return mtd_block_finish(bw, &file_block);
}

sf_result_t sf_mtd_write_to_memory(const sf_mtd_t *mtd, uint8_t **out_data,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(mtd != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) {
        sf_ostream_close(stream);
        return r;
    }

    r = mtd_write_to_writer(mtd, bw);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out_data, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_mtd_write_to_stream(const sf_mtd_t *mtd, sf_ostream_t *stream,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(mtd != NULL && stream != NULL);
    alloc = sf_alloc_or_default(alloc);
    sf_binary_writer_t *bw = NULL;
    sf_result_t r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) return r;
    r = mtd_write_to_writer(mtd, bw);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    sf_binary_writer_destroy(bw);
    return r;
}

sf_result_t sf_mtd_write_to_path(const sf_mtd_t *mtd, const wchar_t *path,
                                 const sf_allocator_t *alloc) {
    SF_CHECK_ARG(mtd != NULL && path != NULL);
    alloc = sf_alloc_or_default(alloc);
    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_wfile(&stream, path, alloc);
    if (r != SF_OK) return r;
    r = sf_mtd_write_to_stream(mtd, stream, alloc);
    sf_ostream_close(stream);
    return r;
}

const char *sf_mtd_shader_path(const sf_mtd_t *mtd) {
    return (mtd && mtd->shader_path) ? mtd->shader_path : "";
}

const char *sf_mtd_description(const sf_mtd_t *mtd) {
    return (mtd && mtd->description) ? mtd->description : "";
}

size_t sf_mtd_param_count(const sf_mtd_t *mtd) { return mtd ? mtd->param_count : 0; }

const sf_mtd_param_t *sf_mtd_param(const sf_mtd_t *mtd, size_t index) {
    if (!mtd || index >= mtd->param_count) return NULL;
    return &mtd->params[index];
}

const char *sf_mtd_param_name(const sf_mtd_param_t *param) {
    return (param && param->name) ? param->name : "";
}

sf_mtd_param_type_t sf_mtd_param_type(const sf_mtd_param_t *param) {
    return param ? param->type : SF_MTD_PARAM_TYPE_BOOL;
}

sf_result_t sf_mtd_param_value_bool(const sf_mtd_param_t *param, bool *out_value) {
    SF_CHECK_ARG(param != NULL && out_value != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_BOOL) return SF_ERR_INVALID_ARG;
    *out_value = param->value.b;
    return SF_OK;
}

sf_result_t sf_mtd_param_value_int(const sf_mtd_param_t *param, int32_t *out_value) {
    SF_CHECK_ARG(param != NULL && out_value != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_INT) return SF_ERR_INVALID_ARG;
    *out_value = param->value.i;
    return SF_OK;
}

sf_result_t sf_mtd_param_value_int2(const sf_mtd_param_t *param, int32_t out_values[2]) {
    SF_CHECK_ARG(param != NULL && out_values != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_INT2) return SF_ERR_INVALID_ARG;
    out_values[0] = param->value.i2[0];
    out_values[1] = param->value.i2[1];
    return SF_OK;
}

sf_result_t sf_mtd_param_value_float(const sf_mtd_param_t *param, float *out_value) {
    SF_CHECK_ARG(param != NULL && out_value != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_FLOAT) return SF_ERR_INVALID_ARG;
    *out_value = param->value.f;
    return SF_OK;
}

sf_result_t sf_mtd_param_value_float2(const sf_mtd_param_t *param, float out_values[2]) {
    SF_CHECK_ARG(param != NULL && out_values != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_FLOAT2) return SF_ERR_INVALID_ARG;
    out_values[0] = param->value.f4[0];
    out_values[1] = param->value.f4[1];
    return SF_OK;
}

sf_result_t sf_mtd_param_value_float3(const sf_mtd_param_t *param, float out_values[3]) {
    SF_CHECK_ARG(param != NULL && out_values != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_FLOAT3) return SF_ERR_INVALID_ARG;
    out_values[0] = param->value.f4[0];
    out_values[1] = param->value.f4[1];
    out_values[2] = param->value.f4[2];
    return SF_OK;
}

sf_result_t sf_mtd_param_value_float4(const sf_mtd_param_t *param, float out_values[4]) {
    SF_CHECK_ARG(param != NULL && out_values != NULL);
    if (param->type != SF_MTD_PARAM_TYPE_FLOAT4) return SF_ERR_INVALID_ARG;
    out_values[0] = param->value.f4[0];
    out_values[1] = param->value.f4[1];
    out_values[2] = param->value.f4[2];
    out_values[3] = param->value.f4[3];
    return SF_OK;
}

size_t sf_mtd_texture_count(const sf_mtd_t *mtd) { return mtd ? mtd->texture_count : 0; }

const sf_mtd_texture_t *sf_mtd_texture(const sf_mtd_t *mtd, size_t index) {
    if (!mtd || index >= mtd->texture_count) return NULL;
    return &mtd->textures[index];
}

const char *sf_mtd_texture_type(const sf_mtd_texture_t *texture) {
    return (texture && texture->type) ? texture->type : "";
}

int32_t sf_mtd_texture_uv_number(const sf_mtd_texture_t *texture) {
    return texture ? texture->uv_number : 0;
}

int32_t sf_mtd_texture_shader_data_index(const sf_mtd_texture_t *texture) {
    return texture ? texture->shader_data_index : 0;
}

bool sf_mtd_texture_has_extended(const sf_mtd_texture_t *texture) {
    return texture ? texture->has_extended : false;
}

const char *sf_mtd_texture_path(const sf_mtd_texture_t *texture) {
    return (texture && texture->path) ? texture->path : "";
}

size_t sf_mtd_texture_unk_float_count(const sf_mtd_texture_t *texture) {
    return texture ? texture->unk_float_count : 0;
}

float sf_mtd_texture_unk_float(const sf_mtd_texture_t *texture, size_t index) {
    if (!texture || index >= texture->unk_float_count || !texture->unk_floats) return 0.0f;
    return texture->unk_floats[index];
}
