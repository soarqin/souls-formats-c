/* SPDX-License-Identifier: GPL-3.0-or-later */

// Upstream: PMDCL.cs

#include "souls_formats/sf_pmdcl.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

/*===========================================================================
 * Internal struct definitions (NOT exposed in the public header)
 *===========================================================================*/

struct sf_pmdcl_decal {
    sf_vec3_t x_angles;
    sf_vec3_t y_angles;
    sf_vec3_t z_angles;
    sf_vec3_t position;
    float     unk3c;
    int32_t   decal_param_id;
    int16_t   size1;
    int16_t   size2;
};

struct sf_pmdcl {
    struct sf_pmdcl_decal *decals;
    size_t                 decal_count;
    const sf_allocator_t  *alloc;
};

/*===========================================================================
 * Read
 * Upstream: PMDCL.cs:Read()
 *===========================================================================*/

sf_result_t sf_pmdcl_read_from_memory(sf_pmdcl_t **out, const void *bytes, size_t size,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_pmdcl_t *pmdcl = NULL;
    sf_result_t e = SF_OK;

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_reader_create(&r, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    /* Read header: count, 0x20, 0, 0 */
    int64_t decal_count = 0;
    e = sf_binary_reader_read_i64(r, &decal_count);       if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i64_one(r, 0x20);         if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i64_one(r, 0);            if (e != SF_OK) goto cleanup;
    e = sf_binary_reader_assert_i64_one(r, 0);            if (e != SF_OK) goto cleanup;

    if (decal_count < 0) { e = SF_ERR_BAD_MAGIC; goto cleanup; }

    /* Allocate the top-level struct */
    pmdcl = (sf_pmdcl_t *)sf_xalloc(alloc, sizeof(*pmdcl));
    if (!pmdcl) { e = SF_ERR_OOM; goto cleanup; }
    memset(pmdcl, 0, sizeof(*pmdcl));
    pmdcl->alloc = alloc;
    pmdcl->decal_count = (size_t)decal_count;

    /* Bulk-allocate decal array */
    if (decal_count > 0) {
        pmdcl->decals = (struct sf_pmdcl_decal *)sf_xalloc(
            alloc, (size_t)decal_count * sizeof(*pmdcl->decals));
        if (!pmdcl->decals) { e = SF_ERR_OOM; goto cleanup; }
        memset(pmdcl->decals, 0, (size_t)decal_count * sizeof(*pmdcl->decals));
    }

    /* Read each decal via offset table (StepIn/StepOut pattern) */
    for (int64_t i = 0; i < decal_count; i++) {
        int64_t offset = 0;
        e = sf_binary_reader_read_i64(r, &offset);        if (e != SF_OK) goto cleanup;
        e = sf_binary_reader_step_in(r, offset);          if (e != SF_OK) goto cleanup;

        struct sf_pmdcl_decal *d = &pmdcl->decals[i];

        e = sf_binary_reader_read_vec3(r, &d->x_angles);  if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i32_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_vec3(r, &d->y_angles);  if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i32_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_vec3(r, &d->z_angles);  if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i32_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_vec3(r, &d->position);  if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_f32(r, &d->unk3c);      if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_i32(r, &d->decal_param_id); if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_i16(r, &d->size1);      if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_read_i16(r, &d->size2);      if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i64_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i64_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }
        e = sf_binary_reader_assert_i64_one(r, 0);        if (e != SF_OK) { sf_binary_reader_step_out(r); goto cleanup; }

        e = sf_binary_reader_step_out(r);                 if (e != SF_OK) goto cleanup;
    }

    *out = pmdcl;
    pmdcl = NULL; /* ownership transferred */

cleanup:
    if (pmdcl) {
        sf_xfree(alloc, pmdcl->decals);
        sf_xfree(alloc, pmdcl);
    }
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    return e;
}

/*===========================================================================
 * Write
 * Upstream: PMDCL.cs:Write()
 *===========================================================================*/

sf_result_t sf_pmdcl_write_to_buffer(const sf_pmdcl_t *pmdcl, void **out_bytes,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(pmdcl != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = SF_OK;

    e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;

    e = sf_binary_writer_create(&w, s, false /* LE */, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    /* Header */
    e = sf_binary_writer_write_i64(w, (int64_t)pmdcl->decal_count); if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i64(w, 0x20);                         if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i64(w, 0);                            if (e != SF_OK) goto cleanup;
    e = sf_binary_writer_write_i64(w, 0);                            if (e != SF_OK) goto cleanup;

    /* Reserve offset slots for each decal */
    for (size_t i = 0; i < pmdcl->decal_count; i++) {
        char name[32];
        /* safe: i < SIZE_MAX, format produces at most ~25 chars */
        (void)snprintf(name, sizeof(name), "Decal%zu", i);
        e = sf_binary_writer_reserve_i64(w, name);                   if (e != SF_OK) goto cleanup;
    }

    /* Pad to 0x20 alignment */
    e = sf_binary_writer_pad(w, 0x20);                               if (e != SF_OK) goto cleanup;

    /* Write each decal, filling its offset slot */
    for (size_t i = 0; i < pmdcl->decal_count; i++) {
        char name[32];
        (void)snprintf(name, sizeof(name), "Decal%zu", i);

        int64_t pos = sf_binary_writer_position(w);
        e = sf_binary_writer_fill_i64(w, name, pos);                 if (e != SF_OK) goto cleanup;

        const struct sf_pmdcl_decal *d = &pmdcl->decals[i];

        e = sf_binary_writer_write_vec3(w, d->x_angles);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec3(w, d->y_angles);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec3(w, d->z_angles);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, 0);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_vec3(w, d->position);             if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_f32(w, d->unk3c);                 if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i32(w, d->decal_param_id);        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i16(w, d->size1);                 if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i16(w, d->size2);                 if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i64(w, 0);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i64(w, 0);                        if (e != SF_OK) goto cleanup;
        e = sf_binary_writer_write_i64(w, 0);                        if (e != SF_OK) goto cleanup;
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);
    w = NULL; /* finish_bytes destroys the writer */

cleanup:
    if (w) sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}

/*===========================================================================
 * Destroy
 *===========================================================================*/

void sf_pmdcl_destroy(sf_pmdcl_t *pmdcl) {
    if (!pmdcl) return;
    sf_xfree(pmdcl->alloc, pmdcl->decals);
    sf_xfree(pmdcl->alloc, pmdcl);
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

size_t sf_pmdcl_decal_count(const sf_pmdcl_t *pmdcl) {
    return pmdcl ? pmdcl->decal_count : 0u;
}

const sf_pmdcl_decal_t *sf_pmdcl_get_decal(const sf_pmdcl_t *pmdcl, size_t index) {
    if (!pmdcl || index >= pmdcl->decal_count) return NULL;
    return (const sf_pmdcl_decal_t *)&pmdcl->decals[index];
}

sf_vec3_t sf_pmdcl_decal_x_angles(const sf_pmdcl_decal_t *decal) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return decal ? decal->x_angles : zero;
}

sf_vec3_t sf_pmdcl_decal_y_angles(const sf_pmdcl_decal_t *decal) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return decal ? decal->y_angles : zero;
}

sf_vec3_t sf_pmdcl_decal_z_angles(const sf_pmdcl_decal_t *decal) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return decal ? decal->z_angles : zero;
}

sf_vec3_t sf_pmdcl_decal_position(const sf_pmdcl_decal_t *decal) {
    sf_vec3_t zero = {0.0f, 0.0f, 0.0f};
    return decal ? decal->position : zero;
}

float sf_pmdcl_decal_unk3c(const sf_pmdcl_decal_t *decal) {
    return decal ? decal->unk3c : 0.0f;
}

int32_t sf_pmdcl_decal_param_id(const sf_pmdcl_decal_t *decal) {
    return decal ? decal->decal_param_id : 0;
}

int16_t sf_pmdcl_decal_size1(const sf_pmdcl_decal_t *decal) {
    return decal ? decal->size1 : 0;
}

int16_t sf_pmdcl_decal_size2(const sf_pmdcl_decal_t *decal) {
    return decal ? decal->size2 : 0;
}
