/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * sf_path_* — PathHelper-equivalent path utilities.
 *
 * Mirrors upstream PathHelper (SoulsFormats/Utilities/IO/PathHelper.cs).
 * Pinned commit: 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
 */

#include "souls_formats/sf_path.h"
#include "souls_formats/sf_common.h"
#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------*/

static sf_result_t utf8_to_wide(const char *utf8, WCHAR **out_wide) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (n <= 0) return SF_ERR_INTERNAL;
    *out_wide = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (size_t)n * sizeof(WCHAR));
    if (!*out_wide) return SF_ERR_OOM;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, *out_wide, n);
    return SF_OK;
}

/* ---------------------------------------------------------------------------
 * sf_path_backup
 * Mirrors PathHelper.Backup (PathHelper.cs:13-19):
 *   bak = file + ".bak";
 *   if (overwrite || !File.Exists(bak)) File.Copy(file, bak, overwrite);
 *   return bak;
 * --------------------------------------------------------------------------*/
sf_result_t sf_path_backup(const char *utf8_path, bool overwrite,
                            char **out_backup_path,
                            const sf_allocator_t *alloc) {
    if (!utf8_path || !out_backup_path) return SF_ERR_INVALID_ARG;

    /* Build bak path = utf8_path + ".bak" */
    size_t src_len = strlen(utf8_path);
    size_t bak_len = src_len + 4 + 1; /* ".bak\0" */
    char *bak_utf8 = (char *)sf_xalloc(alloc, bak_len);
    if (!bak_utf8) return SF_ERR_OOM;
    memcpy(bak_utf8, utf8_path, src_len);
    memcpy(bak_utf8 + src_len, ".bak", 5);

    sf_result_t res = SF_OK;
    WCHAR *src_wide = NULL;
    WCHAR *bak_wide = NULL;

    res = utf8_to_wide(utf8_path, &src_wide);
    if (res != SF_OK) goto cleanup;
    res = utf8_to_wide(bak_utf8, &bak_wide);
    if (res != SF_OK) goto cleanup;

    /* Check if .bak exists (mirrors !File.Exists(bak)) */
    DWORD attrs = GetFileAttributesW(bak_wide);
    bool bak_exists = (attrs != INVALID_FILE_ATTRIBUTES);

    if (overwrite || !bak_exists) {
        /* File.Copy(file, bak, overwrite) */
        if (!CopyFileW(src_wide, bak_wide, overwrite ? FALSE : TRUE)) {
            res = SF_ERR_IO;
            goto cleanup;
        }
    }

    *out_backup_path = bak_utf8;
    bak_utf8 = NULL; /* transferred ownership */

cleanup:
    if (src_wide) HeapFree(GetProcessHeap(), 0, src_wide);
    if (bak_wide) HeapFree(GetProcessHeap(), 0, bak_wide);
    if (bak_utf8) sf_xfree(alloc, bak_utf8);
    return res;
}

/* ---------------------------------------------------------------------------
 * sf_path_get_real_extension
 * Mirrors PathHelper.GetRealExtension (PathHelper.cs:24-30):
 *   extension = Path.GetExtension(path);
 *   if (extension == ".dcx")
 *       extension = Path.GetExtension(Path.GetFileNameWithoutExtension(path));
 *   return extension;
 * --------------------------------------------------------------------------*/
sf_result_t sf_path_get_real_extension(const char *utf8_path, char **out_ext,
                                        const sf_allocator_t *alloc) {
    if (!utf8_path || !out_ext) return SF_ERR_INVALID_ARG;

    /* Find basename (after last / or \) */
    const char *base = utf8_path;
    for (const char *p = utf8_path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    /* Find last '.' in basename */
    const char *last_dot = NULL;
    for (const char *p = base; *p; p++) {
        if (*p == '.') last_dot = p;
    }

    const char *ext = last_dot ? last_dot : "";

    const char *ext_end = ext + strlen(ext); /* points to NUL of original string */

    const char *ext_end = base + strlen(base); /* points past NUL */
    if (strcmp(ext, ".dcx") == 0) {
        /* Strip .dcx: find the previous '.' in the name before .dcx */
        size_t name_len = (size_t)(last_dot - base);
        const char *prev_dot = NULL;
        for (size_t i = 0; i < name_len; i++) {
            if (base[i] == '.') prev_dot = base + i;
        }
        if (prev_dot) {
            ext = prev_dot;
            ext_end = last_dot; /* extension ends at the .dcx dot */
        } else {
            ext = "";
            ext_end = ext;
        }
    }

    size_t ext_len = (size_t)(ext_end - ext);
    char *buf = (char *)sf_xalloc(alloc, ext_len + 1);
    if (!buf) return SF_ERR_OOM;
    memcpy(buf, ext, ext_len);
    buf[ext_len] = '\0';
    *out_ext = buf;
    return SF_OK;
}

/* ---------------------------------------------------------------------------
 * sf_path_get_real_file_name
 * Mirrors PathHelper.GetRealFileName (PathHelper.cs:35-41):
 *   name = Path.GetFileNameWithoutExtension(path);
 *   if (Path.GetExtension(path) == ".dcx")
 *       name = Path.GetFileNameWithoutExtension(name);
 *   return name;
 * --------------------------------------------------------------------------*/
sf_result_t sf_path_get_real_file_name(const char *utf8_path, char **out_name,
                                        const sf_allocator_t *alloc) {
    if (!utf8_path || !out_name) return SF_ERR_INVALID_ARG;

    /* Find basename */
    const char *base = utf8_path;
    for (const char *p = utf8_path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    /* Find last '.' in basename */
    const char *last_dot = NULL;
    for (const char *p = base; *p; p++) {
        if (*p == '.') last_dot = p;
    }

    const char *ext = last_dot ? last_dot : "";
    size_t name_len = last_dot ? (size_t)(last_dot - base) : strlen(base);

    if (strcmp(ext, ".dcx") == 0) {
        /* Strip .dcx, then strip the inner extension */
        const char *prev_dot = NULL;
        for (size_t i = 0; i < name_len; i++) {
            if (base[i] == '.') prev_dot = base + i;
        }
        if (prev_dot) name_len = (size_t)(prev_dot - base);
    }

    char *buf = (char *)sf_xalloc(alloc, name_len + 1);
    if (!buf) return SF_ERR_OOM;
    memcpy(buf, base, name_len);
    buf[name_len] = '\0';
    *out_name = buf;
    return SF_OK;
}
