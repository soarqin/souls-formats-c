/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — NGP navmesh (DS2).
 *
 * Mirrors:
 *   SoulsFormats/Formats/NGP.cs
 *
 * This implementation preserves the file at byte level. Header parsing
 * validates magic ("NVG2"), endianness, version, and reads count fields
 * required for downstream inspection. The complete file body is kept as a
 * byte buffer so writes are exact byte copies of the input.
 */

#include "souls_formats/sf_ngp.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct sf_ngp {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    sf_ngp_version_t      version;
    int32_t               unk1c;
    int32_t               mesh_count;
    int32_t               count_a;
    int32_t               count_b;
    int32_t               count_c;
    int32_t               count_d;
    uint8_t              *raw;
    size_t                raw_size;
};

void sf_ngp_destroy(sf_ngp_t *ngp) {
    if (!ngp) return;
    sf_xfree(ngp->alloc, ngp->raw);
    sf_xfree(ngp->alloc, ngp);
}

sf_result_t sf_ngp_create_from_bytes(sf_ngp_t **out, const void *data, size_t size,
                                     const sf_allocator_t *alloc) {
    return sf_ngp_read_from_memory(out, data, size, alloc);
}

bool             sf_ngp_big_endian(const sf_ngp_t *n) { return n ? n->big_endian : false; }
sf_ngp_version_t sf_ngp_version(const sf_ngp_t *n) {
    return n ? n->version : SF_NGP_VERSION_VANILLA;
}
int32_t sf_ngp_unk1c(const sf_ngp_t *n) { return n ? n->unk1c : 0; }
int32_t sf_ngp_mesh_count(const sf_ngp_t *n) { return n ? n->mesh_count : 0; }
int32_t sf_ngp_struct_a_count(const sf_ngp_t *n) { return n ? n->count_a : 0; }
int32_t sf_ngp_struct_b_count(const sf_ngp_t *n) { return n ? n->count_b : 0; }
int32_t sf_ngp_struct_c_count(const sf_ngp_t *n) { return n ? n->count_c : 0; }
int32_t sf_ngp_struct_d_count(const sf_ngp_t *n) { return n ? n->count_d : 0; }
const uint8_t *sf_ngp_raw_data(const sf_ngp_t *n) { return n ? n->raw : NULL; }
size_t  sf_ngp_raw_size(const sf_ngp_t *n) { return n ? n->raw_size : 0; }

sf_result_t sf_ngp_read_from_memory(sf_ngp_t **out, const void *data, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    SF_CHECK_ARG(size >= 32);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r != SF_OK) { sf_istream_close(stream); return r; }

    sf_ngp_t *ngp = NULL;

    int16_t version_le = 0;
    r = sf_binary_reader_get_i16(br, 4, &version_le);
    if (r != SF_OK) goto cleanup;
    bool big_endian = (version_le == 0x0100);
    sf_binary_reader_set_big_endian(br, big_endian);

    r = sf_binary_reader_assert_ascii(br, "NVG2");
    if (r != SF_OK) goto cleanup;

    uint16_t version_u = 0;
    r = sf_binary_reader_read_u16(br, &version_u);
    if (r != SF_OK) goto cleanup;
    if (version_u != SF_NGP_VERSION_VANILLA && version_u != SF_NGP_VERSION_SCHOLAR) {
        r = SF_ERR_UNSUPPORTED_VERSION; goto cleanup;
    }
    r = sf_binary_reader_assert_i16_one(br, 0);
    if (r != SF_OK) goto cleanup;

    int32_t mesh_count = 0, count_a = 0, count_b = 0, count_c = 0, count_d = 0, unk1c = 0;
    r = sf_binary_reader_read_i32(br, &mesh_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_a);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_b);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_c);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &count_d);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &unk1c);
    if (r != SF_OK) goto cleanup;

    ngp = (sf_ngp_t *)sf_xalloc(alloc, sizeof(*ngp));
    if (!ngp) { r = SF_ERR_OOM; goto cleanup; }
    memset(ngp, 0, sizeof(*ngp));
    ngp->alloc = alloc;
    ngp->big_endian = big_endian;
    ngp->version = (sf_ngp_version_t)version_u;
    ngp->unk1c = unk1c;
    ngp->mesh_count = mesh_count;
    ngp->count_a = count_a;
    ngp->count_b = count_b;
    ngp->count_c = count_c;
    ngp->count_d = count_d;

    ngp->raw = (uint8_t *)sf_xalloc(alloc, size);
    if (!ngp->raw) { r = SF_ERR_OOM; goto cleanup; }
    memcpy(ngp->raw, data, size);
    ngp->raw_size = size;

    *out = ngp;
    ngp = NULL;

cleanup:
    if (ngp) sf_ngp_destroy(ngp);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

sf_result_t sf_ngp_write_to_memory(const sf_ngp_t *ngp, void **out_data, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(ngp != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    alloc = sf_alloc_or_default(alloc);
    uint8_t *bytes = (uint8_t *)sf_xalloc(alloc, ngp->raw_size);
    if (!bytes) return SF_ERR_OOM;
    memcpy(bytes, ngp->raw, ngp->raw_size);
    *out_data = bytes;
    *out_size = ngp->raw_size;
    return SF_OK;
}
