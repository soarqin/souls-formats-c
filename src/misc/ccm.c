/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_ccm.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdlib.h>
#include <string.h>

struct sf_ccm {
    const sf_allocator_t *alloc;
    sf_ccm_version_t version;
    int16_t full_width;
    int16_t tex_width;
    int16_t tex_height;
    int16_t unk0e;
    uint8_t unk1c;
    uint8_t unk1d;
    uint8_t tex_count;
    sf_ccm_glyph_t *glyphs;
    size_t glyph_count;
    size_t glyph_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static int ccm_glyph_cmp(const void *a, const void *b) {
    int32_t ca = ((const sf_ccm_glyph_t *)a)->code;
    int32_t cb = ((const sf_ccm_glyph_t *)b)->code;
    if (ca < cb) return -1;
    if (ca > cb) return  1;
    return 0;
}

static sf_result_t ccm_reserve_glyphs(sf_ccm_t *c, size_t need) {
    if (need <= c->glyph_cap) return SF_OK;
    size_t new_cap = c->glyph_cap == 0 ? 16u : c->glyph_cap;
    while (new_cap < need) new_cap *= 2u;
    sf_ccm_glyph_t *ng = (sf_ccm_glyph_t *)sf_xalloc(c->alloc, new_cap * sizeof(*ng));
    if (!ng) return SF_ERR_OOM;
    if (c->glyphs) {
        memcpy(ng, c->glyphs, c->glyph_count * sizeof(*ng));
        sf_xfree(c->alloc, c->glyphs);
    }
    c->glyphs = ng;
    c->glyph_cap = new_cap;
    return SF_OK;
}

static sf_result_t ccm_upsert_glyph(sf_ccm_t *c, sf_ccm_glyph_t g) {
    for (size_t i = 0; i < c->glyph_count; i++) {
        if (c->glyphs[i].code == g.code) {
            c->glyphs[i] = g;
            return SF_OK;
        }
    }
    TRY(ccm_reserve_glyphs(c, c->glyph_count + 1));
    c->glyphs[c->glyph_count++] = g;
    return SF_OK;
}

sf_result_t sf_ccm_create(sf_ccm_t **out, sf_ccm_version_t version,
                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    if (version != SF_CCM_VERSION_DEMONS_SOULS &&
        version != SF_CCM_VERSION_DARK_SOULS_1 &&
        version != SF_CCM_VERSION_DARK_SOULS_2) {
        return SF_ERR_INVALID_ARG;
    }
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_ccm_t *c = (sf_ccm_t *)sf_xalloc(alloc, sizeof(*c));
    if (!c) return SF_ERR_OOM;
    memset(c, 0, sizeof(*c));
    c->alloc = alloc;
    c->version = version;
    *out = c;
    return SF_OK;
}

void sf_ccm_destroy(sf_ccm_t *c) {
    if (!c) return;
    sf_xfree(c->alloc, c->glyphs);
    sf_xfree(c->alloc, c);
}

sf_ccm_version_t sf_ccm_version(const sf_ccm_t *c) {
    return c ? c->version : SF_CCM_VERSION_DARK_SOULS_1;
}

int16_t sf_ccm_full_width(const sf_ccm_t *c) { return c ? c->full_width : 0; }
void sf_ccm_set_full_width(sf_ccm_t *c, int16_t v) { if (c) c->full_width = v; }

int16_t sf_ccm_tex_width(const sf_ccm_t *c) { return c ? c->tex_width : 0; }
void sf_ccm_set_tex_width(sf_ccm_t *c, int16_t v) { if (c) c->tex_width = v; }

int16_t sf_ccm_tex_height(const sf_ccm_t *c) { return c ? c->tex_height : 0; }
void sf_ccm_set_tex_height(sf_ccm_t *c, int16_t v) { if (c) c->tex_height = v; }

int16_t sf_ccm_unk0e(const sf_ccm_t *c) { return c ? c->unk0e : 0; }
void sf_ccm_set_unk0e(sf_ccm_t *c, int16_t v) { if (c) c->unk0e = v; }

uint8_t sf_ccm_unk1c(const sf_ccm_t *c) { return c ? c->unk1c : 0; }
void sf_ccm_set_unk1c(sf_ccm_t *c, uint8_t v) { if (c) c->unk1c = v; }

uint8_t sf_ccm_unk1d(const sf_ccm_t *c) { return c ? c->unk1d : 0; }
void sf_ccm_set_unk1d(sf_ccm_t *c, uint8_t v) { if (c) c->unk1d = v; }

uint8_t sf_ccm_tex_count(const sf_ccm_t *c) { return c ? c->tex_count : 0; }
void sf_ccm_set_tex_count(sf_ccm_t *c, uint8_t v) { if (c) c->tex_count = v; }

size_t sf_ccm_glyph_count(const sf_ccm_t *c) { return c ? c->glyph_count : 0u; }

sf_result_t sf_ccm_get_glyph(const sf_ccm_t *c, size_t index, sf_ccm_glyph_t *out) {
    SF_CHECK_ARG(c != NULL && out != NULL);
    if (index >= c->glyph_count) return SF_ERR_OUT_OF_RANGE;
    *out = c->glyphs[index];
    return SF_OK;
}

sf_result_t sf_ccm_set_glyph(sf_ccm_t *c, sf_ccm_glyph_t g) {
    SF_CHECK_ARG(c != NULL);
    return ccm_upsert_glyph(c, g);
}

sf_result_t sf_ccm_find_glyph(const sf_ccm_t *c, int32_t code, sf_ccm_glyph_t *out) {
    SF_CHECK_ARG(c != NULL && out != NULL);
    for (size_t i = 0; i < c->glyph_count; i++) {
        if (c->glyphs[i].code == code) {
            *out = c->glyphs[i];
            return SF_OK;
        }
    }
    return SF_ERR_NOT_FOUND;
}

/*===========================================================================
 * Read
 *===========================================================================*/

typedef struct ccm_code_group {
    int32_t start_code;
    int32_t end_code;
    int32_t glyph_index;
} ccm_code_group_t;

typedef struct ccm_tex_region {
    int64_t offset;
    int16_t x1, y1, x2, y2;
} ccm_tex_region_t;

sf_result_t sf_ccm_read_from_memory(sf_ccm_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_ccm_t *c = NULL;
    ccm_code_group_t *code_groups = NULL;
    sf_ccm_glyph_t *raw_glyphs = NULL;
    ccm_tex_region_t *tex_regions = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    uint32_t version = 0;
    e = sf_binary_reader_read_u32(r, &version); if (e != SF_OK) goto done;
    if (version != SF_CCM_VERSION_DEMONS_SOULS &&
        version != SF_CCM_VERSION_DARK_SOULS_1 &&
        version != SF_CCM_VERSION_DARK_SOULS_2) {
        e = SF_ERR_UNSUPPORTED_VERSION; goto done;
    }
    if (version == SF_CCM_VERSION_DEMONS_SOULS) {
        sf_binary_reader_set_big_endian(r, true);
    }

    int32_t file_size = 0;
    e = sf_binary_reader_read_i32(r, &file_size); if (e != SF_OK) goto done;
    int16_t full_width = 0, tex_width = 0, tex_height = 0;
    e = sf_binary_reader_read_i16(r, &full_width); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i16(r, &tex_width);  if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i16(r, &tex_height); if (e != SF_OK) goto done;

    int16_t code_group_count = -1, tex_region_count = -1, glyph_count = 0;
    int16_t unk0e = 0;
    if (version == SF_CCM_VERSION_DEMONS_SOULS || version == SF_CCM_VERSION_DARK_SOULS_1) {
        e = sf_binary_reader_read_i16(r, &unk0e); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i16(r, &code_group_count); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i16(r, &glyph_count); if (e != SF_OK) goto done;
    } else {
        e = sf_binary_reader_read_i16(r, &tex_region_count); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i16(r, &glyph_count); if (e != SF_OK) goto done;
        e = sf_binary_reader_assert_i16_one(r, 0); if (e != SF_OK) goto done;
    }

    e = sf_binary_reader_assert_i32_one(r, 0x20); if (e != SF_OK) goto done;
    int32_t glyph_offset = 0;
    e = sf_binary_reader_read_i32(r, &glyph_offset); if (e != SF_OK) goto done;
    uint8_t unk1c = 0, unk1d = 0, tex_count = 0;
    e = sf_binary_reader_read_u8(r, &unk1c); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_u8(r, &unk1d); if (e != SF_OK) goto done;
    e = sf_binary_reader_read_u8(r, &tex_count); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_u8_one(r, 0); if (e != SF_OK) goto done;

    e = sf_ccm_create(&c, (sf_ccm_version_t)version, alloc); if (e != SF_OK) goto done;
    c->full_width = full_width;
    c->tex_width = tex_width;
    c->tex_height = tex_height;
    c->unk0e = unk0e;
    c->unk1c = unk1c;
    c->unk1d = unk1d;
    c->tex_count = tex_count;

    if (glyph_count < 0) { e = SF_ERR_BAD_MAGIC; goto done; }

    if (version == SF_CCM_VERSION_DEMONS_SOULS || version == SF_CCM_VERSION_DARK_SOULS_1) {
        if (code_group_count < 0) { e = SF_ERR_BAD_MAGIC; goto done; }
        if (code_group_count > 0) {
            code_groups = (ccm_code_group_t *)sf_xalloc(
                alloc, (size_t)code_group_count * sizeof(*code_groups));
            if (!code_groups) { e = SF_ERR_OOM; goto done; }
        }
        for (int16_t i = 0; i < code_group_count; i++) {
            e = sf_binary_reader_read_i32(r, &code_groups[i].start_code); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i32(r, &code_groups[i].end_code);   if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i32(r, &code_groups[i].glyph_index);if (e != SF_OK) goto done;
        }

        if (glyph_count > 0) {
            raw_glyphs = (sf_ccm_glyph_t *)sf_xalloc(
                alloc, (size_t)glyph_count * sizeof(*raw_glyphs));
            if (!raw_glyphs) { e = SF_ERR_OOM; goto done; }
        }
        for (int16_t i = 0; i < glyph_count; i++) {
            sf_ccm_glyph_t g;
            memset(&g, 0, sizeof(g));
            e = sf_binary_reader_read_f32(r, &g.uv1_x); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_f32(r, &g.uv1_y); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_f32(r, &g.uv2_x); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_f32(r, &g.uv2_y); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.pre_space); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.width);     if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.advance);   if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.tex_index); if (e != SF_OK) goto done;
            raw_glyphs[i] = g;
        }

        e = ccm_reserve_glyphs(c, (size_t)glyph_count); if (e != SF_OK) goto done;
        for (int16_t gi = 0; gi < code_group_count; gi++) {
            const ccm_code_group_t *gr = &code_groups[gi];
            int32_t code_count = gr->end_code - gr->start_code + 1;
            for (int32_t j = 0; j < code_count; j++) {
                int32_t code = gr->start_code + j;
                int32_t raw_idx = gr->glyph_index + j;
                if (raw_idx < 0 || raw_idx >= glyph_count) { e = SF_ERR_BAD_MAGIC; goto done; }
                sf_ccm_glyph_t g = raw_glyphs[raw_idx];
                g.code = code;
                e = ccm_upsert_glyph(c, g); if (e != SF_OK) goto done;
            }
        }
    } else {
        if (tex_region_count < 0) { e = SF_ERR_BAD_MAGIC; goto done; }
        if (tex_region_count > 0) {
            tex_regions = (ccm_tex_region_t *)sf_xalloc(
                alloc, (size_t)tex_region_count * sizeof(*tex_regions));
            if (!tex_regions) { e = SF_ERR_OOM; goto done; }
        }
        for (int16_t i = 0; i < tex_region_count; i++) {
            tex_regions[i].offset = sf_binary_reader_position(r);
            e = sf_binary_reader_read_i16(r, &tex_regions[i].x1); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &tex_regions[i].y1); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &tex_regions[i].x2); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &tex_regions[i].y2); if (e != SF_OK) goto done;
        }
        e = ccm_reserve_glyphs(c, (size_t)glyph_count); if (e != SF_OK) goto done;
        for (int16_t i = 0; i < glyph_count; i++) {
            sf_ccm_glyph_t g;
            memset(&g, 0, sizeof(g));
            int32_t tex_region_offset = 0;
            e = sf_binary_reader_read_i32(r, &g.code); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i32(r, &tex_region_offset); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.tex_index); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.pre_space); if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.width);     if (e != SF_OK) goto done;
            e = sf_binary_reader_read_i16(r, &g.advance);   if (e != SF_OK) goto done;
            e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
            e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;

            int found = 0;
            for (int16_t j = 0; j < tex_region_count; j++) {
                if (tex_regions[j].offset == (int64_t)tex_region_offset) {
                    if (tex_width == 0 || tex_height == 0) { e = SF_ERR_BAD_MAGIC; goto done; }
                    g.uv1_x = (float)tex_regions[j].x1 / (float)tex_width;
                    g.uv1_y = (float)tex_regions[j].y1 / (float)tex_height;
                    g.uv2_x = (float)tex_regions[j].x2 / (float)tex_width;
                    g.uv2_y = (float)tex_regions[j].y2 / (float)tex_height;
                    found = 1;
                    break;
                }
            }
            if (!found) { e = SF_ERR_BAD_MAGIC; goto done; }
            e = ccm_upsert_glyph(c, g); if (e != SF_OK) goto done;
        }
    }

done:
    sf_xfree(alloc, code_groups);
    sf_xfree(alloc, raw_glyphs);
    sf_xfree(alloc, tex_regions);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_ccm_destroy(c); return e; }
    *out = c;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

typedef struct ccm_write_region {
    int16_t x1, y1, x2, y2;
    int64_t file_offset;
} ccm_write_region_t;

static int16_t ccm_round_to_i16(float f) {
    if (f >= 0.0f) return (int16_t)(f + 0.5f);
    return (int16_t)(f - 0.5f);
}

sf_result_t sf_ccm_write_to_memory(const sf_ccm_t *c, void **out_bytes,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(c != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_ccm_glyph_t *sorted = NULL;
    ccm_code_group_t *code_groups = NULL;
    ccm_write_region_t *unique_regions = NULL;
    int32_t *region_per_glyph = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, false, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_u32(w, (uint32_t)c->version); if (e != SF_OK) goto done;
    if (c->version == SF_CCM_VERSION_DEMONS_SOULS) {
        sf_binary_writer_set_big_endian(w, true);
    }

    e = sf_binary_writer_reserve_i32(w, "FileSize"); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, c->full_width); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, c->tex_width);  if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, c->tex_height); if (e != SF_OK) goto done;

    if (c->version == SF_CCM_VERSION_DEMONS_SOULS ||
        c->version == SF_CCM_VERSION_DARK_SOULS_1) {
        e = sf_binary_writer_write_i16(w, c->unk0e); if (e != SF_OK) goto done;
        e = sf_binary_writer_reserve_i16(w, "CodeGroupCount"); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, (int16_t)c->glyph_count); if (e != SF_OK) goto done;
    } else {
        e = sf_binary_writer_reserve_i16(w, "TexRegionCount"); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, (int16_t)c->glyph_count); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, 0); if (e != SF_OK) goto done;
    }

    e = sf_binary_writer_write_i32(w, 0x20); if (e != SF_OK) goto done;
    e = sf_binary_writer_reserve_i32(w, "GlyphOffset"); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, c->unk1c);    if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, c->unk1d);    if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, c->tex_count);if (e != SF_OK) goto done;
    e = sf_binary_writer_write_u8(w, 0);           if (e != SF_OK) goto done;

    if (c->glyph_count > 0) {
        sorted = (sf_ccm_glyph_t *)sf_xalloc(alloc, c->glyph_count * sizeof(*sorted));
        if (!sorted) { e = SF_ERR_OOM; goto done; }
        memcpy(sorted, c->glyphs, c->glyph_count * sizeof(*sorted));
        qsort(sorted, c->glyph_count, sizeof(*sorted), ccm_glyph_cmp);
    }

    if (c->version == SF_CCM_VERSION_DEMONS_SOULS ||
        c->version == SF_CCM_VERSION_DARK_SOULS_1) {
        size_t group_count = 0;
        if (c->glyph_count > 0) {
            code_groups = (ccm_code_group_t *)sf_xalloc(
                alloc, c->glyph_count * sizeof(*code_groups));
            if (!code_groups) { e = SF_ERR_OOM; goto done; }
            size_t i = 0;
            while (i < c->glyph_count) {
                int32_t start_code = sorted[i].code;
                int32_t glyph_index = (int32_t)i;
                size_t j = i + 1;
                while (j < c->glyph_count && sorted[j].code == sorted[j - 1].code + 1) j++;
                int32_t end_code = sorted[j - 1].code;
                code_groups[group_count].start_code = start_code;
                code_groups[group_count].end_code = end_code;
                code_groups[group_count].glyph_index = glyph_index;
                group_count++;
                i = j;
            }
        }

        e = sf_binary_writer_fill_i16(w, "CodeGroupCount", (int16_t)group_count);
        if (e != SF_OK) goto done;
        for (size_t i = 0; i < group_count; i++) {
            e = sf_binary_writer_write_i32(w, code_groups[i].start_code);  if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i32(w, code_groups[i].end_code);    if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i32(w, code_groups[i].glyph_index); if (e != SF_OK) goto done;
        }

        e = sf_binary_writer_fill_i32(w, "GlyphOffset",
                                      (int32_t)sf_binary_writer_position(w));
        if (e != SF_OK) goto done;
        for (size_t i = 0; i < c->glyph_count; i++) {
            const sf_ccm_glyph_t *g = &sorted[i];
            e = sf_binary_writer_write_f32(w, g->uv1_x); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_f32(w, g->uv1_y); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_f32(w, g->uv2_x); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_f32(w, g->uv2_y); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->pre_space); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->width);     if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->advance);   if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->tex_index); if (e != SF_OK) goto done;
        }
    } else {
        size_t unique_count = 0;
        if (c->glyph_count > 0) {
            unique_regions = (ccm_write_region_t *)sf_xalloc(
                alloc, c->glyph_count * sizeof(*unique_regions));
            if (!unique_regions) { e = SF_ERR_OOM; goto done; }
            region_per_glyph = (int32_t *)sf_xalloc(
                alloc, c->glyph_count * sizeof(*region_per_glyph));
            if (!region_per_glyph) { e = SF_ERR_OOM; goto done; }

            for (size_t i = 0; i < c->glyph_count; i++) {
                int16_t x1 = ccm_round_to_i16(sorted[i].uv1_x * (float)c->tex_width);
                int16_t y1 = ccm_round_to_i16(sorted[i].uv1_y * (float)c->tex_height);
                int16_t x2 = ccm_round_to_i16(sorted[i].uv2_x * (float)c->tex_width);
                int16_t y2 = ccm_round_to_i16(sorted[i].uv2_y * (float)c->tex_height);
                int found = -1;
                for (size_t j = 0; j < unique_count; j++) {
                    if (unique_regions[j].x1 == x1 && unique_regions[j].y1 == y1 &&
                        unique_regions[j].x2 == x2 && unique_regions[j].y2 == y2) {
                        found = (int)j; break;
                    }
                }
                if (found < 0) {
                    unique_regions[unique_count].x1 = x1;
                    unique_regions[unique_count].y1 = y1;
                    unique_regions[unique_count].x2 = x2;
                    unique_regions[unique_count].y2 = y2;
                    unique_regions[unique_count].file_offset = 0;
                    region_per_glyph[i] = (int32_t)unique_count;
                    unique_count++;
                } else {
                    region_per_glyph[i] = (int32_t)found;
                }
            }
        }

        e = sf_binary_writer_fill_i16(w, "TexRegionCount", (int16_t)unique_count);
        if (e != SF_OK) goto done;
        for (size_t i = 0; i < unique_count; i++) {
            unique_regions[i].file_offset = sf_binary_writer_position(w);
            e = sf_binary_writer_write_i16(w, unique_regions[i].x1); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, unique_regions[i].y1); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, unique_regions[i].x2); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, unique_regions[i].y2); if (e != SF_OK) goto done;
        }

        e = sf_binary_writer_fill_i32(w, "GlyphOffset",
                                      (int32_t)sf_binary_writer_position(w));
        if (e != SF_OK) goto done;
        for (size_t i = 0; i < c->glyph_count; i++) {
            const sf_ccm_glyph_t *g = &sorted[i];
            int32_t ri = region_per_glyph[i];
            e = sf_binary_writer_write_i32(w, g->code); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i32(w, (int32_t)unique_regions[ri].file_offset);
            if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->tex_index); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->pre_space); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->width);     if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i16(w, g->advance);   if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
            e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
        }
    }

    e = sf_binary_writer_fill_i32(w, "FileSize", (int32_t)sf_binary_writer_position(w));
    if (e != SF_OK) goto done;

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_xfree(alloc, sorted);
    sf_xfree(alloc, code_groups);
    sf_xfree(alloc, unique_regions);
    sf_xfree(alloc, region_per_glyph);
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
