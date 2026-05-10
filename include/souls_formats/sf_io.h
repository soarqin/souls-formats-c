/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — streams + binary reader/writer.
 *
 * This is the foundation every format module is built on. The public API
 * mirrors upstream BinaryReaderEx / BinaryWriterEx semantics:
 *   - mutable big-endian flag
 *   - mutable varint-long flag (4 vs 8 byte varints)
 *   - an offset stack (StepIn / StepOut)
 *   - per-type Reserve / Fill placeholder backfill (writer)
 *   - explicit ASCII / Shift-JIS / UTF-16 string handling
 *   - all strings are exchanged with the host as UTF-8
 *
 * Memory ownership:
 *   - sf_istream_t / sf_ostream_t are heap-owned by the caller; close them.
 *   - sf_binary_reader_t / sf_binary_writer_t are heap-owned by the caller;
 *     destroy them. They borrow their stream (do not close it).
 *   - Strings returned by reader read APIs are owned by the caller and freed
 *     via sf_free() with the same allocator used to create the reader.
 */

#ifndef SOULS_FORMATS_SF_IO_H
#define SOULS_FORMATS_SF_IO_H

#include "sf_common.h"
#include "sf_dcx.h"
#include "sf_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Generic allocator helper
 *===========================================================================*/

/*  Free `ptr` (if non-NULL) using `a`. NULL `a` uses sf_default_allocator(). */
SF_API void sf_free(const sf_allocator_t *a, void *ptr);

/*===========================================================================
 * Read-only stream
 *
 * Represents a seekable byte source. Currently file-backed (Win32) or
 * memory-backed.
 *===========================================================================*/

typedef struct sf_istream sf_istream_t;

/*  Open a file by wide path. The path is UTF-16, suitable for direct passing
 *  to Win32 file APIs (CreateFileW). */
SF_API sf_result_t sf_istream_open_wfile(sf_istream_t **out, const wchar_t *path,
                                         const sf_allocator_t *a);

/*  Open a file by UTF-8 path. The path is converted to UTF-16 internally. */
SF_API sf_result_t sf_istream_open_file(sf_istream_t **out, const char *utf8_path,
                                        const sf_allocator_t *a);

/*  Wrap a caller-owned byte buffer. The buffer must outlive the stream. */
SF_API sf_result_t sf_istream_open_memory(sf_istream_t **out, const void *data, size_t size,
                                          const sf_allocator_t *a);

/*  Close and free the stream. NULL-safe. */
SF_API void sf_istream_close(sf_istream_t *s);

SF_API int64_t sf_istream_position(const sf_istream_t *s);
SF_API int64_t sf_istream_length  (const sf_istream_t *s);
SF_API int64_t sf_istream_remaining(const sf_istream_t *s);
SF_API sf_result_t sf_istream_seek(sf_istream_t *s, int64_t pos);
SF_API sf_result_t sf_istream_read(sf_istream_t *s, void *buf, size_t n);

/*===========================================================================
 * Write-only stream
 *
 * Memory-backed (grows on demand) or file-backed (Win32).
 *===========================================================================*/

typedef struct sf_ostream sf_ostream_t;

SF_API sf_result_t sf_ostream_open_memory(sf_ostream_t **out, const sf_allocator_t *a);
SF_API sf_result_t sf_ostream_open_wfile (sf_ostream_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
SF_API sf_result_t sf_ostream_open_file  (sf_ostream_t **out, const char *utf8_path,
                                          const sf_allocator_t *a);

/*  Close and free. NULL-safe. */
SF_API void sf_ostream_close(sf_ostream_t *s);

SF_API int64_t sf_ostream_position(const sf_ostream_t *s);
SF_API int64_t sf_ostream_length  (const sf_ostream_t *s);
SF_API sf_result_t sf_ostream_seek (sf_ostream_t *s, int64_t pos);
SF_API sf_result_t sf_ostream_write(sf_ostream_t *s, const void *buf, size_t n);

/*  For memory-backed streams: detach the contents into a heap buffer that
 *  the caller owns and frees via sf_free(s->allocator, ...). The stream is
 *  left with zero length (still valid for further writes). On a file stream,
 *  returns SF_ERR_INVALID_ARG. */
SF_API sf_result_t sf_ostream_detach_buffer(sf_ostream_t *s, void **out_data, size_t *out_size);

/*===========================================================================
 * Binary reader
 *===========================================================================*/

typedef struct sf_binary_reader sf_binary_reader_t;

/*  Build a reader on top of an existing stream. The reader does NOT own the
 *  stream; close the stream separately. */
SF_API sf_result_t sf_binary_reader_create(sf_binary_reader_t **out, sf_istream_t *s,
                                            bool big_endian, const sf_allocator_t *a);

/*  Build a reader on top of a heap buffer. The reader TAKES OWNERSHIP of
 *  `data`: sf_binary_reader_destroy will free it (and the internal memory
 *  istream wrapping it) using `a`. Mirrors upstream
 *  `new BinaryReaderEx(big_endian, byte[] data)`. */
SF_API sf_result_t sf_binary_reader_create_from_memory(sf_binary_reader_t **out,
                                                       bool big_endian, void *data,
                                                       size_t size,
                                                       const sf_allocator_t *a);

SF_API void        sf_binary_reader_destroy(sf_binary_reader_t *r);

SF_API bool sf_binary_reader_flexible_default(void);
SF_API void sf_binary_reader_set_flexible_default(bool flexible);
SF_API bool sf_binary_reader_flexible(const sf_binary_reader_t *r);
SF_API void sf_binary_reader_set_flexible(sf_binary_reader_t *r, bool flexible);
SF_API sf_istream_t *sf_binary_reader_stream(sf_binary_reader_t *r);

/*  Endian + varint state. */
SF_API bool sf_binary_reader_big_endian (const sf_binary_reader_t *r);
SF_API void sf_binary_reader_set_big_endian(sf_binary_reader_t *r, bool be);
SF_API bool sf_binary_reader_varint_long(const sf_binary_reader_t *r);
SF_API void sf_binary_reader_set_varint_long(sf_binary_reader_t *r, bool long64);

/*  Position queries. */
SF_API int64_t sf_binary_reader_position (const sf_binary_reader_t *r);
SF_API int64_t sf_binary_reader_length   (const sf_binary_reader_t *r);
SF_API int64_t sf_binary_reader_remaining(const sf_binary_reader_t *r);

/*  StepIn / StepOut offset stack. */
SF_API sf_result_t sf_binary_reader_step_in (sf_binary_reader_t *r, int64_t pos);
SF_API sf_result_t sf_binary_reader_step_out(sf_binary_reader_t *r);

/*  Pad to alignment / skip. */
SF_API sf_result_t sf_binary_reader_pad         (sf_binary_reader_t *r, int align);
SF_API sf_result_t sf_binary_reader_pad_relative(sf_binary_reader_t *r, int64_t start, int align);
SF_API sf_result_t sf_binary_reader_skip        (sf_binary_reader_t *r, int64_t n);

/*  Primitive read APIs. Each returns SF_OK or SF_ERR_TRUNCATED. */
SF_API sf_result_t sf_binary_reader_read_bool (sf_binary_reader_t *r, bool *out);
SF_API sf_result_t sf_binary_reader_read_i8   (sf_binary_reader_t *r, int8_t   *out);
SF_API sf_result_t sf_binary_reader_read_u8   (sf_binary_reader_t *r, uint8_t  *out);
SF_API sf_result_t sf_binary_reader_read_i16  (sf_binary_reader_t *r, int16_t  *out);
SF_API sf_result_t sf_binary_reader_read_u16  (sf_binary_reader_t *r, uint16_t *out);
SF_API sf_result_t sf_binary_reader_read_i32  (sf_binary_reader_t *r, int32_t  *out);
SF_API sf_result_t sf_binary_reader_read_u32  (sf_binary_reader_t *r, uint32_t *out);
SF_API sf_result_t sf_binary_reader_read_i64  (sf_binary_reader_t *r, int64_t  *out);
SF_API sf_result_t sf_binary_reader_read_u64  (sf_binary_reader_t *r, uint64_t *out);
SF_API sf_result_t sf_binary_reader_read_f32  (sf_binary_reader_t *r, float    *out);
SF_API sf_result_t sf_binary_reader_read_f64  (sf_binary_reader_t *r, double   *out);
SF_API sf_result_t sf_binary_reader_read_varint(sf_binary_reader_t *r, int64_t *out);

SF_API sf_result_t sf_binary_reader_read_bytes(sf_binary_reader_t *r, void *buf, size_t n);

SF_API sf_result_t sf_binary_reader_read_bools(sf_binary_reader_t *r, size_t n, bool *out_array);
SF_API sf_result_t sf_binary_reader_read_i8s  (sf_binary_reader_t *r, size_t n, int8_t   *out_array);
SF_API sf_result_t sf_binary_reader_read_u8s  (sf_binary_reader_t *r, size_t n, uint8_t  *out_array);
SF_API sf_result_t sf_binary_reader_read_i16s (sf_binary_reader_t *r, size_t n, int16_t  *out_array);
SF_API sf_result_t sf_binary_reader_read_u16s (sf_binary_reader_t *r, size_t n, uint16_t *out_array);
SF_API sf_result_t sf_binary_reader_read_i32s (sf_binary_reader_t *r, size_t n, int32_t  *out_array);
SF_API sf_result_t sf_binary_reader_read_u32s (sf_binary_reader_t *r, size_t n, uint32_t *out_array);
SF_API sf_result_t sf_binary_reader_read_i64s (sf_binary_reader_t *r, size_t n, int64_t  *out_array);
SF_API sf_result_t sf_binary_reader_read_u64s (sf_binary_reader_t *r, size_t n, uint64_t *out_array);
SF_API sf_result_t sf_binary_reader_read_f32s (sf_binary_reader_t *r, size_t n, float    *out_array);
SF_API sf_result_t sf_binary_reader_read_f64s (sf_binary_reader_t *r, size_t n, double   *out_array);
SF_API sf_result_t sf_binary_reader_read_varints(sf_binary_reader_t *r, size_t n,
                                                 int64_t *out_array);

/*  Get* — read at absolute offset without moving the stream cursor. */
SF_API sf_result_t sf_binary_reader_get_bool (sf_binary_reader_t *r, int64_t off, bool    *out);
SF_API sf_result_t sf_binary_reader_get_i8   (sf_binary_reader_t *r, int64_t off, int8_t  *out);
SF_API sf_result_t sf_binary_reader_get_u8   (sf_binary_reader_t *r, int64_t off, uint8_t *out);
SF_API sf_result_t sf_binary_reader_get_i16  (sf_binary_reader_t *r, int64_t off, int16_t *out);
SF_API sf_result_t sf_binary_reader_get_u16  (sf_binary_reader_t *r, int64_t off, uint16_t *out);
SF_API sf_result_t sf_binary_reader_get_u32  (sf_binary_reader_t *r, int64_t off, uint32_t *out);
SF_API sf_result_t sf_binary_reader_get_i32  (sf_binary_reader_t *r, int64_t off, int32_t  *out);
SF_API sf_result_t sf_binary_reader_get_u64  (sf_binary_reader_t *r, int64_t off, uint64_t *out);
SF_API sf_result_t sf_binary_reader_get_i64  (sf_binary_reader_t *r, int64_t off, int64_t  *out);
SF_API sf_result_t sf_binary_reader_get_f32  (sf_binary_reader_t *r, int64_t off, float    *out);
SF_API sf_result_t sf_binary_reader_get_f64  (sf_binary_reader_t *r, int64_t off, double   *out);
SF_API sf_result_t sf_binary_reader_get_varint(sf_binary_reader_t *r, int64_t off, int64_t *out);
SF_API sf_result_t sf_binary_reader_get_bytes(sf_binary_reader_t *r, int64_t off,
                                               void *buf, size_t n);

SF_API sf_result_t sf_binary_reader_get_bools(sf_binary_reader_t *r, int64_t off, size_t n,
                                              bool *out_array);
SF_API sf_result_t sf_binary_reader_get_i8s  (sf_binary_reader_t *r, int64_t off, size_t n,
                                              int8_t *out_array);
SF_API sf_result_t sf_binary_reader_get_u8s  (sf_binary_reader_t *r, int64_t off, size_t n,
                                              uint8_t *out_array);
SF_API sf_result_t sf_binary_reader_get_i16s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              int16_t *out_array);
SF_API sf_result_t sf_binary_reader_get_u16s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              uint16_t *out_array);
SF_API sf_result_t sf_binary_reader_get_i32s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              int32_t *out_array);
SF_API sf_result_t sf_binary_reader_get_u32s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              uint32_t *out_array);
SF_API sf_result_t sf_binary_reader_get_i64s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              int64_t *out_array);
SF_API sf_result_t sf_binary_reader_get_u64s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              uint64_t *out_array);
SF_API sf_result_t sf_binary_reader_get_f32s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              float *out_array);
SF_API sf_result_t sf_binary_reader_get_f64s (sf_binary_reader_t *r, int64_t off, size_t n,
                                              double *out_array);
SF_API sf_result_t sf_binary_reader_get_varints(sf_binary_reader_t *r, int64_t off, size_t n,
                                                int64_t *out_array);

/*  Assert reads — value must match one option; returns SF_ERR_BAD_MAGIC otherwise.
 *  Multi-option forms mirror upstream params arrays. `_one` helpers are
 *  single-option conveniences for callers that do not need the read value. */
SF_API sf_result_t sf_binary_reader_assert_bool(sf_binary_reader_t *r, size_t n_options,
                                                const bool *options, bool *out_value);
SF_API sf_result_t sf_binary_reader_assert_i8 (sf_binary_reader_t *r, size_t n_options,
                                               const int8_t *options, int8_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_u8 (sf_binary_reader_t *r, size_t n_options,
                                               const uint8_t *options, uint8_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_i16(sf_binary_reader_t *r, size_t n_options,
                                               const int16_t *options, int16_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_u16(sf_binary_reader_t *r, size_t n_options,
                                               const uint16_t *options, uint16_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_i32(sf_binary_reader_t *r, size_t n_options,
                                               const int32_t *options, int32_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_u32(sf_binary_reader_t *r, size_t n_options,
                                               const uint32_t *options, uint32_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_i64(sf_binary_reader_t *r, size_t n_options,
                                               const int64_t *options, int64_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_u64(sf_binary_reader_t *r, size_t n_options,
                                               const uint64_t *options, uint64_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_f32(sf_binary_reader_t *r, size_t n_options,
                                               const float *options, float *out_value);
SF_API sf_result_t sf_binary_reader_assert_f64(sf_binary_reader_t *r, size_t n_options,
                                               const double *options, double *out_value);
SF_API sf_result_t sf_binary_reader_assert_varint(sf_binary_reader_t *r, size_t n_options,
                                                  const int64_t *options, int64_t *out_value);
SF_API sf_result_t sf_binary_reader_assert_bool_one(sf_binary_reader_t *r, bool expect);
SF_API sf_result_t sf_binary_reader_assert_i8_one (sf_binary_reader_t *r, int8_t expect);
SF_API sf_result_t sf_binary_reader_assert_u8_one (sf_binary_reader_t *r, uint8_t expect);
SF_API sf_result_t sf_binary_reader_assert_i16_one(sf_binary_reader_t *r, int16_t expect);
SF_API sf_result_t sf_binary_reader_assert_u16_one(sf_binary_reader_t *r, uint16_t expect);
SF_API sf_result_t sf_binary_reader_assert_i32_one(sf_binary_reader_t *r, int32_t expect);
SF_API sf_result_t sf_binary_reader_assert_u32_one(sf_binary_reader_t *r, uint32_t expect);
SF_API sf_result_t sf_binary_reader_assert_i64_one(sf_binary_reader_t *r, int64_t expect);
SF_API sf_result_t sf_binary_reader_assert_u64_one(sf_binary_reader_t *r, uint64_t expect);
SF_API sf_result_t sf_binary_reader_assert_f32_one(sf_binary_reader_t *r, float expect);
SF_API sf_result_t sf_binary_reader_assert_f64_one(sf_binary_reader_t *r, double expect);
SF_API sf_result_t sf_binary_reader_assert_varint_one(sf_binary_reader_t *r, int64_t expect);
SF_API sf_result_t sf_binary_reader_assert_pattern(sf_binary_reader_t *r, size_t length,
                                                    uint8_t pattern);

SF_API sf_result_t sf_binary_reader_read_enum_8 (sf_binary_reader_t *r, size_t n_options,
                                                 const uint8_t *options, uint8_t *out_value);
SF_API sf_result_t sf_binary_reader_read_enum_16(sf_binary_reader_t *r, size_t n_options,
                                                 const uint16_t *options, uint16_t *out_value);
SF_API sf_result_t sf_binary_reader_read_enum_32(sf_binary_reader_t *r, size_t n_options,
                                                 const uint32_t *options, uint32_t *out_value);
SF_API sf_result_t sf_binary_reader_read_enum_64(sf_binary_reader_t *r, size_t n_options,
                                                 const uint64_t *options, uint64_t *out_value);
SF_API sf_result_t sf_binary_reader_get_enum_8 (sf_binary_reader_t *r, int64_t off,
                                                size_t n_options, const uint8_t *options,
                                                uint8_t *out_value);
SF_API sf_result_t sf_binary_reader_get_enum_16(sf_binary_reader_t *r, int64_t off,
                                                size_t n_options, const uint16_t *options,
                                                uint16_t *out_value);
SF_API sf_result_t sf_binary_reader_get_enum_32(sf_binary_reader_t *r, int64_t off,
                                                size_t n_options, const uint32_t *options,
                                                uint32_t *out_value);
SF_API sf_result_t sf_binary_reader_get_enum_64(sf_binary_reader_t *r, int64_t off,
                                                size_t n_options, const uint64_t *options,
                                                uint64_t *out_value);

/*  Vector / quat / color (4-byte). */
SF_API sf_result_t sf_binary_reader_read_vec2(sf_binary_reader_t *r, sf_vec2_t *out);
SF_API sf_result_t sf_binary_reader_read_vec3(sf_binary_reader_t *r, sf_vec3_t *out);
SF_API sf_result_t sf_binary_reader_read_vec4(sf_binary_reader_t *r, sf_vec4_t *out);
SF_API sf_result_t sf_binary_reader_read_quat(sf_binary_reader_t *r, sf_quat_t *out);

/*  Read a packed 11_11_10 vec3 (signed 11/11/10 bits). */
SF_API sf_result_t sf_binary_reader_read_11_11_10_vec3(sf_binary_reader_t *r, sf_vec3_t *out);

SF_API sf_result_t sf_binary_reader_read_argb(sf_binary_reader_t *r, sf_color_t *out);
SF_API sf_result_t sf_binary_reader_read_abgr(sf_binary_reader_t *r, sf_color_t *out);
SF_API sf_result_t sf_binary_reader_read_rgba(sf_binary_reader_t *r, sf_color_t *out);
SF_API sf_result_t sf_binary_reader_read_bgra(sf_binary_reader_t *r, sf_color_t *out);

/*  String reads. All return UTF-8 in `*out_utf8`, length in `*out_len_bytes`
 *  (excluding final NUL; the buffer IS NUL-terminated). Caller frees with
 *  sf_free(reader_allocator, ptr). NULL `out_len_bytes` is allowed. */
SF_API sf_result_t sf_binary_reader_read_ascii (sf_binary_reader_t *r,
                                                char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_ascii_n(sf_binary_reader_t *r, size_t n_bytes,
                                                 char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_shift_jis(sf_binary_reader_t *r,
                                                   char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_shift_jis_n(sf_binary_reader_t *r, size_t n_bytes,
                                                     char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_utf16   (sf_binary_reader_t *r,
                                                  char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_fix_str (sf_binary_reader_t *r, size_t size,
                                                  char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_read_fix_str_w(sf_binary_reader_t *r, size_t size,
                                                    char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_assert_ascii(sf_binary_reader_t *r, const char *expected);
SF_API sf_result_t sf_binary_reader_get_ascii(sf_binary_reader_t *r, int64_t off,
                                              char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_get_ascii_n(sf_binary_reader_t *r, int64_t off,
                                                size_t n_bytes, char **out_utf8,
                                                size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_get_shift_jis(sf_binary_reader_t *r, int64_t off,
                                                  char **out_utf8, size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_get_shift_jis_n(sf_binary_reader_t *r, int64_t off,
                                                    size_t n_bytes, char **out_utf8,
                                                    size_t *out_len_bytes);
SF_API sf_result_t sf_binary_reader_get_utf16(sf_binary_reader_t *r, int64_t off,
                                              char **out_utf8, size_t *out_len_bytes);

/*===========================================================================
 * SFUtil — DCX-aware reader entry point
 *
 * Mirrors upstream SFUtil.GetDecompressedBinaryReader.
 *
 * If @in contains DCX-compressed data:
 *   - Decompresses into a heap buffer.
 *   - Creates a NEW sf_binary_reader_t that owns both that buffer and an
 *     internal memory istream wrapping it.
 *   - Sets *out_reader to the new reader (CALLER OWNS: a single
 *     sf_binary_reader_destroy(*out_reader) frees the backing buffer and
 *     the internal istream).
 *   - Fills *out_info with the concrete compression variant.
 *
 * If @in does NOT contain DCX data:
 *   - Sets *out_reader = in (BORROW — do NOT destroy this pointer here;
 *     it remains owned by whoever created `in`).
 *   - Sets out_info->type = SF_DCX_TYPE_NONE.
 *
 * Callers must check out_info->type to determine ownership.
 *===========================================================================*/

SF_API sf_result_t sf_get_decompressed_reader(
    sf_binary_reader_t *in,
    sf_binary_reader_t **out_reader,
    sf_dcx_compression_info_t *out_info,
    const sf_allocator_t *alloc);

/*===========================================================================
 * Binary writer
 *===========================================================================*/

typedef struct sf_binary_writer sf_binary_writer_t;

SF_API sf_result_t sf_binary_writer_create(sf_binary_writer_t **out, sf_ostream_t *s,
                                           bool big_endian, const sf_allocator_t *a);

/*  Destroy without checking reservations. Use sf_binary_writer_finish() to
 *  validate first. */
SF_API void sf_binary_writer_destroy(sf_binary_writer_t *w);

/*  Verify all reservations are filled and close this writer handle. Returns
 *  SF_ERR_INTERNAL if not. The borrowed stream remains caller-owned. */
SF_API sf_result_t sf_binary_writer_finish(sf_binary_writer_t *w);

/*  Snapshot currently written bytes from the borrowed stream without closing
 *  this writer. Caller frees `*out` with sf_free(writer allocator, ...). */
SF_API sf_result_t sf_binary_writer_to_array(sf_binary_writer_t *w,
                                             uint8_t **out, size_t *out_size);

/*  Snapshot bytes, verify reservations, then close this writer handle. */
SF_API sf_result_t sf_binary_writer_finish_bytes(sf_binary_writer_t *w,
                                                 uint8_t **out, size_t *out_size);

SF_API bool sf_binary_writer_big_endian (const sf_binary_writer_t *w);
SF_API void sf_binary_writer_set_big_endian(sf_binary_writer_t *w, bool be);
SF_API bool sf_binary_writer_varint_long(const sf_binary_writer_t *w);
SF_API void sf_binary_writer_set_varint_long(sf_binary_writer_t *w, bool long64);
SF_API sf_ostream_t *sf_binary_writer_stream(sf_binary_writer_t *w);

SF_API int64_t sf_binary_writer_position(const sf_binary_writer_t *w);
SF_API int64_t sf_binary_writer_length  (const sf_binary_writer_t *w);

SF_API sf_result_t sf_binary_writer_step_in (sf_binary_writer_t *w, int64_t pos);
SF_API sf_result_t sf_binary_writer_step_out(sf_binary_writer_t *w);
SF_API sf_result_t sf_binary_writer_pad      (sf_binary_writer_t *w, int align);
SF_API sf_result_t sf_binary_writer_pad_byte (sf_binary_writer_t *w, int align, uint8_t fill);
SF_API sf_result_t sf_binary_writer_pad_ff   (sf_binary_writer_t *w, int align);
SF_API sf_result_t sf_binary_writer_pad_relative(sf_binary_writer_t *w, int64_t start, int align);
SF_API sf_result_t sf_binary_writer_write_pattern(sf_binary_writer_t *w, size_t length, uint8_t v);

/*  Primitives. */
SF_API sf_result_t sf_binary_writer_write_bool(sf_binary_writer_t *w, bool v);
SF_API sf_result_t sf_binary_writer_write_i8  (sf_binary_writer_t *w, int8_t  v);
SF_API sf_result_t sf_binary_writer_write_u8  (sf_binary_writer_t *w, uint8_t v);
SF_API sf_result_t sf_binary_writer_write_i16 (sf_binary_writer_t *w, int16_t  v);
SF_API sf_result_t sf_binary_writer_write_u16 (sf_binary_writer_t *w, uint16_t v);
SF_API sf_result_t sf_binary_writer_write_i32 (sf_binary_writer_t *w, int32_t  v);
SF_API sf_result_t sf_binary_writer_write_u32 (sf_binary_writer_t *w, uint32_t v);
SF_API sf_result_t sf_binary_writer_write_i64 (sf_binary_writer_t *w, int64_t  v);
SF_API sf_result_t sf_binary_writer_write_u64 (sf_binary_writer_t *w, uint64_t v);
SF_API sf_result_t sf_binary_writer_write_f32 (sf_binary_writer_t *w, float    v);
SF_API sf_result_t sf_binary_writer_write_f64 (sf_binary_writer_t *w, double   v);
SF_API sf_result_t sf_binary_writer_write_varint(sf_binary_writer_t *w, int64_t v);
SF_API sf_result_t sf_binary_writer_write_bytes(sf_binary_writer_t *w, const void *buf, size_t n);

/*  Primitive array writes. `count` precedes the pointer per C mapping policy. */
SF_API sf_result_t sf_binary_writer_write_bools  (sf_binary_writer_t *w, size_t count,
                                                  const bool *values);
SF_API sf_result_t sf_binary_writer_write_i8s    (sf_binary_writer_t *w, size_t count,
                                                  const int8_t *values);
SF_API sf_result_t sf_binary_writer_write_u8s    (sf_binary_writer_t *w, size_t count,
                                                  const uint8_t *values);
SF_API sf_result_t sf_binary_writer_write_i16s   (sf_binary_writer_t *w, size_t count,
                                                  const int16_t *values);
SF_API sf_result_t sf_binary_writer_write_u16s   (sf_binary_writer_t *w, size_t count,
                                                  const uint16_t *values);
SF_API sf_result_t sf_binary_writer_write_i32s   (sf_binary_writer_t *w, size_t count,
                                                  const int32_t *values);
SF_API sf_result_t sf_binary_writer_write_u32s   (sf_binary_writer_t *w, size_t count,
                                                  const uint32_t *values);
SF_API sf_result_t sf_binary_writer_write_i64s   (sf_binary_writer_t *w, size_t count,
                                                  const int64_t *values);
SF_API sf_result_t sf_binary_writer_write_u64s   (sf_binary_writer_t *w, size_t count,
                                                  const uint64_t *values);
SF_API sf_result_t sf_binary_writer_write_f32s   (sf_binary_writer_t *w, size_t count,
                                                  const float *values);
SF_API sf_result_t sf_binary_writer_write_f64s   (sf_binary_writer_t *w, size_t count,
                                                  const double *values);
SF_API sf_result_t sf_binary_writer_write_varints(sf_binary_writer_t *w, size_t count,
                                                  const int64_t *values);

/*  Reserve / fill: store a (name + width) tuple at the current position,
 *  then later jump back and write the real value. Each Reserve must be
 *  paired with exactly one Fill of the matching width before finish. */
SF_API sf_result_t sf_binary_writer_reserve_bool (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_i8   (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_u8   (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_i16  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_u16  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_u32  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_i32  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_u64  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_i64  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_varint(sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_f32  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_reserve_f64  (sf_binary_writer_t *w, const char *name);
SF_API sf_result_t sf_binary_writer_fill_bool (sf_binary_writer_t *w, const char *name, bool v);
SF_API sf_result_t sf_binary_writer_fill_i8   (sf_binary_writer_t *w, const char *name, int8_t v);
SF_API sf_result_t sf_binary_writer_fill_u8   (sf_binary_writer_t *w, const char *name, uint8_t v);
SF_API sf_result_t sf_binary_writer_fill_i16  (sf_binary_writer_t *w, const char *name, int16_t v);
SF_API sf_result_t sf_binary_writer_fill_u16  (sf_binary_writer_t *w, const char *name, uint16_t v);
SF_API sf_result_t sf_binary_writer_fill_u32  (sf_binary_writer_t *w, const char *name, uint32_t v);
SF_API sf_result_t sf_binary_writer_fill_i32  (sf_binary_writer_t *w, const char *name, int32_t  v);
SF_API sf_result_t sf_binary_writer_fill_u64  (sf_binary_writer_t *w, const char *name, uint64_t v);
SF_API sf_result_t sf_binary_writer_fill_i64  (sf_binary_writer_t *w, const char *name, int64_t  v);
SF_API sf_result_t sf_binary_writer_fill_varint(sf_binary_writer_t *w, const char *name, int64_t v);
SF_API sf_result_t sf_binary_writer_fill_f32  (sf_binary_writer_t *w, const char *name, float v);
SF_API sf_result_t sf_binary_writer_fill_f64  (sf_binary_writer_t *w, const char *name, double v);

/*  Strings. Input is UTF-8. */
SF_API sf_result_t sf_binary_writer_write_ascii   (sf_binary_writer_t *w,
                                                   const char *utf8, bool terminate);
SF_API sf_result_t sf_binary_writer_write_shift_jis(sf_binary_writer_t *w,
                                                    const char *utf8, bool terminate);
SF_API sf_result_t sf_binary_writer_write_utf16   (sf_binary_writer_t *w,
                                                   const char *utf8, bool terminate);
SF_API sf_result_t sf_binary_writer_write_fix_str (sf_binary_writer_t *w,
                                                   const char *utf8, size_t size, uint8_t pad);
SF_API sf_result_t sf_binary_writer_write_fix_str_w(sf_binary_writer_t *w,
                                                    const char *utf8, size_t size, uint8_t pad);

/*  Vector / quat / color. */
SF_API sf_result_t sf_binary_writer_write_vec2(sf_binary_writer_t *w, sf_vec2_t v);
SF_API sf_result_t sf_binary_writer_write_vec3(sf_binary_writer_t *w, sf_vec3_t v);
SF_API sf_result_t sf_binary_writer_write_vec4(sf_binary_writer_t *w, sf_vec4_t v);
SF_API sf_result_t sf_binary_writer_write_quat(sf_binary_writer_t *w, sf_quat_t q);
SF_API sf_result_t sf_binary_writer_write_argb(sf_binary_writer_t *w, sf_color_t c);
SF_API sf_result_t sf_binary_writer_write_abgr(sf_binary_writer_t *w, sf_color_t c);
SF_API sf_result_t sf_binary_writer_write_rgba(sf_binary_writer_t *w, sf_color_t c);
SF_API sf_result_t sf_binary_writer_write_bgra(sf_binary_writer_t *w, sf_color_t c);

/*===========================================================================
 * Bit utilities
 *
 * Mirrors upstream SoulsFormats.Utilities.EndianHelper.
 *===========================================================================*/

/*  Reverses the order of bits in a byte (bit 0 ↔ bit 7, bit 1 ↔ bit 6, …).
 *  Mirrors upstream EndianHelper.ReverseBits. Used by binder format readers
 *  that store flag fields with reversed bit ordering. */
SF_API uint8_t sf_reverse_bits_u8(uint8_t b);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_IO_H */
