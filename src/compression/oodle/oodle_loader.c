/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "compression/oodle/oodle_loader.h"
#include "souls_formats/sf_oodle.h"
#include "internal/sf_internal.h"

#include <stdint.h>
#include <string.h>
#include <wchar.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef WINAPI
#define WINAPI __stdcall
#endif
typedef void *HMODULE;
typedef intptr_t (__stdcall *FARPROC)(void);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(const wchar_t *path);
__declspec(dllimport) int WINAPI FreeLibrary(HMODULE module);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE module, const char *name);

typedef int64_t (__stdcall *OodleLZ_Decompress_t)(const void *, int64_t, void *, int64_t, int,
                                                  int, int, void *, int64_t, void *, void *,
                                                  void *, int64_t, int);
typedef int64_t (__stdcall *OodleLZ_Compress_t)(int, const void *, int64_t, void *, int,
                                                const void *, void *, void *, void *, int64_t);
typedef int64_t (__stdcall *OodleLZ_GetCompressedBufferSizeNeeded_t)(int64_t);
typedef int64_t (__stdcall *OodleLZ_GetCompressedBufferSizeNeeded_v8_t)(uint8_t, int64_t);
typedef void *(__stdcall *OodleLZ_CompressOptions_GetDefault_t)(int, int);
typedef void *(__stdcall *OodleLZ_CompressOptions_GetDefault_v8_t)(void);

typedef struct oodle_options {
    uint32_t verbosity;
    int32_t minMatchLen;
    int32_t seekChunkReset;
    int32_t seekChunkLen;
    int32_t profile;
    int32_t dictionarySize;
    int32_t spaceSpeedTradeoffBytes;
    int32_t maxHuffmansPerChunk;
    int32_t sendQuantumCRCs;
    int32_t maxLocalDictionarySize;
    int32_t makeLongRangeMatcher;
    int32_t matchTableSizeLog2;
} oodle_options_t;

static HMODULE g_oodle;
static sf_oodle_version_t g_version;
static bool g_tried_failed;
static wchar_t g_search_path[MAX_PATH];
static OodleLZ_Decompress_t g_decompress;
static OodleLZ_Compress_t g_compress;
static OodleLZ_GetCompressedBufferSizeNeeded_t g_bound;
static OodleLZ_GetCompressedBufferSizeNeeded_v8_t g_bound_v8;
static OodleLZ_CompressOptions_GetDefault_t g_options;
static OodleLZ_CompressOptions_GetDefault_v8_t g_options_v8;

static FARPROC sym(const char *name) {
    return g_oodle ? GetProcAddress(g_oodle, name) : NULL;
}

static sf_result_t bind_symbols(void) {
    union { FARPROC fp; OodleLZ_Decompress_t fn; } dec = { sym("OodleLZ_Decompress") };
    union { FARPROC fp; OodleLZ_Compress_t fn; } comp = { sym("OodleLZ_Compress") };
    union { FARPROC fp; OodleLZ_GetCompressedBufferSizeNeeded_t fn; } bound = { sym("OodleLZ_GetCompressedBufferSizeNeeded") };
    union { FARPROC fp; OodleLZ_GetCompressedBufferSizeNeeded_v8_t fn; } bound_v8 = { sym("OodleLZ_GetCompressedBufferSizeNeeded") };
    union { FARPROC fp; OodleLZ_CompressOptions_GetDefault_t fn; } opts = { sym("OodleLZ_CompressOptions_GetDefault") };
    union { FARPROC fp; OodleLZ_CompressOptions_GetDefault_v8_t fn; } opts_v8 = { sym("OodleLZ_CompressOptions_GetDefault") };
    g_decompress = dec.fn;
    g_compress = comp.fn;
    g_bound = bound.fn;
    g_bound_v8 = bound_v8.fn;
    g_options = opts.fn;
    g_options_v8 = opts_v8.fn;
    return (g_decompress && g_compress && g_bound && g_options) ? SF_OK : SF_ERR_OODLE_NOT_FOUND;
}

static bool make_path(wchar_t *dst, size_t dst_count, int version) {
    const wchar_t *prefix = g_search_path[0] ? g_search_path : L".";
    int n = swprintf(dst, dst_count, L"%ls%ls%ls%d%ls", prefix,
                     (prefix[wcslen(prefix) - 1u] == L'\\' || prefix[wcslen(prefix) - 1u] == L'/') ? L"" : L"\\",
                     L"oo2core_", version, L"_win64.dll");
    return n > 0 && (size_t)n < dst_count;
}

sf_result_t sf_oodle_set_search_path(const wchar_t *dir) {
    SF_CHECK_ARG(dir);
    size_t n = wcslen(dir);
    SF_RETURN_IF(n >= MAX_PATH, SF_ERR_OUT_OF_RANGE);
    memcpy(g_search_path, dir, (n + 1u) * sizeof(wchar_t));
    g_tried_failed = false;
    return SF_OK;
}

sf_result_t sf_oodle_load(void) {
    if (g_oodle) return SF_OK;
    if (g_tried_failed) return SF_ERR_OODLE_NOT_FOUND;
    const sf_oodle_version_t versions[] = {SF_OODLE_VERSION_9, SF_OODLE_VERSION_8,
                                           SF_OODLE_VERSION_6};
    for (size_t i = 0; i < sizeof(versions) / sizeof(versions[0]); i++) {
        wchar_t path[MAX_PATH];
        if (!make_path(path, MAX_PATH, versions[i])) continue;
        g_oodle = LoadLibraryW(path);
        if (!g_oodle) continue;
        g_version = versions[i];
        if (bind_symbols() == SF_OK) return SF_OK;
        FreeLibrary(g_oodle);
        g_oodle = NULL;
        g_version = SF_OODLE_VERSION_UNKNOWN;
    }
    g_tried_failed = true;
    return SF_ERR_OODLE_NOT_FOUND;
}

void sf_oodle_unload(void) {
    if (g_oodle) FreeLibrary(g_oodle);
    g_oodle = NULL;
    g_version = SF_OODLE_VERSION_UNKNOWN;
    g_decompress = NULL;
    g_compress = NULL;
    g_bound = NULL;
    g_bound_v8 = NULL;
    g_options = NULL;
    g_options_v8 = NULL;
}

sf_oodle_version_t sf_oodle_version(void) {
    return g_version;
}

sf_result_t sfi_oodle_decompress(const void *in, size_t in_size, void *out, size_t out_size) {
    SF_CHECK_ARG(in && out);
    SF_RETURN_IF(sf_oodle_load() != SF_OK, SF_ERR_OODLE_NOT_FOUND);
    int64_t got = g_decompress(in, (int64_t)in_size, out, (int64_t)out_size, 1, 0, 0, NULL, 0,
                              NULL, NULL, NULL, 0, 3);
    return got == (int64_t)out_size ? SF_OK : SF_ERR_DECOMPRESS;
}

sf_result_t sfi_oodle_compress(int level, const void *in, size_t in_size, void **out,
                               size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);
    SF_RETURN_IF(sf_oodle_load() != SF_OK, SF_ERR_OODLE_NOT_FOUND);
    int64_t bound = g_version == SF_OODLE_VERSION_6 ? g_bound((int64_t)in_size)
                                                    : g_bound_v8(0, (int64_t)in_size);
    SF_RETURN_IF(bound <= 0, SF_ERR_DECOMPRESS);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, (size_t)bound);
    if (!buf) return SF_ERR_OOM;
    oodle_options_t opts;
    memset(&opts, 0, sizeof(opts));
    void *def = g_version == SF_OODLE_VERSION_6 ? g_options(8, 4) : g_options_v8();
    if (def) memcpy(&opts, def, sizeof(opts));
    opts.seekChunkReset = 1;
    opts.seekChunkLen = 0x40000;
    int64_t got = g_compress(8, in, (int64_t)in_size, buf, level, &opts, NULL, NULL, NULL, 0);
    if (got <= 0) { sf_xfree(a, buf); return SF_ERR_DECOMPRESS; }
    *out = buf;
    *out_size = (size_t)got;
    return SF_OK;
}
