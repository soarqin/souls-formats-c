/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "compression/compression_internal.h"
#include "internal/sf_internal.h"

#include <zstd.h>

sf_result_t sfi_zstd_decompress(const void *in, size_t in_size, void **out, size_t out_size,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(in && out);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, out_size ? out_size : 1u);
    if (!buf) return SF_ERR_OOM;
    size_t got = ZSTD_decompress(buf, out_size, in, in_size);
    if (ZSTD_isError(got) || got != out_size) { sf_xfree(a, buf); return SF_ERR_DECOMPRESS; }
    *out = buf;
    return SF_OK;
}

sf_result_t sfi_zstd_compress(const void *in, size_t in_size, int level, void **out,
                              size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);
    size_t cap = ZSTD_compressBound(in_size);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, cap ? cap : 1u);
    if (!buf) return SF_ERR_OOM;
    ZSTD_CCtx *ctx = ZSTD_createCCtx();
    if (!ctx) { sf_xfree(a, buf); return SF_ERR_OOM; }
    size_t zr = ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
    if (!ZSTD_isError(zr)) zr = ZSTD_CCtx_setParameter(ctx, ZSTD_c_contentSizeFlag, 0);
    if (!ZSTD_isError(zr)) zr = ZSTD_CCtx_setParameter(ctx, ZSTD_c_windowLog, 16);
    if (!ZSTD_isError(zr)) zr = ZSTD_compress2(ctx, buf, cap, in, in_size);
    ZSTD_freeCCtx(ctx);
    if (ZSTD_isError(zr)) { sf_xfree(a, buf); return SF_ERR_DECOMPRESS; }
    *out = buf;
    *out_size = zr;
    return SF_OK;
}
