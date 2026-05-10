/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_COMPRESSION_INTERNAL_H
#define SF_COMPRESSION_INTERNAL_H

#include "souls_formats/sf_common.h"

#include <stddef.h>

sf_result_t sfi_deflate_raw_decompress(const void *in, size_t in_size, void *out,
                                       size_t out_size);
sf_result_t sfi_deflate_raw_compress(const void *in, size_t in_size, int level, void **out,
                                     size_t *out_size, const sf_allocator_t *a);
sf_result_t sfi_zlib_decompress(const void *in, size_t in_size, void **out, size_t out_size,
                                const sf_allocator_t *a);
sf_result_t sfi_zlib_compress(const void *in, size_t in_size, void **out, size_t *out_size,
                              const sf_allocator_t *a);
sf_result_t sfi_zstd_decompress(const void *in, size_t in_size, void **out, size_t out_size,
                                const sf_allocator_t *a);
sf_result_t sfi_zstd_compress(const void *in, size_t in_size, int level, void **out,
                              size_t *out_size, const sf_allocator_t *a);
int sfi_deflate_chunked_translation_unit_anchor(void);

#endif /* SF_COMPRESSION_INTERNAL_H */
