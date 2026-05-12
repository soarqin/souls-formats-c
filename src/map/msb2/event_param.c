/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msb2_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msb2_event_param_free(sf_msb2_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msb2_event_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                       msb2_event_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, base_offset = 0, type_offset = 0, zero = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &zero); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &base_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_offset); if (rc != SF_OK) return rc;
    (void)id;
    if (zero != 0 || type != MSB2_EVENT_LIGHT || name_offset == 0 || base_offset == 0 ||
        type_offset == 0) return SF_ERR_BAD_MAGIC;
    out->type = MSB2_EVENT_LIGHT;
    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + base_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;
    rc = sf_istream_seek(stream, (int64_t)entry_offset + type_offset); if (rc != SF_OK) return rc;
    return sf_binary_reader_read_i32(r, &out->type_value0);
}

sf_result_t msb2_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msb2_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->event_count = count;
    if (count == 0) return SF_OK;
    out->events = (sf_msb2_event_t *)sf_xalloc(a, (size_t)count * sizeof(*out->events));
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, (size_t)count * sizeof(*out->events));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->events); out->events = NULL; out->event_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msb2_event_read_one(r, offsets[i], &out->events[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msb2_event_param_free(out->events, count, a); sf_xfree(a, out->events); out->events = NULL; out->event_count = 0; }
    return rc;
}

static sf_result_t msb2_event_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msb2_event_write_entry(sf_binary_writer_t *w, const void *entry,
                                          size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msb2_event_t *event = (const sf_msb2_event_t *)entry;
    if (event->data.type != MSB2_EVENT_LIGHT) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[32], base_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "Msb2EventName%zu", index);
    snprintf(base_key, sizeof base_key, "Msb2EventBase%zu", index);
    snprintf(type_key, sizeof type_key, "Msb2EventType%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, event->data.event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)event->data.type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, base_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msb2_event_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, event->data.name ? event->data.name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_event_fill_rel_i32(w, base_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, event->data.part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->data.region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->data.entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, msb2_event_fill_rel_i32(w, type_key, start), return rc);
    return sf_binary_writer_write_i32(w, event->data.type_value0);
}

sf_result_t msb2_event_param_write(sf_binary_writer_t *w, const sf_msb2_t *msb2) {
    if (!w || !msb2 || msb2->event_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "EVENT_PARAM_ST", "Msb2NextList1", msb2->events,
                                       (size_t)msb2->event_count, sizeof(*msb2->events),
                                       msb2_event_write_entry, NULL);
}
