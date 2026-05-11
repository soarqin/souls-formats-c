/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Probe: extract ER c0000.flver and report empirical FLVER2 BufferLayout members. */

#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"

#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLVER2_HEADER_SIZE 0x80u
#define DUMMY_FIXED_SIZE 64u
#define MATERIAL_FIXED_SIZE 32u
#define NODE_FIXED_SIZE 128u
#define MESH_FIXED_SIZE 48u
#define FACESET_FIXED_SIZE 32u
#define VERTEX_BUFFER_FIXED_SIZE 32u
#define BUFFER_LAYOUT_HEADER_SIZE 16u
#define LAYOUT_MEMBER_SIZE 20u
#define MAX_UNIQUE_LAYOUT_ITEMS 256u

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

typedef struct flver2_header_probe {
    uint32_t version;
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t dummy_count;
    uint32_t material_count;
    uint32_t bone_count;
    uint32_t mesh_count;
    uint32_t vertex_buffer_count;
    uint8_t  vertex_indices_size;
    bool     unicode;
    uint32_t face_set_count;
    uint32_t buffer_layout_count;
    uint32_t texture_count;
    int16_t  special_modifier;
} flver2_header_probe_t;

typedef struct layout_header_probe {
    uint32_t member_count;
    uint32_t members_offset;
    size_t   header_offset;
} layout_header_probe_t;

typedef struct layout_triple_probe {
    uint32_t type;
    uint32_t semantic;
    int32_t  index;
} layout_triple_probe_t;

typedef struct layout_pair_probe {
    uint32_t type;
    uint32_t semantic;
} layout_pair_probe_t;

typedef struct archive_probe {
    const char    *label;
    const wchar_t *bhd_path;
    const wchar_t *bdt_path;
    const char    *pem;
} archive_probe_t;

typedef struct bhd_entry_probe {
    int32_t padded_size;
    int32_t unpadded_size;
    int64_t file_offset;
    int64_t sha_hash_offset;
    int64_t aes_key_offset;
} bhd_entry_probe_t;

typedef struct extracted_probe {
    void       *bytes;
    size_t      size;
    const char *archive_label;
    const char *path;
} extracted_probe_t;

static const char *const k_chrbnd_paths[] = {
    "/chr/c0000.chrbnd.dcx",
    "/chr/c0000.chrbnd",
    "chr/c0000.chrbnd.dcx",
    "chr/c0000.chrbnd",
    "/chr/c0000/c0000.chrbnd.dcx",
    "/chr/c0000/c0000.chrbnd",
    "chr/c0000/c0000.chrbnd.dcx",
    "chr/c0000/c0000.chrbnd",
    NULL,
};

static const char k_data0_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA9Rju2whruXDVQZpfylVEPeNxm7XgMHcDyaaRUIpXQE0qEo+6Y36L\n"
    "P0xpFvL0H0kKxHwpuISsdgrnMHJ/yj4S61MWzhO8y4BQbw/zJehhDSRCecFJmFBz\n"
    "3I2JC5FCjoK+82xd9xM5XXdfsdBzRiSghuIHL4qk2WZ/0f/nK5VygeWXn/oLeYBL\n"
    "jX1S8wSSASza64JXjt0bP/i6mpV2SLZqKRxo7x2bIQrR1yHNekSF2jBhZIgcbtMB\n"
    "xjCywn+7p954wjcfjxB5VWaZ4hGbKhi1bhYPccht4XnGhcUTWO3NmJWslwccjQ4k\n"
    "sutLq3uRjLMM0IeTkQO6Pv8/R7UNFtdCWwIERzH8IQ==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char k_data1_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAxaBCHQJrtLJiJNdG9nq3deA9sY4YCZ4dbTOHO+v+YgWRMcE6iK6o\n"
    "ZIJq+nBMUNBbGPmbRrEjkkH9M7LAypAFOPKC6wMHzqIMBsUMuYffulBuOqtEBD11\n"
    "CAwfx37rjwJ+/1tnEqtJjYkrK9yyrIN6Y+jy4ftymQtjk83+L89pvMMmkNeZaPON\n"
    "4O9q5M9PnFoKvK8eY45ZV/Jyk+Pe+xc6+e4h4cx8ML5U2kMM3VDAJush4z/05hS3\n"
    "/bC4B6K9+7dPwgqZgKx1J7DBtLdHSAgwRPpijPeOjKcAa2BDaNp9Cfon70oC+ZCB\n"
    "+HkQ7FjJcF7KaHsH5oHvuI7EZAl2XTsLEQIENa/2JQ==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char k_data2_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEA0iDVVQ230RgrkIHJNDgxE7I/2AaH6Li1Eu9mtpfrrfhfoK2e7y4O\n"
    "WU+lj7AGI4GIgkWpPw8JHaV970Cr6+sTG4Tr5eMQPxrCIH7BJAPCloypxcs2BNfT\n"
    "GXzm6veUfrGzLIDp7wy24lIA8r9ZwUvpKlN28kxBDGeCbGCkYeSVNuF+R9rN4OAM\n"
    "RYh0r1Q950xc2qSNloNsjpDoSKoYN0T7u5rnMn/4mtclnWPVRWU940zr1rymv4Jc\n"
    "3umNf6cT1XqrS1gSaK1JWZfsSeD6Dwk3uvquvfY6YlGRygIlVEMAvKrDRMHylsLt\n"
    "qqhYkZNXMdy0NXopf1rEHKy9poaHEmJldwIFAP////8=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char k_data3_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAvRRNBnVq3WknCNHrJRelcEA2v/OzKlQkxZw1yKll0Y2Kn6G9ts94\n"
    "SfgZYbdFCnIXy5NEuyHRKrxXz5vurjhrcuoYAI2ZUhXPXZJdgHywac/i3S/IY0V/\n"
    "eDbqepyJWHpP6I565ySqlol1p/BScVjbEsVyvZGtWIXLPDbx4EYFKA5B52uK6Gdz\n"
    "4qcyVFtVEhNoMvg+EoWnyLD7EUzuB2Khl46CuNictyWrLlIHgpKJr1QD8a0ld0PD\n"
    "PHDZn03q6QDvZd23UW2d9J+/HeBt52j08+qoBXPwhndZsmPMWngQDaik6FM7EVRQ\n"
    "etKPi6h5uprVmMAS5wR/jQIVTMpTj/zJdwIEXszeQw==\n"
    "-----END RSA PUBLIC KEY-----\n";

static const archive_probe_t k_archives[] = {
    {"Data0", SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bdt", k_data0_pem},
    {"Data1", SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bdt", k_data1_pem},
    {"Data2", SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bdt", k_data2_pem},
    {"Data3", SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bdt", k_data3_pem},
};

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t read_i32_le(const uint8_t *p)
{
    return (int32_t)read_u32_le(p);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return ((uint64_t)read_u32_le(p)) | ((uint64_t)read_u32_le(p + 4) << 32);
}

static int16_t read_i16_le(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static bool ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) return false;
    const size_t name_len = strlen(name);
    const size_t suffix_len = strlen(suffix);
    if (suffix_len > name_len) return false;
    return memcmp(name + name_len - suffix_len, suffix, suffix_len) == 0;
}

static bool checked_span(size_t size, size_t offset, size_t count, size_t elem_size)
{
    if (elem_size != 0 && count > (SIZE_MAX / elem_size)) return false;
    const size_t bytes = count * elem_size;
    return offset <= size && bytes <= size - offset;
}

static uint64_t probe_path_hash_64(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = *p == '\\' ? '/' : *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static sf_result_t read_wfile_bytes(const wchar_t *path, uint8_t **out, size_t *out_size)
{
    sf_istream_t *s = NULL;
    sf_result_t   r = sf_istream_open_wfile(&s, path, NULL);
    if (r != SF_OK) return r;
    const int64_t len = sf_istream_length(s);
    if (len < 0 || (uint64_t)len > SIZE_MAX) {
        sf_istream_close(s);
        return SF_ERR_OUT_OF_RANGE;
    }
    uint8_t *buf = NULL;
    if (len > 0) {
        buf = sf_default_allocator()->alloc((size_t)len, sf_default_allocator()->user);
        if (!buf) {
            sf_istream_close(s);
            return SF_ERR_OOM;
        }
        r = sf_istream_read(s, buf, (size_t)len);
    }
    sf_istream_close(s);
    if (r != SF_OK) {
        sf_free(NULL, buf);
        return r;
    }
    *out = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static sf_result_t rsa_unwrap_bhd(const uint8_t *in, size_t in_size, const char *pem,
                                  uint8_t **out, size_t *out_size)
{
    if (in_size == 0 || (in_size % 256u) != 0) return SF_ERR_CRYPTO;
    uint8_t *plain = sf_default_allocator()->alloc(in_size, sf_default_allocator()->user);
    if (!plain) return SF_ERR_OOM;
    size_t used = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t *chunk = NULL;
        size_t   chunk_size = 0;
        sf_result_t r = sfi_rsa_decrypt_pkcs1(pem, in + off, 256u, &chunk, &chunk_size, NULL);
        if (r != SF_OK) {
            sf_free(NULL, plain);
            return r;
        }
        if (chunk_size > in_size - used) {
            sf_free(NULL, chunk);
            sf_free(NULL, plain);
            return SF_ERR_OUT_OF_RANGE;
        }
        if (chunk_size > 255u || 255u > in_size - used) {
            sf_free(NULL, chunk);
            sf_free(NULL, plain);
            return SF_ERR_OUT_OF_RANGE;
        }
        memset(plain + used, 0, 255u - chunk_size);
        memcpy(plain + used + (255u - chunk_size), chunk, chunk_size);
        used += 255u;
        sf_free(NULL, chunk);
    }
    *out = plain;
    *out_size = used;
    return SF_OK;
}

static bool find_bhd_entry(const uint8_t *bhd, size_t bhd_size, const char *path,
                           bhd_entry_probe_t *out)
{
    const uint64_t hashes[] = {sf_path_hash_64(path), probe_path_hash_64(path)};

    if (bhd_size < 0x28u || memcmp(bhd, "BHD5", 4) != 0) return false;
    const bool is64_bit = read_u32_le(bhd + 0x14) == 0 && read_u32_le(bhd + 0x1C) == 0;
    const uint64_t bucket_count64 = is64_bit ? read_u64_le(bhd + 0x10) : read_u32_le(bhd + 0x10);
    const uint64_t buckets_offset64 = is64_bit ? read_u64_le(bhd + 0x18) : read_u32_le(bhd + 0x14);
    if (bucket_count64 > 1000000u || buckets_offset64 > SIZE_MAX) return false;

    const size_t bucket_count = (size_t)bucket_count64;
    const size_t buckets_offset = (size_t)buckets_offset64;
    const size_t bucket_size = is64_bit ? 16u : 8u;
    if (!checked_span(bhd_size, buckets_offset, bucket_count, bucket_size)) return false;

    for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
        const size_t bucket_offset = buckets_offset + bucket * bucket_size;
        const uint32_t file_count = read_u32_le(bhd + bucket_offset);
        const uint64_t header_offset64 = is64_bit ? read_u64_le(bhd + bucket_offset + 8u)
                                                  : read_u32_le(bhd + bucket_offset + 4u);
        if (header_offset64 > SIZE_MAX) return false;
        const size_t header_offset = (size_t)header_offset64;
        if (!checked_span(bhd_size, header_offset, file_count, 40u)) return false;

        for (uint32_t file = 0; file < file_count; ++file) {
            const size_t pos = header_offset + (size_t)file * 40u;
            const uint64_t candidate_hash = read_u64_le(bhd + pos);
            bool matches = false;
            for (size_t hash_index = 0; hash_index < sizeof(hashes) / sizeof(hashes[0]);
                 ++hash_index) {
                if (candidate_hash == hashes[hash_index]) {
                    matches = true;
                    break;
                }
            }
            if (!matches) continue;

            const int32_t padded = (int32_t)read_u32_le(bhd + pos + 8);
            const int32_t unpadded = (int32_t)read_u32_le(bhd + pos + 12);
            const int64_t file_offset = (int64_t)read_u64_le(bhd + pos + 16);
            const int64_t sha_offset = (int64_t)read_u64_le(bhd + pos + 24);
            const int64_t aes_offset = (int64_t)read_u64_le(bhd + pos + 32);
            if (padded > 0 && unpadded >= 0 && (unpadded == 0 || unpadded <= padded) && file_offset >= 0 &&
                sha_offset >= 0 && aes_offset >= 0 && (uint64_t)sha_offset < bhd_size &&
                (uint64_t)aes_offset < bhd_size) {
                out->padded_size = padded;
                out->unpadded_size = unpadded;
                out->file_offset = file_offset;
                out->sha_hash_offset = sha_offset;
                out->aes_key_offset = aes_offset;
                return true;
            }
        }
    }
    return false;
}

static sf_result_t decrypt_bhd_ranges(uint8_t *data, size_t size, int64_t file_offset,
                                      const uint8_t *bhd, size_t bhd_size, int64_t aes_offset)
{
    size_t meta_offset = 0;
    bool   found = false;
    const size_t begin = aes_offset > 32 ? (size_t)aes_offset - 32u : 0u;
    const size_t end = (uint64_t)aes_offset + 64u < bhd_size ? (size_t)aes_offset + 64u : bhd_size;
    for (size_t cand = begin; cand + 20u <= end; ++cand) {
        const int32_t count = (int32_t)read_u32_le(bhd + cand + 16u);
        if (count <= 0 || count > 16 || cand + 20u + (size_t)count * 16u > bhd_size) continue;
        const int64_t start0 = (int64_t)read_u64_le(bhd + cand + 20u);
        const int64_t end0 = (int64_t)read_u64_le(bhd + cand + 28u);
        const bool relative_range = start0 == 0 && end0 > 0 && (uint64_t)end0 <= size;
        const bool absolute_range = start0 >= file_offset && end0 > start0 &&
                                    (uint64_t)(end0 - file_offset) <= size;
        if ((relative_range || absolute_range) && ((end0 - start0) % 16) == 0) {
            meta_offset = cand;
            found = true;
            break;
        }
    }
    if (!found) return SF_ERR_NOT_FOUND;
    const uint8_t *meta = bhd + meta_offset;
    uint8_t        key[16];
    memcpy(key, meta, sizeof(key));
    const int32_t range_count = (int32_t)read_u32_le(meta + 16);
    if (range_count < 0 || range_count > 1024) return SF_ERR_OUT_OF_RANGE;
    if (meta_offset + 20u + (size_t)range_count * 16u > bhd_size) return SF_ERR_TRUNCATED;
    for (int32_t i = 0; i < range_count; ++i) {
        const uint8_t *range = meta + 20 + (size_t)i * 16u;
        const int64_t start = (int64_t)read_u64_le(range);
        const int64_t end_range = (int64_t)read_u64_le(range + 8);
        if (end_range == start) continue;
        if (start < 0 || end_range < start || ((end_range - start) % 16) != 0) {
            return SF_ERR_OUT_OF_RANGE;
        }
        int64_t rel_start = start;
        int64_t rel_end = end_range;
        if ((uint64_t)end_range > size) {
            if (start < file_offset || end_range < file_offset) return SF_ERR_TRUNCATED;
            rel_start = start - file_offset;
            rel_end = end_range - file_offset;
        }
        if (rel_start < 0 || rel_end < rel_start || (uint64_t)rel_end > size) {
            return SF_ERR_TRUNCATED;
        }
        sf_result_t r = sfi_aes_decrypt_ecb_buffer(key, data + rel_start,
                                                  (size_t)(rel_end - rel_start));
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static bool is_supported_version(uint32_t version)
{
    static const uint32_t versions[] = {0x20005u, 0x20007u, 0x20009u, 0x2000Bu,
                                        0x2000Cu, 0x2000Du, 0x2000Eu, 0x2000Fu,
                                        0x20010u, 0x20013u, 0x20014u, 0x20016u,
                                        0x20017u, 0x2001Au, 0x2001Bu, 0x20021u};
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); ++i) {
        if (version == versions[i]) return true;
    }
    return false;
}

static const char *layout_type_name(uint32_t type)
{
    switch (type) {
    case 0: return "Float1";
    case 1: return "Float2";
    case 2: return "Float3";
    case 3: return "Float4";
    case 16: return "Color";
    case 17: return "UByte4";
    case 18: return "Byte4";
    case 19: return "UByte4Norm";
    case 20: return "Byte4Norm";
    case 21: return "Short2";
    case 22: return "Short4";
    case 23: return "UShort2";
    case 24: return "UShort4";
    case 26: return "Short4Norm";
    case 45: return "Half2";
    case 46: return "Half4";
    case 47: return "Byte4E";
    case 240: return "EdgeCompressed";
    default: return "Unknown";
    }
}

static const char *layout_semantic_name(uint32_t semantic)
{
    switch (semantic) {
    case 0: return "Position";
    case 1: return "BoneWeights";
    case 2: return "BoneIndices";
    case 3: return "Normal";
    case 5: return "UV";
    case 6: return "Tangent";
    case 7: return "Bitangent";
    case 10: return "VertexColor";
    default: return "Unknown";
    }
}

static int compare_triples(const void *a, const void *b)
{
    const layout_triple_probe_t *lhs = a;
    const layout_triple_probe_t *rhs = b;
    if (lhs->type != rhs->type) return lhs->type < rhs->type ? -1 : 1;
    if (lhs->semantic != rhs->semantic) return lhs->semantic < rhs->semantic ? -1 : 1;
    if (lhs->index != rhs->index) return lhs->index < rhs->index ? -1 : 1;
    return 0;
}

static int compare_pairs(const void *a, const void *b)
{
    const layout_pair_probe_t *lhs = a;
    const layout_pair_probe_t *rhs = b;
    if (lhs->type != rhs->type) return lhs->type < rhs->type ? -1 : 1;
    if (lhs->semantic != rhs->semantic) return lhs->semantic < rhs->semantic ? -1 : 1;
    return 0;
}

static bool add_unique_triple(layout_triple_probe_t *items, size_t *count,
                              layout_triple_probe_t item)
{
    for (size_t i = 0; i < *count; ++i) {
        if (items[i].type == item.type && items[i].semantic == item.semantic &&
            items[i].index == item.index) {
            return true;
        }
    }
    if (*count >= MAX_UNIQUE_LAYOUT_ITEMS) return false;
    items[*count] = item;
    ++(*count);
    return true;
}

static bool add_unique_pair(layout_pair_probe_t *items, size_t *count, layout_pair_probe_t item)
{
    for (size_t i = 0; i < *count; ++i) {
        if (items[i].type == item.type && items[i].semantic == item.semantic) return true;
    }
    if (*count >= MAX_UNIQUE_LAYOUT_ITEMS) return false;
    items[*count] = item;
    ++(*count);
    return true;
}

static int parse_header(const uint8_t *flver, size_t size, flver2_header_probe_t *out)
{
    if (!checked_span(size, 0, 1, FLVER2_HEADER_SIZE)) {
        fprintf(stderr, "FLVER is smaller than fixed FLVER2 header: %zu bytes\n", size);
        return 1;
    }
    if (memcmp(flver, "FLVER\0", 6) != 0) {
        fprintf(stderr, "Unexpected FLVER magic\n");
        return 1;
    }
    if (flver[6] == 'B' && flver[7] == '\0') {
        fprintf(stderr, "Big-endian FLVER2 is unsupported by this probe\n");
        return 1;
    }
    if (!(flver[6] == 'L' && flver[7] == '\0')) {
        fprintf(stderr, "Unexpected endian marker at 0x06: 0x%02X 0x%02X\n", flver[6], flver[7]);
        return 1;
    }

    out->version = read_u32_le(flver + 0x08);
    if (!is_supported_version(out->version)) {
        fprintf(stderr, "Unsupported FLVER2 version: 0x%X\n", out->version);
        return 1;
    }
    if (out->version < 0x20014u) {
        fprintf(stderr, "Probe expects the 0x80-byte v0x20014+ header shape\n");
        return 1;
    }

    out->data_offset = read_u32_le(flver + 0x0C);
    out->data_length = read_u32_le(flver + 0x10);
    out->dummy_count = read_u32_le(flver + 0x14);
    out->material_count = read_u32_le(flver + 0x18);
    out->bone_count = read_u32_le(flver + 0x1C);
    out->mesh_count = read_u32_le(flver + 0x20);
    out->vertex_buffer_count = read_u32_le(flver + 0x24);
    out->vertex_indices_size = flver[0x48];
    out->unicode = flver[0x49] != 0;
    out->face_set_count = read_u32_le(flver + 0x50);
    out->buffer_layout_count = read_u32_le(flver + 0x54);
    out->texture_count = read_u32_le(flver + 0x58);
    out->special_modifier = read_i16_le(flver + 0x6A);
    return 0;
}

static int add_section_skip(size_t size, const char *name, size_t start, uint32_t count,
                            size_t elem_size, size_t *cursor)
{
    if (!checked_span(size, start, count, elem_size)) {
        fprintf(stderr, "%s section exceeds FLVER bounds\n", name);
        return 1;
    }
    const size_t skip = (size_t)count * elem_size;
    printf("%s_start: 0x%zX skip=%zu %s_end=0x%zX\n", name, start, skip, name,
           start + skip);
    *cursor = start + skip;
    return 0;
}

static int report_layouts(const uint8_t *flver, size_t size,
                          const flver2_header_probe_t *header, size_t layouts_offset)
{
    if (!checked_span(size, layouts_offset, header->buffer_layout_count,
                      BUFFER_LAYOUT_HEADER_SIZE)) {
        fprintf(stderr, "BufferLayout header table exceeds FLVER bounds\n");
        return 1;
    }

    layout_header_probe_t *layouts = calloc(header->buffer_layout_count, sizeof(*layouts));
    if (!layouts) {
        fprintf(stderr, "Out of memory allocating layout header table\n");
        return 1;
    }

    layout_triple_probe_t triples[MAX_UNIQUE_LAYOUT_ITEMS];
    layout_pair_probe_t   pairs[MAX_UNIQUE_LAYOUT_ITEMS];
    size_t                triple_count = 0;
    size_t                pair_count = 0;

    for (uint32_t i = 0; i < header->buffer_layout_count; ++i) {
        const size_t offset = layouts_offset + (size_t)i * BUFFER_LAYOUT_HEADER_SIZE;
        layouts[i].header_offset = offset;
        layouts[i].member_count = read_u32_le(flver + offset);
        const uint32_t unk04 = read_u32_le(flver + offset + 4);
        const uint32_t unk08 = read_u32_le(flver + offset + 8);
        layouts[i].members_offset = read_u32_le(flver + offset + 12);
        if (unk04 != 0 || unk08 != 0) {
            fprintf(stderr, "BufferLayout[%" PRIu32 "] has non-zero reserved fields\n", i);
            free(layouts);
            return 1;
        }
    }

    for (uint32_t i = 0; i < header->buffer_layout_count; ++i) {
        if (!checked_span(size, layouts[i].members_offset, layouts[i].member_count,
                          LAYOUT_MEMBER_SIZE)) {
            fprintf(stderr, "BufferLayout[%" PRIu32 "] members exceed FLVER bounds\n", i);
            free(layouts);
            return 1;
        }

        for (uint32_t j = 0; j < layouts[i].member_count; ++j) {
            const size_t member_offset = layouts[i].members_offset + (size_t)j * LAYOUT_MEMBER_SIZE;
            const uint32_t type = read_u32_le(flver + member_offset + 8);
            const uint32_t semantic = read_u32_le(flver + member_offset + 12);
            const int32_t index = read_i32_le(flver + member_offset + 16);
            if (!add_unique_triple(triples, &triple_count,
                                   (layout_triple_probe_t){type, semantic, index}) ||
                !add_unique_pair(pairs, &pair_count, (layout_pair_probe_t){type, semantic})) {
                fprintf(stderr, "Too many unique layout items for probe fixed arrays\n");
                free(layouts);
                return 1;
            }
        }
    }

    qsort(triples, triple_count, sizeof(triples[0]), compare_triples);
    qsort(pairs, pair_count, sizeof(pairs[0]), compare_pairs);

    printf("LAYOUT_PAIR_COUNT: %zu\n", pair_count);
    printf("LAYOUT_TRIPLE_COUNT: %zu\n", triple_count);
    printf("--- UNIQUE LAYOUT PAIRS ---\n");
    for (size_t i = 0; i < pair_count; ++i) {
        printf("TYPE=%" PRIu32 "(%s) SEMANTIC=%" PRIu32 "(%s)\n", pairs[i].type,
               layout_type_name(pairs[i].type), pairs[i].semantic,
               layout_semantic_name(pairs[i].semantic));
    }

    printf("--- UNIQUE LAYOUT TRIPLES ---\n");
    for (size_t i = 0; i < triple_count; ++i) {
        printf("TYPE=%" PRIu32 "(%s) SEMANTIC=%" PRIu32 "(%s) INDEX=%" PRId32 "\n",
               triples[i].type, layout_type_name(triples[i].type), triples[i].semantic,
               layout_semantic_name(triples[i].semantic), triples[i].index);
    }

    printf("--- BUFFER LAYOUT MEMBER DIAGNOSTICS ---\n");
    for (uint32_t i = 0; i < header->buffer_layout_count; ++i) {
        const size_t members_end = layouts[i].members_offset +
                                   (size_t)layouts[i].member_count * LAYOUT_MEMBER_SIZE;
        printf("layout[%.3" PRIu32 "]_header: 0x%zX member_count=%" PRIu32
               " members_offset=0x%" PRIX32 "\n",
               i, layouts[i].header_offset, layouts[i].member_count, layouts[i].members_offset);
        printf("layout[%.3" PRIu32 "]_members: 0x%" PRIX32 " skip=%zu members_end=0x%zX\n",
               i, layouts[i].members_offset,
               (size_t)layouts[i].member_count * LAYOUT_MEMBER_SIZE, members_end);
    }

    free(layouts);
    return 0;
}

static const sf_binder_file_t *find_flver_entry(const sf_bnd4_t *bnd,
                                                flver2_header_probe_t *out_header)
{
    const size_t count = sf_bnd4_file_count(bnd);
    const sf_binder_file_t *fallback = NULL;
    flver2_header_probe_t fallback_header;

    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || !ends_with(file->name_utf8, ".flver")) {
            continue;
        }

        flver2_header_probe_t header;
        if (parse_header(file->data, file->size, &header) != 0) continue;
        if (header.buffer_layout_count > 0 && header.mesh_count > 0) {
            *out_header = header;
            return file;
        }
        if (!fallback && ends_with(file->name_utf8, "c0000.flver")) {
            fallback = file;
            fallback_header = header;
        }
    }

    if (fallback) *out_header = fallback_header;
    return fallback;
}

static sf_result_t unwrap_one_dcx_if_present(void *raw, size_t raw_size, void **out,
                                             size_t *out_size)
{
    sf_dcx_type_t     type = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx = sniff_r == SF_OK && type != SF_DCX_TYPE_NONE &&
                        type != SF_DCX_TYPE_UNKNOWN;
    if (!is_dcx) {
        *out = raw;
        *out_size = raw_size;
        return SF_OK;
    }

    void         *decompressed = NULL;
    size_t        decompressed_size = 0;
    sf_dcx_type_t out_type = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   r = sf_dcx_decompress(raw, raw_size, &decompressed, &decompressed_size,
                                      &out_type, NULL);
    sf_free(NULL, raw);
    if (r != SF_OK) return r;

    *out = decompressed;
    *out_size = decompressed_size;
    return SF_OK;
}

static sf_result_t extract_from_archive(const archive_probe_t *archive, const char *path,
                                        void **out, size_t *out_size)
{
    uint8_t *bhd_raw = NULL;
    uint8_t *bhd = NULL;
    uint8_t *raw = NULL;
    size_t   bhd_raw_size = 0;
    size_t   bhd_size = 0;
    sf_result_t r = read_wfile_bytes(archive->bhd_path, &bhd_raw, &bhd_raw_size);
    if (r != SF_OK) return r;
    r = rsa_unwrap_bhd(bhd_raw, bhd_raw_size, archive->pem, &bhd, &bhd_size);
    sf_free(NULL, bhd_raw);
    if (r != SF_OK) return r;
    if (bhd_size < 4 || memcmp(bhd, "BHD5", 4) != 0) {
        sf_free(NULL, bhd);
        return SF_ERR_BAD_MAGIC;
    }

    bhd_entry_probe_t entry;
    if (!find_bhd_entry(bhd, bhd_size, path, &entry)) {
        sf_free(NULL, bhd);
        return SF_ERR_NOT_FOUND;
    }

    sf_istream_t *bdt = NULL;
    r = sf_istream_open_wfile(&bdt, archive->bdt_path, NULL);
    if (r != SF_OK) {
        sf_free(NULL, bhd);
        return r;
    }
    raw = sf_default_allocator()->alloc((size_t)entry.padded_size, sf_default_allocator()->user);
    if (!raw) {
        sf_istream_close(bdt);
        sf_free(NULL, bhd);
        return SF_ERR_OOM;
    }
    r = sf_istream_seek(bdt, entry.file_offset);
    if (r == SF_OK) r = sf_istream_read(bdt, raw, (size_t)entry.padded_size);
    sf_istream_close(bdt);
    if (r == SF_OK && entry.aes_key_offset != 0) {
        r = decrypt_bhd_ranges(raw, (size_t)entry.padded_size, entry.file_offset, bhd, bhd_size,
                               entry.aes_key_offset);
    }
    sf_free(NULL, bhd);
    if (r != SF_OK) {
        sf_free(NULL, raw);
        return r;
    }

    return unwrap_one_dcx_if_present(raw, (size_t)entry.padded_size, out, out_size);
}

static sf_result_t extract_chrbnd(extracted_probe_t *out)
{
    memset(out, 0, sizeof(*out));

    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    sf_result_t first_error = SF_ERR_NOT_FOUND;
    for (size_t archive_index = 0; archive_index < sizeof(k_archives) / sizeof(k_archives[0]);
         ++archive_index) {
        for (size_t path_index = 0; k_chrbnd_paths[path_index] != NULL; ++path_index) {
            void  *bytes = NULL;
            size_t size = 0;
            sf_result_t r = extract_from_archive(&k_archives[archive_index],
                                                 k_chrbnd_paths[path_index], &bytes, &size);
            if (r == SF_OK && bytes && size > 0) {
                out->bytes = bytes;
                out->size = size;
                out->archive_label = k_archives[archive_index].label;
                out->path = k_chrbnd_paths[path_index];
                return SF_OK;
            }
            sf_free(NULL, bytes);
            if (first_error == SF_ERR_NOT_FOUND) first_error = r;
            if (r == SF_ERR_OODLE_NOT_FOUND) return r;
        }
    }

    return first_error;
}

int main(void)
{
    extracted_probe_t extracted;
    int               exit_code = 1;
    sf_bnd4_t        *chrbnd = NULL;

    sf_result_t r = extract_chrbnd(&extracted);
    if (r != SF_OK) {
        fprintf(stderr, "extract c0000.chrbnd failed: %d\n", (int)r);
        return 1;
    }

    r = sf_bnd4_read_from_memory(&chrbnd, extracted.bytes, extracted.size, NULL);
    sf_free(NULL, extracted.bytes);
    extracted.bytes = NULL;
    if (r != SF_OK) {
        const uint8_t *p = extracted.bytes;
        fprintf(stderr, "sf_bnd4_read_from_memory(c0000.chrbnd) failed: %d first=%02X %02X %02X %02X\n",
                (int)r, p ? p[0] : 0, p ? p[1] : 0, p ? p[2] : 0, p ? p[3] : 0);
        return 1;
    }

    flver2_header_probe_t header;
    const sf_binder_file_t *flver_file = find_flver_entry(chrbnd, &header);
    if (!flver_file || !flver_file->data) {
        fprintf(stderr, "Could not find a FLVER entry in c0000.chrbnd.dcx\n");
        goto cleanup;
    }

    size_t cursor = FLVER2_HEADER_SIZE;
    printf("SOURCE_ARCHIVE: %s\n", extracted.archive_label);
    printf("SOURCE_PATH: %s\n", extracted.path);
    printf("SOURCE_FLVER: %s\n", flver_file->name_utf8 ? flver_file->name_utf8 : "(null)");
    printf("HEADER_VERSION: 0x%X\n", header.version);
    printf("ENDIAN_BYTE: 0\n");
    printf("BUFFER_LAYOUT_COUNT: %" PRIu32 "\n", header.buffer_layout_count);
    printf("MESH_COUNT: %" PRIu32 "\n", header.mesh_count);
    printf("DUMMY_COUNT: %" PRIu32 "\n", header.dummy_count);
    printf("MATERIAL_COUNT: %" PRIu32 "\n", header.material_count);
    printf("BONE_COUNT: %" PRIu32 "\n", header.bone_count);
    printf("FACE_SET_COUNT: %" PRIu32 "\n", header.face_set_count);
    printf("VERTEX_BUFFER_COUNT: %" PRIu32 "\n", header.vertex_buffer_count);
    printf("TEXTURE_COUNT: %" PRIu32 "\n", header.texture_count);
    printf("DATA_OFFSET: 0x%" PRIX32 "\n", header.data_offset);
    printf("DATA_LENGTH: %" PRIu32 "\n", header.data_length);
    printf("VERTEX_INDICES_SIZE: %" PRIu8 "\n", header.vertex_indices_size);
    printf("UNICODE: %u\n", header.unicode ? 1u : 0u);
    printf("SPECIAL_MODIFIER: %" PRId16 "\n", header.special_modifier);
    printf("--- SECTION SKIP DIAGNOSTICS ---\n");
    printf("header_start: 0x0 skip=%u header_end=0x%X\n", FLVER2_HEADER_SIZE,
           FLVER2_HEADER_SIZE);

    if (add_section_skip(flver_file->size, "dummies", cursor, header.dummy_count,
                         DUMMY_FIXED_SIZE, &cursor) != 0 ||
        add_section_skip(flver_file->size, "materials", cursor, header.material_count,
                         MATERIAL_FIXED_SIZE, &cursor) != 0 ||
        add_section_skip(flver_file->size, "nodes", cursor, header.bone_count, NODE_FIXED_SIZE,
                         &cursor) != 0 ||
        add_section_skip(flver_file->size, "meshes", cursor, header.mesh_count, MESH_FIXED_SIZE,
                         &cursor) != 0 ||
        add_section_skip(flver_file->size, "face_sets", cursor, header.face_set_count,
                         FACESET_FIXED_SIZE, &cursor) != 0 ||
        add_section_skip(flver_file->size, "vertex_buffers", cursor,
                         header.vertex_buffer_count, VERTEX_BUFFER_FIXED_SIZE, &cursor) != 0) {
        goto cleanup;
    }
    printf("buffer_layout_headers_start: 0x%zX skip=%zu buffer_layout_headers_end=0x%zX\n",
           cursor, (size_t)header.buffer_layout_count * BUFFER_LAYOUT_HEADER_SIZE,
           cursor + (size_t)header.buffer_layout_count * BUFFER_LAYOUT_HEADER_SIZE);

    if (report_layouts(flver_file->data, flver_file->size, &header, cursor) != 0) goto cleanup;
    exit_code = 0;

cleanup:
    sf_bnd4_destroy(chrbnd);
    sf_free(NULL, extracted.bytes);
    return exit_code;
}
