/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_mwc.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define SF_MWC_MMD_MAGIC "MMD\0"
#define SF_MWC_OTR_MAGIC "OTR\0"
#define SF_MWC_HEADER_SIZE 4u

struct sf_mwc_mmd {
    const sf_allocator_t *alloc;
};

struct sf_mwc_otr {
    const sf_allocator_t *alloc;
};

bool sf_mwc_mmd_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_MWC_HEADER_SIZE) return false;
    return memcmp(bytes, SF_MWC_MMD_MAGIC, 4) == 0;
}

void sf_mwc_mmd_destroy(sf_mwc_mmd_t *mmd) {
    if (!mmd) return;
    sf_xfree(mmd->alloc, mmd);
}

sf_result_t sf_mwc_mmd_read_from_memory(sf_mwc_mmd_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_MWC_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(bytes, SF_MWC_MMD_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);
    sf_mwc_mmd_t *mmd = (sf_mwc_mmd_t *)sf_xalloc(alloc, sizeof(*mmd));
    if (!mmd) return SF_ERR_OOM;
    memset(mmd, 0, sizeof(*mmd));
    mmd->alloc = alloc;
    *out = mmd;
    return SF_OK;
}

bool sf_mwc_otr_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_MWC_HEADER_SIZE) return false;
    return memcmp(bytes, SF_MWC_OTR_MAGIC, 4) == 0;
}

void sf_mwc_otr_destroy(sf_mwc_otr_t *otr) {
    if (!otr) return;
    sf_xfree(otr->alloc, otr);
}

sf_result_t sf_mwc_otr_read_from_memory(sf_mwc_otr_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_MWC_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(bytes, SF_MWC_OTR_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);
    sf_mwc_otr_t *otr = (sf_mwc_otr_t *)sf_xalloc(alloc, sizeof(*otr));
    if (!otr) return SF_ERR_OOM;
    memset(otr, 0, sizeof(*otr));
    otr->alloc = alloc;
    *out = otr;
    return SF_OK;
}
