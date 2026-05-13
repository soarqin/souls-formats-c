/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_nmb.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <string.h>

#define NMB_MIN_BUNDLE_PREFIX_SIZE 32u

struct sf_nmb_bundle {
    sf_nmb_bundle_type_t type;
    const uint8_t *data;
    size_t raw_offset;
    size_t raw_size;
    size_t data_offset;
    size_t data_size;
};

struct sf_nmb {
    const sf_allocator_t *alloc;
    uint8_t *raw;
    size_t raw_size;
    sf_nmb_bundle_t *bundles;
    size_t bundle_count;
    size_t bundle_capacity;
};

typedef struct nmb_bundle_layout {
    size_t data_offset;
    size_t data_size;
    size_t next_offset;
} nmb_bundle_layout_t;

static uint32_t nmb_read_u32_at(const uint8_t *p, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t nmb_read_u64_at(const uint8_t *p, bool big_endian) {
    if (big_endian) {
        return ((uint64_t)nmb_read_u32_at(p, true) << 32) | nmb_read_u32_at(p + 4, true);
    }
    return (uint64_t)nmb_read_u32_at(p, false) | ((uint64_t)nmb_read_u32_at(p + 4, false) << 32);
}

static bool nmb_checked_add(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}

static bool nmb_align_up(size_t value, size_t alignment, size_t *out) {
    if (alignment <= 1u) {
        *out = value;
        return true;
    }
    size_t rem = value % alignment;
    if (rem == 0) {
        *out = value;
        return true;
    }
    return nmb_checked_add(value, alignment - rem, out);
}

static bool nmb_magic_at(const uint8_t *p, size_t size, bool *out_big_endian) {
    if (size < 8u) return false;
    uint32_t first_le = nmb_read_u32_at(p, false);
    uint32_t second_le = nmb_read_u32_at(p + 4, false);
    if (first_le == 0x18u && (second_le == 0x6u || second_le == 0xAu)) {
        *out_big_endian = false;
        return true;
    }
    uint32_t first_be = nmb_read_u32_at(p, true);
    uint32_t second_be = nmb_read_u32_at(p + 4, true);
    if (first_be == 0x18u && (second_be == 0x6u || second_be == 0xAu)) {
        *out_big_endian = true;
        return true;
    }
    return false;
}

static bool nmb_try_layout(const uint8_t *bytes,
                           size_t size,
                           size_t bundle_offset,
                           bool big_endian,
                           size_t varint_size,
                           bool has_x64_format_tag,
                           bool prompt_style,
                           nmb_bundle_layout_t *out) {
    size_t cursor = bundle_offset + NMB_MIN_BUNDLE_PREFIX_SIZE;
    if (cursor > size) return false;

    uint64_t data_size_u64 = 0;
    if (varint_size == 8u) {
        if (cursor > size - 8u) return false;
        data_size_u64 = nmb_read_u64_at(bytes + cursor, big_endian);
    } else {
        if (cursor > size - 4u) return false;
        data_size_u64 = nmb_read_u32_at(bytes + cursor, big_endian);
    }
    if (data_size_u64 > (uint64_t)SIZE_MAX) return false;
    cursor += varint_size;

    uint64_t alignment_u64 = 0;
    if (prompt_style && varint_size == 8u) {
        if (cursor > size - 8u) return false;
        alignment_u64 = nmb_read_u64_at(bytes + cursor, big_endian);
        cursor += 8u;
        if (cursor > size - 8u) return false;
        cursor += 8u; /* formatTag */
        if (cursor > size - 4u) return false;
        cursor += 4u; /* unk0 */
    } else if (prompt_style) {
        if (cursor > size - 12u) return false;
        alignment_u64 = nmb_read_u32_at(bytes + cursor, big_endian);
        cursor += 4u + 4u + 4u; /* alignment, formatTag, unk0 */
    } else {
        if (cursor > size - 4u) return false;
        alignment_u64 = nmb_read_u32_at(bytes + cursor, big_endian);
        cursor += 4u;
        if (has_x64_format_tag) {
            if (cursor > size - 4u) return false;
            cursor += 4u;
        }
    }

    if (alignment_u64 > (uint64_t)INT_MAX) return false;
    size_t data_offset = 0;
    if (!nmb_align_up(cursor, (size_t)alignment_u64, &data_offset)) return false;
    size_t next_offset = 0;
    if (!nmb_checked_add(data_offset, (size_t)data_size_u64, &next_offset)) return false;
    if (next_offset > size) return false;

    out->data_offset = data_offset;
    out->data_size = (size_t)data_size_u64;
    out->next_offset = next_offset;
    return true;
}

static bool nmb_parse_layout(const uint8_t *bytes,
                             size_t size,
                             size_t bundle_offset,
                             bool big_endian,
                             nmb_bundle_layout_t *out) {
    static const struct {
        size_t varint_size;
        bool has_x64_format_tag;
        bool prompt_style;
    } candidates[] = {
        { 8u, true, false },
        { 4u, false, false },
        { 4u, false, true },
        { 8u, false, true },
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        nmb_bundle_layout_t candidate;
        if (!nmb_try_layout(bytes, size, bundle_offset, big_endian,
                            candidates[i].varint_size,
                            candidates[i].has_x64_format_tag,
                            candidates[i].prompt_style, &candidate)) {
            continue;
        }
        if (candidate.next_offset >= size - 0xCu || candidate.next_offset == size) {
            *out = candidate;
            return true;
        }
        bool next_big = false;
        if (nmb_magic_at(bytes + candidate.next_offset, size - candidate.next_offset, &next_big)) {
            *out = candidate;
            return true;
        }
    }
    return false;
}

static void nmb_clear(sf_nmb_t *nmb) {
    if (!nmb) return;
    sf_xfree(nmb->alloc, nmb->raw);
    sf_xfree(nmb->alloc, nmb->bundles);
    nmb->raw = NULL;
    nmb->bundles = NULL;
    nmb->raw_size = 0;
    nmb->bundle_count = 0;
    nmb->bundle_capacity = 0;
}

static sf_result_t nmb_copy_raw(sf_nmb_t *nmb, const uint8_t *bytes, size_t size) {
    if (size == 0) return SF_OK;
    nmb->raw = (uint8_t *)sf_xalloc(nmb->alloc, size);
    if (!nmb->raw) return SF_ERR_OOM;
    memcpy(nmb->raw, bytes, size);
    nmb->raw_size = size;
    return SF_OK;
}

static sf_result_t nmb_push_bundle(sf_nmb_t *nmb, const sf_nmb_bundle_t *bundle) {
    if (nmb->bundle_count == nmb->bundle_capacity) {
        size_t new_capacity = nmb->bundle_capacity == 0 ? 4u : nmb->bundle_capacity * 2u;
        if (new_capacity < nmb->bundle_capacity) return SF_ERR_OUT_OF_RANGE;
        sf_nmb_bundle_t *new_bundles =
            (sf_nmb_bundle_t *)sf_xalloc(nmb->alloc, new_capacity * sizeof(*new_bundles));
        if (!new_bundles) return SF_ERR_OOM;
        if (nmb->bundle_count > 0) {
            memcpy(new_bundles, nmb->bundles, nmb->bundle_count * sizeof(*new_bundles));
        }
        sf_xfree(nmb->alloc, nmb->bundles);
        nmb->bundles = new_bundles;
        nmb->bundle_capacity = new_capacity;
    }
    nmb->bundles[nmb->bundle_count++] = *bundle;
    return SF_OK;
}

sf_result_t sf_nmb_create(sf_nmb_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_nmb_t *nmb = (sf_nmb_t *)sf_xalloc(alloc, sizeof(*nmb));
    if (!nmb) return SF_ERR_OOM;
    memset(nmb, 0, sizeof(*nmb));
    nmb->alloc = alloc;
    *out = nmb;
    return SF_OK;
}

void sf_nmb_destroy(sf_nmb_t *nmb) {
    if (!nmb) return;
    nmb_clear(nmb);
    sf_xfree(nmb->alloc, nmb);
}

bool sf_nmb_is(const void *bytes, size_t size) {
    if (!bytes) return false;
    bool big_endian = false;
    return nmb_magic_at((const uint8_t *)bytes, size, &big_endian);
}

sf_result_t sf_nmb_read_from_memory(sf_nmb_t **out,
                                    const void *bytes,
                                    size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && bytes != NULL);
    *out = NULL;
    if (!sf_nmb_is(bytes, size)) return SF_ERR_BAD_MAGIC;

    sf_nmb_t *nmb = NULL;
    sf_result_t r = sf_nmb_create(&nmb, alloc);
    if (r != SF_OK) return r;

    const uint8_t *p = (const uint8_t *)bytes;
    r = nmb_copy_raw(nmb, p, size);
    if (r != SF_OK) {
        sf_nmb_destroy(nmb);
        return r;
    }

    size_t cursor = 0;
    while (cursor < size && cursor < size - 0xCu) {
        bool big_endian = false;
        if (!nmb_magic_at(p + cursor, size - cursor, &big_endian)) {
            sf_nmb_destroy(nmb);
            return SF_ERR_BAD_MAGIC;
        }

        nmb_bundle_layout_t layout;
        if (!nmb_parse_layout(p, size, cursor, big_endian, &layout)) {
            sf_nmb_destroy(nmb);
            return SF_ERR_TRUNCATED;
        }

        sf_nmb_bundle_t bundle;
        memset(&bundle, 0, sizeof(bundle));
        bundle.type = (sf_nmb_bundle_type_t)nmb_read_u32_at(p + cursor + 8u, big_endian);
        bundle.data = nmb->raw + layout.data_offset;
        bundle.raw_offset = cursor;
        bundle.raw_size = layout.next_offset - cursor;
        bundle.data_offset = layout.data_offset;
        bundle.data_size = layout.data_size;
        r = nmb_push_bundle(nmb, &bundle);
        if (r != SF_OK) {
            sf_nmb_destroy(nmb);
            return r;
        }
        cursor = layout.next_offset;
    }

    *out = nmb;
    return SF_OK;
}

sf_result_t sf_nmb_write_to_memory(const sf_nmb_t *nmb,
                                   void **out_bytes,
                                   size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(nmb != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (nmb->raw_size == 0) return SF_OK;
    uint8_t *copy = (uint8_t *)sf_xalloc(alloc, nmb->raw_size);
    if (!copy) return SF_ERR_OOM;
    memcpy(copy, nmb->raw, nmb->raw_size);
    *out_bytes = copy;
    *out_size = nmb->raw_size;
    return SF_OK;
}

size_t sf_nmb_bundle_count(const sf_nmb_t *nmb) { return nmb ? nmb->bundle_count : 0; }

sf_nmb_bundle_t *sf_nmb_bundle_at(const sf_nmb_t *nmb, size_t i) {
    if (!nmb || i >= nmb->bundle_count) return NULL;
    return &nmb->bundles[i];
}

sf_nmb_bundle_type_t sf_nmb_bundle_type(const sf_nmb_bundle_t *bundle) {
    return bundle ? bundle->type : SF_NMB_BUNDLE_INVALID;
}

const void *sf_nmb_bundle_data(const sf_nmb_bundle_t *bundle, size_t *out_size) {
    if (out_size) *out_size = bundle ? bundle->data_size : 0;
    return bundle ? bundle->data : NULL;
}
