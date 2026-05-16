#include "internal/flver2_internal.h"
#include "souls_formats/sf_flver.h"
#include "souls_formats/sf_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Helper functions for reading/writing */
static inline float read_f32(const uint8_t *p) {
    float v;
    memcpy(&v, p, 4);
    return v;
}

static inline void write_f32(uint8_t *p, float v) {
    memcpy(p, &v, 4);
}

static SF_UNUSED inline uint32_t read_u32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static SF_UNUSED inline void write_u32(uint8_t *p, uint32_t v) {
    memcpy(p, &v, 4);
}

static SF_UNUSED inline int32_t read_i32(const uint8_t *p) {
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

static SF_UNUSED inline void write_i32(uint8_t *p, int32_t v) {
    memcpy(p, &v, 4);
}

static inline uint16_t read_u16(const uint8_t *p) {
    uint16_t v;
    memcpy(&v, p, 2);
    return v;
}

static inline void write_u16(uint8_t *p, uint16_t v) {
    memcpy(p, &v, 2);
}

static inline int16_t read_i16(const uint8_t *p) {
    int16_t v;
    memcpy(&v, p, 2);
    return v;
}

static inline void write_i16(uint8_t *p, int16_t v) {
    memcpy(p, &v, 2);
}

static inline uint8_t read_u8(const uint8_t *p) {
    return *p;
}

static inline void write_u8(uint8_t *p, uint8_t v) {
    *p = v;
}

static inline int8_t read_i8(const uint8_t *p) {
    return (int8_t)*p;
}

static inline void write_i8(uint8_t *p, int8_t v) {
    *p = (uint8_t)v;
}

/* Normalization helpers */
static inline sf_vec3_t read_byte_norm_xyz(const uint8_t *p) {
    sf_vec3_t v;
    v.x = (read_u8(p + 0) - 127.0f) / 127.0f;
    v.y = (read_u8(p + 1) - 127.0f) / 127.0f;
    v.z = (read_u8(p + 2) - 127.0f) / 127.0f;
    return v;
}

static inline void write_byte_norm_xyz(uint8_t *p, sf_vec3_t v) {
    write_u8(p + 0, (uint8_t)roundf(v.x * 127.0f + 127.0f));
    write_u8(p + 1, (uint8_t)roundf(v.y * 127.0f + 127.0f));
    write_u8(p + 2, (uint8_t)roundf(v.z * 127.0f + 127.0f));
}

static inline sf_vec3_t read_sbyte_norm_zyx(const uint8_t *p) {
    sf_vec3_t v;
    v.z = read_i8(p + 0) / 127.0f;
    v.y = read_i8(p + 1) / 127.0f;
    v.x = read_i8(p + 2) / 127.0f;
    return v;
}

static inline void write_sbyte_norm_zyx(uint8_t *p, sf_vec3_t v) {
    write_i8(p + 0, (int8_t)roundf(v.z * 127.0f));
    write_i8(p + 1, (int8_t)roundf(v.y * 127.0f));
    write_i8(p + 2, (int8_t)roundf(v.x * 127.0f));
}

static inline sf_vec3_t read_short_norm_xyz(const uint8_t *p) {
    sf_vec3_t v;
    v.x = read_i16(p + 0) / 32767.0f;
    v.y = read_i16(p + 2) / 32767.0f;
    v.z = read_i16(p + 4) / 32767.0f;
    return v;
}

static inline void write_short_norm_xyz(uint8_t *p, sf_vec3_t v) {
    write_i16(p + 0, (int16_t)roundf(v.x * 32767.0f));
    write_i16(p + 2, (int16_t)roundf(v.y * 32767.0f));
    write_i16(p + 4, (int16_t)roundf(v.z * 32767.0f));
}

static inline sf_vec3_t read_ushort_norm_xyz(const uint8_t *p) {
    sf_vec3_t v;
    v.x = (read_u16(p + 0) - 32767.0f) / 32767.0f;
    v.y = (read_u16(p + 2) - 32767.0f) / 32767.0f;
    v.z = (read_u16(p + 4) - 32767.0f) / 32767.0f;
    return v;
}

static inline void write_ushort_norm_xyz(uint8_t *p, sf_vec3_t v) {
    write_u16(p + 0, (uint16_t)roundf(v.x * 32767.0f + 32767.0f));
    write_u16(p + 2, (uint16_t)roundf(v.y * 32767.0f + 32767.0f));
    write_u16(p + 4, (uint16_t)roundf(v.z * 32767.0f + 32767.0f));
}

static inline sf_vec3_t read_short_norm_xyz_ac6(const uint8_t *p) {
    sf_vec3_t v;
    v.x = read_i16(p + 0) / 32767.0f;
    v.y = read_i16(p + 2) / 32767.0f;
    v.z = read_i16(p + 4) / 32767.0f;
    return v;
}

static inline void write_short_norm_xyz_ac6(uint8_t *p, sf_vec3_t v) {
    write_i16(p + 0, (int16_t)roundf(v.x * 32767.0f));
    write_i16(p + 2, (int16_t)roundf(v.y * 32767.0f));
    write_i16(p + 4, (int16_t)roundf(v.z * 32767.0f));
}

static inline void write_short_norm_ac6(uint8_t *p, float w) {
    write_i16(p, (int16_t)roundf(w * 32767.0f));
}

static inline sf_vec4_t read_byte_norm_xyzw(const uint8_t *p) {
    sf_vec4_t v;
    v.x = (read_u8(p + 0) - 127.0f) / 127.0f;
    v.y = (read_u8(p + 1) - 127.0f) / 127.0f;
    v.z = (read_u8(p + 2) - 127.0f) / 127.0f;
    v.w = (read_u8(p + 3) - 127.0f) / 127.0f;
    return v;
}

static inline void write_byte_norm_xyzw(uint8_t *p, sf_vec4_t v) {
    write_u8(p + 0, (uint8_t)roundf(v.x * 127.0f + 127.0f));
    write_u8(p + 1, (uint8_t)roundf(v.y * 127.0f + 127.0f));
    write_u8(p + 2, (uint8_t)roundf(v.z * 127.0f + 127.0f));
    write_u8(p + 3, (uint8_t)roundf(v.w * 127.0f + 127.0f));
}

static inline sf_vec4_t read_sbyte_norm_wzyx(const uint8_t *p) {
    sf_vec4_t v;
    v.w = read_i8(p + 0) / 127.0f;
    v.z = read_i8(p + 1) / 127.0f;
    v.y = read_i8(p + 2) / 127.0f;
    v.x = read_i8(p + 3) / 127.0f;
    return v;
}

static inline void write_sbyte_norm_wzyx(uint8_t *p, sf_vec4_t v) {
    write_i8(p + 0, (int8_t)roundf(v.w * 127.0f));
    write_i8(p + 1, (int8_t)roundf(v.z * 127.0f));
    write_i8(p + 2, (int8_t)roundf(v.y * 127.0f));
    write_i8(p + 3, (int8_t)roundf(v.x * 127.0f));
}

static inline sf_vec4_t read_short_norm_xyzw(const uint8_t *p) {
    sf_vec4_t v;
    v.x = read_i16(p + 0) / 32767.0f;
    v.y = read_i16(p + 2) / 32767.0f;
    v.z = read_i16(p + 4) / 32767.0f;
    v.w = read_i16(p + 6) / 32767.0f;
    return v;
}

static inline void write_short_norm_xyzw(uint8_t *p, sf_vec4_t v) {
    write_i16(p + 0, (int16_t)roundf(v.x * 32767.0f));
    write_i16(p + 2, (int16_t)roundf(v.y * 32767.0f));
    write_i16(p + 4, (int16_t)roundf(v.z * 32767.0f));
    write_i16(p + 6, (int16_t)roundf(v.w * 32767.0f));
}

static inline sf_flver_vertex_color_t read_float_rgba(const uint8_t *p) {
    sf_flver_vertex_color_t c;
    c.r = read_f32(p + 0);
    c.g = read_f32(p + 4);
    c.b = read_f32(p + 8);
    c.a = read_f32(p + 12);
    return c;
}

static inline void write_float_rgba(uint8_t *p, sf_flver_vertex_color_t c) {
    write_f32(p + 0, c.r);
    write_f32(p + 4, c.g);
    write_f32(p + 8, c.b);
    write_f32(p + 12, c.a);
}

static inline sf_flver_vertex_color_t read_byte_rgba(const uint8_t *p) {
    sf_flver_vertex_color_t c;
    c.r = read_u8(p + 0) / 255.0f;
    c.g = read_u8(p + 1) / 255.0f;
    c.b = read_u8(p + 2) / 255.0f;
    c.a = read_u8(p + 3) / 255.0f;
    return c;
}

static inline void write_byte_rgba(uint8_t *p, sf_flver_vertex_color_t c) {
    write_u8(p + 0, (uint8_t)roundf(c.r * 255.0f));
    write_u8(p + 1, (uint8_t)roundf(c.g * 255.0f));
    write_u8(p + 2, (uint8_t)roundf(c.b * 255.0f));
    write_u8(p + 3, (uint8_t)roundf(c.a * 255.0f));
}

sf_result_t sfi_flver2_vertex_decode_one(
    const sf_flver2_buffer_layout_t *layout,
    const uint8_t *vertex_bytes,
    const sf_flver2_vertex_context_t *ctx,
    sf_flver2_decoded_vertex_t *out)
{
    memset(out, 0, sizeof(*out));

    for (size_t i = 0; i < layout->member_count; ++i) {
        const sf_flver2_layout_member_t *member = &layout->members[i];
        const uint8_t *p = vertex_bytes + member->struct_offset;

        if (member->special_modifier == -32768) {
            continue;
        }

        switch (member->semantic) {
            case SF_FLVER_LAYOUT_SEMANTIC_POSITION:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                    out->position.x = read_f32(p + 0);
                    out->position.y = read_f32(p + 4);
                    out->position.z = read_f32(p + 8);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    out->position.x = read_f32(p + 0);
                    out->position.y = read_f32(p + 4);
                    out->position.z = read_f32(p + 8);
                    /* w is ignored */
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED) {
                    return SF_ERR_UNSUPPORTED_VERSION;
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS:
                if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    for (int j = 0; j < 4; j++) out->bone_weights.v[j] = read_i8(p + j) / 127.0f;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    for (int j = 0; j < 4; j++) out->bone_weights.v[j] = read_u8(p + j) / 255.0f;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4) {
                    for (int j = 0; j < 4; j++) {
                        int32_t weight = read_u16(p + j * 2);
                        if (weight >= 0x8000) weight -= 0x8000;
                        else weight += 0x8000;
                        out->bone_weights.v[j] = weight / 65535.0f;
                    }
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    for (int j = 0; j < 4; j++) out->bone_weights.v[j] = read_i16(p + j * 2) / 32767.0f;
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES:
                if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    for (int j = 0; j < 4; j++) out->bone_indices.v[j] = read_u8(p + j);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT2) {
                    for (int j = 0; j < 2; j++) out->bone_indices.v[j] = read_u16(p + j * 2);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT4) {
                    for (int j = 0; j < 4; j++) out->bone_indices.v[j] = read_u16(p + j * 2);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    for (int j = 0; j < 4; j++) out->bone_indices.v[j] = read_u8(p + j);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                    for (int j = 0; j < 4; j++) out->bone_indices.v[j] = read_u8(p + j);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_NORMAL:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                    out->normal.x = read_f32(p + 0);
                    out->normal.y = read_f32(p + 4);
                    out->normal.z = read_f32(p + 8);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    out->normal.x = read_f32(p + 0);
                    out->normal.y = read_f32(p + 4);
                    out->normal.z = read_f32(p + 8);
                    out->normal_w = (int32_t)read_f32(p + 12);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    out->normal = read_byte_norm_xyz(p);
                    out->normal_w = read_u8(p + 3);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    out->normal = read_byte_norm_xyz(p);
                    out->normal_w = read_u8(p + 3);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                    out->normal_w = read_u8(p + 0);
                    out->normal = read_sbyte_norm_zyx(p + 1);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    out->normal = read_byte_norm_xyz(p);
                    out->normal_w = read_u8(p + 3);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    out->normal = read_short_norm_xyz(p);
                    out->normal_w = read_i16(p + 6);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF4) {
                    out->normal = read_ushort_norm_xyz(p);
                    out->normal_w = read_i16(p + 6);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    out->normal = read_byte_norm_xyz(p);
                    out->normal_w = read_u8(p + 3);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT4) {
                    out->normal = read_short_norm_xyz_ac6(p);
                    out->normal_w = read_i16(p + 6);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_UV:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT2) {
                    out->uvs[out->uv_count].x = read_f32(p + 0);
                    out->uvs[out->uv_count].y = read_f32(p + 4);
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                    out->uvs[out->uv_count].x = read_f32(p + 0);
                    out->uvs[out->uv_count].y = read_f32(p + 4);
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    out->uvs[out->uv_count].x = read_f32(p + 0);
                    out->uvs[out->uv_count].y = read_f32(p + 4);
                    out->uv_count++;
                    out->uvs[out->uv_count].x = read_f32(p + 8);
                    out->uvs[out->uv_count].y = read_f32(p + 12);
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    out->uvs[out->uv_count].x = read_u8(p + 0) / 255.0f;
                    out->uvs[out->uv_count].y = read_u8(p + 1) / 255.0f;
                    out->uv_count++;
                    out->uvs[out->uv_count].x = read_u8(p + 2) / 255.0f;
                    out->uvs[out->uv_count].y = read_u8(p + 3) / 255.0f;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT2) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF2) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                    out->uvs[out->uv_count].x = read_i16(p + 4) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 6) / ctx->uv_factor;
                    out->uv_count++;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF4) {
                    out->uvs[out->uv_count].x = read_i16(p + 0) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 2) / ctx->uv_factor;
                    out->uv_count++;
                    out->uvs[out->uv_count].x = read_i16(p + 4) / ctx->uv_factor;
                    out->uvs[out->uv_count].y = read_i16(p + 6) / ctx->uv_factor;
                    out->uv_count++;
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_TANGENT:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    out->tangent.x = read_f32(p + 0);
                    out->tangent.y = read_f32(p + 4);
                    out->tangent.z = read_f32(p + 8);
                    out->tangent.w = read_f32(p + 12);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    out->tangent = read_byte_norm_xyzw(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    out->tangent = read_byte_norm_xyzw(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    out->tangent = read_byte_norm_xyzw(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4_NORM) {
                    out->tangent = read_sbyte_norm_wzyx(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    out->tangent = read_short_norm_xyzw(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    out->tangent = read_byte_norm_xyzw(p);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BITANGENT:
                if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    out->bitangent.x = read_byte_norm_xyzw(p).x;
                    out->bitangent.y = read_byte_norm_xyzw(p).y;
                    out->bitangent.z = read_byte_norm_xyzw(p).z;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    out->bitangent.x = read_byte_norm_xyzw(p).x;
                    out->bitangent.y = read_byte_norm_xyzw(p).y;
                    out->bitangent.z = read_byte_norm_xyzw(p).z;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    out->bitangent.x = read_byte_norm_xyzw(p).x;
                    out->bitangent.y = read_byte_norm_xyzw(p).y;
                    out->bitangent.z = read_byte_norm_xyzw(p).z;
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    out->bitangent.x = read_byte_norm_xyzw(p).x;
                    out->bitangent.y = read_byte_norm_xyzw(p).y;
                    out->bitangent.z = read_byte_norm_xyzw(p).z;
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    out->colors[out->color_count++] = read_float_rgba(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    out->colors[out->color_count++] = read_byte_rgba(p);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    out->colors[out->color_count++] = read_byte_rgba(p);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            default:
                fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                return SF_ERR_UNSUPPORTED_VERSION;
        }
    }

    return SF_OK;
}

sf_result_t sfi_flver2_vertex_encode_one(
    const sf_flver2_buffer_layout_t *layout,
    const sf_flver2_decoded_vertex_t *in,
    const sf_flver2_vertex_context_t *ctx,
    uint8_t *vertex_bytes)
{
    uint8_t uv_idx = 0;
    uint8_t color_idx = 0;

    for (size_t i = 0; i < layout->member_count; ++i) {
        const sf_flver2_layout_member_t *member = &layout->members[i];
        uint8_t *p = vertex_bytes + member->struct_offset;

        if (member->special_modifier == -32768) {
            continue;
        }

        switch (member->semantic) {
            case SF_FLVER_LAYOUT_SEMANTIC_POSITION:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                    write_f32(p + 0, in->position.x);
                    write_f32(p + 4, in->position.y);
                    write_f32(p + 8, in->position.z);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    write_f32(p + 0, in->position.x);
                    write_f32(p + 4, in->position.y);
                    write_f32(p + 8, in->position.z);
                    write_f32(p + 12, 0.0f);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_EDGE_COMPRESSED) {
                    return SF_ERR_UNSUPPORTED_VERSION;
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BONE_WEIGHTS:
                if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    for (int j = 0; j < 4; j++) write_i8(p + j, (int8_t)roundf(in->bone_weights.v[j] * 127.0f));
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    for (int j = 0; j < 4; j++) write_u8(p + j, (uint8_t)roundf(in->bone_weights.v[j] * 255.0f));
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4) {
                    for (int j = 0; j < 4; j++) {
                        double weight = round(in->bone_weights.v[j] * 65535.0);
                        if (weight > 0x8000) weight -= 0x8000;
                        else weight += 0x8000;
                        write_u16(p + j * 2, (uint16_t)weight);
                    }
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    for (int j = 0; j < 4; j++) write_i16(p + j * 2, (int16_t)roundf(in->bone_weights.v[j] * 32767.0f));
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BONE_INDICES:
                if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    for (int j = 0; j < 4; j++) write_u8(p + j, (uint8_t)in->bone_indices.v[j]);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT2) {
                    for (int j = 0; j < 2; j++) write_u16(p + j * 2, (uint16_t)in->bone_indices.v[j]);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT4) {
                    for (int j = 0; j < 4; j++) write_u16(p + j * 2, (uint16_t)in->bone_indices.v[j]);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    for (int j = 0; j < 4; j++) write_u8(p + j, (uint8_t)in->bone_indices.v[j]);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                    for (int j = 0; j < 4; j++) write_u8(p + j, (uint8_t)in->bone_indices.v[j]);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_NORMAL:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                    write_f32(p + 0, in->normal.x);
                    write_f32(p + 4, in->normal.y);
                    write_f32(p + 8, in->normal.z);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    write_f32(p + 0, in->normal.x);
                    write_f32(p + 4, in->normal.y);
                    write_f32(p + 8, in->normal.z);
                    write_f32(p + 12, (float)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    write_byte_norm_xyz(p, in->normal);
                    write_u8(p + 3, (uint8_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    write_byte_norm_xyz(p, in->normal);
                    write_u8(p + 3, (uint8_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                    write_u8(p + 0, (uint8_t)in->normal_w);
                    write_sbyte_norm_zyx(p + 1, in->normal);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    write_byte_norm_xyz(p, in->normal);
                    write_u8(p + 3, (uint8_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    write_short_norm_xyz(p, in->normal);
                    write_i16(p + 6, (int16_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF4) {
                    write_ushort_norm_xyz(p, in->normal);
                    write_i16(p + 6, (int16_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    write_byte_norm_xyz(p, in->normal);
                    write_u8(p + 3, (uint8_t)in->normal_w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_USHORT4) {
                    write_short_norm_xyz_ac6(p, in->normal);
                    write_short_norm_ac6(p + 6, (float)in->normal_w);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_UV:
                {
                    sf_vec2_t uv = in->uvs[uv_idx++];
                    if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT2) {
                        write_f32(p + 0, uv.x);
                        write_f32(p + 4, uv.y);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT3) {
                        write_f32(p + 0, uv.x);
                        write_f32(p + 4, uv.y);
                        write_f32(p + 8, 0.0f);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                        write_f32(p + 0, uv.x);
                        write_f32(p + 4, uv.y);
                        uv = in->uvs[uv_idx++];
                        write_f32(p + 8, uv.x);
                        write_f32(p + 12, uv.y);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                        write_u8(p + 0, (uint8_t)roundf(uv.x * 255.0f));
                        write_u8(p + 1, (uint8_t)roundf(uv.y * 255.0f));
                        uv = in->uvs[uv_idx++];
                        write_u8(p + 2, (uint8_t)roundf(uv.x * 255.0f));
                        write_u8(p + 3, (uint8_t)roundf(uv.y * 255.0f));
                    } else {
                        uv.x *= ctx->uv_factor;
                        uv.y *= ctx->uv_factor;
                        if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT2) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF2) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                            uv = in->uvs[uv_idx++];
                            uv.x *= ctx->uv_factor;
                            uv.y *= ctx->uv_factor;
                            write_i16(p + 4, (int16_t)roundf(uv.x));
                            write_i16(p + 6, (int16_t)roundf(uv.y));
                        } else if (member->type == SF_FLVER_LAYOUT_TYPE_HALF4) {
                            write_i16(p + 0, (int16_t)roundf(uv.x));
                            write_i16(p + 2, (int16_t)roundf(uv.y));
                            uv = in->uvs[uv_idx++];
                            uv.x *= ctx->uv_factor;
                            uv.y *= ctx->uv_factor;
                            write_i16(p + 4, (int16_t)roundf(uv.x));
                            write_i16(p + 6, (int16_t)roundf(uv.y));
                        } else {
                            fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                            return SF_ERR_UNSUPPORTED_VERSION;
                        }
                    }
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_TANGENT:
                if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                    write_f32(p + 0, in->tangent.x);
                    write_f32(p + 4, in->tangent.y);
                    write_f32(p + 8, in->tangent.z);
                    write_f32(p + 12, in->tangent.w);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                    write_byte_norm_xyzw(p, in->tangent);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                    write_byte_norm_xyzw(p, in->tangent);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                    write_byte_norm_xyzw(p, in->tangent);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4_NORM) {
                    write_sbyte_norm_wzyx(p, in->tangent);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_SHORT4_NORM) {
                    write_short_norm_xyzw(p, in->tangent);
                } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                    write_byte_norm_xyzw(p, in->tangent);
                } else {
                    fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                    return SF_ERR_UNSUPPORTED_VERSION;
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_BITANGENT:
                {
                    sf_vec4_t bitangent = {in->bitangent.x, in->bitangent.y, in->bitangent.z, 0.0f};
                    if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                        write_byte_norm_xyzw(p, bitangent);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4) {
                        write_byte_norm_xyzw(p, bitangent);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                        write_byte_norm_xyzw(p, bitangent);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_BYTE4E) {
                        write_byte_norm_xyzw(p, bitangent);
                    } else {
                        fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                        return SF_ERR_UNSUPPORTED_VERSION;
                    }
                }
                break;

            case SF_FLVER_LAYOUT_SEMANTIC_VERTEX_COLOR:
                {
                    sf_flver_vertex_color_t color = in->colors[color_idx++];
                    if (member->type == SF_FLVER_LAYOUT_TYPE_FLOAT4) {
                        write_float_rgba(p, color);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_COLOR) {
                        write_byte_rgba(p, color);
                    } else if (member->type == SF_FLVER_LAYOUT_TYPE_UBYTE4_NORM) {
                        write_byte_rgba(p, color);
                    } else {
                        fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                        return SF_ERR_UNSUPPORTED_VERSION;
                    }
                }
                break;

            default:
                fprintf(stderr, "KNOWN_LAYOUT_GAP: type=0x%X semantic=0x%X\n", member->type, member->semantic);
                return SF_ERR_UNSUPPORTED_VERSION;
        }
    }

    return SF_OK;
}
