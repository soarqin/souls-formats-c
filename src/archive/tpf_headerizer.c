/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TPF Headerizer (PC pass-through path only).
 *
 * Mirrors the PC arm of upstream `SoulsFormats/Formats/TPF/Headerizer.cs`.
 * On PC, TPF textures already carry a full DDS header; the "Headerize"
 * operation degenerates to returning a copy of the texture bytes.
 *
 * Console arms (Xbox360 / Xbone / PS3 / PS4 / PS5) require platform-specific
 * swizzling and DDS reconstruction and are out of scope for v1; calling
 * sfi_tpf_headerize for those platforms returns SF_ERR_UNSUPPORTED_VERSION.
 */

#include "archive/tpf_headerizer.h"

#include "internal/sf_internal.h"
#include "souls_formats/sf_tpf.h"

#include <stddef.h>
#include <string.h>

sf_result_t sfi_tpf_headerize(const sf_tpf_texture_t *tex,
                              sf_tpf_platform_t       platform,
                              void                  **out,
                              size_t                 *out_size,
                              const sf_allocator_t   *a) {
    SF_CHECK_ARG(tex != NULL);
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(out_size != NULL);

    if (platform != SF_TPF_PLATFORM_PC) {
        return SF_ERR_UNSUPPORTED_VERSION;
    }

    a = sf_alloc_or_default(a);

    size_t         sz = 0;
    const uint8_t *bytes = sf_tpf_texture_get_bytes(tex, &sz);
    if (sz == 0 || bytes == NULL) {
        *out      = NULL;
        *out_size = 0;
        return SF_OK;
    }

    void *copy = sf_xalloc(a, sz);
    if (!copy) return SF_ERR_OOM;
    memcpy(copy, bytes, sz);
    *out      = copy;
    *out_size = sz;
    return SF_OK;
}
