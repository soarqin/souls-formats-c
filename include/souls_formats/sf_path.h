/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — PathHelper-equivalent path utilities.
 *
 * Mirrors upstream PathHelper (SoulsFormats/Utilities/IO/PathHelper.cs).
 * Pinned commit: 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 */

#ifndef SOULS_FORMATS_SF_PATH_H
#define SOULS_FORMATS_SF_PATH_H

#include "souls_formats/sf_common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sf_path_backup — mirrors PathHelper.Backup (PathHelper.cs:13-19).
 *
 * Creates "<utf8_path>.bak". If overwrite==false AND .bak already exists,
 * does NOT copy (returns the .bak path unchanged). If overwrite==true,
 * forcibly overwrites. Upstream default: overwrite=false (no default in C;
 * document per POLICY.md §4).
 *
 * Uses Win32 CopyFileW. out_backup_path is heap-owned by caller;
 * free with sf_free(alloc, ptr).
 */
SF_API sf_result_t sf_path_backup(
    const char          *utf8_path,
    bool                 overwrite,
    char               **out_backup_path,
    const sf_allocator_t *alloc);

/*
 * sf_path_get_real_extension — mirrors PathHelper.GetRealExtension (PathHelper.cs:24-30).
 *
 * If the rightmost extension is ".dcx", returns the next-level extension.
 * Otherwise returns the single extension. Returns "" for paths with no extension.
 * Examples: "bar.flver.dcx" → ".flver", "bar.txt" → ".txt", "bar" → "".
 *
 * out_ext is heap-owned by caller; free with sf_free(alloc, ptr).
 */
SF_API sf_result_t sf_path_get_real_extension(
    const char          *utf8_path,
    char               **out_ext,
    const sf_allocator_t *alloc);

/*
 * sf_path_get_real_file_name — mirrors PathHelper.GetRealFileName (PathHelper.cs:35-41).
 *
 * If the rightmost extension is ".dcx", strips both ".dcx" and the inner extension.
 * Otherwise strips the single extension.
 * Examples: "bar.flver.dcx" → "bar", "bar.txt" → "bar", "/path/bar.flver.dcx" → "bar".
 *
 * out_name is heap-owned by caller; free with sf_free(alloc, ptr).
 */
SF_API sf_result_t sf_path_get_real_file_name(
    const char          *utf8_path,
    char               **out_name,
    const sf_allocator_t *alloc);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SOULS_FORMATS_SF_PATH_H */
