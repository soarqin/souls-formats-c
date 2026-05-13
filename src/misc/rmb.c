/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_rmb.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

struct sf_rmb_rumble {
    sf_rmb_state_t *heavy;
    size_t heavy_count;
    size_t heavy_cap;
    sf_rmb_state_t *light;
    size_t light_count;
    size_t light_cap;
};

struct sf_rmb {
    const sf_allocator_t *alloc;
    bool big_endian;
    struct sf_rmb_rumble *rumbles;
    size_t rumble_count;
    size_t rumble_cap;
};

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

static sf_result_t rmb_grow_rumbles(sf_rmb_t *rmb) {
    if (rmb->rumble_count < rmb->rumble_cap) return SF_OK;
    size_t new_cap = rmb->rumble_cap == 0 ? 8u : rmb->rumble_cap * 2u;
    struct sf_rmb_rumble *nr = (struct sf_rmb_rumble *)sf_xalloc(
        rmb->alloc, new_cap * sizeof(*nr));
    if (!nr) return SF_ERR_OOM;
    memset(nr, 0, new_cap * sizeof(*nr));
    if (rmb->rumbles) {
        memcpy(nr, rmb->rumbles, rmb->rumble_count * sizeof(*nr));
        sf_xfree(rmb->alloc, rmb->rumbles);
    }
    rmb->rumbles = nr;
    rmb->rumble_cap = new_cap;
    return SF_OK;
}

static sf_result_t rmb_push_state(sf_rmb_t *rmb, sf_rmb_state_t **states,
                                  size_t *count, size_t *cap, sf_rmb_state_t state) {
    if (*count >= *cap) {
        size_t new_cap = *cap == 0 ? 8u : *cap * 2u;
        sf_rmb_state_t *ns = (sf_rmb_state_t *)sf_xalloc(rmb->alloc, new_cap * sizeof(*ns));
        if (!ns) return SF_ERR_OOM;
        if (*states) {
            memcpy(ns, *states, *count * sizeof(*ns));
            sf_xfree(rmb->alloc, *states);
        }
        *states = ns;
        *cap = new_cap;
    }
    (*states)[(*count)++] = state;
    return SF_OK;
}

sf_result_t sf_rmb_create(sf_rmb_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_rmb_t *rmb = (sf_rmb_t *)sf_xalloc(alloc, sizeof(*rmb));
    if (!rmb) return SF_ERR_OOM;
    memset(rmb, 0, sizeof(*rmb));
    rmb->alloc = alloc;
    *out = rmb;
    return SF_OK;
}

void sf_rmb_destroy(sf_rmb_t *rmb) {
    if (!rmb) return;
    for (size_t i = 0; i < rmb->rumble_count; i++) {
        sf_xfree(rmb->alloc, rmb->rumbles[i].heavy);
        sf_xfree(rmb->alloc, rmb->rumbles[i].light);
    }
    sf_xfree(rmb->alloc, rmb->rumbles);
    sf_xfree(rmb->alloc, rmb);
}

bool sf_rmb_big_endian(const sf_rmb_t *rmb) { return rmb ? rmb->big_endian : false; }
void sf_rmb_set_big_endian(sf_rmb_t *rmb, bool big_endian) {
    if (rmb) rmb->big_endian = big_endian;
}

size_t sf_rmb_rumble_count(const sf_rmb_t *rmb) { return rmb ? rmb->rumble_count : 0u; }

sf_result_t sf_rmb_add_rumble(sf_rmb_t *rmb, size_t *out_rumble_index) {
    SF_CHECK_ARG(rmb != NULL);
    TRY(rmb_grow_rumbles(rmb));
    if (out_rumble_index) *out_rumble_index = rmb->rumble_count;
    memset(&rmb->rumbles[rmb->rumble_count], 0, sizeof(rmb->rumbles[0]));
    rmb->rumble_count++;
    return SF_OK;
}

sf_result_t sf_rmb_get_heavy_state_count(const sf_rmb_t *rmb, size_t rumble_index,
                                         size_t *out_count) {
    SF_CHECK_ARG(rmb != NULL && out_count != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    *out_count = rmb->rumbles[rumble_index].heavy_count;
    return SF_OK;
}

sf_result_t sf_rmb_get_light_state_count(const sf_rmb_t *rmb, size_t rumble_index,
                                         size_t *out_count) {
    SF_CHECK_ARG(rmb != NULL && out_count != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    *out_count = rmb->rumbles[rumble_index].light_count;
    return SF_OK;
}

sf_result_t sf_rmb_get_heavy_state(const sf_rmb_t *rmb, size_t rumble_index,
                                   size_t state_index, sf_rmb_state_t *out_state) {
    SF_CHECK_ARG(rmb != NULL && out_state != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    if (state_index >= rmb->rumbles[rumble_index].heavy_count) return SF_ERR_OUT_OF_RANGE;
    *out_state = rmb->rumbles[rumble_index].heavy[state_index];
    return SF_OK;
}

sf_result_t sf_rmb_get_light_state(const sf_rmb_t *rmb, size_t rumble_index,
                                   size_t state_index, sf_rmb_state_t *out_state) {
    SF_CHECK_ARG(rmb != NULL && out_state != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    if (state_index >= rmb->rumbles[rumble_index].light_count) return SF_ERR_OUT_OF_RANGE;
    *out_state = rmb->rumbles[rumble_index].light[state_index];
    return SF_OK;
}

sf_result_t sf_rmb_add_heavy_state(sf_rmb_t *rmb, size_t rumble_index,
                                   float power, float duration) {
    SF_CHECK_ARG(rmb != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    sf_rmb_state_t st = { power, duration };
    struct sf_rmb_rumble *ru = &rmb->rumbles[rumble_index];
    return rmb_push_state(rmb, &ru->heavy, &ru->heavy_count, &ru->heavy_cap, st);
}

sf_result_t sf_rmb_add_light_state(sf_rmb_t *rmb, size_t rumble_index,
                                   float power, float duration) {
    SF_CHECK_ARG(rmb != NULL);
    if (rumble_index >= rmb->rumble_count) return SF_ERR_OUT_OF_RANGE;
    sf_rmb_state_t st = { power, duration };
    struct sf_rmb_rumble *ru = &rmb->rumbles[rumble_index];
    return rmb_push_state(rmb, &ru->light, &ru->light_count, &ru->light_cap, st);
}

sf_result_t sf_rmb_read_from_memory(sf_rmb_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_rmb_t *rmb = NULL;
    int16_t *heavy_counts = NULL;
    int16_t *light_counts = NULL;
    int32_t *heavy_offsets = NULL;
    int32_t *light_offsets = NULL;
    sf_result_t e = SF_OK;

    alloc = sf_alloc_or_default(alloc);

    bool big_endian = false;
    if (size >= 8) {
        uint32_t be_check = 0;
        memcpy(&be_check, (const uint8_t *)bytes + 4, 4);
        big_endian = (be_check == 0x10000000u);
    }

    e = sf_istream_open_memory(&s, bytes, size, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_reader_create(&r, s, big_endian, alloc);
    if (e != SF_OK) { sf_istream_close(s); return e; }

    int16_t rumble_count = 0;
    e = sf_binary_reader_read_i16(r, &rumble_count); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i16_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0x10); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
    e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;

    if (rumble_count < 0) { e = SF_ERR_INVALID_ARG; goto done; }

    e = sf_rmb_create(&rmb, alloc); if (e != SF_OK) goto done;
    rmb->big_endian = big_endian;

    size_t n = (size_t)rumble_count;
    if (n > 0) {
        heavy_counts = (int16_t *)sf_xalloc(alloc, n * sizeof(int16_t));
        light_counts = (int16_t *)sf_xalloc(alloc, n * sizeof(int16_t));
        heavy_offsets = (int32_t *)sf_xalloc(alloc, n * sizeof(int32_t));
        light_offsets = (int32_t *)sf_xalloc(alloc, n * sizeof(int32_t));
        if (!heavy_counts || !light_counts || !heavy_offsets || !light_offsets) {
            e = SF_ERR_OOM; goto done;
        }
    }

    for (size_t i = 0; i < n; i++) {
        e = sf_binary_reader_read_i16(r, &heavy_counts[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i16(r, &light_counts[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_assert_i32_one(r, 0); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &heavy_offsets[i]); if (e != SF_OK) goto done;
        e = sf_binary_reader_read_i32(r, &light_offsets[i]); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < n; i++) {
        size_t rumble_idx = 0;
        e = sf_rmb_add_rumble(rmb, &rumble_idx); if (e != SF_OK) goto done;

        int16_t hc = heavy_counts[i];
        int16_t lc = light_counts[i];
        if (hc < 0 || lc < 0) { e = SF_ERR_INVALID_ARG; goto done; }

        if (hc > 0) {
            e = sf_binary_reader_step_in(r, (int64_t)heavy_offsets[i]);
            if (e != SF_OK) goto done;
            for (int16_t j = 0; j < hc; j++) {
                float p = 0.0f, d = 0.0f;
                e = sf_binary_reader_read_f32(r, &p);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
                e = sf_binary_reader_read_f32(r, &d);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
                e = sf_rmb_add_heavy_state(rmb, rumble_idx, p, d);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            }
            e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
        }

        if (lc > 0) {
            e = sf_binary_reader_step_in(r, (int64_t)light_offsets[i]);
            if (e != SF_OK) goto done;
            for (int16_t j = 0; j < lc; j++) {
                float p = 0.0f, d = 0.0f;
                e = sf_binary_reader_read_f32(r, &p);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
                e = sf_binary_reader_read_f32(r, &d);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
                e = sf_rmb_add_light_state(rmb, rumble_idx, p, d);
                if (e != SF_OK) { sf_binary_reader_step_out(r); goto done; }
            }
            e = sf_binary_reader_step_out(r); if (e != SF_OK) goto done;
        }
    }

done:
    sf_xfree(alloc, heavy_counts);
    sf_xfree(alloc, light_counts);
    sf_xfree(alloc, heavy_offsets);
    sf_xfree(alloc, light_offsets);
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (e != SF_OK) { sf_rmb_destroy(rmb); return e; }
    *out = rmb;
    return SF_OK;
}

sf_result_t sf_rmb_write_to_memory(const sf_rmb_t *rmb, void **out_bytes,
                                   size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(rmb != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL; *out_size = 0;

    sf_ostream_t *s = NULL;
    sf_binary_writer_t *w = NULL;
    sf_result_t e = sf_ostream_open_memory(&s, alloc);
    if (e != SF_OK) return e;
    e = sf_binary_writer_create(&w, s, rmb->big_endian, alloc);
    if (e != SF_OK) { sf_ostream_close(s); return e; }

    e = sf_binary_writer_write_i16(w, (int16_t)rmb->rumble_count); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i16(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0x10); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
    e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;

    for (size_t i = 0; i < rmb->rumble_count; i++) {
        char hkey[48], lkey[48];
        snprintf(hkey, sizeof(hkey), "HeavyOffset[%zu]", i);
        snprintf(lkey, sizeof(lkey), "LightOffset[%zu]", i);
        e = sf_binary_writer_write_i16(w, (int16_t)rmb->rumbles[i].heavy_count); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i16(w, (int16_t)rmb->rumbles[i].light_count); if (e != SF_OK) goto done;
        e = sf_binary_writer_write_i32(w, 0); if (e != SF_OK) goto done;
        e = sf_binary_writer_reserve_i32(w, hkey); if (e != SF_OK) goto done;
        e = sf_binary_writer_reserve_i32(w, lkey); if (e != SF_OK) goto done;
    }

    for (size_t i = 0; i < rmb->rumble_count; i++) {
        char hkey[48], lkey[48];
        snprintf(hkey, sizeof(hkey), "HeavyOffset[%zu]", i);
        snprintf(lkey, sizeof(lkey), "LightOffset[%zu]", i);

        if (rmb->rumbles[i].heavy_count == 0) {
            e = sf_binary_writer_fill_i32(w, hkey, 0); if (e != SF_OK) goto done;
        } else {
            e = sf_binary_writer_fill_i32(w, hkey,
                                           (int32_t)sf_binary_writer_position(w));
            if (e != SF_OK) goto done;
            for (size_t j = 0; j < rmb->rumbles[i].heavy_count; j++) {
                e = sf_binary_writer_write_f32(w, rmb->rumbles[i].heavy[j].power);
                if (e != SF_OK) goto done;
                e = sf_binary_writer_write_f32(w, rmb->rumbles[i].heavy[j].duration);
                if (e != SF_OK) goto done;
            }
        }

        if (rmb->rumbles[i].light_count == 0) {
            e = sf_binary_writer_fill_i32(w, lkey, 0); if (e != SF_OK) goto done;
        } else {
            e = sf_binary_writer_fill_i32(w, lkey,
                                           (int32_t)sf_binary_writer_position(w));
            if (e != SF_OK) goto done;
            for (size_t j = 0; j < rmb->rumbles[i].light_count; j++) {
                e = sf_binary_writer_write_f32(w, rmb->rumbles[i].light[j].power);
                if (e != SF_OK) goto done;
                e = sf_binary_writer_write_f32(w, rmb->rumbles[i].light[j].duration);
                if (e != SF_OK) goto done;
            }
        }
    }

    e = sf_binary_writer_finish_bytes(w, (uint8_t **)out_bytes, out_size);

done:
    sf_binary_writer_destroy(w);
    sf_ostream_close(s);
    return e;
}
