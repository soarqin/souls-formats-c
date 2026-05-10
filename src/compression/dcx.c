/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"
#include "compression/compression_internal.h"
#include "compression/oodle/oodle_loader.h"
#include "internal/sf_internal.h"

#include <zlib.h>

#include <string.h>

_Static_assert(SF_DCX_TYPE_COUNT_ == 9, "sf_dcx_type_t table drift");

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static bool tag(const uint8_t *p, const char *s) {
    return memcmp(p, s, 4) == 0;
}

static size_t align16(size_t n) {
    return (n + 15u) & ~(size_t)15u;
}

static void init_info(sf_dcx_compression_info_t *info, sf_dcx_type_t type) {
    memset(info, 0, sizeof(*info));
    info->type = type;
    if (type == SF_DCX_TYPE_DCX_KRAK) {
        info->u.dcx_krak.oodle_compressor_type = SF_OODLE_LZ_COMPRESSOR_KRAKEN;
    }
}

static bool has_dcx_magic(const uint8_t *buf, size_t size) {
    if (!buf || size < 4u) return false;
    return tag(buf, "DCP\0") || tag(buf, "DCX\0");
}

sf_result_t sf_dcx_compression_info_from_dflt_preset(
    sf_dcx_dflt_compression_preset_t preset, sf_dcx_compression_info_t *out) {
    SF_CHECK_ARG(out);

    init_info(out, SF_DCX_TYPE_DCX_DFLT);
    switch (preset) {
    case SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_24_9:
        out->u.dcx_dflt.unk04 = 0x10000;
        out->u.dcx_dflt.unk10 = 0x24;
        out->u.dcx_dflt.unk14 = 0x2C;
        out->u.dcx_dflt.unk30 = 9;
        out->u.dcx_dflt.unk38 = 0;
        return SF_OK;
    case SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_44_9:
        out->u.dcx_dflt.unk04 = 0x10000;
        out->u.dcx_dflt.unk10 = 0x44;
        out->u.dcx_dflt.unk14 = 0x4C;
        out->u.dcx_dflt.unk30 = 9;
        out->u.dcx_dflt.unk38 = 0;
        return SF_OK;
    case SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_8:
        out->u.dcx_dflt.unk04 = 0x11000;
        out->u.dcx_dflt.unk10 = 0x44;
        out->u.dcx_dflt.unk14 = 0x4C;
        out->u.dcx_dflt.unk30 = 8;
        out->u.dcx_dflt.unk38 = 0;
        return SF_OK;
    case SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9:
        out->u.dcx_dflt.unk04 = 0x11000;
        out->u.dcx_dflt.unk10 = 0x44;
        out->u.dcx_dflt.unk14 = 0x4C;
        out->u.dcx_dflt.unk30 = 9;
        out->u.dcx_dflt.unk38 = 0;
        return SF_OK;
    case SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9_15:
        out->u.dcx_dflt.unk04 = 0x11000;
        out->u.dcx_dflt.unk10 = 0x44;
        out->u.dcx_dflt.unk14 = 0x4C;
        out->u.dcx_dflt.unk30 = 9;
        out->u.dcx_dflt.unk38 = 15;
        return SF_OK;
    default:
        memset(out, 0, sizeof(*out));
        out->type = SF_DCX_TYPE_UNKNOWN;
        return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_dcx_compression_info_from_krak_preset(
    sf_dcx_krak_compression_preset_t preset, sf_dcx_compression_info_t *out) {
    SF_CHECK_ARG(out);

    init_info(out, SF_DCX_TYPE_DCX_KRAK);
    out->u.dcx_krak.oodle_compressor_type = SF_OODLE_LZ_COMPRESSOR_KRAKEN;
    switch (preset) {
    case SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING:
        out->u.dcx_krak.compression_level = 6;
        return SF_OK;
    case SF_DCX_KRAK_COMPRESSION_PRESET_ARMORED_CORE_6:
        out->u.dcx_krak.compression_level = 9;
        return SF_OK;
    default:
        memset(out, 0, sizeof(*out));
        out->type = SF_DCX_TYPE_UNKNOWN;
        return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_dcx_compression_info_from_default_type(
    sf_dcx_default_type_t default_type, sf_dcx_compression_info_t *out) {
    SF_CHECK_ARG(out);

    switch (default_type) {
    case SF_DCX_DEFAULT_TYPE_DEMONS_SOULS:
        init_info(out, SF_DCX_TYPE_DCX_EDGE);
        return SF_OK;
    case SF_DCX_DEFAULT_TYPE_DARK_SOULS_1:
    case SF_DCX_DEFAULT_TYPE_DARK_SOULS_2:
        return sf_dcx_compression_info_from_dflt_preset(
            SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_24_9, out);
    case SF_DCX_DEFAULT_TYPE_BLOODBORNE:
    case SF_DCX_DEFAULT_TYPE_DARK_SOULS_3:
        return sf_dcx_compression_info_from_dflt_preset(
            SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_10000_44_9, out);
    case SF_DCX_DEFAULT_TYPE_SEKIRO:
    case SF_DCX_DEFAULT_TYPE_ELDEN_RING:
        return sf_dcx_compression_info_from_krak_preset(
            SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING, out);
    case SF_DCX_DEFAULT_TYPE_AC6:
        return sf_dcx_compression_info_from_krak_preset(
            SF_DCX_KRAK_COMPRESSION_PRESET_ARMORED_CORE_6, out);
    default:
        memset(out, 0, sizeof(*out));
        out->type = SF_DCX_TYPE_UNKNOWN;
        return SF_ERR_INVALID_ARG;
    }
}

static sf_result_t default_info_from_type(sf_dcx_type_t type, sf_dcx_compression_info_t *out) {
    SF_CHECK_ARG(out);

    switch (type) {
    case SF_DCX_TYPE_UNKNOWN:
    case SF_DCX_TYPE_NONE:
    case SF_DCX_TYPE_ZLIB:
    case SF_DCX_TYPE_DCP_EDGE:
    case SF_DCX_TYPE_DCP_DFLT:
    case SF_DCX_TYPE_DCX_EDGE:
        init_info(out, type);
        return SF_OK;
    case SF_DCX_TYPE_DCX_DFLT:
        return sf_dcx_compression_info_from_dflt_preset(
            SF_DCX_DFLT_COMPRESSION_PRESET_DCX_DFLT_11000_44_9, out);
    case SF_DCX_TYPE_DCX_KRAK:
        return sf_dcx_compression_info_from_krak_preset(
            SF_DCX_KRAK_COMPRESSION_PRESET_ELDEN_RING, out);
    case SF_DCX_TYPE_DCX_ZSTD:
        init_info(out, SF_DCX_TYPE_DCX_ZSTD);
        out->u.dcx_zstd.compression_level = 15;
        return SF_OK;
    default:
        return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_dcx_sniff(const void *buf, size_t size, sf_dcx_type_t *out_type) {
    SF_CHECK_ARG(out_type);
    *out_type = SF_DCX_TYPE_UNKNOWN;
    if (!buf || size < 4u) return SF_OK;

    const uint8_t *p = (const uint8_t *)buf;
    if (tag(p, "DCP\0")) {
        if (size >= 8u && tag(p + 4u, "DFLT")) {
            *out_type = SF_DCX_TYPE_DCP_DFLT;
        } else if (size >= 8u && tag(p + 4u, "EDGE")) {
            *out_type = SF_DCX_TYPE_DCP_EDGE;
        }
    } else if (tag(p, "DCX\0")) {
        if (size >= 0x2Cu && tag(p + 0x28u, "EDGE")) {
            *out_type = SF_DCX_TYPE_DCX_EDGE;
        } else if (size >= 0x2Cu && tag(p + 0x28u, "DFLT")) {
            *out_type = SF_DCX_TYPE_DCX_DFLT;
        } else if (size >= 0x2Cu && tag(p + 0x28u, "KRAK")) {
            *out_type = SF_DCX_TYPE_DCX_KRAK;
        } else if (size >= 0x2Cu && tag(p + 0x28u, "ZSTD")) {
            *out_type = SF_DCX_TYPE_DCX_ZSTD;
        }
    } else if (p[0] == 0x78u &&
               (p[1] == 0x01u || p[1] == 0x5Eu || p[1] == 0x9Cu || p[1] == 0xDAu)) {
        *out_type = SF_DCX_TYPE_ZLIB;
    }
    return SF_OK;
}

sf_result_t sf_dcx_is_from_buffer(const uint8_t *buf, size_t size, bool *out) {
    SF_CHECK_ARG(out);
    if (size > 0u) SF_CHECK_ARG(buf);

    *out = has_dcx_magic(buf, size);
    return SF_OK;
}

sf_result_t sf_dcx_is_from_stream(sf_istream_t *stream, bool *out) {
    SF_CHECK_ARG(stream && out);
    *out = false;

    int64_t pos = sf_istream_position(stream);
    SF_RETURN_IF(pos != 0, SF_ERR_INVALID_ARG);

    int64_t len = sf_istream_length(stream);
    SF_RETURN_IF(len < 0, SF_ERR_IO);
    if (len < 4) return SF_OK;

    uint8_t magic[4];
    sf_result_t r = sf_istream_read(stream, magic, sizeof(magic));
    if (r != SF_OK) return r;
    r = sf_istream_seek(stream, 0);
    if (r != SF_OK) return r;

    *out = has_dcx_magic(magic, sizeof(magic));
    return SF_OK;
}

sf_result_t sf_dcx_is_from_path(const char *utf8_path, bool *out) {
    SF_CHECK_ARG(utf8_path && out);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_file(&stream, utf8_path, NULL);
    if (r != SF_OK) return r;
    r = sf_dcx_is_from_stream(stream, out);
    sf_istream_close(stream);
    return r;
}

static sf_result_t read_compression_info(const uint8_t *p, size_t n,
                                         sf_dcx_compression_info_t *out) {
    SF_CHECK_ARG(out);

    sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
    sf_result_t r = sf_dcx_sniff(p, n, &type);
    if (r != SF_OK) return r;

    init_info(out, type);
    switch (type) {
    case SF_DCX_TYPE_ZLIB:
    case SF_DCX_TYPE_DCP_DFLT:
    case SF_DCX_TYPE_DCP_EDGE:
    case SF_DCX_TYPE_DCX_EDGE:
        return SF_OK;
    case SF_DCX_TYPE_DCX_DFLT:
        SF_RETURN_IF(n < 0x39u, SF_ERR_TRUNCATED);
        out->u.dcx_dflt.unk04 = (int32_t)be32(p + 0x04u);
        out->u.dcx_dflt.unk10 = (int32_t)be32(p + 0x10u);
        out->u.dcx_dflt.unk14 = (int32_t)be32(p + 0x14u);
        out->u.dcx_dflt.unk30 = p[0x30u];
        out->u.dcx_dflt.unk38 = p[0x38u];
        return SF_OK;
    case SF_DCX_TYPE_DCX_KRAK:
        SF_RETURN_IF(n < 0x31u, SF_ERR_TRUNCATED);
        out->u.dcx_krak.compression_level = p[0x30u];
        out->u.dcx_krak.oodle_compressor_type = SF_OODLE_LZ_COMPRESSOR_KRAKEN;
        return SF_OK;
    case SF_DCX_TYPE_DCX_ZSTD:
        SF_RETURN_IF(n < 0x31u, SF_ERR_TRUNCATED);
        out->u.dcx_zstd.compression_level = p[0x30u];
        return SF_OK;
    case SF_DCX_TYPE_UNKNOWN:
    case SF_DCX_TYPE_NONE:
    default:
        return SF_OK;
    }
}

static sf_result_t decompress_plain_zlib(const void *in, size_t in_size, uint8_t **out,
                                         size_t *out_size, const sf_allocator_t *alloc) {
    size_t cap = in_size * 4u + 1024u;
    if (cap < 4096u) cap = 4096u;

    for (unsigned tries = 0; tries < 12u; tries++) {
        uint8_t *buf = (uint8_t *)sf_xalloc(alloc, cap);
        if (!buf) return SF_ERR_OOM;

        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        zs.next_in = (Bytef *)in;
        zs.avail_in = (uInt)in_size;
        zs.next_out = buf;
        zs.avail_out = (uInt)cap;

        int zr = inflateInit2(&zs, 15);
        if (zr == Z_OK) zr = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
        if (zr == Z_STREAM_END) {
            *out = buf;
            *out_size = zs.total_out;
            return SF_OK;
        }

        sf_xfree(alloc, buf);
        cap *= 2u;
    }

    return SF_ERR_DECOMPRESS;
}

static sf_result_t decompress_edge(const uint8_t *p, size_t n, bool dcx, uint8_t **out,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    size_t pos = dcx ? 0x18u : 0x20u;
    SF_RETURN_IF(n < pos + 12u || !tag(p + pos, "DCS\0"), SF_ERR_BAD_MAGIC);
    uint32_t usize = be32(p + pos + 4u);
    uint32_t csize = be32(p + pos + 8u);

    uint8_t *dst = (uint8_t *)sf_xalloc(alloc, usize ? usize : 1u);
    if (!dst) return SF_ERR_OOM;

    size_t data_start = 0;
    size_t table = 0;
    uint32_t count = 0;
    if (dcx) {
        SF_RETURN_IF(n < 0x50u || !tag(p + 0x18u, "DCS\0") ||
                         !tag(p + 0x24u, "DCP\0") || !tag(p + 0x28u, "EDGE"),
                     SF_ERR_BAD_MAGIC);
        size_t dca_start = 0x44u;
        uint32_t dca_size = be32(p + dca_start + 4u);
        table = dca_start + 8u + 0x24u;
        count = be32(p + dca_start + 8u + 0x1Cu);
        data_start = dca_start + dca_size;
    } else {
        data_start = 0x30u;
        size_t dca = data_start + csize;
        SF_RETURN_IF(n < dca + 0x28u || !tag(p + dca, "DCA\0") ||
                         !tag(p + dca + 8u, "EgdT"),
                     SF_ERR_BAD_MAGIC);
        table = dca + 8u + 0x20u;
        count = be32(p + dca + 8u + 0x18u);
    }

    SF_RETURN_IF(count > 0x100000u || table + (size_t)count * 16u > n,
                 SF_ERR_TRUNCATED);

    size_t out_pos = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *entry = p + table + (size_t)i * 16u;
        uint32_t off = be32(entry + 4u);
        uint32_t sz = be32(entry + 8u);
        uint32_t comp = be32(entry + 12u);
        size_t chunk_usize = usize - out_pos;
        if (chunk_usize > 0x10000u) chunk_usize = 0x10000u;

        SF_RETURN_IF(data_start + off + sz > n || out_pos + chunk_usize > usize,
                     SF_ERR_TRUNCATED);

        sf_result_t r = SF_OK;
        if (comp) {
            r = sfi_deflate_raw_decompress(p + data_start + off, sz, dst + out_pos,
                                           chunk_usize);
        } else {
            memcpy(dst + out_pos, p + data_start + off, sz);
        }
        if (r != SF_OK) {
            sf_xfree(alloc, dst);
            return r;
        }
        out_pos += comp ? chunk_usize : sz;
    }

    *out = dst;
    *out_size = usize;
    return SF_OK;
}

sf_result_t sf_dcx_decompress_from_buffer(const uint8_t *in, size_t in_size, uint8_t **out,
                                          size_t *out_size,
                                          sf_dcx_compression_info_t *out_info,
                                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);
    *out = NULL;
    *out_size = 0;

    sf_dcx_compression_info_t info;
    sf_result_t r = read_compression_info(in, in_size, &info);
    if (r != SF_OK) return r;

    switch (info.type) {
    case SF_DCX_TYPE_ZLIB:
        r = decompress_plain_zlib(in, in_size, out, out_size, alloc);
        break;
    case SF_DCX_TYPE_DCP_DFLT:
        SF_RETURN_IF(in_size < 0x30u, SF_ERR_TRUNCATED);
        *out_size = be32(in + 0x24u);
        r = sfi_zlib_decompress(in + 0x2Cu, be32(in + 0x28u), (void **)out,
                                *out_size, alloc);
        break;
    case SF_DCX_TYPE_DCP_EDGE:
        r = decompress_edge(in, in_size, false, out, out_size, alloc);
        break;
    case SF_DCX_TYPE_DCX_EDGE:
        r = decompress_edge(in, in_size, true, out, out_size, alloc);
        break;
    case SF_DCX_TYPE_DCX_DFLT:
        SF_RETURN_IF(in_size < 0x4Cu, SF_ERR_TRUNCATED);
        *out_size = be32(in + 0x1Cu);
        r = sfi_zlib_decompress(in + 0x4Cu, be32(in + 0x20u), (void **)out,
                                *out_size, alloc);
        break;
    case SF_DCX_TYPE_DCX_KRAK:
        SF_RETURN_IF(in_size < 0x4Cu, SF_ERR_TRUNCATED);
        *out_size = be32(in + 0x1Cu);
        *out = (uint8_t *)sf_xalloc(alloc, *out_size ? *out_size : 1u);
        if (!*out) return SF_ERR_OOM;
        r = sfi_oodle_decompress(in + 0x4Cu, be32(in + 0x20u), *out, *out_size);
        if (r != SF_OK) {
            sf_xfree(alloc, *out);
            *out = NULL;
            *out_size = 0;
        }
        break;
    case SF_DCX_TYPE_DCX_ZSTD:
        SF_RETURN_IF(in_size < 0x4Cu, SF_ERR_TRUNCATED);
        *out_size = be32(in + 0x1Cu);
        r = sfi_zstd_decompress(in + 0x4Cu, be32(in + 0x20u), (void **)out,
                                *out_size, alloc);
        break;
    case SF_DCX_TYPE_UNKNOWN:
    case SF_DCX_TYPE_NONE:
    default:
        r = SF_ERR_UNSUPPORTED_VERSION;
        break;
    }

    if (r == SF_OK && out_info) *out_info = info;
    return r;
}

static sf_result_t read_stream_all(sf_istream_t *stream, uint8_t **out, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(stream && out && out_size);
    *out = NULL;
    *out_size = 0;

    int64_t len = sf_istream_length(stream);
    SF_RETURN_IF(len < 0, SF_ERR_IO);

    size_t size = (size_t)len;
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, size ? size : 1u);
    if (!buf) return SF_ERR_OOM;

    sf_result_t r = sf_istream_read(stream, buf, size);
    if (r != SF_OK) {
        sf_xfree(alloc, buf);
        return r;
    }

    *out = buf;
    *out_size = size;
    return SF_OK;
}

sf_result_t sf_dcx_decompress_from_stream(sf_istream_t *stream, uint8_t **out,
                                          size_t *out_size,
                                          sf_dcx_compression_info_t *out_info,
                                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG(stream && out && out_size);
    SF_RETURN_IF(sf_istream_position(stream) != 0, SF_ERR_INVALID_ARG);

    uint8_t *bytes = NULL;
    size_t size = 0;
    sf_result_t r = read_stream_all(stream, &bytes, &size, alloc);
    if (r != SF_OK) return r;

    r = sf_dcx_decompress_from_buffer(bytes, size, out, out_size, out_info, alloc);
    sf_xfree(alloc, bytes);
    return r;
}

sf_result_t sf_dcx_decompress_from_path(const char *utf8_path, uint8_t **out,
                                        size_t *out_size,
                                        sf_dcx_compression_info_t *out_info,
                                        const sf_allocator_t *alloc) {
    SF_CHECK_ARG(utf8_path && out && out_size);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_file(&stream, utf8_path, alloc);
    if (r != SF_OK) return r;
    r = sf_dcx_decompress_from_stream(stream, out, out_size, out_info, alloc);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_dcx_decompress(const void *in, size_t in_size, void **out, size_t *out_size,
                              sf_dcx_type_t *out_type, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out);

    uint8_t *bytes = NULL;
    sf_dcx_compression_info_t info;
    sf_result_t r = sf_dcx_decompress_from_buffer((const uint8_t *)in, in_size, &bytes,
                                                  out_size, &info, alloc);
    *out = bytes;
    if (r == SF_OK && out_type) *out_type = info.type;
    return r;
}

sf_result_t sf_dcx_unwrap(const void *in, size_t in_size, void **out, size_t *out_size,
                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && out && out_size);

    const void *cur = in;
    size_t cur_size = in_size;
    void *tmp = NULL;

    for (int depth = 0; depth < 8; depth++) {
        sf_dcx_type_t type = SF_DCX_TYPE_UNKNOWN;
        sf_result_t r = sf_dcx_sniff(cur, cur_size, &type);
        if (r != SF_OK) {
            sf_xfree(alloc, tmp);
            return r;
        }

        if (type == SF_DCX_TYPE_UNKNOWN || type == SF_DCX_TYPE_NONE) {
            void *dst = sf_xalloc(alloc, cur_size ? cur_size : 1u);
            if (!dst) {
                sf_xfree(alloc, tmp);
                return SF_ERR_OOM;
            }
            if (cur_size > 0u) memcpy(dst, cur, cur_size);
            sf_xfree(alloc, tmp);
            *out = dst;
            *out_size = cur_size;
            return SF_OK;
        }

        void *next = NULL;
        size_t next_size = 0;
        r = sf_dcx_decompress(cur, cur_size, &next, &next_size, NULL, alloc);
        sf_xfree(alloc, tmp);
        if (r != SF_OK) return r;
        tmp = next;
        cur = next;
        cur_size = next_size;
    }

    sf_xfree(alloc, tmp);
    return SF_ERR_UNSUPPORTED_VERSION;
}

static sf_result_t copy_uncompressed(const uint8_t *in, size_t in_size, uint8_t **out,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, in_size ? in_size : 1u);
    if (!buf) return SF_ERR_OOM;
    if (in_size > 0u) memcpy(buf, in, in_size);
    *out = buf;
    *out_size = in_size;
    return SF_OK;
}

static sf_result_t wrap_dcp_dflt(const uint8_t *in, size_t in_size, uint8_t **out,
                                 size_t *out_size, const sf_allocator_t *alloc) {
    void *z = NULL;
    size_t zsz = 0;
    sf_result_t r = sfi_zlib_compress(in, in_size, &z, &zsz, alloc);
    if (r != SF_OK) return r;

    size_t total = 0x2Cu + zsz + 8u;
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, total);
    if (!buf) {
        sf_xfree(alloc, z);
        return SF_ERR_OOM;
    }

    memcpy(buf, "DCP\0DFLT", 8);
    wr32(buf + 8, 0x20);
    wr32(buf + 12, 0x09000000);
    wr32(buf + 16, 0);
    wr32(buf + 20, 0);
    wr32(buf + 24, 0);
    wr32(buf + 28, 0x00010100);
    memcpy(buf + 32, "DCS\0", 4);
    wr32(buf + 36, (uint32_t)in_size);
    wr32(buf + 40, (uint32_t)zsz);
    memcpy(buf + 44, z, zsz);
    memcpy(buf + 44 + zsz, "DCA\0", 4);
    wr32(buf + 48 + zsz, 8);

    sf_xfree(alloc, z);
    *out = buf;
    *out_size = total;
    return SF_OK;
}

static sf_result_t wrap_dcx_payload(const uint8_t *in, size_t in_size,
                                    const sf_dcx_compression_info_t *info,
                                    uint8_t **out, size_t *out_size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(info);

    void *comp = NULL;
    size_t csz = 0;
    sf_result_t r = SF_OK;
    if (info->type == SF_DCX_TYPE_DCX_DFLT) {
        r = sfi_zlib_compress(in, in_size, &comp, &csz, alloc);
    } else if (info->type == SF_DCX_TYPE_DCX_ZSTD) {
        r = sfi_zstd_compress(in, in_size, (int)info->u.dcx_zstd.compression_level,
                              &comp, &csz, alloc);
    } else {
        r = sfi_oodle_compress((int)info->u.dcx_krak.oodle_compressor_type,
                               (int)info->u.dcx_krak.compression_level, in, in_size,
                               &comp, &csz, alloc);
    }
    if (r != SF_OK) return r;

    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, 0x4Cu + csz);
    if (!buf) {
        sf_xfree(alloc, comp);
        return SF_ERR_OOM;
    }

    int32_t unk04 = 0x11000;
    int32_t unk10 = 0x44;
    int32_t unk14 = 0x4C;
    uint8_t level = 0;
    uint8_t unk38 = 0;
    const char *format = "KRAK";

    if (info->type == SF_DCX_TYPE_DCX_DFLT) {
        unk04 = info->u.dcx_dflt.unk04;
        unk10 = info->u.dcx_dflt.unk10;
        unk14 = info->u.dcx_dflt.unk14;
        level = info->u.dcx_dflt.unk30;
        unk38 = info->u.dcx_dflt.unk38;
        format = "DFLT";
    } else if (info->type == SF_DCX_TYPE_DCX_ZSTD) {
        level = info->u.dcx_zstd.compression_level;
        format = "ZSTD";
    } else {
        level = info->u.dcx_krak.compression_level;
    }

    memcpy(buf, "DCX\0", 4);
    wr32(buf + 4, (uint32_t)unk04);
    wr32(buf + 8, 0x18);
    wr32(buf + 12, 0x24);
    wr32(buf + 16, (uint32_t)unk10);
    wr32(buf + 20, (uint32_t)unk14);
    memcpy(buf + 24, "DCS\0", 4);
    wr32(buf + 28, (uint32_t)in_size);
    wr32(buf + 32, (uint32_t)csz);
    memcpy(buf + 36, "DCP\0", 4);
    memcpy(buf + 40, format, 4);
    wr32(buf + 44, 0x20);
    buf[48] = level;
    buf[49] = 0;
    buf[50] = 0;
    buf[51] = 0;
    wr32(buf + 52, 0);
    wr32(buf + 56, (uint32_t)unk38 << 24);
    wr32(buf + 60, 0);
    wr32(buf + 64, 0x00010100);
    memcpy(buf + 68, "DCA\0", 4);
    wr32(buf + 72, 8);
    memcpy(buf + 76, comp, csz);

    sf_xfree(alloc, comp);
    *out = buf;
    *out_size = 0x4Cu + csz;
    return SF_OK;
}

static sf_result_t wrap_edge(const uint8_t *in, size_t in_size, bool dcx, uint8_t **out,
                             size_t *out_size, const sf_allocator_t *alloc) {
    uint32_t chunks = (uint32_t)((in_size + 0xFFFFu) / 0x10000u);
    if (chunks == 0u) chunks = 1u;

    size_t data_padded = 0;
    for (uint32_t i = 0; i < chunks; i++) {
        size_t chunk_size = in_size > (size_t)i * 0x10000u
                                ? in_size - (size_t)i * 0x10000u
                                : 0u;
        if (chunk_size > 0x10000u) chunk_size = 0x10000u;
        data_padded += align16(chunk_size);
    }

    size_t header = dcx ? (0x70u + (size_t)chunks * 16u) : 0x30u;
    size_t dca = dcx ? 0u : (8u + 0x20u + (size_t)chunks * 16u);
    size_t total = dcx ? header + data_padded : header + data_padded + dca;
    uint8_t *buf = (uint8_t *)sf_xalloc(alloc, total);
    if (!buf) return SF_ERR_OOM;
    memset(buf, 0, total);

    if (dcx) {
        memcpy(buf, "DCX\0", 4);
        wr32(buf + 4, 0x10000);
        wr32(buf + 8, 0x18);
        wr32(buf + 12, 0x24);
        wr32(buf + 16, 0x24);
        wr32(buf + 20, 0x50 + chunks * 16u);
        memcpy(buf + 24, "DCS\0", 4);
        wr32(buf + 28, (uint32_t)in_size);
        wr32(buf + 32, (uint32_t)data_padded);
        memcpy(buf + 36, "DCP\0EDGE", 8);
        wr32(buf + 44, 0x20);
        wr32(buf + 48, 0x09000000);
        wr32(buf + 52, 0x10000);
        wr32(buf + 64, 0x00100100);
        memcpy(buf + 68, "DCA\0", 4);
        wr32(buf + 72, 0x24 + chunks * 16u + 8u);
        memcpy(buf + 76, "EgdT", 4);
        wr32(buf + 80, 0x00010100);
        wr32(buf + 84, 0x24);
        wr32(buf + 88, 0x10);
        wr32(buf + 92, 0x10000);
        wr32(buf + 96, (uint32_t)(in_size % 0x10000u));
        wr32(buf + 100, 0x24 + chunks * 16u);
        wr32(buf + 104, chunks);
        wr32(buf + 108, 0x100000);

        size_t data_start = header;
        size_t off = 0;
        for (uint32_t i = 0; i < chunks; i++) {
            size_t chunk_size = in_size - (size_t)i * 0x10000u;
            if (chunk_size > 0x10000u) chunk_size = 0x10000u;
            uint8_t *entry = buf + 112 + (size_t)i * 16u;
            wr32(entry + 4, (uint32_t)off);
            wr32(entry + 8, (uint32_t)chunk_size);
            if (chunk_size > 0u) {
                memcpy(buf + data_start + off, in + (size_t)i * 0x10000u, chunk_size);
            }
            off += align16(chunk_size);
        }
    } else {
        memcpy(buf, "DCP\0EDGE", 8);
        wr32(buf + 8, 0x20);
        wr32(buf + 12, 0x09000000);
        wr32(buf + 16, 0x10000);
        wr32(buf + 28, 0x00100100);
        memcpy(buf + 32, "DCS\0", 4);
        wr32(buf + 36, (uint32_t)in_size);
        wr32(buf + 40, (uint32_t)data_padded);

        size_t off = 0;
        for (uint32_t i = 0; i < chunks; i++) {
            size_t chunk_size = in_size - (size_t)i * 0x10000u;
            if (chunk_size > 0x10000u) chunk_size = 0x10000u;
            if (chunk_size > 0u) memcpy(buf + 48 + off, in + (size_t)i * 0x10000u, chunk_size);
            off += align16(chunk_size);
        }

        size_t dca_pos = 48 + data_padded;
        memcpy(buf + dca_pos, "DCA\0", 4);
        wr32(buf + dca_pos + 4, (uint32_t)dca);
        memcpy(buf + dca_pos + 8, "EgdT", 4);
        wr32(buf + dca_pos + 12, 0x10000);
        wr32(buf + dca_pos + 16, 0x20);
        wr32(buf + dca_pos + 20, 0x10);
        wr32(buf + dca_pos + 24, 0x10000);
        wr32(buf + dca_pos + 28, 0x20 + chunks * 16u);
        wr32(buf + dca_pos + 32, chunks);
        wr32(buf + dca_pos + 36, 0x100000);

        off = 0;
        for (uint32_t i = 0; i < chunks; i++) {
            size_t chunk_size = in_size - (size_t)i * 0x10000u;
            if (chunk_size > 0x10000u) chunk_size = 0x10000u;
            uint8_t *entry = buf + dca_pos + 40 + (size_t)i * 16u;
            wr32(entry + 4, (uint32_t)off);
            wr32(entry + 8, (uint32_t)chunk_size);
            off += align16(chunk_size);
        }
    }

    *out = buf;
    *out_size = total;
    return SF_OK;
}

sf_result_t sf_dcx_compress_to_buffer(const uint8_t *in, size_t in_size,
                                      const sf_dcx_compression_info_t *info,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && info && out && out_size);
    *out = NULL;
    *out_size = 0;

    void *tmp = NULL;
    sf_result_t r = SF_OK;
    switch (info->type) {
    case SF_DCX_TYPE_NONE:
        return copy_uncompressed(in, in_size, out, out_size, alloc);
    case SF_DCX_TYPE_ZLIB:
        r = sfi_zlib_compress(in, in_size, &tmp, out_size, alloc);
        *out = (uint8_t *)tmp;
        return r;
    case SF_DCX_TYPE_DCP_DFLT:
        return wrap_dcp_dflt(in, in_size, out, out_size, alloc);
    case SF_DCX_TYPE_DCP_EDGE:
        return wrap_edge(in, in_size, false, out, out_size, alloc);
    case SF_DCX_TYPE_DCX_EDGE:
        return wrap_edge(in, in_size, true, out, out_size, alloc);
    case SF_DCX_TYPE_DCX_DFLT:
    case SF_DCX_TYPE_DCX_KRAK:
    case SF_DCX_TYPE_DCX_ZSTD:
        return wrap_dcx_payload(in, in_size, info, out, out_size, alloc);
    case SF_DCX_TYPE_UNKNOWN:
    default:
        return SF_ERR_INVALID_ARG;
    }
}

sf_result_t sf_dcx_compress_to_stream(const uint8_t *in, size_t in_size,
                                      const sf_dcx_compression_info_t *info,
                                      sf_ostream_t *stream,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && info && stream);

    uint8_t *bytes = NULL;
    size_t size = 0;
    sf_result_t r = sf_dcx_compress_to_buffer(in, in_size, info, &bytes, &size, alloc);
    if (r != SF_OK) return r;

    r = sf_ostream_write(stream, bytes, size);
    sf_xfree(alloc, bytes);
    return r;
}

sf_result_t sf_dcx_compress_to_path(const uint8_t *in, size_t in_size,
                                    const sf_dcx_compression_info_t *info,
                                    const char *utf8_path,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG((in || in_size == 0u) && info && utf8_path);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_file(&stream, utf8_path, alloc);
    if (r != SF_OK) return r;
    r = sf_dcx_compress_to_stream(in, in_size, info, stream, alloc);
    sf_ostream_close(stream);
    return r;
}

sf_result_t sf_dcx_compress_ex(const void *in, size_t in_size,
                               const sf_dcx_compression_info_t *info, void **out,
                               size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out);

    uint8_t *bytes = NULL;
    sf_result_t r = sf_dcx_compress_to_buffer((const uint8_t *)in, in_size, info, &bytes,
                                              out_size, alloc);
    *out = bytes;
    return r;
}

sf_result_t sf_dcx_compress(const void *in, size_t in_size, sf_dcx_type_t type, void **out,
                            size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out);

    sf_dcx_compression_info_t info;
    sf_result_t r = default_info_from_type(type, &info);
    if (r != SF_OK) return r;
    return sf_dcx_compress_ex(in, in_size, &info, out, out_size, alloc);
}
