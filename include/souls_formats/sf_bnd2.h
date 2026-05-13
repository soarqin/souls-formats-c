/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — legacy Binder2 (BND2) archive container.
 *
 * Upstream reference (pinned commit, see docs/api-mapping/UPSTREAM.md):
 *   SoulsFormats/Formats/Binder/BND2/BND2.cs
 *   SoulsFormats/Formats/Binder/BND2/BND2FileHeader.cs
 *   SoulsFormats/Formats/Binder/BND2/BND2Reader.cs
 *   SoulsFormats/Formats/Binder/BND2/IBND2.cs
 */

#ifndef SOULS_FORMATS_SF_BND2_H
#define SOULS_FORMATS_SF_BND2_H

#include "souls_formats/sf_binder.h"
#include "sf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t sf_bnd2_file_path_mode_t;
#define SF_BND2_FILE_PATH_MODE_NAMELESS       ((sf_bnd2_file_path_mode_t)0)
#define SF_BND2_FILE_PATH_MODE_FILE_NAME      ((sf_bnd2_file_path_mode_t)1)
#define SF_BND2_FILE_PATH_MODE_FULL_PATH      ((sf_bnd2_file_path_mode_t)2)
#define SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY ((sf_bnd2_file_path_mode_t)3)

_Static_assert(SF_BND2_FILE_PATH_MODE_NAMELESS == 0, "BND2 path mode drift (Nameless)");
_Static_assert(SF_BND2_FILE_PATH_MODE_FILE_NAME == 1, "BND2 path mode drift (FileName)");
_Static_assert(SF_BND2_FILE_PATH_MODE_FULL_PATH == 2, "BND2 path mode drift (FullPath)");
_Static_assert(SF_BND2_FILE_PATH_MODE_BASE_DIRECTORY == 3, "BND2 path mode drift (BaseDirectory)");

typedef uint8_t sf_bnd2_header_info_flags_t;
#define SF_BND2_HEADER_INFO_HEADER_ITEM    ((sf_bnd2_header_info_flags_t)0x01)
#define SF_BND2_HEADER_INFO_ENDIAN         ((sf_bnd2_header_info_flags_t)0x02)
#define SF_BND2_HEADER_INFO_FILE_VERSION   ((sf_bnd2_header_info_flags_t)0x04)
#define SF_BND2_HEADER_INFO_FILE_SIZE      ((sf_bnd2_header_info_flags_t)0x08)
#define SF_BND2_HEADER_INFO_FILE_NUM       ((sf_bnd2_header_info_flags_t)0x10)
#define SF_BND2_HEADER_INFO_BASE_DIR_OFFSET ((sf_bnd2_header_info_flags_t)0x20)
#define SF_BND2_HEADER_INFO_ALIGNMENT_SIZE ((sf_bnd2_header_info_flags_t)0x40)
#define SF_BND2_HEADER_INFO_OPTION         ((sf_bnd2_header_info_flags_t)0x80)

_Static_assert(SF_BND2_HEADER_INFO_HEADER_ITEM == 0x01, "BND2 header flag drift (HeaderItem)");
_Static_assert(SF_BND2_HEADER_INFO_ENDIAN == 0x02, "BND2 header flag drift (Endian)");
_Static_assert(SF_BND2_HEADER_INFO_FILE_VERSION == 0x04, "BND2 header flag drift (FileVersion)");
_Static_assert(SF_BND2_HEADER_INFO_FILE_SIZE == 0x08, "BND2 header flag drift (FileSize)");
_Static_assert(SF_BND2_HEADER_INFO_FILE_NUM == 0x10, "BND2 header flag drift (FileNum)");
_Static_assert(SF_BND2_HEADER_INFO_BASE_DIR_OFFSET == 0x20, "BND2 header flag drift (BaseDirOffset)");
_Static_assert(SF_BND2_HEADER_INFO_ALIGNMENT_SIZE == 0x40, "BND2 header flag drift (AlignmentSize)");
_Static_assert(SF_BND2_HEADER_INFO_OPTION == 0x80, "BND2 header flag drift (Option)");

typedef uint8_t sf_bnd2_file_info_flags_t;
#define SF_BND2_FILE_INFO_ID          ((sf_bnd2_file_info_flags_t)0x01)
#define SF_BND2_FILE_INFO_OFFSET      ((sf_bnd2_file_info_flags_t)0x02)
#define SF_BND2_FILE_INFO_SIZE        ((sf_bnd2_file_info_flags_t)0x04)
#define SF_BND2_FILE_INFO_NAME_OFFSET ((sf_bnd2_file_info_flags_t)0x08)
#define SF_BND2_FILE_INFO_FLAG5       ((sf_bnd2_file_info_flags_t)0x10)
#define SF_BND2_FILE_INFO_FLAG6       ((sf_bnd2_file_info_flags_t)0x20)
#define SF_BND2_FILE_INFO_FLAG7       ((sf_bnd2_file_info_flags_t)0x40)
#define SF_BND2_FILE_INFO_FLAG8       ((sf_bnd2_file_info_flags_t)0x80)

_Static_assert(SF_BND2_FILE_INFO_ID == 0x01, "BND2 file flag drift (ID)");
_Static_assert(SF_BND2_FILE_INFO_OFFSET == 0x02, "BND2 file flag drift (Offset)");
_Static_assert(SF_BND2_FILE_INFO_SIZE == 0x04, "BND2 file flag drift (Size)");
_Static_assert(SF_BND2_FILE_INFO_NAME_OFFSET == 0x08, "BND2 file flag drift (NameOffset)");
_Static_assert(SF_BND2_FILE_INFO_FLAG5 == 0x10, "BND2 file flag drift (Flag5)");
_Static_assert(SF_BND2_FILE_INFO_FLAG6 == 0x20, "BND2 file flag drift (Flag6)");
_Static_assert(SF_BND2_FILE_INFO_FLAG7 == 0x40, "BND2 file flag drift (Flag7)");
_Static_assert(SF_BND2_FILE_INFO_FLAG8 == 0x80, "BND2 file flag drift (Flag8)");

typedef struct sf_bnd2_file {
    int32_t        id;
    const char    *name_utf8;
    const uint8_t *data;
    size_t         size;
} sf_bnd2_file_t;

typedef struct sf_bnd2_file_header {
    int32_t     id;
    const char *name_utf8;
    int32_t     offset;
    int32_t     size;
} sf_bnd2_file_header_t;

SF_API bool sf_bnd2_is_format(const uint8_t *data, size_t size);

SF_API sf_result_t sf_bnd2_create(sf_bnd2_t **out, const sf_allocator_t *a);
SF_API void        sf_bnd2_destroy(sf_bnd2_t *b);

SF_API sf_result_t sf_bnd2_read_from_path(sf_bnd2_t **out, const wchar_t *path,
                                          const sf_allocator_t *a);
SF_API sf_result_t sf_bnd2_read_from_memory(sf_bnd2_t **out, const uint8_t *data,
                                            size_t size, const sf_allocator_t *a);
SF_API sf_result_t sf_bnd2_write_to_path(const sf_bnd2_t *b, const wchar_t *path);
SF_API sf_result_t sf_bnd2_write_to_memory(const sf_bnd2_t *b, uint8_t **out,
                                           size_t *out_size, const sf_allocator_t *a);

SF_API size_t                sf_bnd2_file_count(const sf_bnd2_t *b);
SF_API const sf_bnd2_file_t *sf_bnd2_get_file(const sf_bnd2_t *b, size_t index);
SF_API sf_result_t           sf_bnd2_add_file(sf_bnd2_t *b, const sf_bnd2_file_t *file);
SF_API sf_result_t           sf_bnd2_remove_file(sf_bnd2_t *b, size_t index);

SF_API sf_bnd2_header_info_flags_t sf_bnd2_get_header_info_flags(const sf_bnd2_t *b);
SF_API sf_bnd2_file_info_flags_t   sf_bnd2_get_file_info_flags(const sf_bnd2_t *b);
SF_API uint8_t                     sf_bnd2_get_unk06(const sf_bnd2_t *b);
SF_API uint8_t                     sf_bnd2_get_unk07(const sf_bnd2_t *b);
SF_API int32_t                     sf_bnd2_get_file_version(const sf_bnd2_t *b);
SF_API uint16_t                    sf_bnd2_get_alignment_size(const sf_bnd2_t *b);
SF_API sf_bnd2_file_path_mode_t    sf_bnd2_get_file_path_mode(const sf_bnd2_t *b);
SF_API uint8_t                     sf_bnd2_get_unk1b(const sf_bnd2_t *b);
SF_API const char                 *sf_bnd2_get_base_directory(const sf_bnd2_t *b);

SF_API void sf_bnd2_set_header_info_flags(sf_bnd2_t *b, sf_bnd2_header_info_flags_t v);
SF_API void sf_bnd2_set_file_info_flags(sf_bnd2_t *b, sf_bnd2_file_info_flags_t v);
SF_API void sf_bnd2_set_unk06(sf_bnd2_t *b, uint8_t v);
SF_API void sf_bnd2_set_unk07(sf_bnd2_t *b, uint8_t v);
SF_API void sf_bnd2_set_file_version(sf_bnd2_t *b, int32_t v);
SF_API void sf_bnd2_set_alignment_size(sf_bnd2_t *b, uint16_t v);
SF_API void sf_bnd2_set_file_path_mode(sf_bnd2_t *b, sf_bnd2_file_path_mode_t v);
SF_API void sf_bnd2_set_unk1b(sf_bnd2_t *b, uint8_t v);
SF_API void sf_bnd2_set_base_directory(sf_bnd2_t *b, const char *base_directory_utf8);

SF_API sf_result_t sf_bnd2_reader_open(sf_bnd2_reader_t **out, const wchar_t *path,
                                       const sf_allocator_t *a);
SF_API void        sf_bnd2_reader_close(sf_bnd2_reader_t *r);
SF_API size_t      sf_bnd2_reader_file_count(const sf_bnd2_reader_t *r);
SF_API const sf_bnd2_file_header_t *sf_bnd2_reader_get_file_header(
    const sf_bnd2_reader_t *r, size_t index);
SF_API sf_result_t sf_bnd2_reader_read_file_by_index(sf_bnd2_reader_t *r, size_t index,
                                                     uint8_t **out, size_t *out_size,
                                                     const sf_allocator_t *a);
SF_API sf_result_t sf_bnd2_reader_read_file_by_id(sf_bnd2_reader_t *r, int32_t id,
                                                  uint8_t **out, size_t *out_size,
                                                  const sf_allocator_t *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_BND2_H */
