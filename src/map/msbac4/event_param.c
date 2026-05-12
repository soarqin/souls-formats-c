/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "msbac4_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

void msbac4_event_param_free(sf_msbac4_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msbac4_event_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                         msbac4_event_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset); if (rc != SF_OK) return rc;
    int32_t name_offset = 0, id = 0, type_data_offset = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unique_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_data_offset); if (rc != SF_OK) return rc;
    (void)id;
    if (type != MSBAC4_EVENT_SCRIPT || name_offset == 0 || type_data_offset != 0) return SF_ERR_BAD_MAGIC;
    out->type = MSBAC4_EVENT_SCRIPT;
    return sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbac4_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbac4_t *out,
                                    const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->event_count = count;
    if (count == 0) return SF_OK;
    out->events = (sf_msbac4_event_t *)sf_xalloc(a, (size_t)count * sizeof(*out->events));
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, (size_t)count * sizeof(*out->events));
    int32_t *offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) { sf_xfree(a, out->events); out->events = NULL; out->event_count = 0; return SF_ERR_OOM; }
    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, offsets);
    for (int32_t i = 0; rc == SF_OK && i < count; i++) rc = msbac4_event_read_one(r, offsets[i], &out->events[i].data, a);
    sf_xfree(a, offsets);
    if (rc != SF_OK) { msbac4_event_param_free(out->events, count, a); sf_xfree(a, out->events); out->events = NULL; out->event_count = 0; }
    return rc;
}

static sf_result_t msbac4_event_fill_rel_i32(sf_binary_writer_t *w, const char *key,
                                             int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbac4_event_write_entry(sf_binary_writer_t *w, const void *entry,
                                            size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbac4_event_t *event = (const sf_msbac4_event_t *)entry;
    if (event->data.type != MSBAC4_EVENT_SCRIPT) return SF_ERR_UNSUPPORTED_VERSION;
    char name_key[40], type_key[40];
    snprintf(name_key, sizeof name_key, "Msbac4EventName%zu", index);
    snprintf(type_key, sizeof type_key, "Msbac4EventType%zu", index);
    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, event->data.unique_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)event->data.type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, (int32_t)index); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, msbac4_event_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, event->data.name ? event->data.name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;
    return sf_binary_writer_fill_i32(w, type_key, 0);
}

sf_result_t msbac4_event_param_write(sf_binary_writer_t *w, const sf_msbac4_t *msb) {
    if (!w || !msb || msb->event_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "EVENT_PARAM_ST", "Msbac4NextList1", msb->events,
                                       (size_t)msb->event_count, sizeof(*msb->events),
                                       msbac4_event_write_entry, NULL);
}
