/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_legacy_misc.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define SF_MGF_MAGIC "MGFL"
#define SF_MGF_HEADER_SIZE 12u

#define SF_DDL_MAGIC "DDL\0"
#define SF_DDL_HEADER_SIZE 4u

struct sf_mgf {
    const sf_allocator_t *alloc;
    int32_t unk04;
    size_t file_count;
};

struct sf_ddl {
    const sf_allocator_t *alloc;
};

bool sf_mgf_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    return memcmp(bytes, SF_MGF_MAGIC, 4) == 0;
}

void sf_mgf_destroy(sf_mgf_t *mgf) {
    if (!mgf) return;
    sf_xfree(mgf->alloc, mgf);
}

int32_t sf_mgf_unk04(const sf_mgf_t *mgf) {
    return mgf ? mgf->unk04 : 0;
}

size_t sf_mgf_file_count(const sf_mgf_t *mgf) {
    return mgf ? mgf->file_count : 0u;
}

sf_result_t sf_mgf_read_from_memory(sf_mgf_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_MGF_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(bytes, SF_MGF_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_mgf_t *mgf = NULL;
    sf_result_t e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, false, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    e = sf_binary_reader_assert_ascii(r, SF_MGF_MAGIC); if (e != SF_OK) goto done;

    int32_t unk04 = 0;
    int32_t file_count = 0;
    e = sf_binary_reader_read_i32(r, &unk04);      if (e != SF_OK) goto done;
    e = sf_binary_reader_read_i32(r, &file_count); if (e != SF_OK) goto done;
    if (file_count < 0) { e = SF_ERR_TRUNCATED; goto done; }

    mgf = (sf_mgf_t *)sf_xalloc(alloc, sizeof(*mgf));
    if (!mgf) { e = SF_ERR_OOM; goto done; }
    memset(mgf, 0, sizeof(*mgf));
    mgf->alloc = alloc;
    mgf->unk04 = unk04;
    mgf->file_count = (size_t)file_count;

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_mgf_destroy(mgf); return e; }
    *out = mgf;
    return SF_OK;
}

bool sf_ddl_is(const void *bytes, size_t size) {
    if (!bytes || size < SF_DDL_HEADER_SIZE) return false;
    return memcmp(bytes, SF_DDL_MAGIC, 4) == 0;
}

void sf_ddl_destroy(sf_ddl_t *ddl) {
    if (!ddl) return;
    sf_xfree(ddl->alloc, ddl);
}

sf_result_t sf_ddl_read_from_memory(sf_ddl_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < SF_DDL_HEADER_SIZE) return SF_ERR_TRUNCATED;
    if (memcmp(bytes, SF_DDL_MAGIC, 4) != 0) return SF_ERR_BAD_MAGIC;

    alloc = sf_alloc_or_default(alloc);
    sf_ddl_t *ddl = (sf_ddl_t *)sf_xalloc(alloc, sizeof(*ddl));
    if (!ddl) return SF_ERR_OOM;
    memset(ddl, 0, sizeof(*ddl));
    ddl->alloc = alloc;
    *out = ddl;
    return SF_OK;
}
