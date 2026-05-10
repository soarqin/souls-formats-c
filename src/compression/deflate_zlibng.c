/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "compression/compression_internal.h"
#include "internal/sf_internal.h"

#include <zlib.h>

#include <string.h>

static uint32_t adler32_sf(const uint8_t *data, size_t size) {
    uint32_t a = 1u;
    uint32_t b = 0u;
    for (size_t i = 0; i < size; i++) {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

sf_result_t sfi_deflate_raw_decompress(const void *in, size_t in_size, void *out, size_t out_size) {
    SF_CHECK_ARG((in || in_size == 0u) && (out || out_size == 0u));
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    zs.next_in = (Bytef *)in;
    zs.avail_in = (uInt)in_size;
    zs.next_out = (Bytef *)out;
    zs.avail_out = (uInt)out_size;
    if (inflateInit2(&zs, -15) != Z_OK) return SF_ERR_DECOMPRESS;
    int zr = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return (zr == Z_STREAM_END && zs.total_out == out_size) ? SF_OK : SF_ERR_DECOMPRESS;
}

sf_result_t sfi_deflate_raw_compress(const void *in, size_t in_size, int level, void **out,
                                     size_t *out_size, const sf_allocator_t *a) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);
    uLong bound = compressBound((uLong)in_size);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, (size_t)bound ? (size_t)bound : 1u);
    if (!buf) return SF_ERR_OOM;
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    zs.next_in = (Bytef *)in;
    zs.avail_in = (uInt)in_size;
    zs.next_out = buf;
    zs.avail_out = (uInt)bound;
    int zr = deflateInit2(&zs, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (zr == Z_OK) zr = deflate(&zs, Z_FINISH);
    if (zr == Z_STREAM_END) zr = deflateEnd(&zs);
    else (void)deflateEnd(&zs);
    if (zr != Z_OK) { sf_xfree(a, buf); return SF_ERR_DECOMPRESS; }
    *out = buf;
    *out_size = zs.total_out;
    return SF_OK;
}

sf_result_t sfi_zlib_decompress(const void *in, size_t in_size, void **out, size_t out_size,
                                const sf_allocator_t *a) {
    SF_CHECK_ARG(in && out);
    SF_RETURN_IF(in_size < 6u, SF_ERR_TRUNCATED);
    const uint8_t *p = (const uint8_t *)in;
    SF_RETURN_IF(p[0] != 0x78u, SF_ERR_BAD_MAGIC);
    SF_RETURN_IF(!(p[1] == 0x01u || p[1] == 0x5Eu || p[1] == 0x9Cu || p[1] == 0xDAu),
                 SF_ERR_BAD_MAGIC);
    uint8_t *buf = (uint8_t *)sf_xalloc(a, out_size ? out_size : 1u);
    if (!buf) return SF_ERR_OOM;
    sf_result_t r = sfi_deflate_raw_decompress(p + 2u, in_size - 6u, buf, out_size);
    uint32_t expect = ((uint32_t)p[in_size - 4u] << 24) | ((uint32_t)p[in_size - 3u] << 16) |
                      ((uint32_t)p[in_size - 2u] << 8) | (uint32_t)p[in_size - 1u];
    if (r == SF_OK && expect != adler32_sf(buf, out_size)) r = SF_ERR_DECOMPRESS;
    if (r != SF_OK) { sf_xfree(a, buf); return r; }
    *out = buf;
    return SF_OK;
}

sf_result_t sfi_zlib_compress(const void *in, size_t in_size, void **out, size_t *out_size,
                              const sf_allocator_t *a) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);
    void *raw = NULL;
    size_t raw_size = 0;
    sf_result_t r = sfi_deflate_raw_compress(in, in_size, Z_BEST_COMPRESSION, &raw, &raw_size, a);
    if (r != SF_OK) return r;
    uint8_t *buf = (uint8_t *)sf_xalloc(a, 2u + raw_size + 4u);
    if (!buf) { sf_xfree(a, raw); return SF_ERR_OOM; }
    buf[0] = 0x78u;
    buf[1] = 0xDAu;
    memcpy(buf + 2u, raw, raw_size);
    uint32_t ad = adler32_sf((const uint8_t *)in, in_size);
    buf[2u + raw_size] = (uint8_t)(ad >> 24);
    buf[3u + raw_size] = (uint8_t)(ad >> 16);
    buf[4u + raw_size] = (uint8_t)(ad >> 8);
    buf[5u + raw_size] = (uint8_t)ad;
    sf_xfree(a, raw);
    *out = buf;
    *out_size = 2u + raw_size + 4u;
    return SF_OK;
}
