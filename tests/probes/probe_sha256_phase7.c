/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Phase 7 sha256 snapshot probe. Uses per-archive RSA keys (same as probe_tae_format.c). */

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#ifndef SF_E2E_ELDEN_RING_DIR
#define SF_E2E_ELDEN_RING_DIR L"C:/Games/ELDEN RING"
#endif
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

static const char k_data0_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEA9Rju2whruXDVQZpfylVEPeNxm7XgMHcDyaaRUIpXQE0qEo+6Y36L\n"
    "P0xpFvL0H0kKxHwpuISsdgrnMHJ/yj4S61MWzhO8y4BQbw/zJehhDSRCecFJmFBz\n"
    "3I2JC5FCjoK+82xd9xM5XXdfsdBzRiSghuIHL4qk2WZ/0f/nK5VygeWXn/oLeYBL\n"
    "jX1S8wSSASza64JXjt0bP/i6mpV2SLZqKRxo7x2bIQrR1yHNekSF2jBhZIgcbtMB\n"
    "xjCywn+7p954wjcfjxB5VWaZ4hGbKhi1bhYPccht4XnGhcUTWO3NmJWslwccjQ4k\n"
    "sutLq3uRjLMM0IeTkQO6Pv8/R7UNFtdCWwIERzH8IQ==\n"
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

static uint64_t path_hash_alt(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = (*p == '\\') ? '/' : *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static int sha256_hex(const uint8_t *data, size_t size, char out_hex[65])
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash_h = NULL;
    uint8_t digest[32];
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(st)) return -1;
    st = BCryptCreateHash(alg, &hash_h, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(st)) { BCryptCloseAlgorithmProvider(alg, 0); return -1; }
    BCryptHashData(hash_h, (PUCHAR)(uintptr_t)data, (ULONG)size, 0);
    st = BCryptFinishHash(hash_h, digest, 32, 0);
    BCryptDestroyHash(hash_h);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!BCRYPT_SUCCESS(st)) return -1;
    for (int i = 0; i < 32; i++) sprintf(out_hex + i * 2, "%02x", digest[i]);
    out_hex[64] = '\0';
    return 0;
}

static sf_result_t read_wfile(const wchar_t *path, uint8_t **out, size_t *out_size)
{
    sf_istream_t *s = NULL;
    sf_result_t r = sf_istream_open_wfile(&s, path, NULL);
    if (r != SF_OK) return r;
    int64_t len = sf_istream_length(s);
    if (len <= 0) { sf_istream_close(s); return SF_ERR_TRUNCATED; }
    uint8_t *buf = sf_default_allocator()->alloc((size_t)len, sf_default_allocator()->user);
    if (!buf) { sf_istream_close(s); return SF_ERR_OOM; }
    r = sf_istream_read(s, buf, (size_t)len);
    sf_istream_close(s);
    if (r != SF_OK) { sf_free(NULL, buf); return r; }
    *out = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static sf_result_t rsa_unwrap(const uint8_t *in, size_t in_size, const char *pem,
                               uint8_t **out, size_t *out_size)
{
    if (in_size == 0u || (in_size % 256u) != 0u) return SF_ERR_CRYPTO;
    uint8_t *plain = sf_default_allocator()->alloc(in_size, sf_default_allocator()->user);
    if (!plain) return SF_ERR_OOM;
    size_t used = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t *chunk = NULL; size_t chunk_size = 0;
        sf_result_t r = sfi_rsa_decrypt_pkcs1(pem, in + off, 256u, &chunk, &chunk_size, NULL);
        if (r != SF_OK) { sf_free(NULL, plain); return r; }
        if (chunk_size > 255u) { sf_free(NULL, chunk); sf_free(NULL, plain); return SF_ERR_OUT_OF_RANGE; }
        memset(plain + used, 0, 255u - chunk_size);
        memcpy(plain + used + (255u - chunk_size), chunk, chunk_size);
        used += 255u;
        sf_free(NULL, chunk);
    }
    *out = plain;
    *out_size = used;
    return SF_OK;
}

typedef struct { int64_t file_offset; int64_t padded_size; } bhd_entry_t;

static bool find_entry(const uint8_t *bhd, size_t bhd_size, const char *path,
                       bhd_entry_t *out)
{
    if (bhd_size < 28u || memcmp(bhd, "BHD5", 4u) != 0) return false;
    uint32_t bucket_count = *(const uint32_t *)(bhd + 16u);
    int64_t bucket_offset = *(const int64_t *)(bhd + 20u);
    if ((size_t)bucket_offset + (size_t)bucket_count * 8u > bhd_size) return false;

    uint64_t hash1 = sf_path_hash_64(path);
    uint64_t hash2 = path_hash_alt(path);

    for (uint32_t b = 0; b < bucket_count; b++) {
        int64_t boff = *(const int64_t *)(bhd + (size_t)bucket_offset + b * 8u);
        if (boff <= 0 || (size_t)boff + 16u > bhd_size) continue;
        uint32_t file_count = *(const uint32_t *)(bhd + (size_t)boff);
        int64_t files_offset = *(const int64_t *)(bhd + (size_t)boff + 8u);
        if (files_offset <= 0) continue;
        for (uint32_t f = 0; f < file_count; f++) {
            size_t fpos = (size_t)files_offset + f * 48u;
            if (fpos + 48u > bhd_size) continue;
            uint64_t fhash = *(const uint64_t *)(bhd + fpos);
            if (fhash != hash1 && fhash != hash2) continue;
            out->file_offset = *(const int64_t *)(bhd + fpos + 8u);
            out->padded_size = *(const int64_t *)(bhd + fpos + 16u);
            return true;
        }
    }
    return false;
}

static void probe_one(const wchar_t *bhd_path, const wchar_t *bdt_path,
                      const char *pem, const char *target_path, const char *label)
{
    uint8_t *bhd_raw = NULL; size_t bhd_raw_size = 0;
    sf_result_t r = read_wfile(bhd_path, &bhd_raw, &bhd_raw_size);
    if (r != SF_OK) { printf("%s: read bhd failed %d\n", label, r); return; }

    uint8_t *bhd = NULL; size_t bhd_size = 0;
    if (memcmp(bhd_raw, "BHD5", 4u) == 0) {
        bhd = bhd_raw; bhd_size = bhd_raw_size; bhd_raw = NULL;
    } else {
        r = rsa_unwrap(bhd_raw, bhd_raw_size, pem, &bhd, &bhd_size);
        sf_free(NULL, bhd_raw);
        if (r != SF_OK) { printf("%s: rsa_unwrap failed %d\n", label, r); return; }
    }

    bhd_entry_t entry;
    if (!find_entry(bhd, bhd_size, target_path, &entry)) {
        sf_free(NULL, bhd);
        printf("%s: entry not found\n", label);
        return;
    }
    sf_free(NULL, bhd);

    sf_istream_t *bdt = NULL;
    r = sf_istream_open_wfile(&bdt, bdt_path, NULL);
    if (r != SF_OK) { printf("%s: open bdt failed %d\n", label, r); return; }

    uint8_t *raw = sf_default_allocator()->alloc((size_t)entry.padded_size,
                                                  sf_default_allocator()->user);
    if (!raw) { sf_istream_close(bdt); printf("%s: OOM\n", label); return; }
    r = sf_istream_seek(bdt, entry.file_offset);
    if (r == SF_OK) r = sf_istream_read(bdt, raw, (size_t)entry.padded_size);
    sf_istream_close(bdt);
    if (r != SF_OK) { sf_free(NULL, raw); printf("%s: read bdt failed %d\n", label, r); return; }

    void *dec = NULL; size_t dec_sz = 0;
    sf_dcx_type_t tp;
    r = sf_dcx_decompress(raw, (size_t)entry.padded_size, &dec, &dec_sz, &tp, NULL);
    sf_free(NULL, raw);
    if (r != SF_OK) { printf("%s: decompress failed %d\n", label, r); return; }

    char hex[65];
    if (sha256_hex((const uint8_t *)dec, dec_sz, hex) == 0) {
        printf("%s sha256: %s\n", label, hex);
        printf("%s size: %zu\n", label, dec_sz);
    } else {
        printf("%s: sha256 failed\n", label);
    }
    sf_free(NULL, dec);
}

int main(void)
{
    (void)sf_oodle_set_search_path(SF_E2E_OODLE_DIR);
    probe_one(SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bhd",
              SF_E2E_ELDEN_RING_DIR L"/Game/Data3.bdt",
              k_data3_pem, "/chr/c0000.anibnd.dcx", "c0000.anibnd.dcx");
    probe_one(SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd",
              SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bdt",
              k_data0_pem, "/sfx/sfxbnd_commoneffects.ffxbnd.dcx",
              "sfxbnd_commoneffects.ffxbnd.dcx");
    return 0;
}
