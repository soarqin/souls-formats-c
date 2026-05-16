/* SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef SF_COMPRESSION_OODLE_LOADER_H
#define SF_COMPRESSION_OODLE_LOADER_H

#include "souls_formats/sf_common.h"

#include <stddef.h>
#include <stdint.h>

sf_result_t sfi_oodle_decompress(const void *in, size_t in_size, void *out, size_t out_size);

sf_result_t sfi_oodle_compress(int compressor, int level, const void *in, size_t in_size,
                               void **out, size_t *out_size, const sf_allocator_t *a);

int64_t sfi_oodle_bound(size_t in_size);

sf_result_t sfi_oodle_compress_into(int compressor, int level, const void *in, size_t in_size,
                                    void *dst, size_t dst_cap, size_t *out_size);

int sfi_oodle_v6_is_compatible(void);
int sfi_oodle_v8_is_compatible(void);
int sfi_oodle_v9_is_compatible(void);

#endif /* SF_COMPRESSION_OODLE_LOADER_H */
