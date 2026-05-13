/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_nsa.h"
#include "internal/sf_internal.h"

#include <string.h>

#define NSA_HEADER_MIN_SIZE 0x88u

struct sf_nsa {
    const sf_allocator_t *alloc;
    uint8_t *raw;
    size_t raw_size;
    uint32_t frame_count;
    uint32_t static_translation_count;
    uint32_t static_rotation_count;
    uint32_t dynamic_translation_count;
    uint32_t dynamic_rotation_count;
};

static uint32_t nsa_read_u32_at(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool nsa_read_ptr_at(const uint8_t *bytes, size_t size, size_t offset, uint32_t *out) {
    if (offset > size - 4u) return false;
    *out = nsa_read_u32_at(bytes + offset);
    return true;
}

static bool nsa_valid_offset(uint32_t offset, size_t size) {
    return offset == 0u || (size_t)offset < size;
}

static bool nsa_parse_counts(const uint8_t *bytes,
                             size_t size,
                             uint32_t *out_frame_count,
                             uint32_t *out_static_translation_count,
                             uint32_t *out_static_rotation_count,
                             uint32_t *out_dynamic_translation_count,
                             uint32_t *out_dynamic_rotation_count) {
    if (size < NSA_HEADER_MIN_SIZE) return false;

    uint32_t p_dynamic = 0;
    uint32_t p_static = 0;
    if (!nsa_read_ptr_at(bytes, size, 0x08u, &p_dynamic)) return false;
    if (!nsa_read_ptr_at(bytes, size, 0x78u, &p_static)) return false;

    uint32_t alignment = nsa_read_u32_at(bytes + 0x0Cu);
    uint32_t declared_size = nsa_read_u32_at(bytes + 0x10u);
    if (alignment == 0u || alignment > 0x10000u) return false;
    if (declared_size != 0u && declared_size > size) return false;
    if (!nsa_valid_offset(p_dynamic, size) || !nsa_valid_offset(p_static, size)) return false;

    uint32_t frame_count = 0;
    uint32_t dynamic_translation_count = 0;
    uint32_t dynamic_rotation_count = 0;
    if (p_dynamic != 0u) {
        if ((size_t)p_dynamic > size - 12u) return false;
        frame_count = nsa_read_u32_at(bytes + p_dynamic);
        dynamic_translation_count = nsa_read_u32_at(bytes + p_dynamic + 4u);
        dynamic_rotation_count = nsa_read_u32_at(bytes + p_dynamic + 8u);
    }

    uint32_t static_translation_count = 0;
    uint32_t static_rotation_count = 0;
    if (p_static != 0u) {
        if ((size_t)p_static > size - 8u) return false;
        static_translation_count = nsa_read_u32_at(bytes + p_static);
        static_rotation_count = nsa_read_u32_at(bytes + p_static + 4u);
    }

    *out_frame_count = frame_count;
    *out_static_translation_count = static_translation_count;
    *out_static_rotation_count = static_rotation_count;
    *out_dynamic_translation_count = dynamic_translation_count;
    *out_dynamic_rotation_count = dynamic_rotation_count;
    return true;
}

static void nsa_clear(sf_nsa_t *nsa) {
    if (!nsa) return;
    sf_xfree(nsa->alloc, nsa->raw);
    nsa->raw = NULL;
    nsa->raw_size = 0;
}

sf_result_t sf_nsa_create(sf_nsa_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_nsa_t *nsa = (sf_nsa_t *)sf_xalloc(alloc, sizeof(*nsa));
    if (!nsa) return SF_ERR_OOM;
    memset(nsa, 0, sizeof(*nsa));
    nsa->alloc = alloc;
    *out = nsa;
    return SF_OK;
}

void sf_nsa_destroy(sf_nsa_t *nsa) {
    if (!nsa) return;
    nsa_clear(nsa);
    sf_xfree(nsa->alloc, nsa);
}

bool sf_nsa_is(const void *bytes, size_t size) {
    if (!bytes) return false;
    uint32_t frame_count = 0;
    uint32_t static_translation_count = 0;
    uint32_t static_rotation_count = 0;
    uint32_t dynamic_translation_count = 0;
    uint32_t dynamic_rotation_count = 0;
    return nsa_parse_counts((const uint8_t *)bytes, size, &frame_count,
                            &static_translation_count, &static_rotation_count,
                            &dynamic_translation_count, &dynamic_rotation_count);
}

sf_result_t sf_nsa_read_from_memory(sf_nsa_t **out,
                                    const void *bytes,
                                    size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && bytes != NULL);
    *out = NULL;

    uint32_t frame_count = 0;
    uint32_t static_translation_count = 0;
    uint32_t static_rotation_count = 0;
    uint32_t dynamic_translation_count = 0;
    uint32_t dynamic_rotation_count = 0;
    if (!nsa_parse_counts((const uint8_t *)bytes, size, &frame_count,
                          &static_translation_count, &static_rotation_count,
                          &dynamic_translation_count, &dynamic_rotation_count)) {
        return SF_ERR_BAD_MAGIC;
    }

    sf_nsa_t *nsa = NULL;
    sf_result_t r = sf_nsa_create(&nsa, alloc);
    if (r != SF_OK) return r;

    if (size > 0) {
        nsa->raw = (uint8_t *)sf_xalloc(nsa->alloc, size);
        if (!nsa->raw) {
            sf_nsa_destroy(nsa);
            return SF_ERR_OOM;
        }
        memcpy(nsa->raw, bytes, size);
        nsa->raw_size = size;
    }
    nsa->frame_count = frame_count;
    nsa->static_translation_count = static_translation_count;
    nsa->static_rotation_count = static_rotation_count;
    nsa->dynamic_translation_count = dynamic_translation_count;
    nsa->dynamic_rotation_count = dynamic_rotation_count;

    *out = nsa;
    return SF_OK;
}

sf_result_t sf_nsa_write_to_memory(const sf_nsa_t *nsa,
                                   void **out_bytes,
                                   size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(nsa != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (nsa->raw_size == 0) return SF_OK;
    uint8_t *copy = (uint8_t *)sf_xalloc(alloc, nsa->raw_size);
    if (!copy) return SF_ERR_OOM;
    memcpy(copy, nsa->raw, nsa->raw_size);
    *out_bytes = copy;
    *out_size = nsa->raw_size;
    return SF_OK;
}

uint32_t sf_nsa_frame_count(const sf_nsa_t *nsa) { return nsa ? nsa->frame_count : 0; }
uint32_t sf_nsa_static_translation_count(const sf_nsa_t *nsa) {
    return nsa ? nsa->static_translation_count : 0;
}
uint32_t sf_nsa_static_rotation_count(const sf_nsa_t *nsa) {
    return nsa ? nsa->static_rotation_count : 0;
}
uint32_t sf_nsa_dynamic_translation_count(const sf_nsa_t *nsa) {
    return nsa ? nsa->dynamic_translation_count : 0;
}
uint32_t sf_nsa_dynamic_rotation_count(const sf_nsa_t *nsa) {
    return nsa ? nsa->dynamic_rotation_count : 0;
}
