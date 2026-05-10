/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — internal helpers shared across BND3/BND4/BXF3/BXF4.
 *
 * Implements upstream's
 *   - SoulsFormats/Formats/Binder/Binder.cs                ReadFormat / WriteFormat / *FileFlags
 *   - SoulsFormats/Formats/Binder/BinderFileHeader.cs      Read/Write BND3/BND4 file headers
 *   - SoulsFormats/Formats/Binder/BinderHashTable.cs       Assert / Write hash table
 */

#include "archive/binder_common.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Format byte (Binder.cs:65-83)
 *
 * Read:  reverse = bitBigEndian || ((raw & 1) != 0 && (raw & 0x80) == 0);
 *        return reverse ? raw : ReverseBits(raw);
 *
 * Write: reverse = bitBigEndian || (ForceBigEndian(f) && (f & Flag6) != 0);
 *        write   reverse ? f   : ReverseBits(f);
 *===========================================================================*/

sf_binder_format_t sfi_binder_read_format(sf_binary_reader_t *br, bool bit_big_endian) {
    if (!br) return SF_BINDER_FORMAT_NONE;
    uint8_t raw = 0;
    if (sf_binary_reader_read_u8(br, &raw) != SF_OK) return SF_BINDER_FORMAT_NONE;
    bool reverse = bit_big_endian || (((raw & 0x01u) != 0) && ((raw & 0x80u) == 0));
    return (sf_binder_format_t)(reverse ? raw : sf_reverse_bits_u8(raw));
}

void sfi_binder_write_format(sf_binary_writer_t *bw, sf_binder_format_t f, bool bit_big_endian) {
    if (!bw) return;
    bool reverse = bit_big_endian
                || (sf_binder_format_force_big_endian(f) && sf_binder_format_has_flag6(f));
    uint8_t raw = reverse ? (uint8_t)f : sf_reverse_bits_u8((uint8_t)f);
    (void)sf_binary_writer_write_u8(bw, raw);
}

/*===========================================================================
 * FileFlags byte (Binder.cs:185-203)
 *
 *   reverse = bitBigEndian; bit-reverse iff !reverse
 *===========================================================================*/

sf_binder_file_flags_t sfi_binder_read_file_flags(sf_binary_reader_t *br,
                                                  bool                bit_big_endian) {
    if (!br) return SF_BINDER_FILE_FLAG_NONE;
    uint8_t raw = 0;
    if (sf_binary_reader_read_u8(br, &raw) != SF_OK) return SF_BINDER_FILE_FLAG_NONE;
    return (sf_binder_file_flags_t)(bit_big_endian ? raw : sf_reverse_bits_u8(raw));
}

void sfi_binder_write_file_flags(sf_binary_writer_t   *bw,
                                 sf_binder_file_flags_t flags,
                                 bool                   bit_big_endian) {
    if (!bw) return;
    uint8_t raw = bit_big_endian ? (uint8_t)flags : sf_reverse_bits_u8((uint8_t)flags);
    (void)sf_binary_writer_write_u8(bw, raw);
}

/*===========================================================================
 * BND4 file header byte size (Binder.cs:123-131)
 *===========================================================================*/

size_t sfi_binder_get_bnd4_file_header_size(sf_binder_format_t f) {
    size_t total = 0x10;
    total += sf_binder_format_has_long_offsets(f) ? 8u : 4u;
    if (sf_binder_format_has_compression(f)) total += 8u;
    if (sf_binder_format_has_ids(f))         total += 4u;
    if (sf_binder_format_has_names1(f) || sf_binder_format_has_names2(f)) total += 4u;
    if (f == SF_BINDER_FORMAT_NAMES1)        total += 8u;
    return total;
}

/*===========================================================================
 * sfi_binder_file_header_t lifetime
 *===========================================================================*/

void sfi_binder_file_header_destroy(sfi_binder_file_header_t *h, const sf_allocator_t *a) {
    if (!h) return;
    sf_xfree(a, h->name_utf8);
    h->name_utf8 = NULL;
}

static void sfi_binder_file_header_init(sfi_binder_file_header_t *h) {
    memset(h, 0, sizeof(*h));
    h->id = -1;
    h->compression_info.type = SF_DCX_TYPE_ZLIB;
}

/*===========================================================================
 * BND3 file header read/write (BinderFileHeader.cs:79-110, 178-200, 253-266)
 *
 * Layout (LE; BE is the same with byte order swapped on each field):
 *   byte    flags
 *   byte[3] {0,0,0}
 *   int32   compressedSize
 *   int32   dataOffset                     (or int64 if LongOffsets)
 *   int32   id                              (only if HasIDs)
 *   int32   nameOffset                      (only if HasNames; absolute
 *                                            offset to a Shift-JIS NUL-
 *                                            terminated string)
 *   int32   uncompressedSize                (only if HasCompression)
 *===========================================================================*/

sf_result_t sfi_binder3_read_file_header(sf_binary_reader_t        *br,
                                         sf_binder_format_t         f,
                                         bool                       bit_big_endian,
                                         sfi_binder_file_header_t  *out,
                                         const sf_allocator_t      *a) {
    SF_CHECK_ARG(br != NULL);
    SF_CHECK_ARG(out != NULL);

    sfi_binder_file_header_init(out);

    out->flags = sfi_binder_read_file_flags(br, bit_big_endian);

    sf_result_t r;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0); if (r != SF_OK) return r;

    int32_t compressed_size = 0;
    r = sf_binary_reader_read_i32(br, &compressed_size); if (r != SF_OK) return r;
    out->compressed_size = (uint64_t)(uint32_t)compressed_size;

    if (sf_binder_format_has_long_offsets(f)) {
        int64_t off = 0;
        r = sf_binary_reader_read_i64(br, &off); if (r != SF_OK) return r;
        out->data_offset = (uint64_t)off;
    } else {
        uint32_t off = 0;
        r = sf_binary_reader_read_u32(br, &off); if (r != SF_OK) return r;
        out->data_offset = (uint64_t)off;
    }

    if (sf_binder_format_has_ids(f)) {
        r = sf_binary_reader_read_i32(br, &out->id); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_names1(f) || sf_binder_format_has_names2(f)) {
        int32_t name_off = 0;
        r = sf_binary_reader_read_i32(br, &name_off); if (r != SF_OK) return r;
        char *name = NULL;
        r = sf_binary_reader_get_shift_jis(br, name_off, &name, NULL);
        if (r != SF_OK) return r;
        if (a && a != sf_default_allocator()) {
            char *copy = sf_strdup(a, name);
            sf_free(NULL, name);
            if (!copy) return SF_ERR_OOM;
            out->name_utf8 = copy;
        } else {
            out->name_utf8 = name;
        }
    }

    if (sf_binder_format_has_compression(f)) {
        int32_t uncompressed = 0;
        r = sf_binary_reader_read_i32(br, &uncompressed); if (r != SF_OK) return r;
        out->uncompressed_size = (uint64_t)(uint32_t)uncompressed;
    }

    return SF_OK;
}

static int sfi_format_index_name(char *buf, size_t cap, const char *prefix, size_t i) {
    return snprintf(buf, cap, "%s%zu", prefix, i);
}

sf_result_t sfi_binder3_write_file_header(sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          bool                             bit_big_endian,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    sfi_binder_write_file_flags(bw, h->flags, bit_big_endian);

    sf_result_t r;
    r = sf_binary_writer_write_u8(bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8(bw, 0); if (r != SF_OK) return r;

    char name[64];

    sfi_format_index_name(name, sizeof name, "FileCompressedSize", entry_index);
    r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;

    if (sf_binder_format_has_long_offsets(f)) {
        sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
        r = sf_binary_writer_reserve_i64(bw, name); if (r != SF_OK) return r;
    } else {
        sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
        r = sf_binary_writer_reserve_u32(bw, name); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_ids(f)) {
        r = sf_binary_writer_write_i32(bw, h->id); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_names1(f) || sf_binder_format_has_names2(f)) {
        sfi_format_index_name(name, sizeof name, "FileNameOffset", entry_index);
        r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_compression(f)) {
        sfi_format_index_name(name, sizeof name, "FileUncompressedSize", entry_index);
        r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;
    }

    return SF_OK;
}

/*===========================================================================
 * write_file_data — internal worker shared by BND3/BND4 (BinderFileHeader.cs:233-251)
 *
 *   if (raw_size > 0)  bw.Pad(0x10);
 *   data_offset = bw.Position;
 *   if (Compressed)    write DCX.Compress(raw, info), compressed_size = ...
 *   else               write raw,                     compressed_size = raw_size
 *===========================================================================*/

static sf_result_t sfi_write_file_data_core(sf_binary_writer_t             *bw,
                                            const sfi_binder_file_header_t *h,
                                            const uint8_t                  *raw,
                                            uint64_t                       *out_data_offset,
                                            uint64_t                       *out_compressed_size) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);
    SF_CHECK_ARG(out_data_offset != NULL);
    SF_CHECK_ARG(out_compressed_size != NULL);

    size_t raw_size = (size_t)h->uncompressed_size;
    if (raw_size > 0) {
        SF_CHECK_ARG(raw != NULL);
        sf_result_t r = sf_binary_writer_pad(bw, 0x10);
        if (r != SF_OK) return r;
    }

    *out_data_offset = (uint64_t)sf_binary_writer_position(bw);

    if ((h->flags & SF_BINDER_FILE_FLAG_COMPRESSED) != 0) {
        const sf_allocator_t *a = sfi_ostream_allocator(sf_binary_writer_stream(bw));
        uint8_t *compressed = NULL;
        size_t   compressed_size = 0;
        sf_result_t r = sf_dcx_compress_to_buffer(raw, raw_size,
                                                  &h->compression_info,
                                                  &compressed, &compressed_size, a);
        if (r != SF_OK) return r;
        r = sf_binary_writer_write_bytes(bw, compressed, compressed_size);
        sf_free(a, compressed);
        if (r != SF_OK) return r;
        *out_compressed_size = (uint64_t)compressed_size;
    } else {
        if (raw_size > 0) {
            sf_result_t r = sf_binary_writer_write_bytes(bw, raw, raw_size);
            if (r != SF_OK) return r;
        }
        *out_compressed_size = (uint64_t)raw_size;
    }

    return SF_OK;
}

sf_result_t sfi_binder3_write_file_data(sf_binary_writer_t              *bw,
                                        sf_binder_format_t               f,
                                        const sfi_binder_file_header_t  *h,
                                        const uint8_t                   *raw,
                                        size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    uint64_t data_offset = 0;
    uint64_t compressed  = 0;
    sf_result_t r = sfi_write_file_data_core(bw, h, raw, &data_offset, &compressed);
    if (r != SF_OK) return r;

    char name[64];
    sfi_format_index_name(name, sizeof name, "FileCompressedSize", entry_index);
    r = sf_binary_writer_fill_i32(bw, name, (int32_t)compressed); if (r != SF_OK) return r;

    if (sf_binder_format_has_compression(f)) {
        sfi_format_index_name(name, sizeof name, "FileUncompressedSize", entry_index);
        r = sf_binary_writer_fill_i32(bw, name, (int32_t)h->uncompressed_size);
        if (r != SF_OK) return r;
    }

    sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
    if (sf_binder_format_has_long_offsets(f)) {
        r = sf_binary_writer_fill_i64(bw, name, (int64_t)data_offset);
    } else {
        r = sf_binary_writer_fill_u32(bw, name, (uint32_t)data_offset);
    }
    return r;
}

sf_result_t sfi_binder3_write_file_name(sf_binary_writer_t              *bw,
                                        sf_binder_format_t               f,
                                        const sfi_binder_file_header_t  *h,
                                        size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    if (!sf_binder_format_has_names1(f) && !sf_binder_format_has_names2(f)) return SF_OK;

    char name[64];
    sfi_format_index_name(name, sizeof name, "FileNameOffset", entry_index);
    sf_result_t r = sf_binary_writer_fill_i32(bw, name,
                                              (int32_t)sf_binary_writer_position(bw));
    if (r != SF_OK) return r;
    return sf_binary_writer_write_shift_jis(bw, h->name_utf8 ? h->name_utf8 : "", true);
}

/*===========================================================================
 * BND4 file header read/write (BinderFileHeader.cs:112-156, 202-231, 268-281)
 *
 * Layout:
 *   byte    flags
 *   byte[3] {0,0,0}
 *   int32   -1
 *   int64   compressedSize
 *   int64   uncompressedSize                (only if HasCompression)
 *   int64   dataOffset                      (or uint32 if !LongOffsets)
 *   int32   id                              (only if HasIDs)
 *   uint32  nameOffset                      (only if HasNames; UTF-16 if
 *                                            unicode else Shift-JIS)
 *   int32   id                              (only if format == Names1)
 *   int32   0                               (only if format == Names1)
 *===========================================================================*/

sf_result_t sfi_binder4_read_file_header(sf_binary_reader_t        *br,
                                         sf_binder_format_t         f,
                                         bool                       bit_big_endian,
                                         bool                       unicode,
                                         sfi_binder_file_header_t  *out,
                                         const sf_allocator_t      *a) {
    SF_CHECK_ARG(br != NULL);
    SF_CHECK_ARG(out != NULL);

    sfi_binder_file_header_init(out);

    out->flags = sfi_binder_read_file_flags(br, bit_big_endian);

    sf_result_t r;
    r = sf_binary_reader_assert_u8_one (br, 0);  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one (br, 0);  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one (br, 0);  if (r != SF_OK) return r;
    r = sf_binary_reader_assert_i32_one(br, -1); if (r != SF_OK) return r;

    int64_t compressed = 0;
    r = sf_binary_reader_read_i64(br, &compressed); if (r != SF_OK) return r;
    out->compressed_size = (uint64_t)compressed;

    if (sf_binder_format_has_compression(f)) {
        int64_t uncompressed = 0;
        r = sf_binary_reader_read_i64(br, &uncompressed); if (r != SF_OK) return r;
        out->uncompressed_size = (uint64_t)uncompressed;
    }

    if (sf_binder_format_has_long_offsets(f)) {
        int64_t off = 0;
        r = sf_binary_reader_read_i64(br, &off); if (r != SF_OK) return r;
        out->data_offset = (uint64_t)off;
    } else {
        uint32_t off = 0;
        r = sf_binary_reader_read_u32(br, &off); if (r != SF_OK) return r;
        out->data_offset = (uint64_t)off;
    }

    if (sf_binder_format_has_ids(f)) {
        r = sf_binary_reader_read_i32(br, &out->id); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_names1(f) || sf_binder_format_has_names2(f)) {
        uint32_t name_off = 0;
        r = sf_binary_reader_read_u32(br, &name_off); if (r != SF_OK) return r;
        char *name = NULL;
        if (unicode) {
            r = sf_binary_reader_get_utf16    (br, (int64_t)name_off, &name, NULL);
        } else {
            r = sf_binary_reader_get_shift_jis(br, (int64_t)name_off, &name, NULL);
        }
        if (r != SF_OK) return r;
        if (a && a != sf_default_allocator()) {
            char *copy = sf_strdup(a, name);
            sf_free(NULL, name);
            if (!copy) return SF_ERR_OOM;
            out->name_utf8 = copy;
        } else {
            out->name_utf8 = name;
        }
    }

    if (f == SF_BINDER_FORMAT_NAMES1) {
        r = sf_binary_reader_read_i32(br, &out->id); if (r != SF_OK) return r;
        r = sf_binary_reader_assert_i32_one(br, 0);  if (r != SF_OK) return r;
    }

    return SF_OK;
}

sf_result_t sfi_binder4_write_file_header(sf_binary_writer_t              *bw,
                                          sf_binder_format_t               f,
                                          bool                             bit_big_endian,
                                          const sfi_binder_file_header_t  *h,
                                          size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    sfi_binder_write_file_flags(bw, h->flags, bit_big_endian);

    sf_result_t r;
    r = sf_binary_writer_write_u8 (bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8 (bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_u8 (bw, 0); if (r != SF_OK) return r;
    r = sf_binary_writer_write_i32(bw, -1); if (r != SF_OK) return r;

    char name[64];

    sfi_format_index_name(name, sizeof name, "FileCompressedSize", entry_index);
    r = sf_binary_writer_reserve_i64(bw, name); if (r != SF_OK) return r;

    if (sf_binder_format_has_compression(f)) {
        sfi_format_index_name(name, sizeof name, "FileUncompressedSize", entry_index);
        r = sf_binary_writer_reserve_i64(bw, name); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_long_offsets(f)) {
        sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
        r = sf_binary_writer_reserve_i64(bw, name); if (r != SF_OK) return r;
    } else {
        sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
        r = sf_binary_writer_reserve_u32(bw, name); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_ids(f)) {
        r = sf_binary_writer_write_i32(bw, h->id); if (r != SF_OK) return r;
    }

    if (sf_binder_format_has_names1(f) || sf_binder_format_has_names2(f)) {
        sfi_format_index_name(name, sizeof name, "FileNameOffset", entry_index);
        r = sf_binary_writer_reserve_i32(bw, name); if (r != SF_OK) return r;
    }

    if (f == SF_BINDER_FORMAT_NAMES1) {
        r = sf_binary_writer_write_i32(bw, h->id); if (r != SF_OK) return r;
        r = sf_binary_writer_write_i32(bw, 0);     if (r != SF_OK) return r;
    }

    return SF_OK;
}

sf_result_t sfi_binder4_write_file_data(sf_binary_writer_t              *bw,
                                        sf_binder_format_t               f,
                                        const sfi_binder_file_header_t  *h,
                                        const uint8_t                   *raw,
                                        size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    uint64_t data_offset = 0;
    uint64_t compressed  = 0;
    sf_result_t r = sfi_write_file_data_core(bw, h, raw, &data_offset, &compressed);
    if (r != SF_OK) return r;

    char name[64];
    sfi_format_index_name(name, sizeof name, "FileCompressedSize", entry_index);
    r = sf_binary_writer_fill_i64(bw, name, (int64_t)compressed); if (r != SF_OK) return r;

    if (sf_binder_format_has_compression(f)) {
        sfi_format_index_name(name, sizeof name, "FileUncompressedSize", entry_index);
        r = sf_binary_writer_fill_i64(bw, name, (int64_t)h->uncompressed_size);
        if (r != SF_OK) return r;
    }

    sfi_format_index_name(name, sizeof name, "FileDataOffset", entry_index);
    if (sf_binder_format_has_long_offsets(f)) {
        r = sf_binary_writer_fill_i64(bw, name, (int64_t)data_offset);
    } else {
        r = sf_binary_writer_fill_u32(bw, name, (uint32_t)data_offset);
    }
    return r;
}

sf_result_t sfi_binder4_write_file_name(sf_binary_writer_t              *bw,
                                        sf_binder_format_t               f,
                                        bool                             unicode,
                                        const sfi_binder_file_header_t  *h,
                                        size_t                           entry_index) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(h  != NULL);

    if (!sf_binder_format_has_names1(f) && !sf_binder_format_has_names2(f)) return SF_OK;

    char name[64];
    sfi_format_index_name(name, sizeof name, "FileNameOffset", entry_index);
    sf_result_t r = sf_binary_writer_fill_i32(bw, name,
                                              (int32_t)sf_binary_writer_position(bw));
    if (r != SF_OK) return r;

    const char *src = h->name_utf8 ? h->name_utf8 : "";
    if (unicode) return sf_binary_writer_write_utf16    (bw, src, true);
    else         return sf_binary_writer_write_shift_jis(bw, src, true);
}

/*===========================================================================
 * Hash table (BinderHashTable.cs)
 *
 * Group count selection mirrors the upstream loop verbatim:
 *
 *   for (uint p = files.Count / 7; p <= 100000; p++)
 *       if (HashHelper.IsPrime(p)) { groupCount = p; break; }
 *
 * NB: upstream uses INTEGER division (i.e. floor), not ceil. The starting
 * value can therefore be 0 for empty/small lists, but IsPrime(0) and
 * IsPrime(1) both return false so the loop walks up to 2 in that case.
 *===========================================================================*/

uint32_t sfi_binder_hash_table_group_count(size_t file_count) {
    uint32_t start = (uint32_t)(file_count / 7);
    for (uint32_t p = start; p <= 100000u; p++) {
        if (sf_is_prime(p)) return p;
    }
    return 0;
}

sf_result_t sfi_binder_hash_table_assert(sf_binary_reader_t *br, size_t file_count) {
    SF_CHECK_ARG(br != NULL);
    (void)file_count;

    int64_t  hashes_offset = 0;
    int32_t  bucket_count  = 0;

    sf_result_t r;
    r = sf_binary_reader_read_i64(br, &hashes_offset); if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &bucket_count);  if (r != SF_OK) return r;
    (void)hashes_offset;
    (void)bucket_count;
    r = sf_binary_reader_assert_u8_one(br, 0x10); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0x08); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0x08); if (r != SF_OK) return r;
    r = sf_binary_reader_assert_u8_one(br, 0x00); if (r != SF_OK) return r;
    return SF_OK;
}

typedef struct sfi_path_hash {
    uint32_t hash;
    int32_t  index;
} sfi_path_hash_t;

static int sfi_path_hash_cmp(const void *a, const void *b) {
    const sfi_path_hash_t *pa = (const sfi_path_hash_t *)a;
    const sfi_path_hash_t *pb = (const sfi_path_hash_t *)b;
    if (pa->hash < pb->hash) return -1;
    if (pa->hash > pb->hash) return  1;
    return 0;
}

sf_result_t sfi_binder_hash_table_write(sf_binary_writer_t       *bw,
                                        const sf_binder_file_t   *files,
                                        size_t                    file_count,
                                        const sf_allocator_t     *a) {
    SF_CHECK_ARG(bw != NULL);
    SF_CHECK_ARG(file_count == 0 || files != NULL);

    uint32_t group_count = sfi_binder_hash_table_group_count(file_count);
    if (group_count == 0) return SF_ERR_INTERNAL;

    a = sf_alloc_or_default(a);
    uint32_t *bucket_lengths = (uint32_t *)sf_xalloc(a, group_count * sizeof(uint32_t));
    sfi_path_hash_t *hashes  = (sfi_path_hash_t *)sf_xalloc(a, file_count * sizeof(sfi_path_hash_t));
    if ((!bucket_lengths && group_count > 0) || (!hashes && file_count > 0)) {
        sf_xfree(a, bucket_lengths);
        sf_xfree(a, hashes);
        return SF_ERR_OOM;
    }

    for (uint32_t i = 0; i < group_count; i++) bucket_lengths[i] = 0;

    for (size_t i = 0; i < file_count; i++) {
        const char *name = files[i].name_utf8 ? files[i].name_utf8 : "";
        uint32_t h = sf_path_hash(name);
        uint32_t g = h % group_count;
        hashes[i].hash  = h;
        hashes[i].index = (int32_t)i;
        bucket_lengths[g]++;
    }

    /* Stable bucket layout: sort the file array by (group, hash) so each
     * bucket is contiguous and the within-bucket order matches upstream's
     * ascending-hash sort exactly. */
    uint32_t *bucket_offsets = (uint32_t *)sf_xalloc(a, group_count * sizeof(uint32_t));
    sfi_path_hash_t *ordered = (sfi_path_hash_t *)sf_xalloc(a, file_count * sizeof(sfi_path_hash_t));
    if ((!bucket_offsets && group_count > 0) || (!ordered && file_count > 0)) {
        sf_xfree(a, bucket_lengths);
        sf_xfree(a, hashes);
        sf_xfree(a, bucket_offsets);
        sf_xfree(a, ordered);
        return SF_ERR_OOM;
    }

    {
        uint32_t cur = 0;
        for (uint32_t i = 0; i < group_count; i++) {
            bucket_offsets[i] = cur;
            cur += bucket_lengths[i];
        }
    }

    {
        uint32_t *cursor = bucket_lengths;
        for (uint32_t i = 0; i < group_count; i++) cursor[i] = 0;
        for (size_t i = 0; i < file_count; i++) {
            uint32_t g = hashes[i].hash % group_count;
            uint32_t pos = bucket_offsets[g] + cursor[g]++;
            ordered[pos] = hashes[i];
        }
    }

    /* Sort each bucket by ascending hash (BinderHashTable.cs:46-47). */
    {
        uint32_t cur = 0;
        for (uint32_t i = 0; i < group_count; i++) {
            uint32_t len = (i + 1 < group_count) ? (bucket_offsets[i + 1] - cur)
                                                  : ((uint32_t)file_count - cur);
            if (len > 1) qsort(ordered + cur, len, sizeof(sfi_path_hash_t), sfi_path_hash_cmp);
            cur += len;
        }
    }

    sf_result_t r;
    r = sf_binary_writer_reserve_i64(bw, "HashesOffset"); if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u32  (bw, group_count);    if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u8   (bw, 0x10);           if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u8   (bw, 0x08);           if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u8   (bw, 0x08);           if (r != SF_OK) goto done;
    r = sf_binary_writer_write_u8   (bw, 0x00);           if (r != SF_OK) goto done;

    /* HashGroup entries: {Length, Index} (BinderHashTable.cs:121-125). */
    {
        uint32_t cur = 0;
        for (uint32_t i = 0; i < group_count; i++) {
            uint32_t len = (i + 1 < group_count) ? (bucket_offsets[i + 1] - cur)
                                                  : ((uint32_t)file_count - cur);
            r = sf_binary_writer_write_i32(bw, (int32_t)len); if (r != SF_OK) goto done;
            r = sf_binary_writer_write_i32(bw, (int32_t)cur); if (r != SF_OK) goto done;
            cur += len;
        }
    }

    r = sf_binary_writer_fill_i64(bw, "HashesOffset",
                                  (int64_t)sf_binary_writer_position(bw));
    if (r != SF_OK) goto done;

    /* PathHash entries: {Hash, Index} (BinderHashTable.cs:98-102). */
    for (size_t i = 0; i < file_count; i++) {
        r = sf_binary_writer_write_u32(bw, ordered[i].hash);          if (r != SF_OK) goto done;
        r = sf_binary_writer_write_i32(bw, ordered[i].index);         if (r != SF_OK) goto done;
    }

done:
    sf_xfree(a, bucket_lengths);
    sf_xfree(a, hashes);
    sf_xfree(a, bucket_offsets);
    sf_xfree(a, ordered);
    return r;
}
