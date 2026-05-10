/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Internal-only PC pass-through Headerizer for TPF.
 * Mirrors the PC arm of upstream `SoulsFormats/Formats/TPF/Headerizer.cs`.
 *
 * Console arms (Xbox360 / Xbone / PS3 / PS4 / PS5) are out of scope for v1
 * and return SF_ERR_UNSUPPORTED_VERSION.
 */

#ifndef SF_INTERNAL_TPF_HEADERIZER_H
#define SF_INTERNAL_TPF_HEADERIZER_H

#include "souls_formats/sf_common.h"
#include "souls_formats/sf_tpf.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

sf_result_t sfi_tpf_headerize(const sf_tpf_texture_t *tex,
                              sf_tpf_platform_t       platform,
                              void                  **out,
                              size_t                 *out_size,
                              const sf_allocator_t   *a);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SF_INTERNAL_TPF_HEADERIZER_H */
