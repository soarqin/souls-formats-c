/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_fsliblzs.h"
#include "internal/sf_internal.h"

#include <string.h>

bool sf_fsliblzs_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_FSLIBLZS_MAGIC_LEN) return false;
    return memcmp(bytes, SF_FSLIBLZS_MAGIC, SF_FSLIBLZS_MAGIC_LEN) == 0;
}

sf_result_t sf_fsliblzs_read_header(const void *bytes, size_t size,
                                    int32_t *out_compressed_size,
                                    int32_t *out_decompressed_size) {
    SF_CHECK_ARG(bytes != NULL);
    SF_CHECK_ARG(out_compressed_size != NULL && out_decompressed_size != NULL);
    if (size < SF_FSLIBLZS_HEADER_SIZE) return SF_ERR_INVALID_ARG;
    if (!sf_fsliblzs_is(bytes, size)) return SF_ERR_BAD_MAGIC;

    const uint8_t *p = (const uint8_t *)bytes;

    /* Little-endian section: magic(8) + i32(0) + i32(0) + compressed_size(i32) + i32(1) + i32(0) + i32(0) */
    /* Offsets: 0=magic, 8=unk0, 12=unk4, 16=compressed_size, 20=unk1, 24=unk2, 28=unk3 */
    int32_t unk0, unk4, compressed_size, unk1, unk2, unk3;
    memcpy(&unk0, p + 8, 4);
    memcpy(&unk4, p + 12, 4);
    memcpy(&compressed_size, p + 16, 4);
    memcpy(&unk1, p + 20, 4);
    memcpy(&unk2, p + 24, 4);
    memcpy(&unk3, p + 28, 4);

    /* Big-endian section starts at offset 32: i16(1) + i16(0) + decompressed_size(i32) + i32(0) */
    if (size < 44u) return SF_ERR_INVALID_ARG;
    int16_t be_unk0, be_unk1;
    int32_t decompressed_size, be_unk2;
    /* Read big-endian i16 */
    be_unk0 = (int16_t)(((uint16_t)p[32] << 8) | (uint16_t)p[33]);
    be_unk1 = (int16_t)(((uint16_t)p[34] << 8) | (uint16_t)p[35]);
    /* Read big-endian i32 */
    decompressed_size = (int32_t)(((uint32_t)p[36] << 24) | ((uint32_t)p[37] << 16) |
                                  ((uint32_t)p[38] << 8) | (uint32_t)p[39]);
    memcpy(&be_unk2, p + 40, 4);

    (void)unk0; (void)unk4; (void)unk1; (void)unk2; (void)unk3;
    (void)be_unk0; (void)be_unk1; (void)be_unk2;

    *out_compressed_size = compressed_size;
    *out_decompressed_size = decompressed_size;
    return SF_OK;
}

sf_result_t sf_fsliblzs_decompress(const void *bytes, size_t size,
                                   void **out_bytes, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    (void)bytes; (void)size; (void)out_bytes; (void)out_size; (void)alloc;
    return SF_ERR_UNSUPPORTED_VERSION;
}
