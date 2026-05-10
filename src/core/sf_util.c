/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mirrors upstream Utilities/SFUtil.cs.
 *
 * Only GetDecompressedBinaryReader is exported. ConcatAll/Dictionize are
 * .NET-LINQ helpers that have no idiomatic C equivalent (see
 * docs/api-mapping/util-sf-util.md and POLICY.md §9).
 */

#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_io.h"

#include "internal/sf_internal.h"

#include <stdint.h>
#include <string.h>

sf_result_t sf_get_decompressed_reader(sf_binary_reader_t *in,
                                       sf_binary_reader_t **out_reader,
                                       sf_dcx_compression_info_t *out_info,
                                       const sf_allocator_t *alloc) {
    SF_CHECK_ARG(in != NULL && out_reader != NULL && out_info != NULL);

    *out_reader = NULL;
    memset(out_info, 0, sizeof(*out_info));

    int64_t pos       = sf_binary_reader_position(in);
    int64_t length    = sf_binary_reader_length(in);
    int64_t remaining = length - pos;

    bool is_dcx = false;
    if (remaining >= 4) {
        uint8_t magic[4] = {0};
        sf_result_t e = sf_binary_reader_get_u8s(in, pos, 4u, magic);
        if (e != SF_OK) return e;
        e = sf_dcx_is_from_buffer(magic, 4u, &is_dcx);
        if (e != SF_OK) return e;
    }

    if (!is_dcx) {
        out_info->type = SF_DCX_TYPE_NONE;
        *out_reader    = in;
        return SF_OK;
    }

    SF_CHECK_ARG(remaining >= 0);
    size_t raw_size = (size_t)remaining;
    uint8_t *raw    = (uint8_t *)sf_xalloc(alloc, raw_size);
    if (!raw) return SF_ERR_OOM;

    sf_result_t e = sf_binary_reader_get_u8s(in, pos, raw_size, raw);
    if (e != SF_OK) {
        sf_xfree(alloc, raw);
        return e;
    }

    uint8_t *decompressed     = NULL;
    size_t   decompressed_len = 0;
    e = sf_dcx_decompress_from_buffer(raw, raw_size, &decompressed,
                                      &decompressed_len, out_info, alloc);
    sf_xfree(alloc, raw);
    if (e != SF_OK) return e;

    sf_binary_reader_t *new_reader = NULL;
    e = sf_binary_reader_create_from_memory(&new_reader, /*big_endian=*/false,
                                            decompressed, decompressed_len, alloc);
    if (e != SF_OK) {
        sf_xfree(alloc, decompressed);
        return e;
    }

    *out_reader = new_reader;
    return SF_OK;
}
