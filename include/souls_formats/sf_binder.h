/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Binder (BND3/BND4/BXF3/BXF4) shared types.
 *
 * This header provides the types and helpers shared by every binder
 * archive format: the Format / FileFlags bitmask byte, the per-entry
 * sf_binder_file_t struct, the BND timestamp struct, and helpers that
 * mirror upstream's static methods on the static class `Binder`.
 *
 * The actual reader/writer entry points for BND3/BND4/BXF3/BXF4 ship in
 * later phase-3 tasks; this header only forward-declares their opaque
 * types so callers can write
 *
 *     sf_bnd4_t *bnd = NULL;
 *     sf_bnd4_read_from_path(..., &bnd);
 *
 * after pulling in <souls_formats/sf_binder.h> alone.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/Binder.cs       — Format, FileFlags, timestamp
 *   SoulsFormats/Formats/Binder/BinderFile.cs   — public file struct
 */

#ifndef SOULS_FORMATS_SF_BINDER_H
#define SOULS_FORMATS_SF_BINDER_H

#include "sf_common.h"
#include "souls_formats/sf_dcx.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Forward declarations of opaque binder reader/writer types
 *
 * Concrete definitions live in src/archive/{bnd3,bnd4,bxf3,bxf4}.c and are
 * added in subsequent phase-3 tasks. They are forward-declared here so that
 * a TU including only <souls_formats/sf_binder.h> can already work with
 * sf_binder_file_t pointers on either side of a binder boundary.
 *===========================================================================*/
typedef struct sf_bnd         sf_bnd_t;
typedef struct sf_bnd2        sf_bnd2_t;
typedef struct sf_bnd3        sf_bnd3_t;
typedef struct sf_bnd4        sf_bnd4_t;
typedef struct sf_bxf3        sf_bxf3_t;
typedef struct sf_bxf4        sf_bxf4_t;
typedef struct sf_bnd2_reader sf_bnd2_reader_t;
typedef struct sf_bnd3_reader sf_bnd3_reader_t;
typedef struct sf_bnd4_reader sf_bnd4_reader_t;
typedef struct sf_bxf3_reader sf_bxf3_reader_t;
typedef struct sf_bxf4_reader sf_bxf4_reader_t;

/*===========================================================================
 * Binder.Format — feature-bitmask byte
 *
 * Upstream Binder.cs:17-63 declares this as `[Flags] enum Format : byte`.
 * C11 enums are int-sized in practice (no portable byte-sized enum),
 * which matters because this byte is stored verbatim inside binder file
 * headers. Use a uint8_t typedef + #define constants so callers always
 * get exactly one byte regardless of toolchain.
 *
 * Bit assignments are taken VERBATIM from Binder.cs and asserted below.
 *===========================================================================*/
typedef uint8_t sf_binder_format_t;

#define SF_BINDER_FORMAT_NONE         ((sf_binder_format_t)0x00)
#define SF_BINDER_FORMAT_BIG_ENDIAN   ((sf_binder_format_t)0x01) /* file is BE regardless of bit-BE byte */
#define SF_BINDER_FORMAT_IDS          ((sf_binder_format_t)0x02) /* files have ID numbers */
#define SF_BINDER_FORMAT_NAMES1       ((sf_binder_format_t)0x04) /* files have name strings (variant 1) */
#define SF_BINDER_FORMAT_NAMES2       ((sf_binder_format_t)0x08) /* files have name strings (variant 2) */
#define SF_BINDER_FORMAT_LONG_OFFSETS ((sf_binder_format_t)0x10) /* file data offsets are 64-bit */
#define SF_BINDER_FORMAT_COMPRESSION  ((sf_binder_format_t)0x20) /* files may be compressed */
#define SF_BINDER_FORMAT_FLAG6        ((sf_binder_format_t)0x40) /* unknown */
#define SF_BINDER_FORMAT_FLAG7        ((sf_binder_format_t)0x80) /* unknown */

_Static_assert(SF_BINDER_FORMAT_NONE         == 0x00, "binder format bit drift (NONE)");
_Static_assert(SF_BINDER_FORMAT_BIG_ENDIAN   == 0x01, "binder format bit drift (BIG_ENDIAN)");
_Static_assert(SF_BINDER_FORMAT_IDS          == 0x02, "binder format bit drift (IDS)");
_Static_assert(SF_BINDER_FORMAT_NAMES1       == 0x04, "binder format bit drift (NAMES1)");
_Static_assert(SF_BINDER_FORMAT_NAMES2       == 0x08, "binder format bit drift (NAMES2)");
_Static_assert(SF_BINDER_FORMAT_LONG_OFFSETS == 0x10, "binder format bit drift (LONG_OFFSETS)");
_Static_assert(SF_BINDER_FORMAT_COMPRESSION  == 0x20, "binder format bit drift (COMPRESSION)");
_Static_assert(SF_BINDER_FORMAT_FLAG6        == 0x40, "binder format bit drift (FLAG6)");
_Static_assert(SF_BINDER_FORMAT_FLAG7        == 0x80, "binder format bit drift (FLAG7)");

/*===========================================================================
 * Binder.FileFlags — per-entry feature byte
 *
 * Upstream Binder.cs:137-183. Same byte-size rationale as Format.
 *===========================================================================*/
typedef uint8_t sf_binder_file_flags_t;

#define SF_BINDER_FILE_FLAG_NONE       ((sf_binder_file_flags_t)0x00)
#define SF_BINDER_FILE_FLAG_COMPRESSED ((sf_binder_file_flags_t)0x01) /* file data is compressed */
#define SF_BINDER_FILE_FLAG_FLAG1      ((sf_binder_file_flags_t)0x02) /* unknown; standard for most files */
#define SF_BINDER_FILE_FLAG_FLAG2      ((sf_binder_file_flags_t)0x04) /* unknown */
#define SF_BINDER_FILE_FLAG_FLAG3      ((sf_binder_file_flags_t)0x08) /* unknown */
#define SF_BINDER_FILE_FLAG_FLAG4      ((sf_binder_file_flags_t)0x10) /* unknown */
#define SF_BINDER_FILE_FLAG_FLAG5      ((sf_binder_file_flags_t)0x20) /* unknown */
#define SF_BINDER_FILE_FLAG_FLAG6      ((sf_binder_file_flags_t)0x40) /* unknown */
#define SF_BINDER_FILE_FLAG_FLAG7      ((sf_binder_file_flags_t)0x80) /* unknown */

_Static_assert(SF_BINDER_FILE_FLAG_NONE       == 0x00, "binder file flag drift (NONE)");
_Static_assert(SF_BINDER_FILE_FLAG_COMPRESSED == 0x01, "binder file flag drift (COMPRESSED)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG1      == 0x02, "binder file flag drift (FLAG1)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG2      == 0x04, "binder file flag drift (FLAG2)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG3      == 0x08, "binder file flag drift (FLAG3)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG4      == 0x10, "binder file flag drift (FLAG4)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG5      == 0x20, "binder file flag drift (FLAG5)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG6      == 0x40, "binder file flag drift (FLAG6)");
_Static_assert(SF_BINDER_FILE_FLAG_FLAG7      == 0x80, "binder file flag drift (FLAG7)");

/*===========================================================================
 * sf_binder_file_t — a single entry inside a binder
 *
 * Mirrors upstream BinderFile.cs:8-75. Lifetime semantics:
 *
 *   - `name_utf8` and `data` are owned by the parent binder reader/writer
 *     handle. Callers MUST NOT free them; freeing the parent handle frees
 *     all entries.
 *   - When constructing entries to add to a writer, set `name_utf8` and
 *     `data` to caller-owned buffers; the writer copies them.
 *   - `id` is upstream Int32; -1 indicates "no ID" (also matches
 *     BinderFile()'s default-construction value).
 *   - `compression_info` preserves the original DCX preset across a
 *     read→write round-trip so the output binder is byte-identical when
 *     possible.
 *===========================================================================*/
typedef struct sf_binder_file {
    int32_t                   id;               /* -1 sentinel for "no ID"        */
    const char               *name_utf8;        /* NULL if no name; do not free   */
    const uint8_t            *data;             /* file payload; do not free      */
    size_t                    size;             /* uncompressed byte count        */
    sf_binder_file_flags_t    flags;            /* per-entry feature flags        */
    sf_dcx_compression_info_t compression_info; /* preset for round-trip         */
} sf_binder_file_t;

/*===========================================================================
 * sf_binder_datetime_t — a parsed BND/BXF timestamp
 *
 * Mirrors upstream Binder.BinderTimestampToDate / DateToBinderTimestamp
 * (Binder.cs:210-244). Stored fields are *raw* binder values (so both
 * letter-encoded and digit-encoded fields round-trip exactly). In
 * particular `month` and `hour` are 0-based (letter 'A' = 0), differing
 * from the C# DateTime convention, because the binder wire format itself
 * is 0-based.
 *
 * Range invariants for a successful round-trip:
 *   year   : 2000..2099                (only 2-digit year offsets fit)
 *   month  : 0..11                     ('A'..'L')
 *   day    : 1..31                     (no upstream validation beyond regex)
 *   hour   : 0..23                     ('A'..'X')
 *   minute : 0..59
 *===========================================================================*/
typedef struct sf_binder_datetime {
    int year;   /* 4-digit, e.g. 2007                          */
    int month;  /* 0-11 (raw upstream: letter 'A'=0..'L'=11)   */
    int day;    /* 1-31                                        */
    int hour;   /* 0-23 (raw upstream: letter 'A'=0..'X'=23)   */
    int minute; /* 0-59                                        */
} sf_binder_datetime_t;

/*===========================================================================
 * Format-byte helpers
 *
 * 1:1 mappings of Binder.HasIDs / Binder.ForceBigEndian static methods.
 *===========================================================================*/

/** True if the format has the IDs bit set (files carry int32 IDs). */
SF_API bool sf_binder_format_has_ids(sf_binder_format_t f);

/** True if the format has the Names1 bit set (files carry name strings). */
SF_API bool sf_binder_format_has_names1(sf_binder_format_t f);

/** True if the format has the Names2 bit set (files carry name strings). */
SF_API bool sf_binder_format_has_names2(sf_binder_format_t f);

/** True if file data offsets are stored as 64-bit values. */
SF_API bool sf_binder_format_has_long_offsets(sf_binder_format_t f);

/** True if the format supports per-file DCX compression. */
SF_API bool sf_binder_format_has_compression(sf_binder_format_t f);

/** True if Format.Flag6 is set (semantics unknown). */
SF_API bool sf_binder_format_has_flag6(sf_binder_format_t f);

/** True if Format.Flag7 is set (semantics unknown). */
SF_API bool sf_binder_format_has_flag7(sf_binder_format_t f);

/** True if the file is big-endian regardless of the bit-BE byte. */
SF_API bool sf_binder_format_force_big_endian(sf_binder_format_t f);

/*===========================================================================
 * Timestamp helpers
 *
 * Upstream's BinderTimestampToDate accepts arbitrary leading garbage
 * because Regex.Match scans for the first match. We are stricter: the
 * timestamp must start with the 5-group sequence. This is consistent
 * with how every upstream caller actually uses it (passes an exact
 * 8-byte field read from a binder header).
 *===========================================================================*/

/** Parse an 8-byte BND timestamp field (e.g. "07D7R6\0\0") into `out`.
 *  Returns SF_ERR_INVALID_ARG on a structurally invalid string and
 *  SF_ERR_OUT_OF_RANGE if numeric components fall outside the documented
 *  ranges (year offset > 99, month_letter > 'L', etc.). */
SF_API sf_result_t sf_binder_timestamp_parse(const char *timestamp,
                                             sf_binder_datetime_t *out);

/** Format `dt` into an 8-byte (+ trailing NUL) timestamp buffer. The
 *  buffer is filled with exactly the same byte sequence upstream's
 *  DateToBinderTimestamp produces — i.e. a printable prefix followed by
 *  '\0' padding to 8 bytes — and `out_timestamp[8]` is always set to
 *  '\0' so the buffer is also a valid C string.
 *
 *  Returns SF_ERR_OUT_OF_RANGE if year is outside 2000..2099, or any
 *  other field falls outside its documented range. */
SF_API sf_result_t sf_binder_timestamp_format(const sf_binder_datetime_t *dt,
                                              char out_timestamp[9]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BINDER_H */
