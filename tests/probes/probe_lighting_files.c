/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Probe: scan v1 game archives for lighting-related file formats.
 *
 * Searches Data0 (and non-Data0 shards via inline PEM keys) of every v1 game
 * for .btab, .btl, .btpb, .gparam, .fltparam, and .pmdcl files.  For each
 * hit, sniffs the first 16 bytes to identify the format and version.
 *
 * Output: .sisyphus/evidence/lighting-probe.md
 *         .sisyphus/evidence/lighting-probe-scope.md
 *
 * Usage: probe_lighting_files.exe
 */

#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"

#include "ac6_test_helper.h"
#include "er_test_helper.h"
#include "nightreign_test_helper.h"
#include "sekiro_test_helper.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Evidence output paths ─────────────────────────────────────────────── */
#define EVIDENCE_PATH       ".sisyphus/evidence/lighting-probe.md"
#define SCOPE_PATH          ".sisyphus/evidence/lighting-probe-scope.md"

/* ── Oodle search paths ─────────────────────────────────────────────────── */
#ifndef SF_E2E_OODLE_DIR
#define SF_E2E_OODLE_DIR L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#endif

/* ── Inline PEM keys for non-Data0 shards ──────────────────────────────── */
/* ER Data1 — from Nordgaren/UXM-Selective-Unpack ArchiveKeys.cs */
static const char k_er_data1_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAxaBCHQJrtLJiJNdG9nq3deA9sY4YCZ4dbTOHO+v+YgWRMcE6iK6o\n"
    "ZIJq+nBMUNBbGPmbRrEjkkH9M7LAypAFOPKC6wMHzqIMBsUMuYffulBuOqtEBD11\n"
    "CAwfx37rjwJ+/1tnEqtJjYkrK9yyrIN6Y+jy4ftymQtjk83+L89pvMMmkNeZaPON\n"
    "4O9q5M9PnFoKvK8eY45ZV/Jyk+Pe+xc6+e4h4cx8ML5U2kMM3VDAJush4z/05hS3\n"
    "/bC4B6K9+7dPwgqZgKx1J7DBtLdHSAgwRPpijPeOjKcAa2BDaNp9Cfon70oC+ZCB\n"
    "+HkQ7FjJcF7KaHsH5oHvuI7EZAl2XTsLEQIENa/2JQ==\n"
    "-----END RSA PUBLIC KEY-----\n";

/* ER Data2 — from Nordgaren/UXM-Selective-Unpack ArchiveKeys.cs */
static const char k_er_data2_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEA0iDVVQ230RgrkIHJNDgxE7I/2AaH6Li1Eu9mtpfrrfhfoK2e7y4O\n"
    "WU+lj7AGI4GIgkWpPw8JHaV970Cr6+sTG4Tr5eMQPxrCIH7BJAPCloypxcs2BNfT\n"
    "GXzm6veUfrGzLIDp7wy24lIA8r9ZwUvpKlN28kxBDGeCbGCkYeSVNuF+R9rN4OAM\n"
    "RYh0r1Q950xc2qSNloNsjpDoSKoYN0T7u5rnMn/4mtclnWPVRWU940zr1rymv4Jc\n"
    "3umNf6cT1XqrS1gSaK1JWZfsSeD6Dwk3uvquvfY6YlGRygIlVEMAvKrDRMHylsLt\n"
    "qqhYkZNXMdy0NXopf1rEHKy9poaHEmJldwIFAP////8=\n"
    "-----END RSA PUBLIC KEY-----\n";

/* ER Data3 — from Nordgaren/UXM-Selective-Unpack ArchiveKeys.cs */
static const char k_er_data3_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBCwKCAQEAvRRNBnVq3WknCNHrJRelcEA2v/OzKlQkxZw1yKll0Y2Kn6G9ts94\n"
    "SfgZYbdFCnIXy5NEuyHRKrxXz5vurjhrcuoYAI2ZUhXPXZJdgHywac/i3S/IY0V/\n"
    "eDbqepyJWHpP6I565ySqlol1p/BScVjbEsVyvZGtWIXLPDbx4EYFKA5B52uK6Gdz\n"
    "4qcyVFtVEhNoMvg+EoWnyLD7EUzuB2Khl46CuNictyWrLlIHgpKJr1QD8a0ld0PD\n"
    "PHDZn03q6QDvZd23UW2d9J+/HeBt52j08+qoBXPwhndZsmPMWngQDaik6FM7EVRQ\n"
    "etKPi6h5uprVmMAS5wR/jQIVTMpTj/zJdwIEXszeQw==\n"
    "-----END RSA PUBLIC KEY-----\n";

/* NR data2 — from probe_nightreign_msb.c (verified working) */
static const char k_nr_data2_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAqpkf9yHnx8k84+WXITLFUW/STypXjZMPuw842pzNHa5L7v9gU4M5\n"
    "hBHwTQs0YIcfnf+mbjqoJYnmYPBblxLjFXgwT4ICJdpnPMY75BwD0Nv28/CvvIsA\n"
    "0QQWOhUeOXnm5BT26dGYi3CHHPvD14F76tJt3TO/CC3fyhdxne9Cra5G87aGTJGv\n"
    "0ImsU0KPCizYX/RHQ2jdJdlB5BHzkMgLhIaEdhC3nhIqMJDNQNGKMo7rRV1tAEGf\n"
    "0zIZ23PGEsPsbVg31nnnRoq338WfD9ArZZG6bM11vlfVcYmrJs7v4vBjKXnYVwVX\n"
    "0rQGIfSNDnaZcEj4tsl04AqnupTdvSrHXwIFANOg6RU=\n"
    "-----END RSA PUBLIC KEY-----\n";

/* ── Format identification ──────────────────────────────────────────────── */
typedef enum {
    FMT_UNKNOWN = 0,
    FMT_BTAB,
    FMT_BTL,
    FMT_BTPB,
    FMT_GPARAM,
    FMT_PMDCL,
} probe_fmt_t;

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return ((uint64_t)read_u32_le(p)) | ((uint64_t)read_u32_le(p + 4) << 32);
}

/* Identify format from first 16 bytes of decompressed payload. */
static probe_fmt_t sniff_format(const uint8_t *hdr, size_t size, int32_t *out_version)
{
    if (!hdr || size < 8) return FMT_UNKNOWN;
    *out_version = -1;

    /* GPARAM: bytes 0-7 = UTF-16LE "filt" = 66 00 69 00 6C 00 74 00 */
    static const uint8_t k_gparam_magic[8] = {0x66, 0x00, 0x69, 0x00, 0x6C, 0x00, 0x74, 0x00};
    if (memcmp(hdr, k_gparam_magic, 8) == 0) {
        if (size >= 12) *out_version = (int32_t)read_u32_le(hdr + 8);
        return FMT_GPARAM;
    }

    /* BTL: bytes 0-3 = 0x02000000, bytes 4-7 = version ∈ {1,2,5,6,16,18} */
    if (size >= 8) {
        uint32_t w0 = read_u32_le(hdr);
        uint32_t w1 = read_u32_le(hdr + 4);
        if (w0 == 2u) {
            if (w1 == 1u || w1 == 2u || w1 == 5u || w1 == 6u || w1 == 16u || w1 == 18u) {
                *out_version = (int32_t)w1;
                return FMT_BTL;
            }
        }
    }

    /* BTAB: bytes 0-3 = 0x01000000, bytes 4-7 = 0x00000000 */
    if (size >= 8) {
        uint32_t w0 = read_u32_le(hdr);
        uint32_t w1 = read_u32_le(hdr + 4);
        if (w0 == 1u && w1 == 0u) {
            return FMT_BTAB;
        }
    }

    /* BTPB: bytes 0-3 = version ∈ {2,3}, bytes 4-7 = ∈ {0,1} */
    if (size >= 8) {
        uint32_t w0 = read_u32_le(hdr);
        uint32_t w1 = read_u32_le(hdr + 4);
        if ((w0 == 2u || w0 == 3u) && (w1 == 0u || w1 == 1u)) {
            *out_version = (int32_t)w0;
            return FMT_BTPB;
        }
    }

    /* PMDCL: bytes 0-7 = int64 count (small positive), bytes 8-15 = 0x20 */
    if (size >= 16) {
        uint64_t count = read_u64_le(hdr);
        uint64_t hdr_sz = read_u64_le(hdr + 8);
        if (count > 0 && count < 100000u && hdr_sz == 0x20u) {
            return FMT_PMDCL;
        }
    }

    return FMT_UNKNOWN;
}

static const char *fmt_name(probe_fmt_t f)
{
    switch (f) {
    case FMT_BTAB:    return "BTAB";
    case FMT_BTL:     return "BTL";
    case FMT_BTPB:    return "BTPB";
    case FMT_GPARAM:  return "GPARAM";
    case FMT_PMDCL:   return "PMDCL";
    default:          return "UNKNOWN";
    }
}

/* ── Report buffer ──────────────────────────────────────────────────────── */
typedef struct report_buf {
    char  *data;
    size_t len;
    size_t cap;
} report_buf_t;

static sf_result_t rbuf_appendf(report_buf_t *b, const char *fmt, ...)
{
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return SF_ERR_INTERNAL;

    const char *src = tmp;
    char       *large = NULL;
    if ((size_t)n >= sizeof(tmp)) {
        large = sf_default_allocator()->alloc((size_t)n + 1u, sf_default_allocator()->user);
        if (!large) return SF_ERR_OOM;
        va_start(ap, fmt);
        vsnprintf(large, (size_t)n + 1u, fmt, ap);
        va_end(ap);
        src = large;
    }

    size_t add = (size_t)n;
    if (b->len + add + 1u > b->cap) {
        size_t new_cap = b->cap ? b->cap * 2u : 4096u;
        while (new_cap < b->len + add + 1u) new_cap *= 2u;
        char *nd = sf_default_allocator()->alloc(new_cap, sf_default_allocator()->user);
        if (!nd) { sf_free(NULL, large); return SF_ERR_OOM; }
        if (b->data) { memcpy(nd, b->data, b->len); sf_free(NULL, b->data); }
        b->data = nd;
        b->cap  = new_cap;
    }
    memcpy(b->data + b->len, src, add);
    b->len += add;
    b->data[b->len] = '\0';
    sf_free(NULL, large);
    return SF_OK;
}

static void rbuf_free(report_buf_t *b)
{
    sf_free(NULL, b->data);
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

static sf_result_t rbuf_write_file(const report_buf_t *b, const char *path)
{
    sf_ostream_t *out = NULL;
    sf_result_t r = sf_ostream_open_file(&out, path, NULL);
    if (r != SF_OK) return r;
    r = sf_ostream_write(out, b->data, b->len);
    sf_ostream_close(out);
    return r;
}

/* ── Hit record ─────────────────────────────────────────────────────────── */
typedef struct hit {
    char       game[8];
    char       archive[16];
    char       path[256];
    uint64_t   hash;
    char       dcx_type[16];
    probe_fmt_t fmt;
    int32_t    version;
    size_t     size;
} hit_t;

#define MAX_HITS 4096u
static hit_t   g_hits[MAX_HITS];
static size_t  g_hit_count = 0;

static void record_hit(const char *game, const char *archive, const char *path,
                       uint64_t hash, sf_dcx_type_t dcx_type,
                       probe_fmt_t fmt, int32_t version, size_t size)
{
    if (g_hit_count >= MAX_HITS) return;
    hit_t *h = &g_hits[g_hit_count++];
    snprintf(h->game,    sizeof(h->game),    "%s", game);
    snprintf(h->archive, sizeof(h->archive), "%s", archive);
    snprintf(h->path,    sizeof(h->path),    "%s", path);
    h->hash    = hash;
    h->version = version;
    h->fmt     = fmt;
    h->size    = size;

    switch (dcx_type) {
    case SF_DCX_TYPE_DCX_DFLT: snprintf(h->dcx_type, sizeof(h->dcx_type), "DFLT"); break;
    case SF_DCX_TYPE_DCP_DFLT: snprintf(h->dcx_type, sizeof(h->dcx_type), "DCP_DFLT"); break;
    case SF_DCX_TYPE_DCX_KRAK: snprintf(h->dcx_type, sizeof(h->dcx_type), "KRAK"); break;
    case SF_DCX_TYPE_DCX_ZSTD: snprintf(h->dcx_type, sizeof(h->dcx_type), "ZSTD"); break;
    case SF_DCX_TYPE_NONE:     snprintf(h->dcx_type, sizeof(h->dcx_type), "NONE"); break;
    default:                   snprintf(h->dcx_type, sizeof(h->dcx_type), "?");    break;
    }
}

/*
 * BHD5 path hash — two algorithms exist in ER:
 *   Data0: FromPathHash (multiplier 37, adds leading '/', 32-bit result zero-extended)
 *          Used by sf_bhd5_open / sf_bhd5_extract_by_path.
 *   Data1/2/3: multiplier 133, no leading '/', true 64-bit result.
 *              Used by probe_nightreign_msb.c and confirmed by linear scan of Data2.
 * Upstream reference: HashHelper.cs:FromPathHash (multiplier 37 = Data0 only).
 */
static uint64_t path_hash_133(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; p && *p; ++p) {
        unsigned char c = (*p == '\\') ? '/' : *p;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

/* ── Candidate path list ────────────────────────────────────────────────── */
static const char *k_map_ids[] = {
    /* ER open-world tiles (m60 = Limgrave/Altus/Mountaintops area) */
    "m60_42_36_00", "m60_00_00_00", "m60_41_36_00", "m60_43_35_00",
    "m60_10_10_00", "m60_20_20_00", "m60_30_30_00", "m60_40_40_00",
    "m60_50_50_00", "m60_52_47_00", "m60_53_47_00",
    /* ER legacy dungeons / legacy areas */
    "m10_00_00_00", "m11_00_00_00", "m12_00_00_00", "m13_00_00_00",
    "m14_00_00_00", "m15_00_00_00", "m16_00_00_00", "m17_00_00_00",
    "m18_00_00_00", "m19_00_00_00",
    "m20_00_00_00", "m21_00_00_00", "m22_00_00_00", "m23_00_00_00",
    "m24_00_00_00", "m25_00_00_00",
    "m30_00_00_00", "m34_00_00_00",
    "m40_00_00_00", "m41_00_00_00", "m45_00_00_00",
    /* AC6 map IDs (m01..m30 range) */
    "m01_00_00_00", "m02_00_00_00", "m03_00_00_00", "m04_00_00_00",
    "m05_00_00_00", "m06_00_00_00", "m07_00_00_00", "m08_00_00_00",
    "m09_00_00_00",
    /* NR map IDs */
    "m60_10_09_02", "m21_20_00_00", "m60_20_19_01", "m60_23_17_01",
};
#define MAP_ID_COUNT (sizeof(k_map_ids) / sizeof(k_map_ids[0]))

typedef struct ext_info {
    const char *ext;       /* e.g. ".gparam" */
    const char *ext_nodot; /* e.g. "gparam"  */
} ext_info_t;

static const ext_info_t k_exts[] = {
    {".btab",     "btab"},
    {".btl",      "btl"},
    {".btpb",     "btpb"},
    {".gparam",   "gparam"},
    {".fltparam", "fltparam"},
    {".pmdcl",    "pmdcl"},
};
#define EXT_COUNT (sizeof(k_exts) / sizeof(k_exts[0]))

/* Extract the area prefix from a map ID (first 3 chars, e.g. "m60" from "m60_42_36_00"). */
static void map_area_prefix(const char *map_id, char *out, size_t out_size)
{
    size_t i = 0;
    while (i < out_size - 1u && map_id[i] && map_id[i] != '_') {
        out[i] = map_id[i];
        ++i;
    }
    out[i] = '\0';
}

/* Build all candidate paths for a given map ID into out[].
 * Returns number of paths written. out must have capacity >= 12 * EXT_COUNT. */
static size_t build_candidates(const char *map_id, char out[][256], size_t out_cap)
{
    char area[8];
    map_area_prefix(map_id, area, sizeof(area));

    size_t n = 0;
    for (size_t e = 0; e < EXT_COUNT && n + 12u <= out_cap; ++e) {
        const char *ext    = k_exts[e].ext;
        const char *ext_nd = k_exts[e].ext_nodot;

        /* Pattern 1: /map/mapstudio/<id>/<id>_<ext_nodot>.dcx */
        snprintf(out[n++], 256, "/map/mapstudio/%s/%s_%s.dcx", map_id, map_id, ext_nd);
        /* Pattern 2: /map/mapstudio/<id>/<id><ext>.dcx */
        snprintf(out[n++], 256, "/map/mapstudio/%s/%s%s.dcx", map_id, map_id, ext);
        /* Pattern 3: /map/mapstudio/<id>/<id><ext> (no .dcx) */
        snprintf(out[n++], 256, "/map/mapstudio/%s/%s%s", map_id, map_id, ext);
        /* Pattern 4: /map/mapstudio/<id>/<id>_lighting<ext>.dcx */
        snprintf(out[n++], 256, "/map/mapstudio/%s/%s_lighting%s.dcx", map_id, map_id, ext);
        /* Pattern 5: /map/mapstudio/<id>/<id>_light<ext>.dcx */
        snprintf(out[n++], 256, "/map/mapstudio/%s/%s_light%s.dcx", map_id, map_id, ext);
        /* Pattern 6: /map/mapstudio/<id><ext>.dcx (flat mapstudio, like MSB) */
        snprintf(out[n++], 256, "/map/mapstudio/%s%s.dcx", map_id, ext);
        /* Pattern 7: /map/mapstudio/<id>_<ext_nodot>.dcx */
        snprintf(out[n++], 256, "/map/mapstudio/%s_%s.dcx", map_id, ext_nd);
        /* Pattern 8: /map/<area>/<id>/<id><ext>.dcx (ER area-prefixed layout) */
        snprintf(out[n++], 256, "/map/%s/%s/%s%s.dcx", area, map_id, map_id, ext);
        /* Pattern 9: /map/<area>/<id>/<id>_<ext_nodot>.dcx */
        snprintf(out[n++], 256, "/map/%s/%s/%s_%s.dcx", area, map_id, map_id, ext_nd);
        /* Pattern 10: /map/<area>/<id>/<id><ext> (no .dcx) */
        snprintf(out[n++], 256, "/map/%s/%s/%s%s", area, map_id, map_id, ext);
        /* Pattern 11: /map/<id>/<id><ext>.dcx (flat layout) */
        snprintf(out[n++], 256, "/map/%s/%s%s.dcx", map_id, map_id, ext);
        /* Pattern 12: /map/<id>/<id>_<ext_nodot>.dcx */
        snprintf(out[n++], 256, "/map/%s/%s_%s.dcx", map_id, map_id, ext_nd);
    }
    return n;
}

/* ── Low-level BHD5 extraction (for non-Data0 shards) ──────────────────── */
static sf_result_t read_file_w(const wchar_t *path, uint8_t **out, size_t *out_size)
{
    sf_istream_t *s = NULL;
    sf_result_t r = sf_istream_open_wfile(&s, path, NULL);
    if (r != SF_OK) return r;

    int64_t len = sf_istream_length(s);
    if (len < 0 || (uint64_t)len > SIZE_MAX) {
        sf_istream_close(s);
        return SF_ERR_OUT_OF_RANGE;
    }

    uint8_t *buf = NULL;
    if (len > 0) {
        buf = sf_default_allocator()->alloc((size_t)len, sf_default_allocator()->user);
        if (!buf) { sf_istream_close(s); return SF_ERR_OOM; }
        r = sf_istream_read(s, buf, (size_t)len);
    }
    sf_istream_close(s);
    if (r != SF_OK) { sf_free(NULL, buf); return r; }
    *out      = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static sf_result_t rsa_unwrap(const uint8_t *in, size_t in_size, const char *pem,
                               uint8_t **out, size_t *out_size)
{
    if (in_size == 0 || (in_size % 256u) != 0) return SF_ERR_CRYPTO;
    uint8_t *plain = sf_default_allocator()->alloc(in_size, sf_default_allocator()->user);
    if (!plain) return SF_ERR_OOM;

    size_t used = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t *chunk = NULL;
        size_t   csz   = 0;
        sf_result_t r = sfi_rsa_decrypt_pkcs1(pem, in + off, 256u, &chunk, &csz, NULL);
        if (r != SF_OK) { sf_free(NULL, plain); return r; }
        if (csz > in_size - used) { sf_free(NULL, chunk); sf_free(NULL, plain); return SF_ERR_OUT_OF_RANGE; }
        memcpy(plain + used, chunk, csz);
        used += csz;
        sf_free(NULL, chunk);
    }
    *out      = plain;
    *out_size = used;
    return SF_OK;
}

typedef struct shard_entry {
    int32_t padded_size;
    int64_t file_offset;
    int64_t aes_key_offset;
} shard_entry_t;

/*
 * Parse a decrypted BHD5 header (ER/Sekiro/NR/AC6 64-bit format) and find
 * the entry matching `path`.
 *
 * BHD5 header layout (after RSA decrypt):
 *   0x00: "BHD5" magic (4)
 *   0x04: endian byte (1) — 0xFF = LE
 *   0x05: unk05 (1)
 *   0x06: 0x00 0x00 (2)
 *   0x08: int32(1) (4)
 *   0x0C: file_size int32 (4)
 *   0x10: bucket_count int64 (8)   [64-bit mode: offsets 0x14 and 0x1C are 0]
 *   0x18: buckets_offset int64 (8)
 *   0x20: salt_len int32 (4)
 *   0x24: salt[salt_len]
 *
 * Bucket table at buckets_offset (64-bit):
 *   per bucket: file_count(4) + flag(4, must be 1) + files_offset(8) = 16 bytes
 *
 * File entry (ER/Sekiro/NR/AC6, 64-bit):
 *   path_hash(8) + padded_size(4) + unpadded_size(4) + file_offset(8) +
 *   sha_hash_offset(8) + aes_key_offset(8) = 40 bytes
 */

static sf_result_t decrypt_ranges(uint8_t *data, size_t size,
                                   const uint8_t *bhd, size_t bhd_size,
                                   int64_t aes_offset)
{
    size_t meta_offset = 0;
    bool   found       = false;
    size_t begin = aes_offset > 32 ? (size_t)aes_offset - 32u : 0u;
    size_t end   = (uint64_t)aes_offset + 64u < bhd_size
                       ? (size_t)aes_offset + 64u : bhd_size;

    for (size_t cand = begin; cand + 20u <= end; ++cand) {
        int32_t count = (int32_t)read_u32_le(bhd + cand + 16u);
        if (count <= 0 || count > 16 || cand + 20u + (size_t)count * 16u > bhd_size) continue;
        int64_t start0 = (int64_t)read_u64_le(bhd + cand + 20u);
        int64_t end0   = (int64_t)read_u64_le(bhd + cand + 28u);
        if (start0 == 0 && end0 > 0 && (uint64_t)end0 <= size && (end0 % 16) == 0) {
            meta_offset = cand;
            found = true;
            break;
        }
    }
    if (!found) return SF_ERR_NOT_FOUND;

    const uint8_t *meta = bhd + meta_offset;
    uint8_t key[16];
    memcpy(key, meta, sizeof(key));
    int32_t range_count = (int32_t)read_u32_le(meta + 16);
    if (range_count < 0 || range_count > 1024) return SF_ERR_OUT_OF_RANGE;
    if ((uint64_t)aes_offset + 20u + (uint64_t)range_count * 16u > bhd_size)
        return SF_ERR_TRUNCATED;

    for (int32_t i = 0; i < range_count; ++i) {
        const uint8_t *range = meta + 20 + (size_t)i * 16u;
        int64_t start = (int64_t)read_u64_le(range);
        int64_t end_r = (int64_t)read_u64_le(range + 8);
        if (end_r == start || start < 0 || end_r < 0) continue;
        if (end_r < start || (uint64_t)end_r > size || ((end_r - start) % 16) != 0)
            return SF_ERR_OUT_OF_RANGE;
        sf_result_t r = sfi_aes_decrypt_ecb_buffer(key, data + start, (size_t)(end_r - start));
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

/* ── Extract + sniff via er_extract_from_data0 ──────────────────────────── */
static void probe_via_data0_helper(
    const char *game,
    const char *archive,
    sf_result_t (*extract_fn)(const char *, void **, size_t *))
{
    char cands[12 * EXT_COUNT][256];

    for (size_t m = 0; m < MAP_ID_COUNT; ++m) {
        size_t n = build_candidates(k_map_ids[m], cands, 12 * EXT_COUNT);
        for (size_t i = 0; i < n; ++i) {
            void  *raw  = NULL;
            size_t rsz  = 0;
            sf_result_t r = extract_fn(cands[i], &raw, &rsz);
            if (r != SF_OK) continue;

            int32_t    ver = -1;
            probe_fmt_t f  = sniff_format((const uint8_t *)raw, rsz, &ver);
            record_hit(game, archive, cands[i], path_hash_133(cands[i]),
                       SF_DCX_TYPE_UNKNOWN, f, ver, rsz);
            sf_free(NULL, raw);
        }
    }
}

/* Probe a non-Data0 shard across all candidate paths.
 * Loads the BHD once, then scans all candidates against the in-memory BHD. */
static void probe_shard(const char *game, const char *archive,
                        const wchar_t *bhd_path, const wchar_t *bdt_path,
                        const char *pem)
{
    uint8_t *bhd_raw = NULL, *bhd = NULL;
    size_t   bhd_raw_sz = 0, bhd_sz = 0;

    sf_result_t r = read_file_w(bhd_path, &bhd_raw, &bhd_raw_sz);
    if (r != SF_OK) {
        printf("SKIP %s %s: archive missing\n", game, archive);
        return;
    }

    r = rsa_unwrap(bhd_raw, bhd_raw_sz, pem, &bhd, &bhd_sz);
    sf_free(NULL, bhd_raw);
    if (r != SF_OK) {
        printf("SKIP %s %s: BHD RSA unwrap failed (%s)\n", game, archive, sf_result_str(r));
        return;
    }
    sf_istream_t *bdt = NULL;
    r = sf_istream_open_wfile(&bdt, bdt_path, NULL);
    if (r != SF_OK) {
        printf("SKIP %s %s: BDT open failed\n", game, archive);
        sf_free(NULL, bhd);
        return;
    }
    printf("Probing %s %s (%zu KB)...\n", game, archive, bhd_sz / 1024);
    fflush(stdout);

    /* Build candidate hash table: hash -> path string.
     * Then do ONE linear scan of the BHD5 to find all matches at once,
     * instead of scanning once per candidate path (which would be O(N*M)). */
#define MAX_CANDS 2048
    typedef struct { uint64_t hash; char path[256]; } cand_t;
    cand_t *cand_table = sf_default_allocator()->alloc(
        MAX_CANDS * sizeof(cand_t), sf_default_allocator()->user);
    if (!cand_table) { sf_istream_close(bdt); sf_free(NULL, bhd); return; }
    size_t cand_count = 0;

    char cands[12 * EXT_COUNT][256];
    for (size_t m = 0; m < MAP_ID_COUNT && cand_count < MAX_CANDS; ++m) {
        size_t n = build_candidates(k_map_ids[m], cands, 12 * EXT_COUNT);
        for (size_t i = 0; i < n && cand_count < MAX_CANDS; ++i) {
            cand_table[cand_count].hash = path_hash_133(cands[i]);
            snprintf(cand_table[cand_count].path, 256, "%s", cands[i]);
            cand_count++;
        }
    }

    /* Single linear scan of BHD5 for all candidate hashes */
    for (size_t pos = 0; pos + 40 <= bhd_sz; ++pos) {
        uint64_t h = read_u64_le(bhd + pos);
        for (size_t ci = 0; ci < cand_count; ++ci) {
            if (cand_table[ci].hash != h) continue;
            int32_t padded   = (int32_t)read_u32_le(bhd + pos + 8u);
            int64_t file_off = (int64_t)read_u64_le(bhd + pos + 16u);
            int64_t aes_off  = (int64_t)read_u64_le(bhd + pos + 24u);
            if (padded <= 0 || file_off < 0 || aes_off < 0 || (uint64_t)aes_off >= bhd_sz) continue;

            uint8_t *dcx_buf = sf_default_allocator()->alloc(
                (size_t)padded, sf_default_allocator()->user);
            if (!dcx_buf) continue;

            r = sf_istream_seek(bdt, file_off);
            if (r == SF_OK) r = sf_istream_read(bdt, dcx_buf, (size_t)padded);
            if (r == SF_OK)
                r = decrypt_ranges(dcx_buf, (size_t)padded, bhd, bhd_sz, aes_off);
            if (r != SF_OK) { sf_free(NULL, dcx_buf); continue; }

            sf_dcx_type_t dtype = SF_DCX_TYPE_UNKNOWN;
            void         *payload = NULL;
            size_t        psz     = 0;
            const sf_result_t sniff_r = sf_dcx_sniff(dcx_buf, (size_t)padded, &dtype);
            const bool is_dcx = sniff_r == SF_OK && dtype != SF_DCX_TYPE_NONE
                                && dtype != SF_DCX_TYPE_UNKNOWN;
            if (is_dcx) {
                sf_dcx_type_t otype = SF_DCX_TYPE_UNKNOWN;
                r = sf_dcx_decompress(dcx_buf, (size_t)padded, &payload, &psz, &otype, NULL);
                sf_free(NULL, dcx_buf);
                if (r != SF_OK) continue;
                dtype = otype;
            } else {
                payload = dcx_buf;
                psz     = (size_t)padded;
                dtype   = SF_DCX_TYPE_NONE;
            }

            int32_t    ver = -1;
            probe_fmt_t f  = sniff_format((const uint8_t *)payload, psz, &ver);
            record_hit(game, archive, cand_table[ci].path, h, dtype, f, ver, psz);
            sf_free(NULL, payload);
        }
    }

    sf_free(NULL, cand_table);
    sf_istream_close(bdt);
    sf_free(NULL, bhd);
}

/* ── Per-game probe functions ───────────────────────────────────────────── */
static void probe_er(void)
{
    printf("Probing ER Data0...\n");
    if (!er_helper_is_available()) {
        printf("SKIP ER: archive missing or Oodle DLL missing\n");
        return;
    }
    probe_via_data0_helper("ER", "Data0", er_extract_from_data0);

    /* ER Data1 */
    probe_shard("ER", "Data1",
                L"C:/Games/ELDEN RING/Game/Data1.bhd",
                L"C:/Games/ELDEN RING/Game/Data1.bdt",
                k_er_data1_pem);

    /* ER Data2 */
    probe_shard("ER", "Data2",
                L"C:/Games/ELDEN RING/Game/Data2.bhd",
                L"C:/Games/ELDEN RING/Game/Data2.bdt",
                k_er_data2_pem);

    /* ER Data3 */
    probe_shard("ER", "Data3",
                L"C:/Games/ELDEN RING/Game/Data3.bhd",
                L"C:/Games/ELDEN RING/Game/Data3.bdt",
                k_er_data3_pem);
}

static void probe_sekiro(void)
{
    printf("Probing Sekiro...\n");
    fflush(stdout);
    if (!sekiro_helper_is_available()) {
        printf("SKIP Sekiro: archive missing or Oodle DLL missing\n");
        return;
    }
    probe_via_data0_helper("Sekiro", "DataN", sekiro_extract_from_anybhd);
}

static void probe_nr(void)
{
    printf("Probing NR Data0...\n");
    if (!nightreign_helper_is_available()) {
        printf("SKIP NR: archive missing or Oodle DLL missing\n");
        return;
    }
    probe_via_data0_helper("NR", "data0", nightreign_extract_from_data0);

    /* NR data2 */
    probe_shard("NR", "data2",
                L"C:/Games/ELDEN RING NIGHTREIGN/Game/data2.bhd",
                L"C:/Games/ELDEN RING NIGHTREIGN/Game/data2.bdt",
                k_nr_data2_pem);
}

static void probe_ac6(void)
{
    printf("Probing AC6 Data0...\n");
    if (!ac6_helper_is_available()) {
        printf("SKIP AC6: archive missing or Oodle DLL missing\n");
        return;
    }
    probe_via_data0_helper("AC6", "Data0", ac6_extract_from_data0);
}

/* ── Report generation ──────────────────────────────────────────────────── */
static sf_result_t build_evidence(report_buf_t *r)
{
    sf_result_t rc = rbuf_appendf(r, "# Lighting File Probe — Evidence\n\n");
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "| Format | Game | Archive | Path | Hash | DcxType | Version | Size |\n");
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "|--------|------|---------|------|------|---------|---------|------|\n");
    if (rc != SF_OK) return rc;

    for (size_t i = 0; i < g_hit_count; ++i) {
        const hit_t *h = &g_hits[i];
        char ver_str[16];
        if (h->version >= 0) snprintf(ver_str, sizeof(ver_str), "V%d", h->version);
        else                  snprintf(ver_str, sizeof(ver_str), "?");

        rc = rbuf_appendf(r, "| %s | %s | %s | `%s` | 0x%016" PRIX64 " | %s | %s | %zu |\n",
                          fmt_name(h->fmt), h->game, h->archive, h->path,
                          h->hash, h->dcx_type, ver_str, h->size);
        if (rc != SF_OK) return rc;
    }

    rc = rbuf_appendf(r, "\nTotal hits: %zu\n", g_hit_count);
    return rc;
}

static sf_result_t build_scope(report_buf_t *r)
{
    /* Count per-format hits. */
    size_t btab_count  = 0, btl_count  = 0, btpb_count  = 0;
    size_t gparam_count = 0, fltparam_count = 0, pmdcl_count = 0;
    bool   btl_v18_found = false;
    char   btl_v18_game[64] = {0};
    bool   fltparam_found = false;
    char   fltparam_game[64] = {0};

    for (size_t i = 0; i < g_hit_count; ++i) {
        const hit_t *h = &g_hits[i];
        switch (h->fmt) {
        case FMT_BTAB:   btab_count++;  break;
        case FMT_BTL:
            btl_count++;
            if (h->version == 18 && !btl_v18_found) {
                btl_v18_found = true;
                snprintf(btl_v18_game, sizeof(btl_v18_game), "%s", h->game);
            }
            break;
        case FMT_BTPB:   btpb_count++;  break;
        case FMT_GPARAM:
            if (strstr(h->path, ".fltparam")) {
                fltparam_count++;
                if (!fltparam_found) {
                    fltparam_found = true;
                    snprintf(fltparam_game, sizeof(fltparam_game), "%s", h->game);
                }
            } else {
                gparam_count++;
            }
            break;
        case FMT_PMDCL:  pmdcl_count++; break;
        default: break;
        }
    }

    sf_result_t rc = rbuf_appendf(r, "# Lighting File Probe — Scope Decisions\n\n");
    if (rc != SF_OK) return rc;

    rc = rbuf_appendf(r, "## Hit counts\n\n");
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- BTAB:     %zu\n", btab_count);
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- BTL:      %zu\n", btl_count);
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- BTPB:     %zu\n", btpb_count);
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- GPARAM:   %zu\n", gparam_count);
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- FLTPARAM: %zu\n", fltparam_count);
    if (rc != SF_OK) return rc;
    rc = rbuf_appendf(r, "- PMDCL:    %zu\n\n", pmdcl_count);
    if (rc != SF_OK) return rc;

    /* BTPB scope decision */
    rc = rbuf_appendf(r, "## BTPB scope decision\n\n");
    if (rc != SF_OK) return rc;
    if (btpb_count == 0) {
        rc = rbuf_appendf(r,
            "LIGHTING-PROBE: BTPB NOT PRESENT IN V1 GAMES — drop from batch\n\n"
            "Decision: DROP — BTPB not found in any v1 game archive.\n\n");
    } else {
        rc = rbuf_appendf(r,
            "Decision: INCLUDE — BTPB found in %zu location(s).\n\n", btpb_count);
    }
    if (rc != SF_OK) return rc;

    /* BTL scope decision */
    rc = rbuf_appendf(r, "## BTL scope decision\n\n");
    if (rc != SF_OK) return rc;
    if (btl_v18_found) {
        rc = rbuf_appendf(r,
            "LIGHTING-PROBE: BTL V18 confirmed in %s\n\n"
            "Decision: V16 + V18 confirmed.\n\n", btl_v18_game);
    } else if (btl_count > 0) {
        /* Report which versions were found */
        rc = rbuf_appendf(r, "Decision: BTL found (%zu hits) but V18 not confirmed.\n\n",
                          btl_count);
    } else {
        rc = rbuf_appendf(r, "Decision: BTL not found in any v1 game archive.\n\n");
    }
    if (rc != SF_OK) return rc;

    /* GPARAM / FLTPARAM scope decision */
    rc = rbuf_appendf(r, "## GPARAM extension scope decision\n\n");
    if (rc != SF_OK) return rc;
    if (fltparam_found) {
        rc = rbuf_appendf(r,
            "LIGHTING-PROBE: GPARAM .fltparam extension confirmed in %s\n\n"
            "Decision: Both .gparam and .fltparam extensions present.\n\n",
            fltparam_game);
    } else if (gparam_count > 0) {
        rc = rbuf_appendf(r,
            "Decision: .gparam extension confirmed (%zu hits); .fltparam not found.\n\n",
            gparam_count);
    } else {
        rc = rbuf_appendf(r, "Decision: GPARAM not found in any v1 game archive.\n\n");
    }
    if (rc != SF_OK) return rc;

    /* BTL version summary */
    rc = rbuf_appendf(r, "## BTL version breakdown\n\n");
    if (rc != SF_OK) return rc;
    int32_t seen_versions[32];
    size_t  seen_count = 0;
    for (size_t i = 0; i < g_hit_count; ++i) {
        if (g_hits[i].fmt != FMT_BTL) continue;
        int32_t v = g_hits[i].version;
        bool already = false;
        for (size_t j = 0; j < seen_count; ++j) {
            if (seen_versions[j] == v) { already = true; break; }
        }
        if (!already && seen_count < 32) seen_versions[seen_count++] = v;
    }
    for (size_t j = 0; j < seen_count; ++j) {
        rc = rbuf_appendf(r, "- V%d\n", seen_versions[j]);
        if (rc != SF_OK) return rc;
    }
    if (seen_count == 0) {
        rc = rbuf_appendf(r, "- (none)\n");
        if (rc != SF_OK) return rc;
    }

    return SF_OK;
}

/* ── main ───────────────────────────────────────────────────────────────── */
int main(void)
{
    sf_oodle_set_search_path(SF_E2E_OODLE_DIR);

    /* Initialize all game helpers (idempotent; failures handled per-game). */
    er_helper_init();
    sekiro_helper_init();
    nightreign_helper_init();
    ac6_helper_init();

    probe_er();
    probe_sekiro();
    probe_nr();
    probe_ac6();

    printf("\nTotal hits: %zu\n", g_hit_count);

    /* Build and write evidence. */
    report_buf_t ev = {0};
    report_buf_t sc = {0};

    sf_result_t r = build_evidence(&ev);
    if (r == SF_OK) r = build_scope(&sc);

    if (r == SF_OK) {
        fwrite(ev.data, 1u, ev.len, stdout);
        fwrite(sc.data, 1u, sc.len, stdout);
    }

    if (r == SF_OK) r = rbuf_write_file(&ev, EVIDENCE_PATH);
    if (r == SF_OK) r = rbuf_write_file(&sc, SCOPE_PATH);

    rbuf_free(&ev);
    rbuf_free(&sc);

    if (r != SF_OK) {
        fprintf(stderr, "probe_lighting_files: failed: %s\n", sf_result_str(r));
        return 1;
    }
    return 0;
}
