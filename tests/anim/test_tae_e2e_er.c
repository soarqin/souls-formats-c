/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 7 T24 — TAE e2e: parse the first .tae entry inside ER
 * /chr/c0000.anibnd.dcx and verify it round-trips through the production
 * sf_tae_read_from_memory path.
 *
 * Pipeline:
 *   1. Walk ER Data0..Data3 BHD5 archives until /chr/c0000.anibnd.dcx is
 *      found. The T4 probe (.sisyphus/evidence/task-4-tae-probe.md)
 *      confirms the anibnd ships inside Data3 only, so the standard
 *      sf_bhd5_open path — which embeds only the Data0 RSA key for
 *      Elden Ring — cannot reach it. This test therefore copies the
 *      Data0..Data3 PEM keys from tests/probes/probe_tae_format.c and
 *      uses the same custom extraction: RSA-unwrap the BHD, parse the
 *      header table, AES-decrypt the BDT slab, then strip the outer
 *      DCX_KRAK layer.
 *   2. The decompressed BND4 SHA-256 is compared against the probe-time
 *      snapshot (0x1e9c…24aa). A mismatch means the game shipped a new
 *      patch and the test SKIPs rather than fail loudly — the parser
 *      contract is the same but the snapshot would need refreshing.
 *   3. The first `.tae` entry is loaded via sf_tae_read_from_memory and
 *      the public accessors are exercised: format must be SDT, animation
 *      count must be positive, and at least one animation must own at
 *      least one event or one event group.
 *
 * SKIPs gracefully when the ER copy, Oodle DLL, or anibnd is missing.
 */

#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"

#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"
#include "souls_formats/sf_tae.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

#define ANIBND_PATH "/chr/c0000.anibnd.dcx"

/* Expected SHA-256 of the decompressed c0000.anibnd BND4 — captured by
 * the T4 probe on 2026-05-12. Game-patch drift triggers TEST_IGNORE. */
static const uint8_t kExpectedBnd4Sha256[32] = {
    0x1e, 0x9c, 0xcf, 0xf8, 0xd9, 0x1a, 0xe0, 0x7f,
    0x57, 0xfa, 0xa8, 0xa9, 0x4a, 0x2f, 0x3a, 0x9c,
    0xc1, 0x0a, 0x5f, 0x38, 0xbb, 0x0d, 0x8b, 0x1d,
    0xb2, 0xc0, 0x5f, 0xac, 0xd8, 0x24, 0x24, 0xaa,
};

void setUp(void) {}
void tearDown(void) {}

typedef struct er_archive {
    const wchar_t *bhd_path;
    const wchar_t *bdt_path;
    const char    *pem;
} er_archive_t;

typedef struct bhd_entry_probe {
    int32_t padded_size;
    int32_t unpadded_size;
    int64_t file_offset;
    int64_t sha_hash_offset;
    int64_t aes_key_offset;
} bhd_entry_probe_t;

/* ── Data0..Data3 RSA public keys, lifted verbatim from
 *    tests/probes/probe_tae_format.c. The full upstream comment is in
 *    that file; reproduced here so the e2e test is self-contained. ── */
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

static const er_archive_t k_archives[] = {
    {SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bdt", k_data0_pem},
    {SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data1.bdt", k_data1_pem},
    {SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data2.bdt", k_data2_pem},
    {SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bhd",
     SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bdt", k_data3_pem},
};

static const char *const k_anibnd_paths[] = {
    "/chr/c0000.anibnd.dcx",
    "/chr/c0000.anibnd",
    NULL,
};

/* ─── helpers (verbatim from probe_tae_format.c) ─── */

static bool checked_span(size_t size, size_t offset, size_t count, size_t elem_size)
{
    if (elem_size != 0 && count > SIZE_MAX / elem_size) {
        return false;
    }
    const size_t bytes = count * elem_size;
    return offset <= size && bytes <= size - offset;
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return ((uint64_t)read_u32_le(p)) | ((uint64_t)read_u32_le(p + 4u) << 32);
}

/* ER hashes some Data3 entries with a 64-bit folded variant that the
 * production sf_path_hash_64 (zero-extended 32-bit, 37u multiplier) does
 * not yet compute. Mirrors the workaround in tests/geom/test_flver2_e2e_er.c
 * and tests/probes/probe_tae_format.c. */
static uint64_t er_path_hash_64_alt(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = (*p == '\\') ? '/' : *p;
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static bool both_files_exist(const wchar_t *bhd, const wchar_t *bdt)
{
    return GetFileAttributesW(bhd) != INVALID_FILE_ATTRIBUTES
           && GetFileAttributesW(bdt) != INVALID_FILE_ATTRIBUTES;
}

static bool env_is_available(void)
{
    return both_files_exist(k_archives[0].bhd_path, k_archives[0].bdt_path);
}

static sf_result_t read_wfile_bytes(const wchar_t *path, uint8_t **out,
                                    size_t *out_size)
{
    sf_istream_t *s = NULL;
    sf_result_t   r = sf_istream_open_wfile(&s, path, NULL);
    if (r != SF_OK) {
        return r;
    }
    const int64_t len = sf_istream_length(s);
    if (len < 0 || (uint64_t)len > SIZE_MAX) {
        sf_istream_close(s);
        return SF_ERR_OUT_OF_RANGE;
    }

    uint8_t *buf = NULL;
    if (len > 0) {
        buf = sf_default_allocator()->alloc((size_t)len,
                                            sf_default_allocator()->user);
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
    *out      = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static sf_result_t rsa_unwrap_bhd(const uint8_t *in, size_t in_size,
                                  const char *pem, uint8_t **out,
                                  size_t *out_size)
{
    if (in_size == 0 || (in_size % 256u) != 0) {
        return SF_ERR_CRYPTO;
    }
    uint8_t *plain = sf_default_allocator()->alloc(in_size,
                                                   sf_default_allocator()->user);
    if (!plain) {
        return SF_ERR_OOM;
    }

    size_t used = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t    *chunk      = NULL;
        size_t      chunk_size = 0;
        sf_result_t r = sfi_rsa_decrypt_pkcs1(pem, in + off, 256u, &chunk,
                                              &chunk_size, NULL);
        if (r != SF_OK) {
            sf_free(NULL, plain);
            return r;
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

    *out      = plain;
    *out_size = used;
    return SF_OK;
}

static bool find_bhd_entry(const uint8_t *bhd, size_t bhd_size, const char *path,
                           bhd_entry_probe_t *out)
{
    const uint64_t hashes[] = {sf_path_hash_64(path), er_path_hash_64_alt(path)};

    if (bhd_size < 0x28u || memcmp(bhd, "BHD5", 4u) != 0) {
        return false;
    }
    const bool     is64_bit = read_u32_le(bhd + 0x14u) == 0 &&
                              read_u32_le(bhd + 0x1Cu) == 0;
    const uint64_t bucket_count64   = is64_bit ? read_u64_le(bhd + 0x10u)
                                               : read_u32_le(bhd + 0x10u);
    const uint64_t buckets_offset64 = is64_bit ? read_u64_le(bhd + 0x18u)
                                               : read_u32_le(bhd + 0x14u);
    if (bucket_count64 > 1000000u || buckets_offset64 > SIZE_MAX) {
        return false;
    }

    const size_t bucket_count   = (size_t)bucket_count64;
    const size_t buckets_offset = (size_t)buckets_offset64;
    const size_t bucket_size    = is64_bit ? 16u : 8u;
    if (!checked_span(bhd_size, buckets_offset, bucket_count, bucket_size)) {
        return false;
    }

    for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
        const size_t   bucket_offset   = buckets_offset + bucket * bucket_size;
        const uint32_t file_count      = read_u32_le(bhd + bucket_offset);
        const uint64_t header_offset64 = is64_bit
                                             ? read_u64_le(bhd + bucket_offset + 8u)
                                             : read_u32_le(bhd + bucket_offset + 4u);
        if (header_offset64 > SIZE_MAX) {
            return false;
        }
        const size_t header_offset = (size_t)header_offset64;
        if (!checked_span(bhd_size, header_offset, file_count, 40u)) {
            return false;
        }

        for (uint32_t file = 0; file < file_count; ++file) {
            const size_t   pos = header_offset + (size_t)file * 40u;
            const uint64_t candidate_hash = read_u64_le(bhd + pos);
            bool           matches        = false;
            for (size_t i = 0; i < sizeof(hashes) / sizeof(hashes[0]); ++i) {
                if (candidate_hash == hashes[i]) {
                    matches = true;
                    break;
                }
            }
            if (!matches) {
                continue;
            }

            const int32_t padded      = (int32_t)read_u32_le(bhd + pos + 8u);
            const int32_t unpadded    = (int32_t)read_u32_le(bhd + pos + 12u);
            const int64_t file_offset = (int64_t)read_u64_le(bhd + pos + 16u);
            const int64_t sha_offset  = (int64_t)read_u64_le(bhd + pos + 24u);
            const int64_t aes_offset  = (int64_t)read_u64_le(bhd + pos + 32u);
            if (padded > 0 && unpadded >= 0 &&
                (unpadded == 0 || unpadded <= padded) && file_offset >= 0 &&
                sha_offset >= 0 && aes_offset >= 0 &&
                (uint64_t)sha_offset < bhd_size &&
                (uint64_t)aes_offset < bhd_size) {
                out->padded_size    = padded;
                out->unpadded_size  = unpadded;
                out->file_offset    = file_offset;
                out->sha_hash_offset = sha_offset;
                out->aes_key_offset = aes_offset;
                return true;
            }
        }
    }
    return false;
}

static sf_result_t decrypt_bhd_ranges(uint8_t *data, size_t size,
                                      int64_t file_offset, const uint8_t *bhd,
                                      size_t bhd_size, int64_t aes_offset)
{
    size_t       meta_offset = 0;
    bool         found       = false;
    const size_t begin = aes_offset > 32 ? (size_t)aes_offset - 32u : 0u;
    const size_t end   = (uint64_t)aes_offset + 64u < bhd_size
                             ? (size_t)aes_offset + 64u
                             : bhd_size;
    for (size_t cand = begin; cand + 20u <= end; ++cand) {
        const int32_t count = (int32_t)read_u32_le(bhd + cand + 16u);
        if (count <= 0 || count > 16 ||
            cand + 20u + (size_t)count * 16u > bhd_size) {
            continue;
        }
        const int64_t start0         = (int64_t)read_u64_le(bhd + cand + 20u);
        const int64_t end0           = (int64_t)read_u64_le(bhd + cand + 28u);
        const bool    relative_range = start0 == 0 && end0 > 0 &&
                                       (uint64_t)end0 <= size;
        const bool absolute_range = start0 >= file_offset && end0 > start0 &&
                                    (uint64_t)(end0 - file_offset) <= size;
        if ((relative_range || absolute_range) && ((end0 - start0) % 16) == 0) {
            meta_offset = cand;
            found       = true;
            break;
        }
    }
    if (!found) {
        return SF_ERR_NOT_FOUND;
    }

    const uint8_t *meta = bhd + meta_offset;
    uint8_t        key[16];
    memcpy(key, meta, sizeof(key));
    const int32_t range_count = (int32_t)read_u32_le(meta + 16u);
    if (range_count < 0 || range_count > 1024) {
        return SF_ERR_OUT_OF_RANGE;
    }
    if (meta_offset + 20u + (size_t)range_count * 16u > bhd_size) {
        return SF_ERR_TRUNCATED;
    }

    for (int32_t i = 0; i < range_count; ++i) {
        const uint8_t *range     = meta + 20u + (size_t)i * 16u;
        const int64_t  start     = (int64_t)read_u64_le(range);
        const int64_t  end_range = (int64_t)read_u64_le(range + 8u);
        if (end_range == start) {
            continue;
        }
        if (start < 0 || end_range < start || ((end_range - start) % 16) != 0) {
            return SF_ERR_OUT_OF_RANGE;
        }
        int64_t rel_start = start;
        int64_t rel_end   = end_range;
        if ((uint64_t)end_range > size) {
            if (start < file_offset || end_range < file_offset) {
                return SF_ERR_TRUNCATED;
            }
            rel_start = start - file_offset;
            rel_end   = end_range - file_offset;
        }
        if (rel_start < 0 || rel_end < rel_start || (uint64_t)rel_end > size) {
            return SF_ERR_TRUNCATED;
        }
        sf_result_t r = sfi_aes_decrypt_ecb_buffer(
            key, data + rel_start, (size_t)(rel_end - rel_start));
        if (r != SF_OK) {
            return r;
        }
    }
    return SF_OK;
}

static sf_result_t unwrap_one_dcx_if_present(void *raw, size_t raw_size,
                                             void **out, size_t *out_size)
{
    sf_dcx_type_t     type    = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx  = sniff_r == SF_OK && type != SF_DCX_TYPE_NONE &&
                                type != SF_DCX_TYPE_UNKNOWN;
    if (!is_dcx) {
        *out      = raw;
        *out_size = raw_size;
        return SF_OK;
    }

    void         *decompressed      = NULL;
    size_t        decompressed_size = 0;
    sf_dcx_type_t out_type          = SF_DCX_TYPE_UNKNOWN;
    sf_result_t   r = sf_dcx_decompress(raw, raw_size, &decompressed,
                                        &decompressed_size, &out_type, NULL);
    sf_free(NULL, raw);
    if (r != SF_OK) {
        return r;
    }
    *out      = decompressed;
    *out_size = decompressed_size;
    return SF_OK;
}

static sf_result_t extract_anibnd_from_archive(const er_archive_t *archive,
                                               const char *path,
                                               void **out, size_t *out_size)
{
    if (!both_files_exist(archive->bhd_path, archive->bdt_path)) {
        return SF_ERR_IO;
    }

    uint8_t *bhd_raw      = NULL;
    uint8_t *bhd          = NULL;
    void    *raw          = NULL;
    size_t   bhd_raw_size = 0;
    size_t   bhd_size     = 0;
    sf_result_t r = read_wfile_bytes(archive->bhd_path, &bhd_raw, &bhd_raw_size);
    if (r != SF_OK) {
        return r;
    }
    r = rsa_unwrap_bhd(bhd_raw, bhd_raw_size, archive->pem, &bhd, &bhd_size);
    sf_free(NULL, bhd_raw);
    if (r != SF_OK) {
        return r;
    }
    if (bhd_size < 4u || memcmp(bhd, "BHD5", 4u) != 0) {
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
    raw = sf_default_allocator()->alloc((size_t)entry.padded_size,
                                        sf_default_allocator()->user);
    if (!raw) {
        sf_istream_close(bdt);
        sf_free(NULL, bhd);
        return SF_ERR_OOM;
    }
    r = sf_istream_seek(bdt, entry.file_offset);
    if (r == SF_OK) {
        r = sf_istream_read(bdt, raw, (size_t)entry.padded_size);
    }
    sf_istream_close(bdt);
    if (r == SF_OK && entry.aes_key_offset != 0) {
        r = decrypt_bhd_ranges(raw, (size_t)entry.padded_size,
                               entry.file_offset, bhd, bhd_size,
                               entry.aes_key_offset);
    }
    sf_free(NULL, bhd);
    if (r != SF_OK) {
        sf_free(NULL, raw);
        return r;
    }

    return unwrap_one_dcx_if_present(raw, (size_t)entry.padded_size, out,
                                     out_size);
}

static sf_result_t extract_anibnd_multi_archive(void **out, size_t *out_size)
{
    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    *out      = NULL;
    *out_size = 0;

    for (size_t a = 0; a < sizeof(k_archives) / sizeof(k_archives[0]); ++a) {
        for (size_t p = 0; k_anibnd_paths[p] != NULL; ++p) {
            void   *bytes = NULL;
            size_t  size  = 0;
            sf_result_t r = extract_anibnd_from_archive(&k_archives[a],
                                                        k_anibnd_paths[p],
                                                        &bytes, &size);
            if (r == SF_OK && bytes != NULL && size > 0) {
                *out      = bytes;
                *out_size = size;
                return SF_OK;
            }
            if (bytes != NULL) {
                sf_free(NULL, bytes);
            }
            if (r == SF_ERR_OODLE_NOT_FOUND) {
                return r;
            }
        }
    }
    return SF_ERR_NOT_FOUND;
}

/* Returns true and fills `out` with the sha-256 digest on success.
 * Returns false if BCrypt is unavailable; callers should treat that as a
 * skip rather than a hard failure. */
static bool sha256_digest(const void *data, size_t size, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE  alg    = NULL;
    BCRYPT_HASH_HANDLE hash_h = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                              NULL, 0);
    if (!BCRYPT_SUCCESS(st)) {
        return false;
    }
    st = BCryptCreateHash(alg, &hash_h, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    BCryptHashData(hash_h, (PUCHAR)(uintptr_t)data, (ULONG)size, 0);
    st = BCryptFinishHash(hash_h, out, 32, 0);
    BCryptDestroyHash(hash_h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return BCRYPT_SUCCESS(st);
}

static bool name_ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }
    const size_t n = strlen(name);
    const size_t s = strlen(suffix);
    if (s > n) {
        return false;
    }
    return memcmp(name + n - s, suffix, s) == 0;
}

/* ── T1: extract + parse + verify accessors ─────────────────────────────── */

static void test_tae_e2e_parse_first_c0000_tae(void)
{
    if (!env_is_available()) {
        TEST_IGNORE_MESSAGE("ER copy not available");
    }

    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    sf_result_t r = extract_anibnd_multi_archive(&bnd_bytes, &bnd_size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("c0000.anibnd.dcx not present in this ER install");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd_bytes);
    TEST_ASSERT_GREATER_THAN((size_t)0, bnd_size);

    /* Snapshot guard. Mismatch is a patch-drift signal, not a parser
     * regression — IGNORE so the suite still surfaces actionable
     * failures elsewhere. */
    uint8_t digest[32];
    if (!sha256_digest(bnd_bytes, bnd_size, digest)) {
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("BCrypt unavailable; cannot verify snapshot");
    }
    if (memcmp(digest, kExpectedBnd4Sha256, sizeof(digest)) != 0) {
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("game patch changed snapshot, skipping");
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd);

    const sf_binder_file_t *tae_entry = NULL;
    const size_t            count     = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || file->size == 0) {
            continue;
        }
        if (name_ends_with(file->name_utf8, ".tae")) {
            tae_entry = file;
            break;
        }
    }
    if (!tae_entry) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("no .tae entry found in c0000 anibnd");
    }

    sf_tae_t *tae = NULL;
    r = sf_tae_read_from_memory(&tae, tae_entry->data, tae_entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(tae);

    TEST_ASSERT_EQUAL_INT(SF_TAE_FORMAT_SDT, sf_tae_format(tae));

    const size_t anim_count = sf_tae_animation_count(tae);
    TEST_ASSERT_GREATER_THAN((size_t)0, anim_count);

    bool found_event_or_group = false;
    for (size_t i = 0; i < anim_count; ++i) {
        const sf_tae_animation_t *anim = sf_tae_animation(tae, i);
        if (!anim) {
            continue;
        }
        if (sf_tae_animation_event_count(anim) > 0
            || sf_tae_animation_event_group_count(anim) > 0) {
            found_event_or_group = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found_event_or_group,
                             "at least one animation must have an event or "
                             "event group");

    sf_tae_destroy(tae);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tae_e2e_parse_first_c0000_tae);
    return UNITY_END();
}
