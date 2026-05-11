/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Probe: compare ER vs NR MSB headers to determine MSBE compatibility
 * Usage: probe_nightreign_msb.exe
 * Output: .sisyphus/evidence/task-4-nightreign-probe.md
 */

#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "souls_formats/sf_oodle.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PROBE_EVIDENCE_PATH ".sisyphus/evidence/task-4-nightreign-probe.md"
#define PROBE_HEADER_SIZE 64u

static const char k_er_data2_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEA0iDVVQ230RgrkIHJNDgxE7I/2AaH6Li1Eu9mtpfrrfhfoK2e7y4O\n"
    "WU+lj7AGI4GIgkWpPw8JHaV970Cr6+sTG4Tr5eMQPxrCIH7BJAPCloypxcs2BNfT\n"
    "GXzm6veUfrGzLIDp7wy24lIA8r9ZwUvpKlN28kxBDGeCbGCkYeSVNuF+R9rN4OAM\n"
    "RYh0r1Q950xc2qSNloNsjpDoSKoYN0T7u5rnMn/4mtclnWPVRWU940zr1rymv4Jc\n"
    "3umNf6cT1XqrS1gSaK1JWZfsSeD6Dwk3uvquvfY6YlGRygIlVEMAvKrDRMHylsLt\n"
    "qqhYkZNXMdy0NXopf1rEHKy9poaHEmJldwIFAP////8=\n"
    "-----END RSA PUBLIC KEY-----\n";

static const char k_nr_data2_pem[] =
    "-----BEGIN RSA PUBLIC KEY-----\n"
    "MIIBDAKCAQEAqpkf9yHnx8k84+WXITLFUW/STypXjZMPuw842pzNHa5L7v9gU4M5\n"
    "hBHwTQs0YIcfnf+mbjqoJYnmYPBblxLjFXgwT4ICJdpnPMY75BwD0Nv28/CvvIsA\n"
    "0QQWOhUeOXnm5BT26dGYi3CHHPvD14F76tJt3TO/CC3fyhdxne9Cra5G87aGTJGv\n"
    "0ImsU0KPCizYX/RHQ2jdJdlB5BHzkMgLhIaEdhC3nhIqMJDNQNGKMo7rRV1tAEGf\n"
    "0zIZ23PGEsPsbVg31nnnRoq338WfD9ArZZG6bM11vlfVcYmrJs7v4vBjKXnYVwVX\n"
    "0rQGIfSNDnaZcEj4tsl04AqnupTdvSrHXwIFANOg6RU=\n"
    "-----END RSA PUBLIC KEY-----\n";

typedef struct probe_archive {
    const wchar_t *bhd_path;
    const wchar_t *bdt_path;
    const char    *pem;
    const char    *msb_path;
} probe_archive_t;

typedef struct probe_entry {
    int32_t padded_size;
    int64_t file_offset;
    int64_t aes_key_offset;
} probe_entry_t;

typedef struct probe_msb_header {
    const char *label;
    const char *path;
    uint8_t    *bytes;
    size_t      size;
    uint8_t     header[PROBE_HEADER_SIZE];
    char        magic[5];
    int32_t     version;
    int32_t     entry_list_count;
} probe_msb_header_t;

static uint32_t read_u32_le(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return ((uint64_t)read_u32_le(p)) | ((uint64_t)read_u32_le(p + 4) << 32);
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

static sf_result_t read_file(const wchar_t *path, uint8_t **out, size_t *out_size)
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

static sf_result_t rsa_unwrap(const uint8_t *in, size_t in_size, const char *pem,
                              uint8_t **out, size_t *out_size)
{
    if (in_size == 0 || (in_size % 256u) != 0) return SF_ERR_CRYPTO;
    uint8_t *plain = sf_default_allocator()->alloc(in_size, sf_default_allocator()->user);
    if (!plain) return SF_ERR_OOM;

    size_t used = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t *chunk = NULL;
        size_t chunk_size = 0;
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
        memcpy(plain + used, chunk, chunk_size);
        used += chunk_size;
        sf_free(NULL, chunk);
    }

    *out = plain;
    *out_size = used;
    return SF_OK;
}

static bool find_entry(const uint8_t *bhd, size_t bhd_size, const char *path, probe_entry_t *out)
{
    uint64_t hash = probe_path_hash_64(path);
    uint8_t needle[8];
    for (size_t i = 0; i < 8; ++i) needle[i] = (uint8_t)(hash >> (i * 8));

    for (size_t pos = 0; pos + 40 <= bhd_size; ++pos) {
        if (memcmp(bhd + pos, needle, sizeof(needle)) != 0) continue;
        int32_t padded = (int32_t)read_u32_le(bhd + pos + 8);
        int64_t file_offset = (int64_t)read_u64_le(bhd + pos + 16);
        int64_t aes_offset = (int64_t)read_u64_le(bhd + pos + 24);
        if (padded > 0 && file_offset >= 0 && aes_offset >= 0 && (uint64_t)aes_offset < bhd_size) {
            out->padded_size = padded;
            out->file_offset = file_offset;
            out->aes_key_offset = aes_offset;
            return true;
        }
    }
    return false;
}

static sf_result_t decrypt_ranges(uint8_t *data, size_t size, int64_t file_offset,
                                  const uint8_t *bhd, size_t bhd_size, int64_t aes_offset)
{
    (void)file_offset;
    size_t meta_offset = 0;
    bool found = false;
    size_t begin = aes_offset > 32 ? (size_t)aes_offset - 32u : 0u;
    size_t end = (uint64_t)aes_offset + 64u < bhd_size ? (size_t)aes_offset + 64u : bhd_size;
    for (size_t cand = begin; cand + 20u <= end; ++cand) {
        int32_t count = (int32_t)read_u32_le(bhd + cand + 16u);
        if (count <= 0 || count > 16 || cand + 20u + (size_t)count * 16u > bhd_size) continue;
        int64_t start0 = (int64_t)read_u64_le(bhd + cand + 20u);
        int64_t end0 = (int64_t)read_u64_le(bhd + cand + 28u);
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
    if ((uint64_t)aes_offset + 20u + (uint64_t)range_count * 16u > bhd_size) return SF_ERR_TRUNCATED;

    for (int32_t i = 0; i < range_count; ++i) {
        const uint8_t *range = meta + 20 + (size_t)i * 16u;
        int64_t start = (int64_t)read_u64_le(range);
        int64_t end = (int64_t)read_u64_le(range + 8);
        if (end == start || start < 0 || end < 0) continue;
        if (end < start || (uint64_t)end > size || ((end - start) % 16) != 0) return SF_ERR_OUT_OF_RANGE;
        sf_result_t r = sfi_aes_decrypt_ecb_buffer(key, data + start, (size_t)(end - start));
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t extract_msb(const probe_archive_t *archive, probe_msb_header_t *out)
{
    uint8_t *bhd_raw = NULL, *bhd = NULL, *dcx = NULL, *msb = NULL;
    size_t bhd_raw_size = 0, bhd_size = 0;
    sf_result_t r = read_file(archive->bhd_path, &bhd_raw, &bhd_raw_size);
    if (r != SF_OK) return r;
    r = rsa_unwrap(bhd_raw, bhd_raw_size, archive->pem, &bhd, &bhd_size);
    sf_free(NULL, bhd_raw);
    if (r != SF_OK) return r;

    probe_entry_t entry;
    if (!find_entry(bhd, bhd_size, archive->msb_path, &entry)) {
        sf_free(NULL, bhd);
        return SF_ERR_NOT_FOUND;
    }

    sf_istream_t *bdt = NULL;
    r = sf_istream_open_wfile(&bdt, archive->bdt_path, NULL);
    if (r != SF_OK) {
        sf_free(NULL, bhd);
        return r;
    }
    dcx = sf_default_allocator()->alloc((size_t)entry.padded_size, sf_default_allocator()->user);
    if (!dcx) {
        sf_istream_close(bdt);
        sf_free(NULL, bhd);
        return SF_ERR_OOM;
    }
    r = sf_istream_seek(bdt, entry.file_offset);
    if (r == SF_OK) r = sf_istream_read(bdt, dcx, (size_t)entry.padded_size);
    sf_istream_close(bdt);
    if (r == SF_OK) r = decrypt_ranges(dcx, (size_t)entry.padded_size, entry.file_offset,
                                       bhd, bhd_size, entry.aes_key_offset);
    sf_free(NULL, bhd);
    if (r != SF_OK) {
        sf_free(NULL, dcx);
        return r;
    }

    size_t msb_size = 0;
    sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
    r = sf_dcx_decompress(dcx, (size_t)entry.padded_size, (void **)&msb, &msb_size, &type, NULL);
    sf_free(NULL, dcx);
    if (r != SF_OK) return r;
    if (msb_size < PROBE_HEADER_SIZE) {
        sf_free(NULL, msb);
        return SF_ERR_TRUNCATED;
    }

    out->path = archive->msb_path;
    out->bytes = msb;
    out->size = msb_size;
    memcpy(out->header, msb, PROBE_HEADER_SIZE);
    memcpy(out->magic, msb, 4);
    out->magic[4] = '\0';
    out->version = (int32_t)read_u32_le(msb + 4);
    out->entry_list_count = (int32_t)read_u32_le(msb + 8);
    return SF_OK;
}

static void print_hex_dump(FILE *out, const uint8_t bytes[PROBE_HEADER_SIZE])
{
    for (size_t row = 0; row < PROBE_HEADER_SIZE; row += 16) {
        fprintf(out, "%04zX:", row);
        for (size_t col = 0; col < 16; ++col) fprintf(out, " %02X", bytes[row + col]);
        fputc('\n', out);
    }
}

static const char *decide_verdict(const probe_msb_header_t *er, const probe_msb_header_t *nr)
{
    if (memcmp(er->magic, nr->magic, 4) == 0 && er->version == nr->version) return "COMPATIBLE (A)";
    if (memcmp(er->magic, nr->magic, 4) == 0) return "MOSTLY_COMPATIBLE (B)";
    return "DIVERGED (C)";
}

static void write_report(FILE *out, const probe_msb_header_t *er, const probe_msb_header_t *nr,
                         const char *verdict)
{
    fprintf(out, "# Nightreign MSB Header Probe\n\n");
    fprintf(out, "Note: mapstudio MSBs are in Data2/data2 on the installed ER/NR builds; Data0/data0 contains no `/map/mapstudio/*.msb.dcx` entries.\n\n");
    fprintf(out, "## ER MSB header hex dump\n\n- Path: `%s`\n- Decompressed size: %zu bytes\n- Magic: `%s`\n- Version field: %d\n- Entry list count field: %d\n\n```text\n",
            er->path, er->size, er->magic, er->version, er->entry_list_count);
    print_hex_dump(out, er->header);
    fprintf(out, "```\n\n");
    fprintf(out, "## NR MSB header hex dump\n\n- Path: `%s`\n- Decompressed size: %zu bytes\n- Magic: `%s`\n- Version field: %d\n- Entry list count field: %d\n\n```text\n",
            nr->path, nr->size, nr->magic, nr->version, nr->entry_list_count);
    print_hex_dump(out, nr->header);
    fprintf(out, "```\n\n## Diff + decision\n\n");
    fprintf(out, "- Magic bytes: %s (`%s` vs `%s`)\n", memcmp(er->magic, nr->magic, 4) == 0 ? "match" : "DIFF", er->magic, nr->magic);
    fprintf(out, "- Version field: %s (%d vs %d)\n", er->version == nr->version ? "match" : "DIFF", er->version, nr->version);
    fprintf(out, "- Entry list count field: %s (%d vs %d)\n", er->entry_list_count == nr->entry_list_count ? "match" : "DIFF", er->entry_list_count, nr->entry_list_count);
    fprintf(out, "\nVERDICT: %s\n", verdict);
}

int main(void)
{
    sf_oodle_set_search_path(L"C:/Games/ELDEN RING NIGHTREIGN/Game");
    const probe_archive_t er_archive = {L"C:/Games/ELDEN RING/Game/Data2.bhd",
                                        L"C:/Games/ELDEN RING/Game/Data2.bdt",
                                        k_er_data2_pem,
                                        "/map/mapstudio/m60_42_36_00.msb.dcx"};
    const probe_archive_t nr_archive = {L"C:/Games/ELDEN RING NIGHTREIGN/Game/data2.bhd",
                                        L"C:/Games/ELDEN RING NIGHTREIGN/Game/data2.bdt",
                                        k_nr_data2_pem,
                                        "/map/mapstudio/m10_00_00_00.msb.dcx"};

    probe_msb_header_t er = {.label = "ER"};
    probe_msb_header_t nr = {.label = "NR"};
    sf_result_t r = extract_msb(&er_archive, &er);
    if (r != SF_OK) {
        fprintf(stderr, "failed to extract ER MSB: %s\n", sf_result_str(r));
        return 1;
    }
    r = extract_msb(&nr_archive, &nr);
    if (r != SF_OK) {
        fprintf(stderr, "failed to extract NR MSB: %s\n", sf_result_str(r));
        sf_free(NULL, er.bytes);
        return 1;
    }

    const char *verdict = decide_verdict(&er, &nr);
    write_report(stdout, &er, &nr, verdict);

    sf_ostream_t *evidence = NULL;
    r = sf_ostream_open_file(&evidence, PROBE_EVIDENCE_PATH, NULL);
    if (r == SF_OK) {
        char buf[4096];
        FILE *tmp = tmpfile();
        if (tmp) {
            write_report(tmp, &er, &nr, verdict);
            rewind(tmp);
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), tmp)) > 0 && r == SF_OK) {
                r = sf_ostream_write(evidence, buf, n);
            }
            fclose(tmp);
        } else {
            r = SF_ERR_IO;
        }
        sf_ostream_close(evidence);
    }

    sf_free(NULL, nr.bytes);
    sf_free(NULL, er.bytes);
    if (r != SF_OK) {
        fprintf(stderr, "failed to write evidence file: %s\n", sf_result_str(r));
        return 1;
    }
    return 0;
}
