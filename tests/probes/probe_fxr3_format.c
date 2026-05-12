/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Probe: extract Elden Ring sfxbnd_commoneffects.ffxbnd.dcx and report the
 * empirical FXR3 version/header-section distribution before implementing the
 * production FXR3 reader.
 *
 * Upstream reference: SoulsFormats/Formats/FXR3.cs
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

#define FXR_SAMPLE_LIMIT 10u
#define FXR3_BASE_HEADER_SIZE 112u
#define FXR3_SEKIRO_HEADER_SIZE 144u

typedef struct er_archive_probe {
    int            index;
    const wchar_t *bhd_path;
    const wchar_t *bdt_path;
} er_archive_probe_t;

typedef struct section_pair_probe {
    int32_t offset;
    int32_t count;
} section_pair_probe_t;

typedef struct fxr3_header_probe {
    int16_t              zero04;
    uint16_t             version;
    int32_t              header_one;
    int32_t              id;
    int32_t              state_map_offset;
    int32_t              state_map_count;
    section_pair_probe_t state;
    section_pair_probe_t transition;
    section_pair_probe_t container;
    section_pair_probe_t effect;
    section_pair_probe_t action;
    section_pair_probe_t property;
    section_pair_probe_t modifier;
    section_pair_probe_t condprop;
    section_pair_probe_t unkfieldlist;
    section_pair_probe_t field;
    int32_t              post_sections_one;
    int32_t              post_sections_zero;
    section_pair_probe_t reference;
    section_pair_probe_t external_value;
    section_pair_probe_t unk_blood_enabler;
    section_pair_probe_t section15;
} fxr3_header_probe_t;

static const er_archive_probe_t k_archives[] = {
    {0, SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd", SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bdt"},
    {1, SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bhd", SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bdt"},
    {2, SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bhd", SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bdt"},
    {3, SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bhd", SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bdt"},
};

static const char *const k_sfxbnd_paths[] = {
    "/sfx/sfxbnd_commoneffects.ffxbnd.dcx",
    "/sfx/sfxbnd_commoneffects.ffxbnd",
    "sfx/sfxbnd_commoneffects.ffxbnd.dcx",
    "sfx/sfxbnd_commoneffects.ffxbnd",
    NULL,
};

static int16_t read_i16_le(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t read_i32_le(const uint8_t *p)
{
    return (int32_t)read_u32_le(p);
}

static bool ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) return false;
    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if (suffix_len > name_len) return false;
    return memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

static uint64_t probe_path_hash_64_133(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = *p == '\\' ? '/' : *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static sf_result_t unwrap_outer_dcx_if_needed(void **bytes, size_t *size)
{
    sf_dcx_type_t     type = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t r    = sf_dcx_sniff(*bytes, *size, &type);
    if (r != SF_OK || type == SF_DCX_TYPE_NONE || type == SF_DCX_TYPE_UNKNOWN) return SF_OK;

    void         *decompressed      = NULL;
    size_t        decompressed_size = 0;
    sf_dcx_type_t out_type          = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   decomp_r =
        sf_dcx_decompress(*bytes, *size, &decompressed, &decompressed_size, &out_type, NULL);
    if (decomp_r != SF_OK) return decomp_r;

    sf_free(NULL, *bytes);
    *bytes = decompressed;
    *size  = decompressed_size;
    return SF_OK;
}

static sf_result_t extract_from_archive(const er_archive_probe_t *archive, const char *path,
                                        void **out, size_t *out_size)
{
    sf_bhd5_t  *bhd = NULL;
    sf_result_t r   = sf_bhd5_open(&bhd, archive->bhd_path, archive->bdt_path,
                                   SF_BHD5_GAME_ELDENRING, NULL);
    if (r != SF_OK) return r;

    void  *raw      = NULL;
    size_t raw_size = 0;
    r = sf_bhd5_extract_by_path(bhd, path, &raw, &raw_size, NULL);
    if (r == SF_ERR_NOT_FOUND) {
        r = sf_bhd5_extract_by_hash_64(bhd, sf_path_hash_64(path), &raw, &raw_size, NULL);
    }
    if (r == SF_ERR_NOT_FOUND) {
        r = sf_bhd5_extract_by_hash_32(bhd, sf_path_hash(path), &raw, &raw_size, NULL);
    }
    if (r == SF_ERR_NOT_FOUND) {
        r = sf_bhd5_extract_by_hash_64(bhd, probe_path_hash_64_133(path), &raw, &raw_size, NULL);
    }
    sf_bhd5_close(bhd);
    if (r != SF_OK) return r;

    r = unwrap_outer_dcx_if_needed(&raw, &raw_size);
    if (r != SF_OK) {
        sf_free(NULL, raw);
        return r;
    }

    *out      = raw;
    *out_size = raw_size;
    return SF_OK;
}

static sf_result_t extract_sfxbnd(void **out, size_t *out_size, int *found_in_data)
{
    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    *out           = NULL;
    *out_size      = 0;
    *found_in_data = -1;

    sf_result_t data0_r = er_extract_from_data0(k_sfxbnd_paths[0], out, out_size);
    if (data0_r == SF_OK && *out != NULL && *out_size > 0) {
        sf_result_t unwrap_r = unwrap_outer_dcx_if_needed(out, out_size);
        if (unwrap_r != SF_OK) return unwrap_r;
        *found_in_data = 0;
        return SF_OK;
    }
    if (*out != NULL) {
        sf_free(NULL, *out);
        *out = NULL;
    }

    for (size_t archive_index = 0; archive_index < sizeof(k_archives) / sizeof(k_archives[0]);
         ++archive_index) {
        for (size_t path_index = 0; k_sfxbnd_paths[path_index] != NULL; ++path_index) {
            void       *bytes = NULL;
            size_t      size  = 0;
            sf_result_t r =
                extract_from_archive(&k_archives[archive_index], k_sfxbnd_paths[path_index], &bytes,
                                     &size);
            if (r == SF_OK && bytes != NULL && size > 0) {
                *out           = bytes;
                *out_size      = size;
                *found_in_data = k_archives[archive_index].index;
                return SF_OK;
            }
            if (bytes != NULL) sf_free(NULL, bytes);
            if (r == SF_ERR_OODLE_NOT_FOUND) return r;
        }
    }

    return data0_r == SF_ERR_OODLE_NOT_FOUND ? data0_r : SF_ERR_NOT_FOUND;
}

static section_pair_probe_t read_pair(const uint8_t *p)
{
    section_pair_probe_t pair;
    pair.offset = read_i32_le(p);
    pair.count  = read_i32_le(p + 4u);
    return pair;
}

static sf_result_t parse_fxr3_header(const uint8_t *data, size_t size, fxr3_header_probe_t *out)
{
    if (!data || !out || size < FXR3_BASE_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(data, "FXR\0", 4u) != 0) return SF_ERR_BAD_MAGIC;

    memset(out, 0, sizeof(*out));
    out->zero04             = read_i16_le(data + 4u);
    out->version            = read_u16_le(data + 6u);
    out->header_one         = read_i32_le(data + 8u);
    out->id                 = read_i32_le(data + 12u);
    out->state_map_offset   = read_i32_le(data + 16u);
    out->state_map_count    = read_i32_le(data + 20u);
    out->state              = read_pair(data + 24u);
    out->transition         = read_pair(data + 32u);
    out->container          = read_pair(data + 40u);
    out->effect             = read_pair(data + 48u);
    out->action             = read_pair(data + 56u);
    out->property           = read_pair(data + 64u);
    out->modifier           = read_pair(data + 72u);
    out->condprop           = read_pair(data + 80u);
    out->unkfieldlist       = read_pair(data + 88u);
    out->field              = read_pair(data + 96u);
    out->post_sections_one  = read_i32_le(data + 104u);
    out->post_sections_zero = read_i32_le(data + 108u);

    if (out->version == 5u) {
        if (size < FXR3_SEKIRO_HEADER_SIZE) return SF_ERR_TRUNCATED;
        out->reference         = read_pair(data + 112u);
        out->external_value    = read_pair(data + 120u);
        out->unk_blood_enabler = read_pair(data + 128u);
        out->section15         = read_pair(data + 136u);
    }

    return SF_OK;
}

static void print_section_counts(const fxr3_header_probe_t *h)
{
    printf("SECTION_COUNTS: statemap=%ld state=%ld transition=%ld container=%ld effect=%ld "
           "action=%ld property=%ld modifier=%ld condprop=%ld unkfieldlist=%ld field=%ld",
           (long)h->state_map_count, (long)h->state.count, (long)h->transition.count,
           (long)h->container.count, (long)h->effect.count, (long)h->action.count,
           (long)h->property.count, (long)h->modifier.count, (long)h->condprop.count,
           (long)h->unkfieldlist.count, (long)h->field.count);
    if (h->version == 5u) {
        printf(" reference=%ld external_value=%ld unk_blood_enabler=%ld section15=%ld",
               (long)h->reference.count, (long)h->external_value.count,
               (long)h->unk_blood_enabler.count, (long)h->section15.count);
    }
    printf("\n");
}

static void print_hex_dump64(const uint8_t *data, size_t size)
{
    const size_t dump_size = size < 64u ? size : 64u;
    printf("HEX_DUMP_64:");
    for (size_t i = 0; i < dump_size; ++i) {
        if ((i % 16u) == 0u) printf("\n%04zx:", i);
        printf(" %02X", data[i]);
    }
    printf("\n");
}

int main(void)
{
    void  *bnd_bytes     = NULL;
    size_t bnd_size      = 0;
    int    found_in_data = -1;

    sf_result_t r = extract_sfxbnd(&bnd_bytes, &bnd_size, &found_in_data);
    if (r != SF_OK) {
        fprintf(stderr, "extract sfxbnd_commoneffects failed: %d\n", (int)r);
        return 1;
    }

    printf("FOUND_IN_DATA: %d\n", found_in_data);

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    if (r != SF_OK) {
        fprintf(stderr, "BND4 parse failed: %d\n", (int)r);
        return 1;
    }

    const size_t entry_count = sf_bnd4_file_count(bnd);
    printf("BND_ENTRY_COUNT: %zu\n", entry_count);

    size_t         fxr_count       = 0;
    size_t         sample_count    = 0;
    size_t         version4_count  = 0;
    size_t         version5_count  = 0;
    bool           unknown_version = false;
    const uint8_t *smallest_data   = NULL;
    size_t         smallest_size   = 0;
    const char    *smallest_path   = NULL;

    for (size_t i = 0; i < entry_count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || file->size == 0u) continue;
        if (!ends_with(file->name_utf8, ".fxr")) continue;

        ++fxr_count;
        if (!smallest_data || file->size < smallest_size) {
            smallest_data = file->data;
            smallest_size = file->size;
            smallest_path = file->name_utf8;
        }

        if (sample_count >= FXR_SAMPLE_LIMIT) continue;

        fxr3_header_probe_t header;
        r = parse_fxr3_header(file->data, file->size, &header);
        if (r != SF_OK) {
            fprintf(stderr, "FXR header parse failed for %s: %d\n", file->name_utf8, (int)r);
            sf_bnd4_destroy(bnd);
            return 1;
        }

        ++sample_count;
        if (header.version == 4u) {
            ++version4_count;
        } else if (header.version == 5u) {
            ++version5_count;
        } else {
            unknown_version = true;
            printf("UNKNOWN_VERSION: %u\n", (unsigned)header.version);
        }

        printf("SAMPLE[%zu]: %s\n", sample_count, file->name_utf8);
        printf("FXR_VERSION: %u\n", (unsigned)header.version);
        printf("FXR_ID: %ld\n", (long)header.id);
        printf("HEADER_SENTINELS: zero04=%ld header_one=%ld state_map_offset=%ld "
               "post_one=%ld post_zero=%ld\n",
               (long)header.zero04, (long)header.header_one, (long)header.state_map_offset,
               (long)header.post_sections_one, (long)header.post_sections_zero);
        print_section_counts(&header);
    }

    printf("FXR_ENTRY_COUNT: %zu\n", fxr_count);
    printf("FXR_VERSION_HIST: DS3=%zu Sekiro=%zu\n", version4_count, version5_count);

    if (smallest_data) {
        printf("SMALLEST_FXR: %s size=%zu\n", smallest_path ? smallest_path : "(null)",
               smallest_size);
        print_hex_dump64(smallest_data, smallest_size);
    }

    sf_bnd4_destroy(bnd);
    return unknown_version || sample_count != FXR_SAMPLE_LIMIT ? 1 : 0;
}
