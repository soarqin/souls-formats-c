/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 6 T13 — FLVER2 Material + Texture + TilingType read/write.
 *
 * Strict mirror of upstream SoulsFormatsNEXT at the pinned commit:
 *   - SoulsFormats/Formats/FLVER/FLVER2/Material.cs
 *   - SoulsFormats/Formats/FLVER/FLVER2/Texture.cs
 *   - SoulsFormats/Formats/FLVER/FLVER2/GXList.cs  (inline-parsed by Material)
 *
 * The Material constructor in upstream triggers GXList parsing as a side
 * effect (via a Dictionary<int,int> to dedup gxOffset -> gx_index). We
 * mirror that here using linear search since FLVER2 files typically
 * contain only a handful of GX lists per file.
 */

#include "souls_formats/sf_flver2.h"

#include "internal/flver2_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_math.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * GX list parsing — mirrors upstream GXList.cs constructor.
 *
 * Called inline by sfi_flver2_material_read when it sees a gxOffset != 0
 * that has not yet been parsed. The gx_offset dedup map is owned by the
 * caller (flver2_read_sections); we just append to it.
 *===========================================================================*/

static sf_result_t flver2_gx_item_read(sf_binary_reader_t *br,
                                       const sf_flver2_header_t *hdr,
                                       sf_flver2_gx_item_t *out,
                                       const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    sf_result_t r;

    if (hdr->version <= 0x20010u) {
        int32_t id_int = 0;
        if ((r = sf_binary_reader_read_i32(br, &id_int)) != SF_OK) return r;
        out->id = (uint32_t)id_int;
    } else {
        uint32_t id_word = 0;
        if ((r = sf_binary_reader_read_u32(br, &id_word)) != SF_OK) return r;
        out->id = id_word;
    }
    if ((r = sf_binary_reader_read_u32(br, &out->unk04)) != SF_OK) return r;

    int32_t length = 0;
    if ((r = sf_binary_reader_read_i32(br, &length)) != SF_OK) return r;
    if (length < 0x0C) return SF_ERR_OUT_OF_RANGE;

    size_t blob = (size_t)length - 0x0C;
    out->data_size = blob;
    if (blob > 0) {
        out->data = (uint8_t *)sf_xalloc(a, blob);
        if (!out->data) return SF_ERR_OOM;
        if ((r = sf_binary_reader_read_bytes(br, out->data, blob)) != SF_OK) {
            sf_xfree(a, out->data);
            out->data = NULL;
            return r;
        }
    }
    return SF_OK;
}

static sf_result_t flver2_gx_list_read(sf_binary_reader_t *br,
                                       const sf_flver2_header_t *hdr,
                                       sf_flver2_gx_list_t *out,
                                       const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->terminator_id     = (int32_t)INT32_MAX;
    out->terminator_length = 0;

    sf_result_t r;

    if (hdr->version < 0x20010u) {
        out->items = (sf_flver2_gx_item_t *)sf_xalloc(a, sizeof(*out->items));
        if (!out->items) return SF_ERR_OOM;
        memset(out->items, 0, sizeof(*out->items));
        out->count = 1;
        if ((r = flver2_gx_item_read(br, hdr, &out->items[0], a)) != SF_OK) {
            sf_xfree(a, out->items[0].data);
            sf_xfree(a, out->items);
            out->items = NULL;
            out->count = 0;
            return r;
        }
        return SF_OK;
    }

    size_t capacity = 0;
    for (;;) {
        int32_t peek = 0;
        int64_t pos = sf_binary_reader_position(br);
        r = sf_binary_reader_get_i32(br, pos, &peek);
        if (r != SF_OK) return r;
        if (peek == (int32_t)INT32_MAX || peek == -1) break;

        if (out->count == capacity) {
            size_t new_cap = capacity ? capacity * 2 : 4;
            sf_flver2_gx_item_t *grown = (sf_flver2_gx_item_t *)sf_xrealloc(
                a, out->items, capacity * sizeof(*out->items),
                new_cap * sizeof(*out->items));
            if (!grown) return SF_ERR_OOM;
            memset(grown + capacity, 0, (new_cap - capacity) * sizeof(*grown));
            out->items = grown;
            capacity = new_cap;
        }
        r = flver2_gx_item_read(br, hdr, &out->items[out->count], a);
        if (r != SF_OK) return r;
        out->count++;
    }

    int32_t terminator_id = 0;
    if ((r = sf_binary_reader_read_i32(br, &terminator_id)) != SF_OK) return r;
    if (terminator_id != (int32_t)INT32_MAX && terminator_id != -1) {
        return SF_ERR_BAD_MAGIC;
    }
    out->terminator_id = terminator_id;
    if ((r = sf_binary_reader_assert_i32_one(br, 100)) != SF_OK) return r;

    int32_t length_field = 0;
    if ((r = sf_binary_reader_read_i32(br, &length_field)) != SF_OK) return r;
    int32_t length_subtraction = (hdr->unk68 == 5) ? 0 : 0x0C;
    int32_t terminator_length = length_field - length_subtraction;
    if (terminator_length < 0) return SF_ERR_OUT_OF_RANGE;
    out->terminator_length = terminator_length;
    if (terminator_length > 0) {
        if ((r = sf_binary_reader_assert_pattern(br, (size_t)terminator_length, 0)) != SF_OK) {
            return r;
        }
    }
    return SF_OK;
}

static sf_result_t flver2_resolve_gx_offset(sf_flver2_t *flver,
                                            sf_binary_reader_t *br,
                                            int32_t gx_offset,
                                            int32_t *out_gx_index) {
    *out_gx_index = -1;
    if (gx_offset == 0) return SF_OK;

    for (size_t i = 0; i < flver->gx_list_count; i++) {
        if (flver->gx_offsets_internal && flver->gx_offsets_internal[i] == gx_offset) {
            *out_gx_index = (int32_t)i;
            return SF_OK;
        }
    }

    size_t new_count = flver->gx_list_count + 1;
    sf_flver2_gx_list_t *grown_lists = (sf_flver2_gx_list_t *)sf_xrealloc(
        flver->alloc, flver->gx_lists,
        flver->gx_list_count * sizeof(*flver->gx_lists),
        new_count * sizeof(*flver->gx_lists));
    if (!grown_lists) return SF_ERR_OOM;
    flver->gx_lists = grown_lists;
    memset(&flver->gx_lists[flver->gx_list_count], 0, sizeof(flver->gx_lists[0]));

    int32_t *grown_offsets = (int32_t *)sf_xrealloc(
        flver->alloc, flver->gx_offsets_internal,
        flver->gx_list_count * sizeof(int32_t),
        new_count * sizeof(int32_t));
    if (!grown_offsets) return SF_ERR_OOM;
    flver->gx_offsets_internal = grown_offsets;
    flver->gx_offsets_internal[flver->gx_list_count] = gx_offset;

    sf_result_t r = sf_binary_reader_step_in(br, (int64_t)gx_offset);
    if (r != SF_OK) return r;
    r = flver2_gx_list_read(br, &flver->header, &flver->gx_lists[flver->gx_list_count],
                            flver->alloc);
    sf_result_t r_out = sf_binary_reader_step_out(br);
    if (r != SF_OK) return r;
    if (r_out != SF_OK) return r_out;

    *out_gx_index = (int32_t)flver->gx_list_count;
    flver->gx_list_count = new_count;
    return SF_OK;
}

/*===========================================================================
 * Texture — 32-byte fixed-size record + 2 string pool entries.
 *===========================================================================*/

sf_result_t sfi_flver2_texture_read(sf_binary_reader_t *br,
                                    const sf_flver2_header_t *hdr,
                                    sf_flver2_texture_t *out,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && hdr != NULL && out != NULL);
    memset(out, 0, sizeof(*out));

    sf_result_t r;
    int32_t path_offset = 0;
    int32_t type_offset = 0;
    uint8_t tiling_u_raw = 0;
    uint8_t tiling_v_raw = 0;

    if ((r = sf_binary_reader_read_i32 (br, &path_offset))      != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32 (br, &type_offset))      != SF_OK) return r;
    if ((r = sf_binary_reader_read_vec2(br, &out->tiling_scale))!= SF_OK) return r;

    if ((r = sf_binary_reader_read_u8(br, &tiling_u_raw))       != SF_OK) return r;
    if ((r = sf_binary_reader_read_u8(br, &tiling_v_raw))       != SF_OK) return r;
    if ((r = sf_binary_reader_assert_u8_one(br, 0))             != SF_OK) return r;
    if ((r = sf_binary_reader_assert_u8_one(br, 0))             != SF_OK) return r;

    if ((r = sf_binary_reader_read_f32(br, &out->unk14))        != SF_OK) return r;
    if ((r = sf_binary_reader_read_f32(br, &out->unk18))        != SF_OK) return r;
    if ((r = sf_binary_reader_read_f32(br, &out->unk1c))        != SF_OK) return r;

    out->tiling_type_u = (sf_flver2_tiling_type_t)tiling_u_raw;
    out->tiling_type_v = (sf_flver2_tiling_type_t)tiling_v_raw;

    if (hdr->unicode) {
        if ((r = sf_binary_reader_get_utf16(br, type_offset, &out->param_name, NULL)) != SF_OK) {
            return r;
        }
        if ((r = sf_binary_reader_get_utf16(br, path_offset, &out->path, NULL)) != SF_OK) {
            sf_xfree(a, out->param_name);
            out->param_name = NULL;
            return r;
        }
    } else {
        if ((r = sf_binary_reader_get_shift_jis(br, type_offset, &out->param_name, NULL)) != SF_OK) {
            return r;
        }
        if ((r = sf_binary_reader_get_shift_jis(br, path_offset, &out->path, NULL)) != SF_OK) {
            sf_xfree(a, out->param_name);
            out->param_name = NULL;
            return r;
        }
    }
    return SF_OK;
}

sf_result_t sfi_flver2_texture_write(sf_binary_writer_t *bw,
                                     const sf_flver2_header_t *hdr,
                                     const sf_flver2_texture_t *t,
                                     size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && t != NULL);

    sf_result_t r;
    char path_name[40];
    char type_name[40];
    int n;
    n = snprintf(path_name, sizeof(path_name), "TexturePath%zu", index);
    if (n < 0 || (size_t)n >= sizeof(path_name)) return SF_ERR_INTERNAL;
    n = snprintf(type_name, sizeof(type_name), "TextureType%zu", index);
    if (n < 0 || (size_t)n >= sizeof(type_name)) return SF_ERR_INTERNAL;

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, path_name), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, type_name), return r);
    if ((r = sf_binary_writer_write_vec2 (bw, t->tiling_scale))           != SF_OK) return r;

    if ((r = sf_binary_writer_write_u8(bw, (uint8_t)t->tiling_type_u))    != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, (uint8_t)t->tiling_type_v))    != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0))                            != SF_OK) return r;
    if ((r = sf_binary_writer_write_u8(bw, 0))                            != SF_OK) return r;

    if ((r = sf_binary_writer_write_f32(bw, t->unk14))                    != SF_OK) return r;
    if ((r = sf_binary_writer_write_f32(bw, t->unk18))                    != SF_OK) return r;
    if ((r = sf_binary_writer_write_f32(bw, t->unk1c))                    != SF_OK) return r;
    return SF_OK;
}

sf_result_t sfi_flver2_texture_write_strings(sf_binary_writer_t *bw,
                                              const sf_flver2_header_t *hdr,
                                              const sf_flver2_texture_t *t,
                                              size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && t != NULL);

    sf_result_t r;
    char path_name[40];
    char type_name[40];
    int n;
    n = snprintf(path_name, sizeof(path_name), "TexturePath%zu", index);
    if (n < 0 || (size_t)n >= sizeof(path_name)) return SF_ERR_INTERNAL;
    n = snprintf(type_name, sizeof(type_name), "TextureType%zu", index);
    if (n < 0 || (size_t)n >= sizeof(type_name)) return SF_ERR_INTERNAL;

    if ((r = sf_binary_writer_fill_i32(bw, path_name,
                                       (int32_t)sf_binary_writer_position(bw))) != SF_OK) {
        return r;
    }
    const char *path = t->path ? t->path : "";
    r = hdr->unicode ? sf_binary_writer_write_utf16(bw, path, true)
                     : sf_binary_writer_write_shift_jis(bw, path, true);
    if (r != SF_OK) return r;

    if ((r = sf_binary_writer_fill_i32(bw, type_name,
                                       (int32_t)sf_binary_writer_position(bw))) != SF_OK) {
        return r;
    }
    const char *param_name = t->param_name ? t->param_name : "";
    return hdr->unicode ? sf_binary_writer_write_utf16(bw, param_name, true)
                        : sf_binary_writer_write_shift_jis(bw, param_name, true);
}

void sfi_flver2_texture_destroy_inplace(sf_flver2_texture_t *t, const sf_allocator_t *a) {
    if (!t) return;
    sf_xfree(a, t->param_name);
    sf_xfree(a, t->path);
    t->param_name = NULL;
    t->path       = NULL;
}

/*===========================================================================
 * Material — 32-byte fixed-size record + 2 string pool entries +
 * (optional) inline GX list.
 *===========================================================================*/

sf_result_t sfi_flver2_material_read(sf_binary_reader_t *br,
                                     sf_flver2_t *flver,
                                     sf_flver2_material_t *out,
                                     const sf_allocator_t *a) {
    SF_CHECK_ARG(br != NULL && flver != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    out->gx_index             = -1;
    out->pretake_texture_index = -1;
    out->pretake_texture_count = -1;

    sf_result_t r;
    int32_t name_offset    = 0;
    int32_t mtd_offset     = 0;
    int32_t texture_count  = 0;
    int32_t texture_index  = 0;
    int32_t num_str_bytes  = 0;
    int32_t gx_offset      = 0;

    if ((r = sf_binary_reader_read_i32(br, &name_offset))   != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &mtd_offset))    != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &texture_count)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &texture_index)) != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &num_str_bytes)) != SF_OK) return r;
    (void)num_str_bytes;
    if ((r = sf_binary_reader_read_i32(br, &gx_offset))     != SF_OK) return r;
    if ((r = sf_binary_reader_read_i32(br, &out->index))    != SF_OK) return r;
    if ((r = sf_binary_reader_assert_i32_one(br, 0))        != SF_OK) return r;

    if (texture_count < 0 || texture_index < 0) return SF_ERR_OUT_OF_RANGE;
    out->pretake_texture_index = texture_index;
    out->pretake_texture_count = texture_count;

    if (flver->header.unicode) {
        if ((r = sf_binary_reader_get_utf16(br, name_offset, &out->name, NULL)) != SF_OK) {
            return r;
        }
        if ((r = sf_binary_reader_get_utf16(br, mtd_offset, &out->mtd, NULL)) != SF_OK) {
            sf_xfree(a, out->name);
            out->name = NULL;
            return r;
        }
    } else {
        if ((r = sf_binary_reader_get_shift_jis(br, name_offset, &out->name, NULL)) != SF_OK) {
            return r;
        }
        if ((r = sf_binary_reader_get_shift_jis(br, mtd_offset, &out->mtd, NULL)) != SF_OK) {
            sf_xfree(a, out->name);
            out->name = NULL;
            return r;
        }
    }

    return flver2_resolve_gx_offset(flver, br, gx_offset, &out->gx_index);
}

sf_result_t sfi_flver2_material_write(sf_binary_writer_t *bw,
                                      const sf_flver2_header_t *hdr,
                                      const sf_flver2_material_t *m,
                                      size_t index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && m != NULL);
    (void)hdr;

    sf_result_t r;
    char name_reserve[40];
    char mtd_reserve[40];
    char tex_reserve[40];
    char gx_reserve[40];
    int n;
    n = snprintf(name_reserve, sizeof(name_reserve), "MaterialName%zu", index);
    if (n < 0 || (size_t)n >= sizeof(name_reserve)) return SF_ERR_INTERNAL;
    n = snprintf(mtd_reserve, sizeof(mtd_reserve), "MaterialMTD%zu", index);
    if (n < 0 || (size_t)n >= sizeof(mtd_reserve)) return SF_ERR_INTERNAL;
    n = snprintf(tex_reserve, sizeof(tex_reserve), "TextureIndex%zu", index);
    if (n < 0 || (size_t)n >= sizeof(tex_reserve)) return SF_ERR_INTERNAL;
    n = snprintf(gx_reserve, sizeof(gx_reserve), "GXOffset%zu", index);
    if (n < 0 || (size_t)n >= sizeof(gx_reserve)) return SF_ERR_INTERNAL;

    /* Mirrors upstream CalculateNumStringBytes for the on-disk hint field. */
    size_t num_str_chars = 0;
    if (m->name) num_str_chars += strlen(m->name) + 1;
    if (m->mtd)  num_str_chars += strlen(m->mtd)  + 1;
    for (size_t i = 0; i < m->texture_count; i++) {
        const sf_flver2_texture_t *t = &m->textures[i];
        if (t->param_name) num_str_chars += strlen(t->param_name) + 1;
        if (t->path)       num_str_chars += strlen(t->path)       + 1;
    }
    int32_t num_str_bytes = (int32_t)(num_str_chars * 2u);

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, name_reserve), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, mtd_reserve), return r);
    if ((r = sf_binary_writer_write_i32  (bw, (int32_t)m->texture_count))  != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, tex_reserve), return r);
    if ((r = sf_binary_writer_write_i32  (bw, num_str_bytes))              != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, gx_reserve), return r);
    if ((r = sf_binary_writer_write_i32  (bw, m->index))                   != SF_OK) return r;
    return sf_binary_writer_write_i32(bw, 0);
}

sf_result_t sfi_flver2_material_write_textures(sf_binary_writer_t *bw,
                                               const sf_flver2_header_t *hdr,
                                               const sf_flver2_material_t *m,
                                               size_t mat_index,
                                               size_t texture_index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && m != NULL);

    sf_result_t r;
    char tex_reserve[40];
    int n = snprintf(tex_reserve, sizeof(tex_reserve), "TextureIndex%zu", mat_index);
    if (n < 0 || (size_t)n >= sizeof(tex_reserve)) return SF_ERR_INTERNAL;

    if ((r = sf_binary_writer_fill_i32(bw, tex_reserve, (int32_t)texture_index)) != SF_OK) {
        return r;
    }
    for (size_t i = 0; i < m->texture_count; i++) {
        r = sfi_flver2_texture_write(bw, hdr, &m->textures[i], texture_index + i);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

sf_result_t sfi_flver2_material_fill_gx_offset(sf_binary_writer_t *bw,
                                               size_t mat_index, int32_t gx_index,
                                               const int32_t *gx_offsets,
                                               size_t gx_offset_count) {
    SF_CHECK_ARG(bw != NULL);

    char gx_reserve[40];
    int n = snprintf(gx_reserve, sizeof(gx_reserve), "GXOffset%zu", mat_index);
    if (n < 0 || (size_t)n >= sizeof(gx_reserve)) return SF_ERR_INTERNAL;

    int32_t value = 0;
    if (gx_index >= 0) {
        if (!gx_offsets || (size_t)gx_index >= gx_offset_count) return SF_ERR_INTERNAL;
        value = gx_offsets[gx_index];
    }
    return sf_binary_writer_fill_i32(bw, gx_reserve, value);
}

sf_result_t sfi_flver2_material_write_strings(sf_binary_writer_t *bw,
                                              const sf_flver2_header_t *hdr,
                                              const sf_flver2_material_t *m,
                                              size_t mat_index,
                                              size_t texture_index) {
    SF_CHECK_ARG(bw != NULL && hdr != NULL && m != NULL);

    sf_result_t r;
    char name_reserve[40];
    char mtd_reserve[40];
    int n;
    n = snprintf(name_reserve, sizeof(name_reserve), "MaterialName%zu", mat_index);
    if (n < 0 || (size_t)n >= sizeof(name_reserve)) return SF_ERR_INTERNAL;
    n = snprintf(mtd_reserve, sizeof(mtd_reserve), "MaterialMTD%zu", mat_index);
    if (n < 0 || (size_t)n >= sizeof(mtd_reserve)) return SF_ERR_INTERNAL;

    if ((r = sf_binary_writer_fill_i32(bw, name_reserve,
                                       (int32_t)sf_binary_writer_position(bw))) != SF_OK) {
        return r;
    }
    const char *name = m->name ? m->name : "";
    r = hdr->unicode ? sf_binary_writer_write_utf16(bw, name, true)
                     : sf_binary_writer_write_shift_jis(bw, name, true);
    if (r != SF_OK) return r;

    if ((r = sf_binary_writer_fill_i32(bw, mtd_reserve,
                                       (int32_t)sf_binary_writer_position(bw))) != SF_OK) {
        return r;
    }
    const char *mtd = m->mtd ? m->mtd : "";
    r = hdr->unicode ? sf_binary_writer_write_utf16(bw, mtd, true)
                     : sf_binary_writer_write_shift_jis(bw, mtd, true);
    if (r != SF_OK) return r;

    for (size_t i = 0; i < m->texture_count; i++) {
        r = sfi_flver2_texture_write_strings(bw, hdr, &m->textures[i], texture_index + i);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

void sfi_flver2_material_destroy_inplace(sf_flver2_material_t *m, const sf_allocator_t *a) {
    if (!m) return;
    sf_xfree(a, m->name);
    sf_xfree(a, m->mtd);
    if (m->textures) {
        for (size_t i = 0; i < m->texture_count; i++) {
            sfi_flver2_texture_destroy_inplace(&m->textures[i], a);
        }
        sf_xfree(a, m->textures);
    }
    m->name = NULL;
    m->mtd  = NULL;
    m->textures = NULL;
    m->texture_count = 0;
}

/*===========================================================================
 * TakeTextures — distributes the global texture pool to per-material lists.
 *
 * Mirrors upstream Material.TakeTextures. After this runs:
 *   - Each material->textures[i] owns its strings (param_name, path)
 *   - flver->textures[] entries are zeroed; the array itself is freed
 *   - pretake_* fields are reset to -1 on every material
 *===========================================================================*/

sf_result_t sfi_flver2_take_textures(sf_flver2_t *f) {
    SF_CHECK_ARG(f != NULL);
    if (f->header.material_count <= 0) {
        sf_xfree(f->alloc, f->textures);
        f->textures = NULL;
        return SF_OK;
    }

    int32_t total = f->header.texture_count;

    for (int32_t mi = 0; mi < f->header.material_count; mi++) {
        sf_flver2_material_t *mat = &f->materials[mi];
        int32_t lo = mat->pretake_texture_index;
        int32_t hi_excl;

        if (mat->pretake_texture_count == 0) {
            mat->textures = NULL;
            mat->texture_count = 0;
            mat->pretake_texture_index = -1;
            mat->pretake_texture_count = -1;
            continue;
        }
        if (lo < 0 || mat->pretake_texture_count < 0) return SF_ERR_OUT_OF_RANGE;
        if (lo > total - mat->pretake_texture_count) return SF_ERR_OUT_OF_RANGE;
        hi_excl = lo + mat->pretake_texture_count;

        mat->textures = (sf_flver2_texture_t *)sf_xalloc(
            f->alloc, (size_t)mat->pretake_texture_count * sizeof(*mat->textures));
        if (!mat->textures) return SF_ERR_OOM;

        for (int32_t i = lo; i < hi_excl; i++) {
            if (!f->textures[i].param_name && !f->textures[i].path) {
                return SF_ERR_INTERNAL;
            }
            mat->textures[i - lo] = f->textures[i];
            memset(&f->textures[i], 0, sizeof(f->textures[i]));
        }
        mat->texture_count = (size_t)mat->pretake_texture_count;
        mat->pretake_texture_index = -1;
        mat->pretake_texture_count = -1;
    }

    sf_xfree(f->alloc, f->textures);
    f->textures = NULL;
    return SF_OK;
}

/*===========================================================================
 * Public accessors for material / texture fields.
 *===========================================================================*/

const char *sf_flver2_material_name(const sf_flver2_material_t *m) {
    return m ? m->name : NULL;
}
const char *sf_flver2_material_mtd(const sf_flver2_material_t *m) {
    return m ? m->mtd : NULL;
}
int32_t sf_flver2_material_index(const sf_flver2_material_t *m) {
    return m ? m->index : 0;
}
int32_t sf_flver2_material_gx_index(const sf_flver2_material_t *m) {
    return m ? m->gx_index : -1;
}
size_t sf_flver2_material_texture_count(const sf_flver2_material_t *m) {
    return m ? m->texture_count : 0;
}
const sf_flver2_texture_t *sf_flver2_material_texture(const sf_flver2_material_t *m, size_t i) {
    return (m && i < m->texture_count) ? &m->textures[i] : NULL;
}

const char *sf_flver2_texture_param_name(const sf_flver2_texture_t *t) {
    return t ? t->param_name : NULL;
}
const char *sf_flver2_texture_path(const sf_flver2_texture_t *t) {
    return t ? t->path : NULL;
}
sf_vec2_t sf_flver2_texture_tiling_scale(const sf_flver2_texture_t *t) {
    sf_vec2_t zero = { 0, 0 };
    return t ? t->tiling_scale : zero;
}
sf_flver2_tiling_type_t sf_flver2_texture_tiling_type_u(const sf_flver2_texture_t *t) {
    return t ? t->tiling_type_u : SF_FLVER2_TILING_TYPE_NONE;
}
sf_flver2_tiling_type_t sf_flver2_texture_tiling_type_v(const sf_flver2_texture_t *t) {
    return t ? t->tiling_type_v : SF_FLVER2_TILING_TYPE_NONE;
}
float sf_flver2_texture_unk14(const sf_flver2_texture_t *t) { return t ? t->unk14 : 0.0f; }
float sf_flver2_texture_unk18(const sf_flver2_texture_t *t) { return t ? t->unk18 : 0.0f; }
float sf_flver2_texture_unk1c(const sf_flver2_texture_t *t) { return t ? t->unk1c : 0.0f; }
