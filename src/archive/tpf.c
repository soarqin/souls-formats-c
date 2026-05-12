/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TPF (texture pack) container.
 *
 * Mirrors:
 *   SoulsFormats/Formats/TPF/TPF.cs
 *
 * Wire format (PC, little-endian):
 *
 *   off 0x00  ASCII "TPF\0"
 *   off 0x04  int32   dataSize        (size of texture data section, post-fill)
 *   off 0x08  int32   fileCount
 *   off 0x0C  byte    Platform        (0 = PC, ...)
 *   off 0x0D  byte    Flag2           (0..3)
 *   off 0x0E  byte    Encoding        (0..2)
 *   off 0x0F  byte    0
 *   off 0x10  Texture[fileCount] {     // PC variant — 0x18 bytes each
 *                 uint32  fileOffset
 *                 int32   fileSize
 *                 byte    Format
 *                 byte    Type/Cubemap (0=Texture, 1=Cubemap, 2=Volume, 3=Array)
 *                 byte    Mipmaps
 *                 byte    Flags1       (0|1|2|3|0x80)
 *                 uint32  nameOffset
 *                 int32   hasFloatStruct (0 or 1)
 *             }
 *   ── name pool (UTF-16LE if Encoding==1, Shift-JIS otherwise, NUL-terminated) ──
 *   ── data pool (each texture padded to 4 bytes; PS3=0x80, PS4=0x10) ──
 */

#include "souls_formats/sf_tpf.h"

#include "archive/tpf_headerizer.h"
#include "internal/dds_header.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Private struct definitions
 *===========================================================================*/

struct sf_tpf_texture {
    const sf_allocator_t *alloc;
    char    *name;          /* heap-owned, UTF-8 NUL-terminated, may be NULL */
    uint8_t *bytes;          /* heap-owned, may be NULL when size==0 */
    size_t   size;
    uint8_t  format;
    uint8_t  flags1;        /* 0 / 1 / 2 / 3 / 0x80 — DCP_EDGE if 2 or 3 */
    uint8_t  flags2;        /* hasFloatStruct on disk; we only carry 0 or 1 */
    uint8_t  mipmap_count;
    bool     cubemap;       /* upstream TexType==1 */
};

struct sf_tpf {
    const sf_allocator_t *alloc;

    sf_tpf_texture_t **textures;     /* array of owned pointers */
    size_t             texture_count;
    size_t             texture_capacity;

    sf_tpf_platform_t  platform;
    uint8_t            encoding;
    uint8_t            flag2;
};

/*===========================================================================
 * Texture lifecycle
 *===========================================================================*/

sf_result_t sf_tpf_texture_create(sf_tpf_texture_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);
    sf_tpf_texture_t *t = (sf_tpf_texture_t *)sf_xalloc(a, sizeof(*t));
    if (!t) return SF_ERR_OOM;
    memset(t, 0, sizeof(*t));
    t->alloc = a;
    *out = t;
    return SF_OK;
}

void sf_tpf_texture_destroy(sf_tpf_texture_t *t) {
    if (!t) return;
    const sf_allocator_t *a = t->alloc;
    sf_xfree(a, t->name);
    sf_xfree(a, t->bytes);
    sf_xfree(a, t);
}

static sf_result_t tpf_texture_dup(sf_tpf_texture_t **out,
                                   const sf_tpf_texture_t *src,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(src != NULL);
    a = sf_alloc_or_default(a);

    sf_tpf_texture_t *dst = NULL;
    sf_result_t r = sf_tpf_texture_create(&dst, a);
    if (r != SF_OK) return r;

    dst->format       = src->format;
    dst->flags1       = src->flags1;
    dst->flags2       = src->flags2;
    dst->mipmap_count = src->mipmap_count;
    dst->cubemap      = src->cubemap;

    if (src->name) {
        dst->name = sf_strdup(a, src->name);
        if (!dst->name) {
            sf_tpf_texture_destroy(dst);
            return SF_ERR_OOM;
        }
    }
    if (src->bytes && src->size > 0) {
        dst->bytes = (uint8_t *)sf_xalloc(a, src->size);
        if (!dst->bytes) {
            sf_tpf_texture_destroy(dst);
            return SF_ERR_OOM;
        }
        memcpy(dst->bytes, src->bytes, src->size);
        dst->size = src->size;
    }
    *out = dst;
    return SF_OK;
}

/*===========================================================================
 * Texture accessors / mutators
 *===========================================================================*/

const char *sf_tpf_texture_get_name(const sf_tpf_texture_t *t) {
    return t ? t->name : NULL;
}
uint8_t sf_tpf_texture_get_format(const sf_tpf_texture_t *t)       { return t ? t->format : 0; }
uint8_t sf_tpf_texture_get_flags1(const sf_tpf_texture_t *t)       { return t ? t->flags1 : 0; }
uint8_t sf_tpf_texture_get_flags2(const sf_tpf_texture_t *t)       { return t ? t->flags2 : 0; }
uint8_t sf_tpf_texture_get_mipmap_count(const sf_tpf_texture_t *t) { return t ? t->mipmap_count : 0; }
bool    sf_tpf_texture_get_cubemap(const sf_tpf_texture_t *t)      { return t ? t->cubemap : false; }
const uint8_t *sf_tpf_texture_get_bytes(const sf_tpf_texture_t *t, size_t *out_size) {
    if (!t) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    if (out_size) *out_size = t->size;
    return t->bytes;
}

sf_result_t sf_tpf_texture_set_name(sf_tpf_texture_t *t, const char *utf8) {
    SF_CHECK_ARG(t != NULL);
    char *copy = NULL;
    if (utf8) {
        copy = sf_strdup(t->alloc, utf8);
        if (!copy) return SF_ERR_OOM;
    }
    sf_xfree(t->alloc, t->name);
    t->name = copy;
    return SF_OK;
}

sf_result_t sf_tpf_texture_set_bytes(sf_tpf_texture_t *t, const uint8_t *data, size_t size) {
    SF_CHECK_ARG(t != NULL);
    uint8_t *copy = NULL;
    if (data && size > 0) {
        copy = (uint8_t *)sf_xalloc(t->alloc, size);
        if (!copy) return SF_ERR_OOM;
        memcpy(copy, data, size);
    }
    sf_xfree(t->alloc, t->bytes);
    t->bytes = copy;
    t->size  = (data && size > 0) ? size : 0;
    return SF_OK;
}

void sf_tpf_texture_set_format(sf_tpf_texture_t *t, uint8_t v)       { if (t) t->format = v; }
void sf_tpf_texture_set_flags1(sf_tpf_texture_t *t, uint8_t v)       { if (t) t->flags1 = v; }
void sf_tpf_texture_set_flags2(sf_tpf_texture_t *t, uint8_t v)       { if (t) t->flags2 = v; }
void sf_tpf_texture_set_mipmap_count(sf_tpf_texture_t *t, uint8_t v) { if (t) t->mipmap_count = v; }
void sf_tpf_texture_set_cubemap(sf_tpf_texture_t *t, bool v)         { if (t) t->cubemap = v; }

/*===========================================================================
 * TPF lifecycle
 *===========================================================================*/

sf_result_t sf_tpf_create(sf_tpf_t **out, const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    a = sf_alloc_or_default(a);
    sf_tpf_t *b = (sf_tpf_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return SF_ERR_OOM;
    memset(b, 0, sizeof(*b));
    b->alloc    = a;
    b->platform = SF_TPF_PLATFORM_PC;
    b->encoding = 1;
    b->flag2    = 3;
    *out = b;
    return SF_OK;
}

void sf_tpf_destroy(sf_tpf_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    if (b->textures) {
        for (size_t i = 0; i < b->texture_count; i++) sf_tpf_texture_destroy(b->textures[i]);
        sf_xfree(a, b->textures);
    }
    sf_xfree(a, b);
}

/*===========================================================================
 * TPF accessors / mutators
 *===========================================================================*/

size_t sf_tpf_texture_count(const sf_tpf_t *b) { return b ? b->texture_count : 0; }

const sf_tpf_texture_t *sf_tpf_get_texture(const sf_tpf_t *b, size_t idx) {
    if (!b || idx >= b->texture_count) return NULL;
    return b->textures[idx];
}

sf_tpf_platform_t sf_tpf_get_platform(const sf_tpf_t *b) {
    return b ? b->platform : SF_TPF_PLATFORM_UNKNOWN;
}
uint8_t sf_tpf_get_encoding(const sf_tpf_t *b) { return b ? b->encoding : 0; }
uint8_t sf_tpf_get_flag2   (const sf_tpf_t *b) { return b ? b->flag2 : 0; }

void sf_tpf_set_platform(sf_tpf_t *b, sf_tpf_platform_t p) { if (b) b->platform = p; }
void sf_tpf_set_encoding(sf_tpf_t *b, uint8_t enc)         { if (b) b->encoding = enc; }
void sf_tpf_set_flag2   (sf_tpf_t *b, uint8_t v)           { if (b) b->flag2    = v; }

sf_result_t sf_tpf_add_texture(sf_tpf_t *b, const sf_tpf_texture_t *tex) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(tex != NULL);

    if (b->texture_count == b->texture_capacity) {
        size_t new_cap   = b->texture_capacity ? b->texture_capacity * 2 : 8;
        size_t old_bytes = b->texture_capacity * sizeof(*b->textures);
        size_t new_bytes = new_cap * sizeof(*b->textures);
        void  *new_buf   = sf_xrealloc(b->alloc, b->textures, old_bytes, new_bytes);
        if (!new_buf) return SF_ERR_OOM;
        b->textures        = (sf_tpf_texture_t **)new_buf;
        memset(&b->textures[b->texture_capacity], 0,
               (new_cap - b->texture_capacity) * sizeof(*b->textures));
        b->texture_capacity = new_cap;
    }

    sf_tpf_texture_t *copy = NULL;
    sf_result_t r = tpf_texture_dup(&copy, tex, b->alloc);
    if (r != SF_OK) return r;

    b->textures[b->texture_count++] = copy;
    return SF_OK;
}

sf_result_t sf_tpf_remove_texture(sf_tpf_t *b, size_t idx) {
    SF_CHECK_ARG(b != NULL);
    if (idx >= b->texture_count) return SF_ERR_OUT_OF_RANGE;
    sf_tpf_texture_destroy(b->textures[idx]);
    for (size_t i = idx + 1; i < b->texture_count; i++) {
        b->textures[i - 1] = b->textures[i];
    }
    b->texture_count--;
    b->textures[b->texture_count] = NULL;
    return SF_OK;
}

/*===========================================================================
 * DX10 cubemap dwCaps2 fix (TPF.cs:357-361)
 *
 * FromSoft erroneously sets the DX10 image count for cubemaps to 6, which
 * causes editors to interpret the image as an array of 6 cubemaps. The fix:
 * if FourCC is "DX10" and the DX10 misc flags carry RESOURCE_MISC_TEXTURECUBE
 * (== 4) and the DX10 arraySize == 6, patch arraySize back to 1.
 *
 * Disk layout (relative to file start):
 *   0x54..0x57   FourCC ("DX10")
 *   0x88         DX10.miscFlag
 *   0x8C         DX10.arraySize (low byte)
 *
 * We mirror upstream byte-checks exactly to avoid drift.
 *===========================================================================*/
static void tpf_apply_dx10_cubemap_fix(uint8_t *bytes, size_t size) {
    if (size <= 0x8C) return;
    /* Check FourCC == "DX10". */
    if (bytes[0x54] != 0x44 || bytes[0x55] != 0x58 ||
        bytes[0x56] != 0x31 || bytes[0x57] != 0x30) return;
    /* DDS_RESOURCE_MISC_TEXTURECUBE == 0x4. */
    if (bytes[0x88] != 0x4) return;
    /* arraySize low byte == 6 → patch to 1. */
    if (bytes[0x8C] == 0x6) bytes[0x8C] = 0x1;
}

/*===========================================================================
 * Read: header + per-texture
 *===========================================================================*/

typedef struct tpf_tex_record {
    uint32_t file_offset;
    int32_t  file_size;
    uint8_t  format;
    uint8_t  type;
    uint8_t  mipmaps;
    uint8_t  flags1;
    uint32_t name_offset;
    int32_t  has_float_struct;
} tpf_tex_record_t;

static sf_result_t tpf_read_one(sf_binary_reader_t *br, sf_tpf_t *b,
                                const tpf_tex_record_t *rec) {
    sf_tpf_texture_t *t = NULL;
    sf_result_t r = sf_tpf_texture_create(&t, b->alloc);
    if (r != SF_OK) return r;

    t->format       = rec->format;
    t->flags1       = rec->flags1;
    t->mipmap_count = rec->mipmaps;
    t->cubemap      = (rec->type == 1);
    t->flags2       = (uint8_t)(rec->has_float_struct ? 1 : 0);

    if (rec->file_size < 0) {
        sf_tpf_texture_destroy(t);
        return SF_ERR_OUT_OF_RANGE;
    }

    uint8_t *raw = NULL;
    if (rec->file_size > 0) {
        raw = (uint8_t *)sf_xalloc(b->alloc, (size_t)rec->file_size);
        if (!raw) { sf_tpf_texture_destroy(t); return SF_ERR_OOM; }
        r = sf_binary_reader_get_bytes(br, (int64_t)rec->file_offset,
                                       raw, (size_t)rec->file_size);
        if (r != SF_OK) {
            sf_xfree(b->alloc, raw);
            sf_tpf_texture_destroy(t);
            return r;
        }
    }

    /* Per-texture DCP_EDGE compression (TPF.cs:347-352). */
    if (rec->flags1 == 2 || rec->flags1 == 3) {
        uint8_t *decoded = NULL;
        size_t   dsize   = 0;
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof info);
        r = sf_dcx_decompress_from_buffer(raw, (size_t)rec->file_size,
                                          &decoded, &dsize, &info, b->alloc);
        sf_xfree(b->alloc, raw);
        if (r != SF_OK) { sf_tpf_texture_destroy(t); return r; }
        if (info.type != SF_DCX_TYPE_DCP_EDGE) {
            sf_xfree(b->alloc, decoded);
            sf_tpf_texture_destroy(t);
            return SF_ERR_UNSUPPORTED_VERSION;
        }
        t->bytes = decoded;
        t->size  = dsize;
    } else {
        t->bytes = raw;
        t->size  = (rec->file_size > 0) ? (size_t)rec->file_size : 0;
    }

    /* DX10 cubemap fix on PC platform only. */
    if (b->platform == SF_TPF_PLATFORM_PC && t->bytes && t->size > 0) {
        tpf_apply_dx10_cubemap_fix(t->bytes, t->size);
    }

    /* Read name from name pool. */
    if (b->encoding == 1) {
        char *name = NULL;
        r = sf_binary_reader_get_utf16(br, (int64_t)rec->name_offset, &name, NULL);
        if (r != SF_OK) { sf_tpf_texture_destroy(t); return r; }
        t->name = name;
    } else if (b->encoding == 0 || b->encoding == 2) {
        char *name = NULL;
        r = sf_binary_reader_get_shift_jis(br, (int64_t)rec->name_offset, &name, NULL);
        if (r != SF_OK) { sf_tpf_texture_destroy(t); return r; }
        t->name = name;
    }

    if (b->texture_count == b->texture_capacity) {
        size_t new_cap   = b->texture_capacity ? b->texture_capacity * 2 : 8;
        size_t old_bytes = b->texture_capacity * sizeof(*b->textures);
        size_t new_bytes = new_cap * sizeof(*b->textures);
        void  *new_buf   = sf_xrealloc(b->alloc, b->textures, old_bytes, new_bytes);
        if (!new_buf) { sf_tpf_texture_destroy(t); return SF_ERR_OOM; }
        b->textures         = (sf_tpf_texture_t **)new_buf;
        memset(&b->textures[b->texture_capacity], 0,
               (new_cap - b->texture_capacity) * sizeof(*b->textures));
        b->texture_capacity = new_cap;
    }
    b->textures[b->texture_count++] = t;
    return SF_OK;
}

static sf_result_t tpf_populate(sf_tpf_t *b, sf_binary_reader_t *br) {
    sf_result_t r;
    sf_binary_reader_set_big_endian(br, false);
    char magic[4];
    r = sf_binary_reader_read_bytes(br, magic, 4); if (r != SF_OK) return r;
    if (memcmp(magic, "TPF\0", 4) != 0) return SF_ERR_BAD_MAGIC;

    uint8_t platform_byte = 0;
    r = sf_binary_reader_get_u8(br, 0xC, &platform_byte);
    if (r != SF_OK) return r;
    b->platform = (sf_tpf_platform_t)platform_byte;
    sf_binary_reader_set_big_endian(br,
        b->platform == SF_TPF_PLATFORM_XBOX360 || b->platform == SF_TPF_PLATFORM_PS3);

    int32_t data_size = 0;
    r = sf_binary_reader_read_i32(br, &data_size); if (r != SF_OK) return r;
    int32_t file_count = 0;
    r = sf_binary_reader_read_i32(br, &file_count); if (r != SF_OK) return r;
    if (file_count < 0) return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_reader_skip(br, 1); if (r != SF_OK) return r;

    static const uint8_t flag2_options[] = { 0, 1, 2, 3 };
    uint8_t flag2 = 0;
    r = sf_binary_reader_assert_u8(br, 4, flag2_options, &flag2);
    if (r != SF_OK) return r;
    b->flag2 = flag2;

    static const uint8_t encoding_options[] = { 0, 1, 2 };
    uint8_t encoding = 0;
    r = sf_binary_reader_assert_u8(br, 3, encoding_options, &encoding);
    if (r != SF_OK) return r;
    b->encoding = encoding;

    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;

    /* Stage 1: read all per-texture header records into a temporary buffer. */
    tpf_tex_record_t *recs = NULL;
    if (file_count > 0) {
        recs = (tpf_tex_record_t *)sf_xalloc(b->alloc,
                                              (size_t)file_count * sizeof(*recs));
        if (!recs) return SF_ERR_OOM;
        memset(recs, 0, (size_t)file_count * sizeof(*recs));
    }

    static const uint8_t flags1_options[] = { 0, 1, 2, 3, 0x80 };
    static const int32_t hfs_options[]    = { 0, 1 };

    for (int32_t i = 0; i < file_count; i++) {
        tpf_tex_record_t *rec = &recs[i];
        r = sf_binary_reader_read_u32(br, &rec->file_offset);   if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_read_i32(br, &rec->file_size);     if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_read_u8 (br, &rec->format);        if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_read_u8 (br, &rec->type);          if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_read_u8 (br, &rec->mipmaps);       if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_assert_u8(br, 5, flags1_options, &rec->flags1);
        if (r != SF_OK) goto cleanup;

        /* Console metadata blocks — skipped for PC; for non-PC, advance the
         * reader cursor accordingly so the next field lands correctly. We
         * carry only minimal data through; round-trip fidelity for non-PC
         * is best-effort and only covers PC fully (per scope). */
        if (b->platform != SF_TPF_PLATFORM_PC) {
            /* Width/Height: 4 bytes */
            r = sf_binary_reader_skip(br, 4); if (r != SF_OK) goto cleanup;
            if (b->platform == SF_TPF_PLATFORM_XBOX360) {
                r = sf_binary_reader_skip(br, 4); if (r != SF_OK) goto cleanup;
            } else if (b->platform == SF_TPF_PLATFORM_PS3) {
                r = sf_binary_reader_skip(br, 4); if (r != SF_OK) goto cleanup;
                if (b->flag2 != 0) {
                    r = sf_binary_reader_skip(br, 4); if (r != SF_OK) goto cleanup;
                }
            } else if (b->platform == SF_TPF_PLATFORM_PS4 ||
                       b->platform == SF_TPF_PLATFORM_XBOX1 ||
                       b->platform == SF_TPF_PLATFORM_PS5) {
                r = sf_binary_reader_skip(br, 8); if (r != SF_OK) goto cleanup;
            }
        }

        r = sf_binary_reader_read_u32(br, &rec->name_offset);   if (r != SF_OK) goto cleanup;
        r = sf_binary_reader_assert_i32(br, 2, hfs_options, &rec->has_float_struct);
        if (r != SF_OK) goto cleanup;

        if (b->platform == SF_TPF_PLATFORM_PS4 ||
            b->platform == SF_TPF_PLATFORM_XBOX1 ||
            b->platform == SF_TPF_PLATFORM_PS5) {
            r = sf_binary_reader_skip(br, 4); if (r != SF_OK) goto cleanup;
        }

        if (rec->has_float_struct == 1) {
            int32_t fs_unk00 = 0, fs_len = 0;
            r = sf_binary_reader_read_i32(br, &fs_unk00); if (r != SF_OK) goto cleanup;
            r = sf_binary_reader_read_i32(br, &fs_len);   if (r != SF_OK) goto cleanup;
            if (fs_len < 0 || (fs_len % 4) != 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
            r = sf_binary_reader_skip(br, fs_len); if (r != SF_OK) goto cleanup;
            (void)fs_unk00;
        }
    }

    /* Stage 2: realise each texture's name + bytes. */
    for (int32_t i = 0; i < file_count; i++) {
        r = tpf_read_one(br, b, &recs[i]);
        if (r != SF_OK) goto cleanup;
    }

    sf_xfree(b->alloc, recs);
    (void)data_size;
    return SF_OK;

cleanup:
    sf_xfree(b->alloc, recs);
    return r;
}

sf_result_t sf_tpf_read_from_memory(sf_tpf_t **out, const uint8_t *data, size_t size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL || size == 0);
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_memory(&is, data, size, a);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    sf_tpf_t *b = NULL;
    r = sf_tpf_create(&b, a);
    if (r != SF_OK) { sf_binary_reader_destroy(br); sf_istream_close(is); return r; }

    r = tpf_populate(b, br);
    if (r != SF_OK) { sf_tpf_destroy(b); b = NULL; }
    else            { *out = b; }

    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

sf_result_t sf_tpf_read_from_path(sf_tpf_t **out, const wchar_t *path,
                                  const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(path != NULL);
    a = sf_alloc_or_default(a);

    sf_istream_t *is = NULL;
    sf_result_t r = sf_istream_open_wfile(&is, path, a);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, is, false, a);
    if (r != SF_OK) { sf_istream_close(is); return r; }

    sf_tpf_t *b = NULL;
    r = sf_tpf_create(&b, a);
    if (r != SF_OK) { sf_binary_reader_destroy(br); sf_istream_close(is); return r; }

    r = tpf_populate(b, br);
    if (r != SF_OK) { sf_tpf_destroy(b); b = NULL; }
    else            { *out = b; }

    sf_binary_reader_destroy(br);
    sf_istream_close(is);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static int tpf_reservation_label(char *buf, size_t buf_size, const char *prefix,
                                 size_t i) {
    return snprintf(buf, buf_size, "%s%zu", prefix, i);
}

static sf_result_t tpf_write_one_header(sf_binary_writer_t *bw, const sf_tpf_t *b,
                                        const sf_tpf_texture_t *t, size_t i) {
    sf_result_t r;
    char label[32];

    /* Update cubemap/mipmaps from the embedded DDS on PC; matches upstream
     * `WriteHeader` (TPF.cs:370-380). */
    bool out_cubemap = t->cubemap;
    uint8_t out_mipmaps = t->mipmap_count;
    if (b->platform == SF_TPF_PLATFORM_PC && t->bytes && t->size > 0) {
        sfi_dds_metadata_t meta;
        memset(&meta, 0, sizeof(meta));
        sf_result_t mr = sfi_dds_parse_header(t->bytes, t->size, &meta);
        if (mr == SF_OK) {
            out_cubemap = meta.cubemap;
            out_mipmaps = (uint8_t)meta.mipmap_count;
        }
    }

    tpf_reservation_label(label, sizeof(label), "FileData", i);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_u32(bw, label), return r);
    tpf_reservation_label(label, sizeof(label), "FileSize", i);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, label), return r);

    r = sf_binary_writer_write_u8(bw, t->format);                      if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, out_cubemap ? 1 : 0);            if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, out_mipmaps);                    if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, t->flags1);                      if (r != SF_OK) return r;

    /* PC variant: no console metadata between flags1 and nameOffset. */
    tpf_reservation_label(label, sizeof(label), "FileName", i);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_u32(bw, label), return r);
    /* hasFloatStruct: we don't carry FloatStruct → always 0. */
    r = sf_binary_writer_write_i32(bw, 0); if (r != SF_OK) return r;
    return SF_OK;
}

static sf_result_t tpf_write_one_name(sf_binary_writer_t *bw, const sf_tpf_t *b,
                                      const sf_tpf_texture_t *t, size_t i) {
    char label[32];
    tpf_reservation_label(label, sizeof(label), "FileName", i);
    sf_result_t r = sf_binary_writer_fill_u32(bw, label,
                                              (uint32_t)sf_binary_writer_position(bw));
    if (r != SF_OK) return r;
    const char *name = t->name ? t->name : "";
    if (b->encoding == 1) {
        return sf_binary_writer_write_utf16(bw, name, true);
    } else {
        return sf_binary_writer_write_shift_jis(bw, name, true);
    }
}

static sf_result_t tpf_write_one_data(sf_binary_writer_t *bw, const sf_tpf_t *b,
                                      const sf_tpf_texture_t *t, size_t i) {
    char label[32];
    sf_result_t r;
    tpf_reservation_label(label, sizeof(label), "FileData", i);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_u32(bw, label, (uint32_t)sf_binary_writer_position(bw)), return r);

    const uint8_t *bytes = t->bytes;
    size_t         size  = t->size;
    uint8_t       *owned = NULL;

    if ((t->flags1 == 2 || t->flags1 == 3) && bytes && size > 0) {
        sf_dcx_compression_info_t info;
        memset(&info, 0, sizeof info);
        info.type = SF_DCX_TYPE_DCP_EDGE;
        void  *cx  = NULL;
        size_t cxn = 0;
        r = sf_dcx_compress_to_buffer(bytes, size, &info,
                                      (uint8_t **)&cx, &cxn, b->alloc);
        if (r != SF_OK) return r;
        owned = (uint8_t *)cx;
        bytes = owned;
        size  = cxn;
    }

    tpf_reservation_label(label, sizeof(label), "FileSize", i);
    r = sf_binary_writer_fill_i32(bw, label, (int32_t)size);
    if (r != SF_OK) { sf_xfree(b->alloc, owned); return r; }
    if (size > 0) {
        r = sf_binary_writer_write_bytes(bw, bytes, size);
        if (r != SF_OK) { sf_xfree(b->alloc, owned); return r; }
    }
    sf_xfree(b->alloc, owned);
    (void)i;
    return SF_OK;
}

static sf_result_t tpf_write_to_writer(const sf_tpf_t *b, sf_binary_writer_t *bw) {
    sf_result_t r;

    bool be = (b->platform == SF_TPF_PLATFORM_XBOX360 ||
               b->platform == SF_TPF_PLATFORM_PS3);
    sf_binary_writer_set_big_endian(bw, be);

    r = sf_binary_writer_write_bytes(bw, "TPF\0", 4);                if (r != SF_OK) return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "DataSize"), return r);
    r = sf_binary_writer_write_i32 (bw, (int32_t)b->texture_count);  if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8  (bw, (uint8_t)b->platform);       if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8  (bw, b->flag2);                   if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8  (bw, b->encoding);                if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8  (bw, 0);                          if (r != SF_OK) return r;

    for (size_t i = 0; i < b->texture_count; i++) {
        r = tpf_write_one_header(bw, b, b->textures[i], i);
        if (r != SF_OK) return r;
    }
    for (size_t i = 0; i < b->texture_count; i++) {
        r = tpf_write_one_name(bw, b, b->textures[i], i);
        if (r != SF_OK) return r;
    }

    /* Padding before data section: 0x100 PS3, 0x10 PS4, 4 elsewhere. */
    int data_pad = 4;
    if (b->platform == SF_TPF_PLATFORM_PS3) {
        r = sf_binary_writer_pad(bw, 0x100); if (r != SF_OK) return r;
        data_pad = 0x80;
    } else if (b->platform == SF_TPF_PLATFORM_PS4) {
        r = sf_binary_writer_pad(bw, 0x10);  if (r != SF_OK) return r;
    }

    int64_t data_start = sf_binary_writer_position(bw);
    for (size_t i = 0; i < b->texture_count; i++) {
        if (b->textures[i]->size > 0) {
            r = sf_binary_writer_pad(bw, data_pad);
            if (r != SF_OK) return r;
        }
        r = tpf_write_one_data(bw, b, b->textures[i], i);
        if (r != SF_OK) return r;
    }
    int32_t data_section = (int32_t)(sf_binary_writer_position(bw) - data_start);
    r = sf_binary_writer_fill_i32(bw, "DataSize", data_section);
    return r;
}

sf_result_t sf_tpf_write_to_memory(const sf_tpf_t *b, uint8_t **out, size_t *out_size,
                                   const sf_allocator_t *a) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);
    a = sf_alloc_or_default(a);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_memory(&os, a);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, a);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = tpf_write_to_writer(b, bw);
    if (r != SF_OK) { sf_binary_writer_destroy(bw); sf_ostream_close(os); return r; }

    r = sf_binary_writer_finish_bytes(bw, out, out_size);
    sf_ostream_close(os);
    return r;
}

sf_result_t sf_tpf_write_to_path(const sf_tpf_t *b, const wchar_t *path) {
    SF_CHECK_ARG(b != NULL);
    SF_CHECK_ARG(path != NULL);

    sf_ostream_t *os = NULL;
    sf_result_t r = sf_ostream_open_wfile(&os, path, b->alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, os, false, b->alloc);
    if (r != SF_OK) { sf_ostream_close(os); return r; }

    r = tpf_write_to_writer(b, bw);
    if (r == SF_OK) r = sf_binary_writer_finish(bw);
    else            sf_binary_writer_destroy(bw);
    sf_ostream_close(os);
    return r;
}
