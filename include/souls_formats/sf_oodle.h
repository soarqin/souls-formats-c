/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SOULS_FORMATS_SF_OODLE_H
#define SOULS_FORMATS_SF_OODLE_H

#include "sf_common.h"

#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_oodle_version {
    SF_OODLE_VERSION_UNKNOWN = 0,
    SF_OODLE_VERSION_6 = 6,
    SF_OODLE_VERSION_8 = 8,
    SF_OODLE_VERSION_9 = 9
} sf_oodle_version_t;

typedef enum sf_oodle_lz_compressor {
    SF_OODLE_LZ_COMPRESSOR_INVALID = -1,
    SF_OODLE_LZ_COMPRESSOR_NONE = 3,

    SF_OODLE_LZ_COMPRESSOR_KRAKEN = 8,
    SF_OODLE_LZ_COMPRESSOR_LEVIATHAN = 13,
    SF_OODLE_LZ_COMPRESSOR_MERMAID = 9,
    SF_OODLE_LZ_COMPRESSOR_SELKIE = 11,
    SF_OODLE_LZ_COMPRESSOR_HYDRA = 12,

    SF_OODLE_LZ_COMPRESSOR_BITKNIT = 10,
    SF_OODLE_LZ_COMPRESSOR_LZB16 = 4,
    SF_OODLE_LZ_COMPRESSOR_LZNA = 7,
    SF_OODLE_LZ_COMPRESSOR_LZH = 0,
    SF_OODLE_LZ_COMPRESSOR_LZHLW = 1,
    SF_OODLE_LZ_COMPRESSOR_LZNIB = 2,
    SF_OODLE_LZ_COMPRESSOR_LZBLW = 5,
    SF_OODLE_LZ_COMPRESSOR_LZA = 6,

    SF_OODLE_LZ_COMPRESSOR_COUNT = 14,
    SF_OODLE_LZ_COMPRESSOR_FORCE32 = 0x40000000
} sf_oodle_lz_compressor_t;

typedef enum sf_oodle_lz_compression_level {
    SF_OODLE_LZ_COMPRESSION_LEVEL_NONE = 0,
    SF_OODLE_LZ_COMPRESSION_LEVEL_SUPER_FAST = 1,
    SF_OODLE_LZ_COMPRESSION_LEVEL_VERY_FAST = 2,
    SF_OODLE_LZ_COMPRESSION_LEVEL_FAST = 3,
    SF_OODLE_LZ_COMPRESSION_LEVEL_NORMAL = 4,

    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL1 = 5,
    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL2 = 6,
    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL3 = 7,
    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL4 = 8,
    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL5 = 9,

    SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST1 = -1,
    SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST2 = -2,
    SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST3 = -3,
    SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST4 = -4,

    SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST = SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST1,
    SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL = SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL2,
    SF_OODLE_LZ_COMPRESSION_LEVEL_MAX = SF_OODLE_LZ_COMPRESSION_LEVEL_OPTIMAL5,
    SF_OODLE_LZ_COMPRESSION_LEVEL_MIN = SF_OODLE_LZ_COMPRESSION_LEVEL_HYPER_FAST4,

    SF_OODLE_LZ_COMPRESSION_LEVEL_FORCE32 = 0x40000000,
    SF_OODLE_LZ_COMPRESSION_LEVEL_INVALID = SF_OODLE_LZ_COMPRESSION_LEVEL_FORCE32
} sf_oodle_lz_compression_level_t;

typedef enum sf_oodle_lz_check_crc {
    SF_OODLE_LZ_CHECK_CRC_NO = 0,
    SF_OODLE_LZ_CHECK_CRC_YES = 1,
    SF_OODLE_LZ_CHECK_CRC_FORCE32 = 0x40000000
} sf_oodle_lz_check_crc_t;

typedef enum sf_oodle_lz_decode_thread_phase {
    SF_OODLE_LZ_DECODE_THREAD_PHASE1 = 1,
    SF_OODLE_LZ_DECODE_THREAD_PHASE2 = 2,
    SF_OODLE_LZ_DECODE_THREAD_PHASE_ALL = 3,
    SF_OODLE_LZ_DECODE_UNTHREADED = SF_OODLE_LZ_DECODE_THREAD_PHASE_ALL
} sf_oodle_lz_decode_thread_phase_t;

typedef enum sf_oodle_lz_fuzz_safe {
    SF_OODLE_LZ_FUZZ_SAFE_NO = 0,
    SF_OODLE_LZ_FUZZ_SAFE_YES = 1
} sf_oodle_lz_fuzz_safe_t;

typedef enum sf_oodle_lz_profile {
    SF_OODLE_LZ_PROFILE_MAIN = 0,
    SF_OODLE_LZ_PROFILE_REDUCED = 1,
    SF_OODLE_LZ_PROFILE_FORCE32 = 0x40000000
} sf_oodle_lz_profile_t;

typedef enum sf_oodle_lz_verbosity {
    SF_OODLE_LZ_VERBOSITY_NONE = 0,
    SF_OODLE_LZ_VERBOSITY_MINIMAL = 1,
    SF_OODLE_LZ_VERBOSITY_SOME = 2,
    SF_OODLE_LZ_VERBOSITY_LOTS = 3,
    SF_OODLE_LZ_VERBOSITY_FORCE32 = 0x40000000
} sf_oodle_lz_verbosity_t;

typedef struct sf_oodle_lz_compress_options {
    uint32_t                    verbosity;
    int32_t                     min_match_len;
    int32_t                     seek_chunk_reset;
    int32_t                     seek_chunk_len;
    sf_oodle_lz_profile_t       profile;
    int32_t                     dictionary_size;
    int32_t                     space_speed_tradeoff_bytes;
    int32_t                     max_huffmans_per_chunk;
    int32_t                     send_quantum_crcs;
    int32_t                     max_local_dictionary_size;
    int32_t                     make_long_range_matcher;
    int32_t                     match_table_size_log2;
} sf_oodle_lz_compress_options_t;

#if defined(__cplusplus)
#define SF_OODLE_STATIC_ASSERT static_assert
#else
#define SF_OODLE_STATIC_ASSERT _Static_assert
#endif

SF_OODLE_STATIC_ASSERT(SF_OODLE_VERSION_UNKNOWN == 0, "sf_oodle_version_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_VERSION_6 == 6, "sf_oodle_version_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_VERSION_8 == 8, "sf_oodle_version_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_VERSION_9 == 9, "sf_oodle_version_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_COMPRESSOR_INVALID == -1,
                       "sf_oodle_lz_compressor_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_COMPRESSOR_COUNT == 14,
                       "sf_oodle_lz_compressor_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_COMPRESSION_LEVEL_MIN == -4,
                       "sf_oodle_lz_compression_level_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_COMPRESSION_LEVEL_MAX == 9,
                       "sf_oodle_lz_compression_level_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_CHECK_CRC_FORCE32 == 0x40000000,
                       "sf_oodle_lz_check_crc_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_DECODE_UNTHREADED == 3,
                       "sf_oodle_lz_decode_thread_phase_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_FUZZ_SAFE_YES == 1,
                       "sf_oodle_lz_fuzz_safe_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_PROFILE_FORCE32 == 0x40000000,
                       "sf_oodle_lz_profile_t drift");
SF_OODLE_STATIC_ASSERT(SF_OODLE_LZ_VERBOSITY_FORCE32 == 0x40000000,
                       "sf_oodle_lz_verbosity_t drift");
SF_OODLE_STATIC_ASSERT(sizeof(sf_oodle_lz_compress_options_t) == 48,
                       "sf_oodle_lz_compress_options_t layout drift");

#undef SF_OODLE_STATIC_ASSERT

SF_API sf_result_t sf_oodle_set_search_path(const wchar_t *dir);
SF_API sf_result_t sf_oodle_load(void);
SF_API void        sf_oodle_unload(void);
SF_API sf_oodle_version_t sf_oodle_version(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_OODLE_H */
