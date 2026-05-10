/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minimal DDS header parser — extracts texture metadata only.
 * See src/internal/dds_header.h for the contract.
 */

#include "internal/dds_header.h"

#include <stddef.h>
#include <stdint.h>

#define SFI_DDS_MAGIC          0x20534444u   /* 'DDS ' little-endian */
#define SFI_DDS_HEADER_SIZE    124u
#define SFI_DDS_FOURCC_DX10    0x30315844u   /* 'DX10' little-endian */
#define SFI_DDSD_DEPTH         0x00800000u
#define SFI_DDSCAPS2_CUBEMAP   0x00000200u

#define SFI_DDS_OFF_MAGIC      0u
#define SFI_DDS_OFF_SIZE       4u
#define SFI_DDS_OFF_FLAGS      8u
#define SFI_DDS_OFF_MIPCOUNT   28u
#define SFI_DDS_OFF_DEPTH      24u
#define SFI_DDS_OFF_FOURCC     84u
#define SFI_DDS_OFF_CAPS2      112u
#define SFI_DDS_HEADER_BYTES   128u
#define SFI_DDS_DX10_BYTES     20u
#define SFI_DDS_OFF_DXGIFORMAT 128u

static uint32_t sfi_load_u32_le(const uint8_t *p) {
    return  (uint32_t)p[0]        |
           ((uint32_t)p[1] <<  8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

sf_result_t sfi_dds_parse_header(const uint8_t *bytes, size_t size,
                                 sfi_dds_metadata_t *out) {
    if (!bytes || !out || size < SFI_DDS_HEADER_BYTES) {
        return SF_ERR_INVALID_ARG;
    }

    uint32_t magic = sfi_load_u32_le(bytes + SFI_DDS_OFF_MAGIC);
    if (magic != SFI_DDS_MAGIC) {
        return SF_ERR_BAD_MAGIC;
    }

    uint32_t dw_size = sfi_load_u32_le(bytes + SFI_DDS_OFF_SIZE);
    if (dw_size != SFI_DDS_HEADER_SIZE) {
        return SF_ERR_BAD_MAGIC;
    }

    uint32_t flags        = sfi_load_u32_le(bytes + SFI_DDS_OFF_FLAGS);
    uint32_t mipmap_count = sfi_load_u32_le(bytes + SFI_DDS_OFF_MIPCOUNT);
    uint32_t depth_raw    = sfi_load_u32_le(bytes + SFI_DDS_OFF_DEPTH);
    uint32_t fourcc       = sfi_load_u32_le(bytes + SFI_DDS_OFF_FOURCC);
    uint32_t caps2        = sfi_load_u32_le(bytes + SFI_DDS_OFF_CAPS2);

    out->cubemap      = (caps2 & SFI_DDSCAPS2_CUBEMAP) != 0;
    out->mipmap_count = mipmap_count;
    out->depth        = (flags & SFI_DDSD_DEPTH) ? depth_raw : 0u;
    out->dxgi_format  = 0u;

    if (fourcc == SFI_DDS_FOURCC_DX10 &&
        size >= SFI_DDS_HEADER_BYTES + SFI_DDS_DX10_BYTES) {
        out->dxgi_format = sfi_load_u32_le(bytes + SFI_DDS_OFF_DXGIFORMAT);
    }

    return SF_OK;
}
