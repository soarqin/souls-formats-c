/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_acb.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

typedef struct sf_acb_asset {
    sf_acb_asset_type_t type;
    char *absolute_path;
    char *relative_path;
} sf_acb_asset_t;

struct sf_acb {
    const sf_allocator_t *alloc;
    bool big_endian;
    sf_acb_asset_t *assets;
    size_t asset_count;
};

sf_result_t sf_acb_create(sf_acb_t **out, bool big_endian, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_acb_t *a = (sf_acb_t *)sf_xalloc(alloc, sizeof(*a));
    if (!a) return SF_ERR_OOM;
    memset(a, 0, sizeof(*a));
    a->alloc = alloc;
    a->big_endian = big_endian;
    *out = a;
    return SF_OK;
}

void sf_acb_destroy(sf_acb_t *a) {
    if (!a) return;
    if (a->assets) {
        for (size_t i = 0; i < a->asset_count; i++) {
            sf_xfree(a->alloc, a->assets[i].absolute_path);
            sf_xfree(a->alloc, a->assets[i].relative_path);
        }
        sf_xfree(a->alloc, a->assets);
    }
    sf_xfree(a->alloc, a);
}

bool sf_acb_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, "ACB\0", 4) == 0;
}

bool sf_acb_big_endian(const sf_acb_t *a) { return a ? a->big_endian : false; }

size_t sf_acb_asset_count(const sf_acb_t *a) {
    return a ? a->asset_count : 0u;
}

sf_result_t sf_acb_get_asset_type(const sf_acb_t *a, size_t index,
                                  sf_acb_asset_type_t *out) {
    SF_CHECK_ARG(a != NULL && out != NULL);
    if (index >= a->asset_count) return SF_ERR_OUT_OF_RANGE;
    *out = a->assets[index].type;
    return SF_OK;
}

sf_result_t sf_acb_get_asset_paths(const sf_acb_t *a, size_t index,
                                   const char **out_absolute,
                                   const char **out_relative) {
    SF_CHECK_ARG(a != NULL);
    if (index >= a->asset_count) return SF_ERR_OUT_OF_RANGE;
    if (out_absolute) *out_absolute = a->assets[index].absolute_path;
    if (out_relative) *out_relative = a->assets[index].relative_path;
    return SF_OK;
}

static sf_result_t acb_read_asset_base(sf_binary_reader_t *r, sf_acb_asset_t *asset,
                                       sf_acb_asset_type_t expected_type,
                                       const sf_allocator_t *alloc) {
    int32_t abs_path_off = 0, rel_path_off = 0;
    uint16_t type_u16 = 0;
    TRY(sf_binary_reader_read_i32(r, &abs_path_off));
    TRY(sf_binary_reader_read_i32(r, &rel_path_off));
    TRY(sf_binary_reader_read_u16(r, &type_u16));
    if (type_u16 != (uint16_t)expected_type) return SF_ERR_BAD_MAGIC;

    char *abs_path = NULL, *rel_path = NULL;
    sf_result_t e = sf_binary_reader_get_utf16(r, abs_path_off, &abs_path, NULL);
    if (e != SF_OK) return e;
    e = sf_binary_reader_get_utf16(r, rel_path_off, &rel_path, NULL);
    if (e != SF_OK) { sf_xfree(alloc, abs_path); return e; }

    asset->absolute_path = abs_path;
    asset->relative_path = rel_path;
    return SF_OK;
}

static sf_result_t acb_read_pwv(sf_binary_reader_t *r) {
    TRY(sf_binary_reader_assert_i16_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    return SF_OK;
}

static sf_result_t acb_read_general(sf_binary_reader_t *r) {
    TRY(sf_binary_reader_assert_i16_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    return SF_OK;
}

static sf_result_t acb_read_texture(sf_binary_reader_t *r) {
    TRY(sf_binary_reader_assert_i16_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    return SF_OK;
}

static sf_result_t acb_read_motion(sf_binary_reader_t *r) {
    TRY(sf_binary_reader_assert_i16_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    return SF_OK;
}

static sf_result_t acb_read_gi_texture(sf_binary_reader_t *r) {
    int32_t unk10 = 0;
    TRY(sf_binary_reader_assert_i16_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    TRY(sf_binary_reader_read_i32(r, &unk10));
    (void)unk10;
    return SF_OK;
}

static sf_result_t acb_read_model(sf_binary_reader_t *r) {
    int16_t unk0a = 0;
    int32_t members_offset = 0;
    int32_t draw_distance = 0;
    int16_t mesh_lod_rate = 0;
    bool reflectible = false;
    bool normal_interaction = false;
    int32_t unk20 = 0;
    uint8_t render_type = 0;
    bool disable_shadow_source = false;
    bool disable_shadow_target = false;
    bool unk27 = false;
    float unk28 = 0.0f;
    bool unk2c = false;
    bool fix_to_camera = false;
    bool unk2e = false;
    int16_t low_texture_distance = 0;
    int16_t cheap_render_distance = 0;
    uint8_t unk34 = 0;
    bool unk35 = false;
    bool unk36 = false;
    bool unk37 = false;

    TRY(sf_binary_reader_read_i16(r, &unk0a));
    TRY(sf_binary_reader_read_i32(r, &members_offset));
    TRY(sf_binary_reader_read_i32(r, &draw_distance));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    TRY(sf_binary_reader_assert_i32_one(r, 0));
    TRY(sf_binary_reader_read_i16(r, &mesh_lod_rate));
    TRY(sf_binary_reader_read_bool(r, &reflectible));
    TRY(sf_binary_reader_read_bool(r, &normal_interaction));
    TRY(sf_binary_reader_read_i32(r, &unk20));
    TRY(sf_binary_reader_read_u8(r, &render_type));
    TRY(sf_binary_reader_read_bool(r, &disable_shadow_source));
    TRY(sf_binary_reader_read_bool(r, &disable_shadow_target));
    TRY(sf_binary_reader_read_bool(r, &unk27));
    TRY(sf_binary_reader_read_f32(r, &unk28));
    TRY(sf_binary_reader_read_bool(r, &unk2c));
    TRY(sf_binary_reader_read_bool(r, &fix_to_camera));
    TRY(sf_binary_reader_read_bool(r, &unk2e));
    TRY(sf_binary_reader_assert_u8_one(r, 0));
    TRY(sf_binary_reader_read_i16(r, &low_texture_distance));
    TRY(sf_binary_reader_read_i16(r, &cheap_render_distance));
    TRY(sf_binary_reader_read_u8(r, &unk34));
    TRY(sf_binary_reader_read_bool(r, &unk35));
    TRY(sf_binary_reader_read_bool(r, &unk36));
    TRY(sf_binary_reader_read_bool(r, &unk37));
    TRY(sf_binary_reader_assert_pattern(r, 0x18, 0x00));

    (void)members_offset;
    return SF_OK;
}

sf_result_t sf_acb_read_from_memory(sf_acb_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_acb_t *acb = NULL;
    int32_t *asset_offsets = NULL;
    sf_result_t err = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    uint32_t probe = 0;
    err = sf_binary_reader_get_u32(r, 0xC, &probe);
    if (err != SF_OK) goto done;
    int64_t length = sf_binary_reader_length(r);
    bool big_endian = ((int64_t)probe > length);
    sf_binary_reader_set_big_endian(r, big_endian);

    err = sf_binary_reader_assert_ascii(r, "ACB"); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 2); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 1); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 0); if (err != SF_OK) goto done;

    int32_t asset_count = 0;
    int32_t offset_index_offset = 0;
    err = sf_binary_reader_read_i32(r, &asset_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &offset_index_offset); if (err != SF_OK) goto done;
    (void)offset_index_offset;

    if (asset_count < 0) { err = SF_ERR_BAD_MAGIC; goto done; }

    err = sf_acb_create(&acb, big_endian, alloc);
    if (err != SF_OK) goto done;

    if (asset_count > 0) {
        asset_offsets = (int32_t *)sf_xalloc(acb->alloc,
            (size_t)asset_count * sizeof(int32_t));
        if (!asset_offsets) { err = SF_ERR_OOM; goto done; }
        err = sf_binary_reader_read_i32s(r, (size_t)asset_count, asset_offsets);
        if (err != SF_OK) goto done;

        acb->assets = (sf_acb_asset_t *)sf_xalloc(acb->alloc,
            (size_t)asset_count * sizeof(sf_acb_asset_t));
        if (!acb->assets) { err = SF_ERR_OOM; goto done; }
        memset(acb->assets, 0, (size_t)asset_count * sizeof(sf_acb_asset_t));
        acb->asset_count = (size_t)asset_count;
    }

    for (int32_t i = 0; i < asset_count; i++) {
        int64_t asset_offset = asset_offsets[i];
        err = sf_istream_seek(sf_binary_reader_stream(r), asset_offset);
        if (err != SF_OK) goto done;

        uint16_t type_u16 = 0;
        err = sf_binary_reader_get_u16(r, asset_offset + 8, &type_u16);
        if (err != SF_OK) goto done;
        sf_acb_asset_type_t type = (sf_acb_asset_type_t)type_u16;
        acb->assets[i].type = type;

        err = acb_read_asset_base(r, &acb->assets[i], type, acb->alloc);
        if (err != SF_OK) goto done;

        switch (type) {
        case SF_ACB_ASSET_PWV:        err = acb_read_pwv(r);        break;
        case SF_ACB_ASSET_GENERAL:    err = acb_read_general(r);    break;
        case SF_ACB_ASSET_TEXTURE:    err = acb_read_texture(r);    break;
        case SF_ACB_ASSET_MOTION:     err = acb_read_motion(r);     break;
        case SF_ACB_ASSET_GI_TEXTURE: err = acb_read_gi_texture(r); break;
        case SF_ACB_ASSET_MODEL:      err = acb_read_model(r);      break;
        default: err = SF_ERR_BAD_MAGIC; break;
        }
        if (err != SF_OK) goto done;
    }

done:
    sf_xfree(alloc, asset_offsets);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_acb_destroy(acb); return err; }
    *out = acb;
    return SF_OK;
}
