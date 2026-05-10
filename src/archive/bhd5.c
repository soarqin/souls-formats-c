/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * BHD5 streaming reader. Mirrors SoulsFormatsNEXT BHD5.cs for modern v1 games:
 * parse the BHD5 index into compact metadata, keep the paired BDT as an
 * sf_istream_t, and read/decrypt only the requested file payload.
 */

#include "souls_formats/sf_bhd5.h"

#include "archive/bhd5_keys.h"
#include "crypto/aes_cng.h"
#include "crypto/rsa_cng.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_hash.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

typedef struct sf_bhd5_range {
    int64_t start_offset;
    int64_t end_offset;
} sf_bhd5_range_t;

typedef struct sf_bhd5_bucket {
    size_t count;
    size_t first_file;
} sf_bhd5_bucket_t;

typedef struct sf_bhd5_file {
    uint64_t path_hash;
    uint32_t padded_size;
    uint32_t unpadded_size;
    int64_t file_offset;

    bool has_sha_hash;
    uint8_t sha_hash[32];

    bool has_aes_key;
    uint8_t aes_key[16];
    size_t aes_range_count;
    sf_bhd5_range_t *aes_ranges;
} sf_bhd5_file_t;

struct sf_bhd5 {
    const sf_allocator_t *alloc;
    sf_bhd5_game_t game;
    bool big_endian;
    bool unk05;
    char *salt;

    sf_istream_t *bdt_stream;
    int64_t bdt_length;

    size_t bucket_count;
    sf_bhd5_bucket_t *buckets;
    size_t file_count;
    sf_bhd5_file_t *files;
};

static void bhd5_file_clear(const sf_allocator_t *a, sf_bhd5_file_t *f) {
    if (!f) return;
    sf_xfree(a, f->aes_ranges);
    memset(f, 0, sizeof(*f));
}

void sf_bhd5_close(sf_bhd5_t *b) {
    if (!b) return;
    const sf_allocator_t *a = b->alloc;
    for (size_t i = 0; i < b->file_count; i++) bhd5_file_clear(a, &b->files[i]);
    sf_xfree(a, b->files);
    sf_xfree(a, b->buckets);
    sf_xfree(a, b->salt);
    sf_istream_close(b->bdt_stream);
    sf_xfree(a, b);
}

static sf_bhd5_t *bhd5_alloc(const sf_allocator_t *a) {
    a = sf_alloc_or_default(a);
    sf_bhd5_t *b = (sf_bhd5_t *)sf_xalloc(a, sizeof(*b));
    if (!b) return NULL;
    memset(b, 0, sizeof(*b));
    b->alloc = a;
    return b;
}

static sf_result_t read_entire_wfile(const wchar_t *path, const sf_allocator_t *a,
                                     uint8_t **out, size_t *out_size) {
    SF_CHECK_ARG(path && out && out_size);
    *out = NULL;
    *out_size = 0;

    sf_istream_t *s = NULL;
    sf_result_t r = sf_istream_open_wfile(&s, path, a);
    if (r != SF_OK) return r;

    int64_t len = sf_istream_length(s);
    if (len < 0 || (uint64_t)len > (uint64_t)SIZE_MAX) {
        sf_istream_close(s);
        return SF_ERR_OUT_OF_RANGE;
    }

    uint8_t *buf = NULL;
    if (len > 0) {
        buf = (uint8_t *)sf_xalloc(a, (size_t)len);
        if (!buf) {
            sf_istream_close(s);
            return SF_ERR_OOM;
        }
        r = sf_istream_read(s, buf, (size_t)len);
        if (r != SF_OK) {
            sf_xfree(a, buf);
            sf_istream_close(s);
            return r;
        }
    }

    sf_istream_close(s);
    *out = buf;
    *out_size = (size_t)len;
    return SF_OK;
}

static bool has_bhd5_magic(const uint8_t *data, size_t size) {
    return size >= 4u && data[0] == 'B' && data[1] == 'H' && data[2] == 'D' && data[3] == '5';
}

static sf_result_t rsa_unwrap_bhd5(sf_bhd5_game_t game,
                                   const uint8_t *in, size_t in_size,
                                   uint8_t **out, size_t *out_size,
                                   const sf_allocator_t *a) {
    const char *pem = sfi_bhd5_get_pem_key(game);
    if (!pem) return SF_ERR_INVALID_ARG;
    if (in_size == 0u || (in_size % 256u) != 0u) return SF_ERR_CRYPTO;

    uint8_t *plain = NULL;
    size_t plain_size = 0;
    size_t plain_cap = 0;
    for (size_t off = 0; off < in_size; off += 256u) {
        uint8_t *chunk = NULL;
        size_t chunk_size = 0;
        sf_result_t r = sfi_rsa_decrypt_pkcs1(pem, in + off, 256u, &chunk, &chunk_size, a);
        if (r != SF_OK) {
            sf_xfree(a, plain);
            return r;
        }
        if (chunk_size > SIZE_MAX - plain_size) {
            sf_free(a, chunk);
            sf_xfree(a, plain);
            return SF_ERR_OUT_OF_RANGE;
        }
        size_t need = plain_size + chunk_size;
        if (need > plain_cap) {
            size_t next = plain_cap ? plain_cap : 512u;
            while (next < need) {
                if (next > SIZE_MAX / 2u) { next = need; break; }
                next *= 2u;
            }
            uint8_t *grown = (uint8_t *)sf_xrealloc(a, plain, plain_cap, next);
            if (!grown) {
                sf_free(a, chunk);
                sf_xfree(a, plain);
                return SF_ERR_OOM;
            }
            plain = grown;
            plain_cap = next;
        }
        memcpy(plain + plain_size, chunk, chunk_size);
        plain_size += chunk_size;
        sf_free(a, chunk);
    }

    if (!has_bhd5_magic(plain, plain_size)) {
        sf_xfree(a, plain);
        return SF_ERR_BAD_MAGIC;
    }
    *out = plain;
    *out_size = plain_size;
    return SF_OK;
}

static sf_result_t read_i32_nonnegative(sf_binary_reader_t *r, int32_t *out) {
    sf_result_t rr = sf_binary_reader_read_i32(r, out);
    if (rr != SF_OK) return rr;
    return *out < 0 ? SF_ERR_OUT_OF_RANGE : SF_OK;
}

static bool plausible_range_count(sf_binary_reader_t *r, int64_t pos, int32_t count) {
    if (count < 0 || count > 1000000) return false;
    int64_t len = sf_binary_reader_length(r);
    int64_t bytes = (int64_t)count * 16;
    return pos >= 0 && bytes >= 0 && pos <= len && bytes <= len - pos;
}

static sf_result_t read_ranges(sf_binary_reader_t *r, sf_bhd5_file_t *file,
                               int32_t range_count, const sf_allocator_t *a) {
    if (range_count == 0) return SF_OK;
    file->aes_ranges = (sf_bhd5_range_t *)sf_xalloc(a, (size_t)range_count * sizeof(*file->aes_ranges));
    if (!file->aes_ranges) return SF_ERR_OOM;
    file->aes_range_count = (size_t)range_count;
    for (int32_t i = 0; i < range_count; i++) {
        sf_result_t rr = sf_binary_reader_read_i64(r, &file->aes_ranges[i].start_offset);
        if (rr != SF_OK) return rr;
        rr = sf_binary_reader_read_i64(r, &file->aes_ranges[i].end_offset);
        if (rr != SF_OK) return rr;
    }
    return SF_OK;
}

static sf_result_t read_aes_key(sf_binary_reader_t *r, sf_bhd5_file_t *file,
                                int64_t aes_key_offset, const sf_allocator_t *a) {
    sf_result_t rr = sf_binary_reader_step_in(r, aes_key_offset);
    if (rr != SF_OK) return rr;

    rr = sf_binary_reader_read_bytes(r, file->aes_key, sizeof(file->aes_key));
    if (rr != SF_OK) goto out;
    file->has_aes_key = true;

    int32_t range_count = 0;
    rr = read_i32_nonnegative(r, &range_count);
    if (rr != SF_OK) goto out;
    if (!plausible_range_count(r, sf_binary_reader_position(r), range_count)) {
        rr = SF_ERR_TRUNCATED;
        goto out;
    }
    rr = read_ranges(r, file, range_count, a);

out:
    {
        sf_result_t step = sf_binary_reader_step_out(r);
        return rr != SF_OK ? rr : step;
    }
}

static sf_result_t read_sha_hash(sf_binary_reader_t *r, sf_bhd5_file_t *file,
                                 int64_t sha_hash_offset) {
    sf_result_t rr = sf_binary_reader_step_in(r, sha_hash_offset);
    if (rr != SF_OK) return rr;
    rr = sf_binary_reader_read_bytes(r, file->sha_hash, sizeof(file->sha_hash));
    if (rr == SF_OK) file->has_sha_hash = true;
    sf_result_t step = sf_binary_reader_step_out(r);
    return rr != SF_OK ? rr : step;
}

static sf_result_t read_file_header(sf_binary_reader_t *r, sf_bhd5_file_t *file,
                                    const sf_allocator_t *a) {
    int64_t sha_hash_offset = 0;
    int64_t aes_key_offset = 0;
    int32_t padded = 0;
    int32_t unpadded = 0;

    sf_result_t rr = sf_binary_reader_read_u64(r, &file->path_hash);
    if (rr != SF_OK) return rr;
    rr = read_i32_nonnegative(r, &padded);
    if (rr != SF_OK) return rr;
    rr = read_i32_nonnegative(r, &unpadded);
    if (rr != SF_OK) return rr;
    rr = sf_binary_reader_read_i64(r, &file->file_offset);
    if (rr != SF_OK) return rr;
    rr = sf_binary_reader_read_i64(r, &sha_hash_offset);
    if (rr != SF_OK) return rr;
    rr = sf_binary_reader_read_i64(r, &aes_key_offset);
    if (rr != SF_OK) return rr;

    if (file->file_offset < 0 || sha_hash_offset < 0 || aes_key_offset < 0) return SF_ERR_OUT_OF_RANGE;
    file->padded_size = (uint32_t)padded;
    file->unpadded_size = (uint32_t)unpadded;

    if (sha_hash_offset != 0) {
        rr = read_sha_hash(r, file, sha_hash_offset);
        if (rr != SF_OK) return rr;
    }
    if (aes_key_offset != 0) {
        rr = read_aes_key(r, file, aes_key_offset, a);
        if (rr != SF_OK) return rr;
    }
    return SF_OK;
}

static sf_result_t parse_bhd5(sf_bhd5_t *b, uint8_t *bytes, size_t size) {
    sf_binary_reader_t *r = NULL;
    int64_t *header_offsets = NULL;
    sf_result_t rr = sf_binary_reader_create_from_memory(&r, false, bytes, size, b->alloc);
    if (rr != SF_OK) return rr;
    bytes = NULL;

    rr = sf_binary_reader_assert_ascii(r, "BHD5");
    if (rr != SF_OK) goto out;

    int8_t endian = 0;
    rr = sf_binary_reader_read_i8(r, &endian);
    if (rr != SF_OK) goto out;
    if (endian == 0) b->big_endian = true;
    else if (endian == -1) b->big_endian = false;
    else { rr = SF_ERR_BAD_MAGIC; goto out; }
    sf_binary_reader_set_big_endian(r, b->big_endian);

    rr = sf_binary_reader_read_bool(r, &b->unk05);
    if (rr != SF_OK) goto out;
    rr = sf_binary_reader_assert_u8_one(r, 0);
    if (rr != SF_OK) goto out;
    rr = sf_binary_reader_assert_u8_one(r, 0);
    if (rr != SF_OK) goto out;
    rr = sf_binary_reader_assert_i32_one(r, 1);
    if (rr != SF_OK) goto out;
    int32_t ignored_file_size = 0;
    rr = sf_binary_reader_read_i32(r, &ignored_file_size);
    if (rr != SF_OK) goto out;

    bool is64_bit = false;
    if (sf_binary_reader_length(r) > 0x28) {
        int32_t test0 = 0;
        int32_t test1 = 0;
        if (sf_binary_reader_get_i32(r, 0x14, &test0) == SF_OK &&
            sf_binary_reader_get_i32(r, 0x1C, &test1) == SF_OK &&
            test0 == 0 && test1 == 0) {
            is64_bit = true;
        }
    }

    int64_t bucket_count = 0;
    int64_t buckets_offset = 0;
    if (is64_bit) {
        rr = sf_binary_reader_read_i64(r, &bucket_count);
        if (rr != SF_OK) goto out;
        rr = sf_binary_reader_read_i64(r, &buckets_offset);
        if (rr != SF_OK) goto out;
    } else {
        int32_t bc32 = 0;
        int32_t bo32 = 0;
        rr = read_i32_nonnegative(r, &bc32);
        if (rr != SF_OK) goto out;
        rr = read_i32_nonnegative(r, &bo32);
        if (rr != SF_OK) goto out;
        bucket_count = bc32;
        buckets_offset = bo32;
    }
    if (bucket_count < 0 || buckets_offset < 0 || (uint64_t)bucket_count > (uint64_t)SIZE_MAX) {
        rr = SF_ERR_OUT_OF_RANGE;
        goto out;
    }

    int32_t salt_len = 0;
    rr = read_i32_nonnegative(r, &salt_len);
    if (rr != SF_OK) goto out;
    char *salt = NULL;
    size_t salt_size = 0;
    rr = sf_binary_reader_read_ascii_n(r, (size_t)salt_len, &salt, &salt_size);
    (void)salt_size;
    if (rr != SF_OK) goto out;
    b->salt = salt;

    b->bucket_count = (size_t)bucket_count;
    if (b->bucket_count > 0) {
        b->buckets = (sf_bhd5_bucket_t *)sf_xalloc(b->alloc, b->bucket_count * sizeof(*b->buckets));
        if (!b->buckets) { rr = SF_ERR_OOM; goto out; }
        memset(b->buckets, 0, b->bucket_count * sizeof(*b->buckets));
    }

    if (b->bucket_count > 0) {
        header_offsets = (int64_t *)sf_xalloc(b->alloc, b->bucket_count * sizeof(*header_offsets));
        if (!header_offsets) { rr = SF_ERR_OOM; goto out; }
    }

    rr = sf_binary_reader_step_in(r, buckets_offset);
    if (rr != SF_OK) goto out;
    size_t total = 0;
    for (size_t i = 0; i < b->bucket_count; i++) {
        int32_t count = 0;
        rr = read_i32_nonnegative(r, &count);
        if (rr != SF_OK) goto out;
        if (is64_bit) {
            int32_t unknown_flag = 0;
            rr = sf_binary_reader_read_i32(r, &unknown_flag);
            if (rr != SF_OK) goto out;
            if (unknown_flag != 1) { rr = SF_ERR_BAD_MAGIC; goto out; }
            rr = sf_binary_reader_read_i64(r, &header_offsets[i]);
            if (rr != SF_OK) goto out;
        } else {
            int32_t off32 = 0;
            rr = read_i32_nonnegative(r, &off32);
            if (rr != SF_OK) goto out;
            header_offsets[i] = off32;
        }
        if (header_offsets[i] < 0) { rr = SF_ERR_OUT_OF_RANGE; goto out; }
        b->buckets[i].count = (size_t)count;
        b->buckets[i].first_file = total;
        if ((size_t)count > SIZE_MAX - total) { rr = SF_ERR_OUT_OF_RANGE; goto out; }
        total += (size_t)count;
    }
    rr = sf_binary_reader_step_out(r);
    if (rr != SF_OK) goto out;

    b->file_count = total;
    if (total > 0) {
        b->files = (sf_bhd5_file_t *)sf_xalloc(b->alloc, total * sizeof(*b->files));
        if (!b->files) { rr = SF_ERR_OOM; goto out; }
        memset(b->files, 0, total * sizeof(*b->files));
    }

    for (size_t i = 0; i < b->bucket_count; i++) {
        if (b->buckets[i].count == 0) continue;
        rr = sf_binary_reader_step_in(r, header_offsets[i]);
        if (rr != SF_OK) goto out;
        for (size_t j = 0; j < b->buckets[i].count; j++) {
            rr = read_file_header(r, &b->files[b->buckets[i].first_file + j], b->alloc);
            if (rr != SF_OK) goto out;
        }
        rr = sf_binary_reader_step_out(r);
        if (rr != SF_OK) goto out;
    }

out:
    sf_xfree(b->alloc, header_offsets);
    sf_binary_reader_destroy(r);
    return rr;
}

sf_result_t sf_bhd5_open(sf_bhd5_t **out, const wchar_t *bhd_path, const wchar_t *bdt_path,
                         sf_bhd5_game_t game, const sf_allocator_t *a) {
    SF_CHECK_ARG(out && bhd_path && bdt_path);
    *out = NULL;
    if (game < 0 || game >= SF_BHD5_GAME_COUNT_) return SF_ERR_INVALID_ARG;

    a = sf_alloc_or_default(a);
    uint8_t *bytes = NULL;
    size_t size = 0;
    sf_result_t rr = read_entire_wfile(bhd_path, a, &bytes, &size);
    if (rr != SF_OK) return rr;
    if (!has_bhd5_magic(bytes, size)) {
        uint8_t *plain = NULL;
        size_t plain_size = 0;
        rr = rsa_unwrap_bhd5(game, bytes, size, &plain, &plain_size, a);
        sf_xfree(a, bytes);
        if (rr != SF_OK) return rr;
        bytes = plain;
        size = plain_size;
    }

    sf_bhd5_t *b = bhd5_alloc(a);
    if (!b) {
        sf_xfree(a, bytes);
        return SF_ERR_OOM;
    }
    b->game = game;

    rr = parse_bhd5(b, bytes, size);
    bytes = NULL;
    if (rr != SF_OK) {
        sf_bhd5_close(b);
        return rr;
    }

    rr = sf_istream_open_wfile(&b->bdt_stream, bdt_path, a);
    if (rr != SF_OK) {
        sf_bhd5_close(b);
        return rr;
    }
    b->bdt_length = sf_istream_length(b->bdt_stream);
    *out = b;
    return SF_OK;
}

size_t sf_bhd5_bucket_count(const sf_bhd5_t *b) { return b ? b->bucket_count : 0u; }
size_t sf_bhd5_total_file_count(const sf_bhd5_t *b) { return b ? b->file_count : 0u; }
const char *sf_bhd5_get_salt(const sf_bhd5_t *b) { return (b && b->salt) ? b->salt : ""; }
bool sf_bhd5_get_big_endian(const sf_bhd5_t *b) { return b ? b->big_endian : false; }

static const sf_bhd5_file_t *find_by_hash64(const sf_bhd5_t *b, uint64_t path_hash) {
    if (!b) return NULL;
    for (size_t i = 0; i < b->file_count; i++) {
        if (b->files[i].path_hash == path_hash) return &b->files[i];
    }
    return NULL;
}

static sf_result_t decrypt_ranges(const sf_bhd5_t *b, const sf_bhd5_file_t *f,
                                  uint8_t *buf, size_t n) {
    if (!f->has_aes_key) return SF_OK;
    for (size_t i = 0; i < f->aes_range_count; i++) {
        int64_t start = f->aes_ranges[i].start_offset;
        int64_t end = f->aes_ranges[i].end_offset;
        if (start == end) continue;
        if (start < 0 || end < start) return SF_ERR_OUT_OF_RANGE;

        int64_t rel_start = start;
        int64_t rel_end = end;
        if ((uint64_t)end > (uint64_t)n) {
            if (start < f->file_offset || end < f->file_offset) return SF_ERR_TRUNCATED;
            rel_start = start - f->file_offset;
            rel_end = end - f->file_offset;
        }
        if (rel_start < 0 || rel_end < rel_start || (uint64_t)rel_end > (uint64_t)n) {
            return SF_ERR_TRUNCATED;
        }
        if (end > b->bdt_length && rel_end + f->file_offset > b->bdt_length) {
            return SF_ERR_TRUNCATED;
        }
        size_t offset = (size_t)rel_start;
        size_t size = (size_t)(rel_end - rel_start);
        sf_result_t rr = sfi_aes_decrypt_ecb_buffer(f->aes_key, buf + offset, size);
        if (rr != SF_OK) return rr;
    }
    return SF_OK;
}

static sf_result_t extract_file(const sf_bhd5_t *b, const sf_bhd5_file_t *f,
                                void **out, size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG(b && f && out && out_size);
    *out = NULL;
    *out_size = 0;
    a = sf_alloc_or_default(a);

    if (f->file_offset < 0 || (uint64_t)f->padded_size > (uint64_t)(b->bdt_length - f->file_offset)) {
        return SF_ERR_TRUNCATED;
    }

    uint8_t *buf = NULL;
    if (f->padded_size > 0) {
        buf = (uint8_t *)sf_xalloc(a, f->padded_size);
        if (!buf) return SF_ERR_OOM;
    }

    sf_result_t rr = sf_istream_seek(b->bdt_stream, f->file_offset);
    if (rr == SF_OK && f->padded_size > 0) rr = sf_istream_read(b->bdt_stream, buf, f->padded_size);
    if (rr == SF_OK) rr = decrypt_ranges(b, f, buf, f->padded_size);
    if (rr != SF_OK) {
        sf_xfree(a, buf);
        return rr;
    }

    *out = buf;
    *out_size = f->padded_size;
    return SF_OK;
}

sf_result_t sf_bhd5_extract_by_hash_64(const sf_bhd5_t *b, uint64_t path_hash,
                                       void **out, size_t *out_size,
                                       const sf_allocator_t *a) {
    const sf_bhd5_file_t *f = find_by_hash64(b, path_hash);
    if (!f) return SF_ERR_NOT_FOUND;
    return extract_file(b, f, out, out_size, a);
}

sf_result_t sf_bhd5_extract_by_hash_32(const sf_bhd5_t *b, uint32_t path_hash,
                                       void **out, size_t *out_size,
                                       const sf_allocator_t *a) {
    return sf_bhd5_extract_by_hash_64(b, (uint64_t)path_hash, out, out_size, a);
}

sf_result_t sf_bhd5_extract_by_path(const sf_bhd5_t *b, const char *utf8_path,
                                    void **out, size_t *out_size,
                                    const sf_allocator_t *a) {
    SF_CHECK_ARG(utf8_path != NULL);
    return sf_bhd5_extract_by_hash_64(b, sf_path_hash_64(utf8_path), out, out_size, a);
}

static sf_result_t write_file_headers(sf_binary_writer_t *w, const sf_bhd5_t *b,
                                      const int64_t *sha_offsets, const int64_t *aes_offsets) {
    for (size_t i = 0; i < b->file_count; i++) {
        const sf_bhd5_file_t *f = &b->files[i];
        sf_result_t rr = sf_binary_writer_write_u64(w, f->path_hash);
        if (rr != SF_OK) return rr;
        rr = sf_binary_writer_write_u32(w, f->padded_size);
        if (rr != SF_OK) return rr;
        rr = sf_binary_writer_write_u32(w, f->unpadded_size);
        if (rr != SF_OK) return rr;
        rr = sf_binary_writer_write_i64(w, f->file_offset);
        if (rr != SF_OK) return rr;
        rr = sf_binary_writer_write_i64(w, sha_offsets[i]);
        if (rr != SF_OK) return rr;
        rr = sf_binary_writer_write_i64(w, aes_offsets[i]);
        if (rr != SF_OK) return rr;
    }
    return SF_OK;
}

sf_result_t sf_bhd5_write(const sf_bhd5_t *b, const wchar_t *bhd_path) {
    SF_CHECK_ARG(b && bhd_path);
    if (b->bucket_count > (size_t)(INT64_MAX / 16)) return SF_ERR_OUT_OF_RANGE;

    const char *salt = b->salt ? b->salt : "";
    size_t salt_len = strlen(salt);
    if (salt_len > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    int64_t buckets_offset = 0x24 + (int64_t)salt_len;
    int64_t headers_offset = buckets_offset + (int64_t)b->bucket_count * 16;
    int64_t metadata_offset = headers_offset + (int64_t)b->file_count * 40;
    int64_t cursor = metadata_offset;

    int64_t *header_offsets = NULL;
    int64_t *sha_offsets = NULL;
    int64_t *aes_offsets = NULL;
    if (b->bucket_count > 0) {
        header_offsets = (int64_t *)sf_xalloc(b->alloc, b->bucket_count * sizeof(*header_offsets));
        if (!header_offsets) return SF_ERR_OOM;
    }
    if (b->file_count > 0) {
        sha_offsets = (int64_t *)sf_xalloc(b->alloc, b->file_count * sizeof(*sha_offsets));
        aes_offsets = (int64_t *)sf_xalloc(b->alloc, b->file_count * sizeof(*aes_offsets));
        if (!sha_offsets || !aes_offsets) {
            sf_xfree(b->alloc, header_offsets);
            sf_xfree(b->alloc, sha_offsets);
            sf_xfree(b->alloc, aes_offsets);
            return SF_ERR_OOM;
        }
        memset(sha_offsets, 0, b->file_count * sizeof(*sha_offsets));
        memset(aes_offsets, 0, b->file_count * sizeof(*aes_offsets));
    }

    int64_t hcur = headers_offset;
    for (size_t i = 0; i < b->bucket_count; i++) {
        header_offsets[i] = hcur;
        hcur += (int64_t)b->buckets[i].count * 40;
    }
    for (size_t i = 0; i < b->file_count; i++) {
        if (b->files[i].has_sha_hash) {
            sha_offsets[i] = cursor;
            cursor += 36;
        }
        if (b->files[i].has_aes_key) {
            aes_offsets[i] = cursor;
            cursor += 20 + (int64_t)b->files[i].aes_range_count * 16;
        }
    }
    if (cursor > INT32_MAX) {
        sf_xfree(b->alloc, header_offsets);
        sf_xfree(b->alloc, sha_offsets);
        sf_xfree(b->alloc, aes_offsets);
        return SF_ERR_OUT_OF_RANGE;
    }

    sf_ostream_t *os = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t rr = sf_ostream_open_wfile(&os, bhd_path, b->alloc);
    if (rr == SF_OK) rr = sf_binary_writer_create(&w, os, b->big_endian, b->alloc);
    if (rr != SF_OK) goto out;

    rr = sf_binary_writer_write_bytes(w, "BHD5", 4);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i8(w, b->big_endian ? 0 : -1);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_bool(w, b->unk05);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_u8(w, 0);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_u8(w, 0);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i32(w, 1);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i32(w, (int32_t)cursor);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i64(w, (int64_t)b->bucket_count);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i64(w, buckets_offset);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_i32(w, (int32_t)salt_len);
    if (rr != SF_OK) goto out;
    rr = sf_binary_writer_write_bytes(w, salt, salt_len);
    if (rr != SF_OK) goto out;

    for (size_t i = 0; i < b->bucket_count; i++) {
        rr = sf_binary_writer_write_i32(w, (int32_t)b->buckets[i].count);
        if (rr != SF_OK) goto out;
        rr = sf_binary_writer_write_i32(w, 1);
        if (rr != SF_OK) goto out;
        rr = sf_binary_writer_write_i64(w, header_offsets[i]);
        if (rr != SF_OK) goto out;
    }
    rr = write_file_headers(w, b, sha_offsets, aes_offsets);
    if (rr != SF_OK) goto out;
    for (size_t i = 0; i < b->file_count; i++) {
        const sf_bhd5_file_t *f = &b->files[i];
        if (f->has_sha_hash) {
            rr = sf_binary_writer_write_bytes(w, f->sha_hash, sizeof(f->sha_hash));
            if (rr != SF_OK) goto out;
            rr = sf_binary_writer_write_i32(w, 0);
            if (rr != SF_OK) goto out;
        }
        if (f->has_aes_key) {
            rr = sf_binary_writer_write_bytes(w, f->aes_key, sizeof(f->aes_key));
            if (rr != SF_OK) goto out;
            rr = sf_binary_writer_write_i32(w, (int32_t)f->aes_range_count);
            if (rr != SF_OK) goto out;
            for (size_t j = 0; j < f->aes_range_count; j++) {
                rr = sf_binary_writer_write_i64(w, f->aes_ranges[j].start_offset);
                if (rr != SF_OK) goto out;
                rr = sf_binary_writer_write_i64(w, f->aes_ranges[j].end_offset);
                if (rr != SF_OK) goto out;
            }
        }
    }
    rr = sf_binary_writer_finish(w);
    w = NULL;

out:
    sf_binary_writer_destroy(w);
    sf_ostream_close(os);
    sf_xfree(b->alloc, header_offsets);
    sf_xfree(b->alloc, sha_offsets);
    sf_xfree(b->alloc, aes_offsets);
    return rr;
}
