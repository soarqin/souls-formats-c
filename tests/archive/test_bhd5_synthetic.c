/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 3 QA — BHD5 synthetic streaming reader checks.
 */

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include "crypto/aes_cng.h"

#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

typedef unsigned long DWORD;
typedef int BOOL;

__declspec(dllimport) DWORD GetTempPathW(DWORD nBufferLength, wchar_t *lpBuffer);
__declspec(dllimport) DWORD GetCurrentProcessId(void);
__declspec(dllimport) BOOL DeleteFileW(const wchar_t *lpFileName);

void setUp(void) {}
void tearDown(void) {}

typedef struct bytebuf {
    uint8_t data[4096];
    size_t size;
} bytebuf_t;

typedef struct synth_file {
    const char *path;
    uint64_t hash;
    uint32_t padded_size;
    uint32_t unpadded_size;
    int64_t file_offset;
    bool has_aes;
    uint8_t aes_key[16];
    int64_t range_start;
    int64_t range_end;
} synth_file_t;

static const uint8_t k_key[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
};

typedef struct counting_allocator {
    size_t alloc_calls;
    size_t realloc_calls;
    size_t free_calls;
} counting_allocator_t;

static void *counting_alloc(size_t size, void *user) {
    counting_allocator_t *c = (counting_allocator_t *)user;
    c->alloc_calls++;
    return malloc(size ? size : 1u);
}

static void *counting_realloc(void *p, size_t old_size, size_t new_size, void *user) {
    (void)old_size;
    counting_allocator_t *c = (counting_allocator_t *)user;
    c->realloc_calls++;
    return realloc(p, new_size ? new_size : 1u);
}

static void counting_free(void *p, void *user) {
    counting_allocator_t *c = (counting_allocator_t *)user;
    c->free_calls++;
    free(p);
}

static void bb_u8(bytebuf_t *b, uint8_t v) {
    TEST_ASSERT_LESS_THAN_size_t(sizeof(b->data), b->size);
    b->data[b->size++] = v;
}

static void bb_bytes(bytebuf_t *b, const void *p, size_t n) {
    TEST_ASSERT_LESS_OR_EQUAL_size_t(sizeof(b->data), b->size + n);
    memcpy(b->data + b->size, p, n);
    b->size += n;
}

static void bb_i32(bytebuf_t *b, int32_t v) {
    bb_u8(b, (uint8_t)((uint32_t)v));
    bb_u8(b, (uint8_t)((uint32_t)v >> 8));
    bb_u8(b, (uint8_t)((uint32_t)v >> 16));
    bb_u8(b, (uint8_t)((uint32_t)v >> 24));
}

static void bb_u32(bytebuf_t *b, uint32_t v) { bb_i32(b, (int32_t)v); }

static void bb_i64(bytebuf_t *b, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; i++) bb_u8(b, (uint8_t)(u >> (i * 8)));
}

static void bb_u64(bytebuf_t *b, uint64_t v) { bb_i64(b, (int64_t)v); }

static void bb_patch_i32(bytebuf_t *b, size_t off, int32_t v) {
    TEST_ASSERT_LESS_OR_EQUAL_size_t(b->size, off + 4u);
    b->data[off + 0u] = (uint8_t)((uint32_t)v);
    b->data[off + 1u] = (uint8_t)((uint32_t)v >> 8);
    b->data[off + 2u] = (uint8_t)((uint32_t)v >> 16);
    b->data[off + 3u] = (uint8_t)((uint32_t)v >> 24);
}

static void make_temp_path(const wchar_t *stem, wchar_t *out) {
    wchar_t tmpdir[MAX_PATH];
    DWORD got = GetTempPathW(MAX_PATH, tmpdir);
    TEST_ASSERT_TRUE(got > 0 && got < MAX_PATH);
    int wrote = swprintf(out, MAX_PATH, L"%ls%ls_%lu.tmp", tmpdir, stem,
                         (unsigned long)GetCurrentProcessId());
    TEST_ASSERT_TRUE(wrote > 0 && wrote < MAX_PATH);
}

static void write_wfile(const wchar_t *path, const void *data, size_t size) {
    sf_ostream_t *s = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_open_wfile(&s, path, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_ostream_write(s, data, size));
    sf_ostream_close(s);
}

static void assert_file_magic_bhd5(const wchar_t *path) {
    sf_istream_t *s = NULL;
    uint8_t magic[4] = {0};
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_open_wfile(&s, path, NULL));
    TEST_ASSERT_EQUAL(SF_OK, sf_istream_read(s, magic, sizeof(magic)));
    TEST_ASSERT_EQUAL_MEMORY("BHD5", magic, sizeof(magic));
    sf_istream_close(s);
}

static void encrypt_first_block(uint8_t *data, const uint8_t key[16]) {
    uint8_t out[16];
    TEST_ASSERT_EQUAL(SF_OK, sfi_aes_ecb_block(key, 16, true, data, out));
    memcpy(data, out, 16);
}

static void build_bhd(bytebuf_t *b, const synth_file_t *files, size_t file_count,
                      const uint32_t *bucket_counts, size_t bucket_count) {
    memset(b, 0, sizeof(*b));
    const char salt[] = "synthetic-salt";
    size_t file_size_pos = 0;
    size_t bucket_table_offset = 0;
    size_t headers_offset = 0;
    size_t metadata_offset = 0;

    bb_bytes(b, "BHD5", 4);
    bb_u8(b, 0xFF); /* little-endian marker (-1) */
    bb_u8(b, 0);    /* Unk05 false */
    bb_u8(b, 0);
    bb_u8(b, 0);
    bb_i32(b, 1);
    file_size_pos = b->size;
    bb_i32(b, 0);
    bb_i64(b, (int64_t)bucket_count);
    bb_i64(b, 0); /* buckets offset placeholder; patched manually below */
    bb_i32(b, (int32_t)strlen(salt));
    bb_bytes(b, salt, strlen(salt));

    bucket_table_offset = b->size;
    for (int i = 0; i < 8; i++) b->data[0x18u + (size_t)i] = (uint8_t)(bucket_table_offset >> (i * 8));

    headers_offset = bucket_table_offset + bucket_count * 16u;
    size_t first_file = 0;
    for (size_t i = 0; i < bucket_count; i++) {
        bb_i32(b, (int32_t)bucket_counts[i]);
        bb_i32(b, 1);
        bb_i64(b, (int64_t)(headers_offset + first_file * 40u));
        first_file += bucket_counts[i];
    }

    metadata_offset = headers_offset + file_count * 40u;
    size_t meta_cursor = metadata_offset;
    for (size_t i = 0; i < file_count; i++) {
        uint64_t sha_off = 0;
        uint64_t aes_off = 0;
        if (files[i].has_aes) {
            aes_off = meta_cursor;
            meta_cursor += 16u + 4u + 16u;
        }
        bb_u64(b, files[i].hash);
        bb_u32(b, files[i].padded_size);
        bb_u32(b, files[i].unpadded_size);
        bb_i64(b, files[i].file_offset);
        bb_u64(b, sha_off);
        bb_u64(b, aes_off);
    }
    for (size_t i = 0; i < file_count; i++) {
        if (!files[i].has_aes) continue;
        bb_bytes(b, files[i].aes_key, 16);
        bb_i32(b, 1);
        bb_i64(b, files[i].range_start);
        bb_i64(b, files[i].range_end);
    }
    bb_patch_i32(b, file_size_pos, (int32_t)b->size);
}

static void run_two_file_case(sf_bhd5_game_t game) {
    const uint8_t plain_a[32] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    };
    const uint8_t plain_b[] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee };
    uint8_t bdt[64];
    memset(bdt, 0, sizeof(bdt));
    memcpy(bdt, plain_a, sizeof(plain_a));
    encrypt_first_block(bdt, k_key);
    memcpy(bdt + 40u, plain_b, sizeof(plain_b));

    synth_file_t files[2];
    memset(files, 0, sizeof(files));
    files[0].path = "chr/a.bin";
    files[0].hash = sf_path_hash_64(files[0].path);
    files[0].padded_size = sizeof(plain_a);
    files[0].unpadded_size = sizeof(plain_a);
    files[0].file_offset = 0;
    files[0].has_aes = true;
    memcpy(files[0].aes_key, k_key, sizeof(k_key));
    files[0].range_start = 0;
    files[0].range_end = 16;
    files[1].path = "map/b.bin";
    files[1].hash = sf_path_hash_64(files[1].path);
    files[1].padded_size = sizeof(plain_b);
    files[1].unpadded_size = sizeof(plain_b);
    files[1].file_offset = 40;

    uint32_t bucket_counts[1] = { 2 };
    bytebuf_t bhd;
    build_bhd(&bhd, files, 2, bucket_counts, 1);

    wchar_t bhd_path[MAX_PATH];
    wchar_t bdt_path[MAX_PATH];
    make_temp_path(L"bhd5_synth_bhd", bhd_path);
    make_temp_path(L"bhd5_synth_bdt", bdt_path);
    write_wfile(bhd_path, bhd.data, bhd.size);
    write_wfile(bdt_path, bdt, sizeof(bdt));
    assert_file_magic_bhd5(bhd_path);

    sf_bhd5_t *reader = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_open(&reader, bhd_path, bdt_path, game, NULL));
    TEST_ASSERT_EQUAL_size_t(1, sf_bhd5_bucket_count(reader));
    TEST_ASSERT_EQUAL_size_t(2, sf_bhd5_total_file_count(reader));
    TEST_ASSERT_EQUAL_STRING("synthetic-salt", sf_bhd5_get_salt(reader));
    TEST_ASSERT_FALSE(sf_bhd5_get_big_endian(reader));

    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_extract_by_path(reader, files[0].path, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(plain_a), out_size);
    TEST_ASSERT_EQUAL_MEMORY(plain_a, out, sizeof(plain_a));
    sf_free(NULL, out);

    out = NULL;
    out_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_extract_by_hash_32(reader, sf_path_hash(files[1].path),
                                                        &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(plain_b), out_size);
    TEST_ASSERT_EQUAL_MEMORY(plain_b, out, sizeof(plain_b));
    sf_free(NULL, out);

    sf_bhd5_close(reader);
    DeleteFileW(bhd_path);
    DeleteFileW(bdt_path);
}

static void test_bhd5_eldenring_style(void) {
    run_two_file_case(SF_BHD5_GAME_ELDENRING);
}

static void test_bhd5_sekiro_style(void) {
    run_two_file_case(SF_BHD5_GAME_SEKIRO);
}

static void test_bhd5_range_skip(void) {
    const uint8_t payload[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    synth_file_t file;
    memset(&file, 0, sizeof(file));
    file.path = "skip/range.bin";
    file.hash = sf_path_hash_64(file.path);
    file.padded_size = sizeof(payload);
    file.unpadded_size = sizeof(payload);
    file.has_aes = true;
    memcpy(file.aes_key, k_key, sizeof(k_key));
    file.range_start = 8;
    file.range_end = 8;
    uint32_t bucket_counts[1] = { 1 };
    bytebuf_t bhd;
    build_bhd(&bhd, &file, 1, bucket_counts, 1);

    wchar_t bhd_path[MAX_PATH];
    wchar_t bdt_path[MAX_PATH];
    make_temp_path(L"bhd5_skip_bhd", bhd_path);
    make_temp_path(L"bhd5_skip_bdt", bdt_path);
    write_wfile(bhd_path, bhd.data, bhd.size);
    write_wfile(bdt_path, payload, sizeof(payload));
    assert_file_magic_bhd5(bhd_path);

    sf_bhd5_t *reader = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_open(&reader, bhd_path, bdt_path,
                                          SF_BHD5_GAME_ELDENRING, NULL));
    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_extract_by_hash_64(reader, file.hash, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), out_size);
    TEST_ASSERT_EQUAL_MEMORY(payload, out, sizeof(payload));
    sf_free(NULL, out);
    sf_bhd5_close(reader);
    DeleteFileW(bhd_path);
    DeleteFileW(bdt_path);
}

static void test_bhd5_empty_bucket(void) {
    const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    synth_file_t file;
    memset(&file, 0, sizeof(file));
    file.path = "nonempty/file.bin";
    file.hash = sf_path_hash_64(file.path);
    file.padded_size = sizeof(payload);
    file.unpadded_size = sizeof(payload);
    uint32_t bucket_counts[2] = { 0, 1 };
    bytebuf_t bhd;
    build_bhd(&bhd, &file, 1, bucket_counts, 2);

    wchar_t bhd_path[MAX_PATH];
    wchar_t bdt_path[MAX_PATH];
    make_temp_path(L"bhd5_empty_bhd", bhd_path);
    make_temp_path(L"bhd5_empty_bdt", bdt_path);
    write_wfile(bhd_path, bhd.data, bhd.size);
    write_wfile(bdt_path, payload, sizeof(payload));
    assert_file_magic_bhd5(bhd_path);

    sf_bhd5_t *reader = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_open(&reader, bhd_path, bdt_path,
                                          SF_BHD5_GAME_ELDENRING, NULL));
    TEST_ASSERT_EQUAL_size_t(2, sf_bhd5_bucket_count(reader));
    TEST_ASSERT_EQUAL_size_t(1, sf_bhd5_total_file_count(reader));
    void *out = NULL;
    size_t out_size = 0;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_extract_by_path(reader, file.path, &out, &out_size, NULL));
    TEST_ASSERT_EQUAL_MEMORY(payload, out, sizeof(payload));
    sf_free(NULL, out);
    sf_bhd5_close(reader);
    DeleteFileW(bhd_path);
    DeleteFileW(bdt_path);
}

static void test_bhd5_hash_lookup_matches_all_bucket_entries(void) {
    enum { file_count = 5 };
    const uint8_t payload[file_count] = { 0x10, 0x21, 0x32, 0x43, 0x54 };
    const char *paths[file_count] = {
        "lookup/a.bin",
        "lookup/b.bin",
        "lookup/c.bin",
        "lookup/d.bin",
        "lookup/e.bin",
    };
    synth_file_t files[file_count];
    memset(files, 0, sizeof(files));
    for (size_t i = 0; i < file_count; i++) {
        files[i].path = paths[i];
        files[i].hash = sf_path_hash_64(paths[i]);
        files[i].padded_size = 1;
        files[i].unpadded_size = 1;
        files[i].file_offset = (int64_t)i;
    }
    uint32_t bucket_counts[4] = { 0, 2, 1, 2 };
    bytebuf_t bhd;
    build_bhd(&bhd, files, file_count, bucket_counts, 4);

    wchar_t bhd_path[MAX_PATH];
    wchar_t bdt_path[MAX_PATH];
    make_temp_path(L"bhd5_lookup_bhd", bhd_path);
    make_temp_path(L"bhd5_lookup_bdt", bdt_path);
    write_wfile(bhd_path, bhd.data, bhd.size);
    write_wfile(bdt_path, payload, sizeof(payload));

    sf_bhd5_t *reader = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_open(&reader, bhd_path, bdt_path,
                                          SF_BHD5_GAME_ELDENRING, NULL));
    TEST_ASSERT_EQUAL_size_t(4, sf_bhd5_bucket_count(reader));
    TEST_ASSERT_EQUAL_size_t(file_count, sf_bhd5_total_file_count(reader));

    for (size_t i = file_count; i > 0; i--) {
        const size_t file_index = i - 1u;
        void *out = NULL;
        size_t out_size = 0;
        TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_extract_by_hash_64(reader, files[file_index].hash,
                                                            &out, &out_size, NULL));
        TEST_ASSERT_EQUAL_size_t(1, out_size);
        TEST_ASSERT_EQUAL_UINT8(payload[file_index], ((const uint8_t *)out)[0]);
        sf_free(NULL, out);
    }

    void *missing = NULL;
    size_t missing_size = 0;
    TEST_ASSERT_EQUAL(SF_ERR_NOT_FOUND,
                      sf_bhd5_extract_by_hash_64(reader, 0xFEDCBA9876543210ull,
                                                 &missing, &missing_size, NULL));
    TEST_ASSERT_NULL(missing);
    TEST_ASSERT_EQUAL_size_t(0, missing_size);

    sf_bhd5_close(reader);
    DeleteFileW(bhd_path);
    DeleteFileW(bdt_path);
}

static void test_bhd5_bulk_aes_ranges_do_not_allocate_per_file(void) {
    enum { file_count = 32 };
    synth_file_t files[file_count];
    memset(files, 0, sizeof(files));
    for (size_t i = 0; i < file_count; i++) {
        files[i].path = "bulk/aes.bin";
        files[i].hash = sf_path_hash_64(files[i].path) + i;
        files[i].padded_size = 16;
        files[i].unpadded_size = 16;
        files[i].file_offset = (int64_t)(i * 16u);
        files[i].has_aes = true;
        memcpy(files[i].aes_key, k_key, sizeof(k_key));
        files[i].range_start = 0;
        files[i].range_end = 16;
    }
    uint32_t bucket_counts[4] = { 8, 8, 8, 8 };
    bytebuf_t bhd;
    build_bhd(&bhd, files, file_count, bucket_counts, 4);

    uint8_t bdt[file_count * 16u];
    memset(bdt, 0xA5, sizeof(bdt));
    wchar_t bhd_path[MAX_PATH];
    wchar_t bdt_path[MAX_PATH];
    make_temp_path(L"bhd5_bulk_bhd", bhd_path);
    make_temp_path(L"bhd5_bulk_bdt", bdt_path);
    write_wfile(bhd_path, bhd.data, bhd.size);
    write_wfile(bdt_path, bdt, sizeof(bdt));

    counting_allocator_t counts = {0};
    const sf_allocator_t alloc = { counting_alloc, counting_realloc, counting_free, &counts };
    sf_bhd5_t *reader = NULL;
    TEST_ASSERT_EQUAL(SF_OK, sf_bhd5_open(&reader, bhd_path, bdt_path,
                                          SF_BHD5_GAME_ELDENRING, &alloc));
    TEST_ASSERT_EQUAL_size_t(file_count, sf_bhd5_total_file_count(reader));
    TEST_ASSERT_LESS_THAN_size_t(24, counts.alloc_calls + counts.realloc_calls);
    sf_bhd5_close(reader);
    DeleteFileW(bhd_path);
    DeleteFileW(bdt_path);
}

static void test_bhd5_rsa_wrapped(void) {
    TEST_PASS_MESSAGE("Skipped: RSA-wrapped BHD5 requires a game-signed fixture/private key");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_bhd5_eldenring_style);
    RUN_TEST(test_bhd5_sekiro_style);
    RUN_TEST(test_bhd5_range_skip);
    RUN_TEST(test_bhd5_empty_bucket);
    RUN_TEST(test_bhd5_hash_lookup_matches_all_bucket_entries);
    RUN_TEST(test_bhd5_bulk_aes_ranges_do_not_allocate_per_file);
    RUN_TEST(test_bhd5_rsa_wrapped);
    return UNITY_END();
}
