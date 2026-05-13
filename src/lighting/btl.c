/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — BTL point-light data for BB / DS3 / Sekiro / ER+.
 *
 * Upstream: BTL.cs
 */

// Upstream: BTL.cs

#include "souls_formats/sf_btl.h"
#include "souls_formats/sf_encoding.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*===========================================================================
 * Internal struct definitions (NOT exposed in the public header)
 *===========================================================================*/

struct sf_btl_light {
    uint8_t              unk00[16];
    const char          *name;             /* borrowed from name_pool */
    sf_btl_light_type_t  type;
    bool                 unk1c;
    sf_color_t           diffuse_color;    /* alpha unused (RGB only on wire) */
    float                diffuse_power;
    sf_color_t           specular_color;   /* alpha unused (RGB only on wire) */
    bool                 cast_shadows;
    float                specular_power;
    float                cone_angle;
    float                unk30;
    float                unk34;
    sf_vec3_t            position;
    sf_vec3_t            rotation;
    int32_t              unk50;
    float                unk54;
    float                radius;
    int32_t              unk5c;
    uint8_t              unk64[4];
    float                unk68;
    sf_color_t           shadow_color;     /* RGBA */
    float                unk70;
    float                flicker_interval_min;
    float                flicker_interval_max;
    float                flicker_brightness_mult;
    int32_t              unk80;
    uint8_t              unk84[4];
    float                unk88;
    float                unk90;
    float                unk98;
    float                near_clip;
    uint8_t              unk_a0[4];
    float                sharpness;
    float                unk_ac;
    float                width;
    float                unk_bc;
    uint8_t              unk_c0[4];
    float                unk_c4;
    /* Sekiro+ tail (version >= 16): */
    float                unk_c8;
    float                unk_cc;
    float                unk_d0;
    float                unk_d4;
    float                unk_d8;
    int32_t              unk_dc;
    float                unk_e0;
    int32_t              unk_e4;
};

struct sf_btl {
    int32_t               version;
    bool                  long_offsets;
    struct sf_btl_light  *lights;
    size_t                light_count;
    char                 *name_pool;       /* single bulk allocation for UTF-8 names */
    size_t                name_pool_size;
    const sf_allocator_t *alloc;
};

/*===========================================================================
 * Internal helpers — RGB (3-byte) color codec.
 * Upstream: BTL.cs:ReadRGB() / WriteRGB()
 *===========================================================================*/

static sf_result_t btl_read_rgb(sf_binary_reader_t *r, sf_color_t *out) {
    uint8_t rgb[3];
    sf_result_t e;
    if ((e = sf_binary_reader_read_u8(r, &rgb[0])) != SF_OK) return e;
    if ((e = sf_binary_reader_read_u8(r, &rgb[1])) != SF_OK) return e;
    if ((e = sf_binary_reader_read_u8(r, &rgb[2])) != SF_OK) return e;
    /* Upstream: Color.FromArgb(255, r, g, b) — alpha is forced to opaque. */
    out->a = 255;
    out->r = rgb[0];
    out->g = rgb[1];
    out->b = rgb[2];
    return SF_OK;
}

static sf_result_t btl_write_rgb(sf_binary_writer_t *w, sf_color_t c) {
    sf_result_t e;
    if ((e = sf_binary_writer_write_u8(w, c.r)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_u8(w, c.g)) != SF_OK) return e;
    if ((e = sf_binary_writer_write_u8(w, c.b)) != SF_OK) return e;
    return SF_OK;
}

/*===========================================================================
 * Read
 * Upstream: BTL.cs:Read()
 *===========================================================================*/

sf_result_t sf_btl_read_from_memory(sf_btl_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t       *s            = NULL;
    sf_binary_reader_t *r            = NULL;
    sf_btl_t           *btl          = NULL;
    uint8_t            *names_buf    = NULL;
    int64_t            *name_offsets = NULL;
    char              **utf8_tmp     = NULL;
    size_t             *utf8_lens    = NULL;
    sf_result_t         e            = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_reader_create(&r, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    /* Upstream: br.AssertInt32(2) */
    e = sf_binary_reader_assert_i32_one(r, 2);                       if (e != SF_OK) goto cleanup;

    /* Upstream: Version = br.AssertInt32(1, 2, 5, 6, 16, 18) */
    int32_t version = 0;
    {
        static const int32_t k_versions[6] = {1, 2, 5, 6, 16, 18};
        e = sf_binary_reader_assert_i32(r, 6, k_versions, &version);
        if (e != SF_OK) { e = SF_ERR_UNSUPPORTED_VERSION; goto cleanup; }
    }

    int32_t light_count_i32 = 0;
    int32_t names_length    = 0;
    e = sf_binary_reader_read_i32(r, &light_count_i32);              if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_read_i32(r, &names_length);                 if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i32_one(r, 0);                       if (e != SF_OK) goto cleanup;

    /* Upstream: lightSize = br.AssertInt32(0xC0, 0xC8, 0xE8) */
    int32_t light_size = 0;
    {
        static const int32_t k_sizes[3] = {0xC0, 0xC8, 0xE8};
        e = sf_binary_reader_assert_i32(r, 3, k_sizes, &light_size);
        if (e != SF_OK) goto cleanup;
    }
    e = sf_binary_reader_assert_pattern(r, 0x24, 0x00);              if (e != SF_OK) goto cleanup;

    /* Upstream: LongOffsets = br.VarintLong = lightSize != 0xC0 */
    bool long_offsets = (light_size != 0xC0);
    sf_binary_reader_set_varint_long(r, long_offsets);

    if (light_count_i32 < 0 || names_length < 0) {
        e = SF_ERR_BAD_MAGIC;
        goto cleanup;
    }
    size_t light_count = (size_t)light_count_i32;

    /* Upstream: long namesStart = br.Position; br.Skip(namesLength);
     * We materialise the names section into a buffer for offset-based access. */
    if (names_length > 0) {
        names_buf = (uint8_t *)sf_xalloc(alloc, (size_t)names_length);
        if (!names_buf) { e = SF_ERR_OOM; goto cleanup; }
        e = sf_binary_reader_read_bytes(r, names_buf, (size_t)names_length);
        if (e != SF_OK) goto cleanup;
    }

    /* Allocate the top-level struct + lights array. */
    btl = (sf_btl_t *)sf_xalloc(alloc, sizeof(*btl));
    if (!btl) { e = SF_ERR_OOM; goto cleanup; }
    memset(btl, 0, sizeof(*btl));
    btl->alloc        = alloc;
    btl->version      = version;
    btl->long_offsets = long_offsets;
    btl->light_count  = light_count;

    if (light_count > 0) {
        btl->lights = (struct sf_btl_light *)sf_xalloc(
            alloc, light_count * sizeof(*btl->lights));
        if (!btl->lights) { e = SF_ERR_OOM; goto cleanup; }
        memset(btl->lights, 0, light_count * sizeof(*btl->lights));

        name_offsets = (int64_t *)sf_xalloc(alloc, light_count * sizeof(int64_t));
        if (!name_offsets) { e = SF_ERR_OOM; goto cleanup; }
    }

    /* Per-light deserialisation. Upstream: new Light(br, namesStart, Version, LongOffsets) */
    for (size_t i = 0; i < light_count; i++) {
        struct sf_btl_light *L = &btl->lights[i];

        e = sf_binary_reader_read_bytes(r, L->unk00, 16);              if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_varint(r, &name_offsets[i]);         if (e != SF_OK) goto cleanup;

        uint32_t type_raw = 0;
        e = sf_binary_reader_read_u32(r, &type_raw);                   if (e != SF_OK) goto cleanup;
        if (type_raw > 2) { e = SF_ERR_BAD_MAGIC; goto cleanup; }
        L->type = (sf_btl_light_type_t)type_raw;

        e = sf_binary_reader_read_bool(r, &L->unk1c);                  if (e != SF_OK) goto cleanup;
        e = btl_read_rgb(r, &L->diffuse_color);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->diffuse_power);           if (e != SF_OK) goto cleanup;
        e = btl_read_rgb(r, &L->specular_color);                       if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_bool(r, &L->cast_shadows);           if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->specular_power);          if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->cone_angle);              if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk30);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk34);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_vec3(r, &L->position);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_vec3(r, &L->rotation);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_i32(r, &L->unk50);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk54);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->radius);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_i32(r, &L->unk5c);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_assert_i32_one(r, 0);                     if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_bytes(r, L->unk64, 4);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk68);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_rgba(r, &L->shadow_color);           if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk70);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->flicker_interval_min);    if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->flicker_interval_max);    if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->flicker_brightness_mult); if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_i32(r, &L->unk80);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_bytes(r, L->unk84, 4);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk88);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_assert_i32_one(r, 0);                     if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk90);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_assert_i32_one(r, 0);                     if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk98);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->near_clip);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_bytes(r, L->unk_a0, 4);              if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->sharpness);               if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_assert_i32_one(r, 0);                     if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk_ac);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_assert_varint_one(r, 0);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->width);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk_bc);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_bytes(r, L->unk_c0, 4);              if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_read_f32(r, &L->unk_c4);                  if (e != SF_OK) goto cleanup;

        /* Upstream: if (version >= 16) { … } — Sekiro/ER+ tail (32 bytes) */
        if (version >= 16) {
            e = sf_binary_reader_read_f32(r, &L->unk_c8);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_f32(r, &L->unk_cc);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_f32(r, &L->unk_d0);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_f32(r, &L->unk_d4);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_f32(r, &L->unk_d8);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_i32(r, &L->unk_dc);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_f32(r, &L->unk_e0);              if (e != SF_OK) goto cleanup;
            e = sf_binary_reader_read_i32(r, &L->unk_e4);              if (e != SF_OK) goto cleanup;
        }
    }

    /* Build the bulk UTF-8 name pool from collected offsets.
     * Phase 1: convert each UTF-16LE name into a temporary UTF-8 buffer,
     *          summing total bytes.
     * Phase 2: allocate the pool once, copy each name in. */
    if (light_count > 0) {
        utf8_tmp  = (char **)sf_xalloc(alloc, light_count * sizeof(char *));
        utf8_lens = (size_t *)sf_xalloc(alloc, light_count * sizeof(size_t));
        if (!utf8_tmp || !utf8_lens) { e = SF_ERR_OOM; goto cleanup; }
        memset(utf8_tmp, 0, light_count * sizeof(char *));
        memset(utf8_lens, 0, light_count * sizeof(size_t));

        size_t pool_size = 0;
        for (size_t i = 0; i < light_count; i++) {
            int64_t off = name_offsets[i];
            if (off < 0 || (size_t)off > (size_t)names_length) {
                e = SF_ERR_OUT_OF_RANGE;
                goto cleanup;
            }
            /* Walk UTF-16LE bytes until a u16 NUL terminator. */
            const uint8_t *start    = names_buf + (size_t)off;
            size_t         remaining = (size_t)names_length - (size_t)off;
            size_t         len_bytes = 0;
            while (len_bytes + 1 < remaining) {
                if (start[len_bytes] == 0 && start[len_bytes + 1] == 0) break;
                len_bytes += 2;
            }
            char  *utf8     = NULL;
            size_t utf8_len = 0;
            e = sf_utf16le_to_utf8(start, len_bytes, &utf8, &utf8_len, alloc);
            if (e != SF_OK) goto cleanup;
            utf8_tmp[i]  = utf8;
            utf8_lens[i] = utf8_len;
            pool_size   += utf8_len + 1;
        }

        if (pool_size == 0) pool_size = 1;
        btl->name_pool = (char *)sf_xalloc(alloc, pool_size);
        if (!btl->name_pool) { e = SF_ERR_OOM; goto cleanup; }
        btl->name_pool_size = pool_size;

        size_t pool_pos = 0;
        for (size_t i = 0; i < light_count; i++) {
            memcpy(btl->name_pool + pool_pos, utf8_tmp[i], utf8_lens[i]);
            btl->name_pool[pool_pos + utf8_lens[i]] = '\0';
            btl->lights[i].name = btl->name_pool + pool_pos;
            pool_pos += utf8_lens[i] + 1;
        }
    }

    *out = btl;
    btl = NULL; /* ownership transferred */

cleanup:
    if (utf8_tmp) {
        for (size_t i = 0; i < light_count; i++) sf_xfree(alloc, utf8_tmp[i]);
        sf_xfree(alloc, utf8_tmp);
    }
    sf_xfree(alloc, utf8_lens);
    if (btl) {
        sf_xfree(alloc, btl->name_pool);
        sf_xfree(alloc, btl->lights);
        sf_xfree(alloc, btl);
    }
    sf_xfree(alloc, name_offsets);
    sf_xfree(alloc, names_buf);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    return e;
}

/*===========================================================================
 * Write
 * Upstream: BTL.cs:Write()
 *===========================================================================*/

sf_result_t sf_btl_write_to_buffer(const sf_btl_t *btl, void **out_bytes,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(btl != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size  = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t       *s            = NULL;
    sf_binary_writer_t *w            = NULL;
    int64_t            *name_offsets = NULL;
    sf_result_t         e            = SF_OK;

    e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_writer_create(&w, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    /* Upstream: bw.VarintLong = LongOffsets */
    sf_binary_writer_set_varint_long(w, btl->long_offsets);

    /* Upstream: bw.WriteInt32(Version >= 16 ? 0xE8 : (LongOffsets ? 0xC8 : 0xC0)) */
    int32_t light_size;
    if (btl->version >= 16)        light_size = 0xE8;
    else if (btl->long_offsets)    light_size = 0xC8;
    else                           light_size = 0xC0;

    /* Header. Upstream: BTL.cs:Write() lines 69-75 */
    e = sf_binary_writer_write_i32(w, 2);                              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, btl->version);                   if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, (int32_t)btl->light_count);     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_reserve_i32(w, "NamesLength");                if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, 0);                              if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i32(w, light_size);                     if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_pattern(w, 0x24, 0x00);                 if (e != SF_OK) goto cleanup;

    int64_t names_start = sf_binary_writer_position(w);

    if (btl->light_count > 0) {
        name_offsets = (int64_t *)sf_xalloc(alloc, btl->light_count * sizeof(int64_t));
        if (!name_offsets) { e = SF_ERR_OOM; goto cleanup; }
    }

    /* Names section. Upstream: BTL.cs:Write() lines 79-86
     * Mirrors upstream's quirky padding: pad length is derived from the
     * name's START offset within the names section, not its end position.
     * This is preserved exactly so byte-equal round-trips work. */
    for (size_t i = 0; i < btl->light_count; i++) {
        int64_t name_offset = sf_binary_writer_position(w) - names_start;
        name_offsets[i]     = name_offset;
        const char *nm      = btl->lights[i].name ? btl->lights[i].name : "";
        e = sf_binary_writer_write_utf16(w, nm, true);
        if (e != SF_OK) goto cleanup;
        if (name_offset % 0x10 != 0) {
            size_t pad = (size_t)(0x10 - (name_offset % 0x10));
            e = sf_binary_writer_write_pattern(w, pad, 0x00);
            if (e != SF_OK) goto cleanup;
        }
    }

    /* Upstream: bw.FillInt32("NamesLength", (int)(bw.Position - namesStart)) */
    {
        int64_t names_end    = sf_binary_writer_position(w);
        int32_t names_length = (int32_t)(names_end - names_start);
        e = sf_binary_writer_fill_i32(w, "NamesLength", names_length);
        if (e != SF_OK) goto cleanup;
    }

    /* Lights. Upstream: Light.Write(bw, nameOffsets[i], Version, LongOffsets) */
    for (size_t i = 0; i < btl->light_count; i++) {
        const struct sf_btl_light *L = &btl->lights[i];

        e = sf_binary_writer_write_bytes(w, L->unk00, 16);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_varint(w, name_offsets[i]);         if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_u32(w, (uint32_t)L->type);          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bool(w, L->unk1c);                  if (e != SF_OK) goto cleanup;
        e = btl_write_rgb(w, L->diffuse_color);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->diffuse_power);           if (e != SF_OK) goto cleanup;
        e = btl_write_rgb(w, L->specular_color);                       if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bool(w, L->cast_shadows);           if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->specular_power);          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->cone_angle);              if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk30);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk34);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec3(w, L->position);               if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec3(w, L->rotation);               if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, L->unk50);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk54);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->radius);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, L->unk5c);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bytes(w, L->unk64, 4);              if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk68);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_rgba(w, L->shadow_color);           if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk70);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->flicker_interval_min);    if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->flicker_interval_max);    if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->flicker_brightness_mult); if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, L->unk80);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bytes(w, L->unk84, 4);              if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk88);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk90);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk98);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->near_clip);               if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bytes(w, L->unk_a0, 4);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->sharpness);               if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                          if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk_ac);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_varint(w, 0);                       if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->width);                   if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk_bc);                  if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_bytes(w, L->unk_c0, 4);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, L->unk_c4);                  if (e != SF_OK) goto cleanup;

        if (btl->version >= 16) {
            e = sf_binary_writer_write_f32(w, L->unk_c8);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_f32(w, L->unk_cc);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_f32(w, L->unk_d0);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_f32(w, L->unk_d4);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_f32(w, L->unk_d8);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_i32(w, L->unk_dc);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_f32(w, L->unk_e0);              if (e != SF_OK) goto cleanup;
            e = sf_binary_writer_write_i32(w, L->unk_e4);              if (e != SF_OK) goto cleanup;
        }
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);
    w = NULL; /* finish_bytes destroyed the writer */

cleanup:
    if (w) sf_binary_writer_destroy(w);
    sf_xfree(alloc, name_offsets);
    sf_ostream_close(s);
    return e;
}

/*===========================================================================
 * Destroy
 *===========================================================================*/

void sf_btl_destroy(sf_btl_t *btl) {
    if (!btl) return;
    sf_xfree(btl->alloc, btl->name_pool);
    sf_xfree(btl->alloc, btl->lights);
    sf_xfree(btl->alloc, btl);
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

int32_t sf_btl_version(const sf_btl_t *btl) {
    return btl ? btl->version : 0;
}

size_t sf_btl_light_count(const sf_btl_t *btl) {
    return btl ? btl->light_count : 0u;
}

const sf_btl_light_t *sf_btl_get_light(const sf_btl_t *btl, size_t index) {
    if (!btl || index >= btl->light_count) return NULL;
    return (const sf_btl_light_t *)&btl->lights[index];
}

const char *sf_btl_light_name(const sf_btl_light_t *light) {
    return light ? light->name : NULL;
}

sf_btl_light_type_t sf_btl_light_type(const sf_btl_light_t *light) {
    return light ? light->type : SF_BTL_LIGHT_TYPE_POINT;
}

const uint8_t *sf_btl_light_unk00(const sf_btl_light_t *light) {
    return light ? light->unk00 : NULL;
}

bool sf_btl_light_unk1c(const sf_btl_light_t *light) {
    return light ? light->unk1c : false;
}

sf_color_t sf_btl_light_diffuse_color(const sf_btl_light_t *light) {
    sf_color_t zero = {0, 0, 0, 0};
    return light ? light->diffuse_color : zero;
}

float sf_btl_light_diffuse_power(const sf_btl_light_t *light) {
    return light ? light->diffuse_power : 0.0f;
}

sf_color_t sf_btl_light_specular_color(const sf_btl_light_t *light) {
    sf_color_t zero = {0, 0, 0, 0};
    return light ? light->specular_color : zero;
}

bool sf_btl_light_cast_shadows(const sf_btl_light_t *light) {
    return light ? light->cast_shadows : false;
}

float sf_btl_light_specular_power(const sf_btl_light_t *light) {
    return light ? light->specular_power : 0.0f;
}

float sf_btl_light_cone_angle(const sf_btl_light_t *light) {
    return light ? light->cone_angle : 0.0f;
}

float sf_btl_light_unk30(const sf_btl_light_t *light) {
    return light ? light->unk30 : 0.0f;
}

float sf_btl_light_unk34(const sf_btl_light_t *light) {
    return light ? light->unk34 : 0.0f;
}

sf_vec3_t sf_btl_light_position(const sf_btl_light_t *light) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return light ? light->position : zero;
}

sf_vec3_t sf_btl_light_rotation(const sf_btl_light_t *light) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return light ? light->rotation : zero;
}

int32_t sf_btl_light_unk50(const sf_btl_light_t *light) {
    return light ? light->unk50 : 0;
}

float sf_btl_light_unk54(const sf_btl_light_t *light) {
    return light ? light->unk54 : 0.0f;
}

float sf_btl_light_radius(const sf_btl_light_t *light) {
    return light ? light->radius : 0.0f;
}

int32_t sf_btl_light_unk5c(const sf_btl_light_t *light) {
    return light ? light->unk5c : 0;
}

const uint8_t *sf_btl_light_unk64(const sf_btl_light_t *light) {
    return light ? light->unk64 : NULL;
}

float sf_btl_light_unk68(const sf_btl_light_t *light) {
    return light ? light->unk68 : 0.0f;
}

sf_color_t sf_btl_light_shadow_color(const sf_btl_light_t *light) {
    sf_color_t zero = {0, 0, 0, 0};
    return light ? light->shadow_color : zero;
}

float sf_btl_light_unk70(const sf_btl_light_t *light) {
    return light ? light->unk70 : 0.0f;
}

float sf_btl_light_flicker_interval_min(const sf_btl_light_t *light) {
    return light ? light->flicker_interval_min : 0.0f;
}

float sf_btl_light_flicker_interval_max(const sf_btl_light_t *light) {
    return light ? light->flicker_interval_max : 0.0f;
}

float sf_btl_light_flicker_brightness_mult(const sf_btl_light_t *light) {
    return light ? light->flicker_brightness_mult : 0.0f;
}

int32_t sf_btl_light_unk80(const sf_btl_light_t *light) {
    return light ? light->unk80 : 0;
}

const uint8_t *sf_btl_light_unk84(const sf_btl_light_t *light) {
    return light ? light->unk84 : NULL;
}

float sf_btl_light_unk88(const sf_btl_light_t *light) {
    return light ? light->unk88 : 0.0f;
}

float sf_btl_light_unk90(const sf_btl_light_t *light) {
    return light ? light->unk90 : 0.0f;
}

float sf_btl_light_unk98(const sf_btl_light_t *light) {
    return light ? light->unk98 : 0.0f;
}

float sf_btl_light_near_clip(const sf_btl_light_t *light) {
    return light ? light->near_clip : 0.0f;
}

const uint8_t *sf_btl_light_unk_a0(const sf_btl_light_t *light) {
    return light ? light->unk_a0 : NULL;
}

float sf_btl_light_sharpness(const sf_btl_light_t *light) {
    return light ? light->sharpness : 0.0f;
}

float sf_btl_light_unk_ac(const sf_btl_light_t *light) {
    return light ? light->unk_ac : 0.0f;
}

float sf_btl_light_width(const sf_btl_light_t *light) {
    return light ? light->width : 0.0f;
}

float sf_btl_light_unk_bc(const sf_btl_light_t *light) {
    return light ? light->unk_bc : 0.0f;
}

const uint8_t *sf_btl_light_unk_c0(const sf_btl_light_t *light) {
    return light ? light->unk_c0 : NULL;
}

float sf_btl_light_unk_c4(const sf_btl_light_t *light) {
    return light ? light->unk_c4 : 0.0f;
}

float sf_btl_light_unk_c8(const sf_btl_light_t *light) {
    return light ? light->unk_c8 : 0.0f;
}

float sf_btl_light_unk_cc(const sf_btl_light_t *light) {
    return light ? light->unk_cc : 0.0f;
}

float sf_btl_light_unk_d0(const sf_btl_light_t *light) {
    return light ? light->unk_d0 : 0.0f;
}

float sf_btl_light_unk_d4(const sf_btl_light_t *light) {
    return light ? light->unk_d4 : 0.0f;
}

float sf_btl_light_unk_d8(const sf_btl_light_t *light) {
    return light ? light->unk_d8 : 0.0f;
}

int32_t sf_btl_light_unk_dc(const sf_btl_light_t *light) {
    return light ? light->unk_dc : 0;
}

float sf_btl_light_unk_e0(const sf_btl_light_t *light) {
    return light ? light->unk_e0 : 0.0f;
}

int32_t sf_btl_light_unk_e4(const sf_btl_light_t *light) {
    return light ? light->unk_e4 : 0;
}
