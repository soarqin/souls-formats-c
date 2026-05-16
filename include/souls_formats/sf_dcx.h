/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_DCX_H
#define SOULS_FORMATS_SF_DCX_H

#include "sf_common.h"
#include "sf_oodle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sf_istream sf_istream_t;
typedef struct sf_ostream sf_ostream_t;

typedef enum sf_dcx_type {
    SF_DCX_TYPE_UNKNOWN = 0,
    SF_DCX_TYPE_NONE,
    SF_DCX_TYPE_ZLIB,
    SF_DCX_TYPE_DCP_EDGE,
    SF_DCX_TYPE_DCP_DFLT,
    SF_DCX_TYPE_DCX_EDGE,
    SF_DCX_TYPE_DCX_DFLT,
    SF_DCX_TYPE_DCX_KRAK,
    SF_DCX_TYPE_DCX_ZSTD,
    SF_DCX_TYPE_COUNT_
} sf_dcx_type_t;

typedef enum sf_dcx_default_type {
    SF_DCX_DEFAULT_TYPE_DEMONS_SOULS = 0,
    SF_DCX_DEFAULT_TYPE_DARK_SOULS_1,
    SF_DCX_DEFAULT_TYPE_DARK_SOULS_2,
    SF_DCX_DEFAULT_TYPE_BLOODBORNE,
    SF_DCX_DEFAULT_TYPE_DARK_SOULS_3,
    SF_DCX_DEFAULT_TYPE_SEKIRO,
    SF_DCX_DEFAULT_TYPE_ELDEN_RING,
    SF_DCX_DEFAULT_TYPE_AC6,
    SF_DCX_DEFAULT_TYPE_COUNT_
} sf_dcx_default_type_t;

typedef enum sf_dcx_dflt_compression_preset {
    SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_24_9 = 0,
    SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_44_9,
    SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_8,
    SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9,
    SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9_15,
    SF_DCX_DFLT_COMPRESSION_PRESET_COUNT_
} sf_dcx_dflt_compression_preset_t;

typedef enum sf_dcx_krak_compression_preset {
    SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING = 0,
    SF_DCX_KRAK_COMPRESSION_PRESET_ARMORED_CORE_6,
    SF_DCX_KRAK_COMPRESSION_PRESET_COUNT_
} sf_dcx_krak_compression_preset_t;

/* Upstream's other CompressionInfo variants are empty structs. C11 has no
 * portable empty structs, so these carry a single unused byte while preserving
 * value-type/no-allocation semantics. */
typedef struct sf_dcx_unk_info {
    uint8_t unused;
} sf_dcx_unk_info_t;

typedef struct sf_dcx_none_info {
    uint8_t unused;
} sf_dcx_none_info_t;

typedef struct sf_dcx_zlib_info {
    uint8_t unused;
} sf_dcx_zlib_info_t;

typedef struct sf_dcx_dcp_edge_info {
    uint8_t unused;
} sf_dcx_dcp_edge_info_t;

typedef struct sf_dcx_dcp_dflt_info {
    uint8_t unused;
} sf_dcx_dcp_dflt_info_t;

typedef struct sf_dcx_dcx_edge_info {
    uint8_t unused;
} sf_dcx_dcx_edge_info_t;

typedef struct sf_dcx_dcx_dflt_info {
    int32_t unk04;
    int32_t unk10;
    int32_t unk14;
    uint8_t unk30;
    uint8_t unk38;
} sf_dcx_dcx_dflt_info_t;

/*
 * Kraken / Oodle compression descriptor.
 *
 *   compression_level     — directly passed to OodleLZ_Compress. Higher
 *                           values cost more CPU; lower values cost more
 *                           output bytes. Game-shipped presets:
 *                             Elden Ring / Sekiro: 6 (Optimal2)
 *                             Armored Core 6     : 9 (Optimal5 / Max)
 *                           Callers who need to ship the exact bytes the
 *                           game does MUST use the matching preset.
 *                           Callers doing internal dev iterations may
 *                           override this field after calling
 *                           sf_dcx_compression_info_from_krak_preset()
 *                           with any value from sf_oodle_lz_compression_level_t
 *                           (e.g. NORMAL=4, FAST=3, HYPER_FAST1=-1).
 *                           Going from 6 to 4 is typically 3-5x faster at
 *                           the cost of 5-10% larger output; the game
 *                           decodes either way.
 *
 *   oodle_compressor_type — keep as SF_OODLE_LZ_COMPRESSOR_KRAKEN unless
 *                           you know exactly why you are not.
 */
typedef struct sf_dcx_dcx_krak_info {
    uint8_t                  compression_level;
    sf_oodle_lz_compressor_t oodle_compressor_type;
} sf_dcx_dcx_krak_info_t;

typedef struct sf_dcx_dcx_zstd_info {
    uint8_t compression_level;
} sf_dcx_dcx_zstd_info_t;

typedef struct sf_dcx_compression_info {
    sf_dcx_type_t type;
    union {
        sf_dcx_unk_info_t      unk;
        sf_dcx_none_info_t     none;
        sf_dcx_zlib_info_t     zlib;
        sf_dcx_dcp_edge_info_t dcp_edge;
        sf_dcx_dcp_dflt_info_t dcp_dflt;
        sf_dcx_dcx_edge_info_t dcx_edge;
        sf_dcx_dcx_dflt_info_t dcx_dflt;
        sf_dcx_dcx_krak_info_t dcx_krak;
        sf_dcx_dcx_zstd_info_t dcx_zstd;
    } u;
} sf_dcx_compression_info_t;

#if defined(__cplusplus)
#define SF_DCX_STATIC_ASSERT static_assert
#else
#define SF_DCX_STATIC_ASSERT _Static_assert
#endif

SF_DCX_STATIC_ASSERT(SF_DCX_TYPE_UNKNOWN == 0, "sf_dcx_type_t drift");
SF_DCX_STATIC_ASSERT(SF_DCX_TYPE_COUNT_ == 9, "sf_dcx_type_t drift");
SF_DCX_STATIC_ASSERT(SF_DCX_DEFAULT_TYPE_COUNT_ == 8, "sf_dcx_default_type_t drift");
SF_DCX_STATIC_ASSERT(SF_DCX_DFLT_COMPRESSION_PRESET_COUNT_ == 5,
                     "sf_dcx_dflt_compression_preset_t drift");
SF_DCX_STATIC_ASSERT(SF_DCX_KRAK_COMPRESSION_PRESET_COUNT_ == 2,
                     "sf_dcx_krak_compression_preset_t drift");

#undef SF_DCX_STATIC_ASSERT

SF_API sf_result_t sf_dcx_compression_info_from_default_type(
    sf_dcx_default_type_t default_type, sf_dcx_compression_info_t *out);
SF_API sf_result_t sf_dcx_compression_info_from_dflt_preset(
    sf_dcx_dflt_compression_preset_t preset, sf_dcx_compression_info_t *out);
SF_API sf_result_t sf_dcx_compression_info_from_krak_preset(
    sf_dcx_krak_compression_preset_t preset, sf_dcx_compression_info_t *out);

SF_API sf_result_t sf_dcx_sniff(const void *buf, size_t size, sf_dcx_type_t *out_type);

SF_API sf_result_t sf_dcx_is_from_buffer(const uint8_t *buf, size_t size, bool *out);
SF_API sf_result_t sf_dcx_is_from_stream(sf_istream_t *stream, bool *out);
SF_API sf_result_t sf_dcx_is_from_path(const char *utf8_path, bool *out);

SF_API sf_result_t sf_dcx_decompress_from_buffer(const uint8_t *in, size_t in_size,
                                                 uint8_t **out, size_t *out_size,
                                                 sf_dcx_compression_info_t *out_info,
                                                 const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_decompress_from_stream(sf_istream_t *stream, uint8_t **out,
                                                 size_t *out_size,
                                                 sf_dcx_compression_info_t *out_info,
                                                 const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_decompress_from_path(const char *utf8_path, uint8_t **out,
                                               size_t *out_size,
                                               sf_dcx_compression_info_t *out_info,
                                               const sf_allocator_t *alloc);

SF_API sf_result_t sf_dcx_compress_to_buffer(const uint8_t *in, size_t in_size,
                                             const sf_dcx_compression_info_t *info,
                                             uint8_t **out, size_t *out_size,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_compress_to_stream(const uint8_t *in, size_t in_size,
                                             const sf_dcx_compression_info_t *info,
                                             sf_ostream_t *stream,
                                             const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_compress_to_path(const uint8_t *in, size_t in_size,
                                           const sf_dcx_compression_info_t *info,
                                           const char *utf8_path,
                                           const sf_allocator_t *alloc);

/* Compatibility conveniences retained from the pre-realignment C API. */
SF_API sf_result_t sf_dcx_decompress(const void *in, size_t in_size, void **out,
                                     size_t *out_size, sf_dcx_type_t *out_type,
                                     const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_compress(const void *in, size_t in_size, sf_dcx_type_t type,
                                   void **out, size_t *out_size,
                                   const sf_allocator_t *alloc);
SF_API sf_result_t sf_dcx_compress_ex(const void *in, size_t in_size,
                                      const sf_dcx_compression_info_t *info,
                                      void **out, size_t *out_size,
                                      const sf_allocator_t *alloc);

/**
 * extension: see docs/api-mapping/extensions.md
 *
 * Iteratively decompress DCX layers until the payload is no longer DCX.
 * Uses sf_dcx_sniff() at each step to detect the type via magic number.
 * Supports up to 8 nested DCX layers.
 *
 * If input is not DCX, this succeeds and copies the input to out unchanged.
 */
SF_API sf_result_t sf_dcx_unwrap(const void *in, size_t in_size, void **out,
                                 size_t *out_size, const sf_allocator_t *alloc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_DCX_H */
