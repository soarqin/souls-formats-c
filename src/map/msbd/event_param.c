/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MSBD legacy MSB EventParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBD/EventParam.cs
 */

#include "msbd_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_legacy_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbd_event_type_is_known(uint32_t type) {
    return type <= MSBD_EVENT_PSEUDO_MULTIPLAYER;
}

void msbd_event_param_free(sf_msbd_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msbd_event_read_type_data(sf_binary_reader_t *r, msbd_event_t *event) {
    switch (event->type) {
    case MSBD_EVENT_LIGHT:
    case MSBD_EVENT_SFX:
    case MSBD_EVENT_TREASURE:
    case MSBD_EVENT_MESSAGE:
    case MSBD_EVENT_OBJ_ACT:
    case MSBD_EVENT_SPAWN_POINT:
    case MSBD_EVENT_NAVMESH:
    case MSBD_EVENT_ENVIRONMENT:
    case MSBD_EVENT_PSEUDO_MULTIPLAYER:
        return sf_binary_reader_read_i32(r, &event->type_value0);
    case MSBD_EVENT_SOUND:
    case MSBD_EVENT_WIND:
    case MSBD_EVENT_GENERATOR:
    case MSBD_EVENT_MAP_OFFSET:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbd_event_write_type_data(sf_binary_writer_t *w, const msbd_event_t *event) {
    switch (event->type) {
    case MSBD_EVENT_LIGHT:
    case MSBD_EVENT_SFX:
    case MSBD_EVENT_TREASURE:
    case MSBD_EVENT_MESSAGE:
    case MSBD_EVENT_OBJ_ACT:
    case MSBD_EVENT_SPAWN_POINT:
    case MSBD_EVENT_NAVMESH:
    case MSBD_EVENT_ENVIRONMENT:
    case MSBD_EVENT_PSEUDO_MULTIPLAYER:
        return sf_binary_writer_write_i32(w, event->type_value0);
    case MSBD_EVENT_SOUND:
    case MSBD_EVENT_WIND:
    case MSBD_EVENT_GENERATOR:
    case MSBD_EVENT_MAP_OFFSET:
    default:
        return SF_ERR_UNSUPPORTED_VERSION;
    }
}

static sf_result_t msbd_event_read_one(sf_binary_reader_t *r, int32_t entry_offset,
                                       msbd_event_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int32_t name_offset = 0, base_data_offset = 0, type_data_offset = 0, id = 0;
    uint32_t type = 0;
    rc = sf_binary_reader_read_i32(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id); if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_read_i32(r, &base_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &type_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    if (!msbd_event_type_is_known(type)) return SF_ERR_UNSUPPORTED_VERSION;
    out->type = (msbd_event_type_t)type;
    if (name_offset == 0 || base_data_offset == 0 || type_data_offset == 0) return SF_ERR_BAD_MAGIC;

    rc = sf_binary_reader_get_ascii(r, (int64_t)entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, (int64_t)entry_offset + base_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_assert_i32_one(r, 0); if (rc != SF_OK) return rc;

    rc = sf_istream_seek(stream, (int64_t)entry_offset + type_data_offset); if (rc != SF_OK) return rc;
    return msbd_event_read_type_data(r, out);
}

sf_result_t msbd_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbd_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->event_count = count;
    out->events = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->events);
    out->events = (sf_msbd_event_t *)sf_xalloc(a, bytes);
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, bytes);

    int32_t *entry_offsets = (int32_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) {
        sf_xfree(a, out->events);
        out->events = NULL;
        out->event_count = 0;
        return SF_ERR_OOM;
    }

    sf_result_t rc = sf_binary_reader_read_i32s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbd_event_read_one(r, entry_offsets[i], &out->events[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbd_event_param_free(out->events, count, a);
        sf_xfree(a, out->events);
        out->events = NULL;
        out->event_count = 0;
    }
    return rc;
}

static sf_result_t msbd_event_fill_rel_i32(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t rel = sf_binary_writer_position(w) - start;
    if (rel < 0 || rel > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)rel);
}

static sf_result_t msbd_event_write_one(sf_binary_writer_t *w, const msbd_event_t *event,
                                        int32_t id, int32_t index) {
    if (!msbd_event_type_is_known((uint32_t)event->type)) return SF_ERR_UNSUPPORTED_VERSION;

    char name_key[32], base_key[32], type_key[32];
    snprintf(name_key, sizeof name_key, "MsbdEventName%d", index);
    snprintf(base_key, sizeof base_key, "MsbdEventBase%d", index);
    snprintf(type_key, sizeof type_key, "MsbdEventType%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, event->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)event->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, base_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i32(w, type_key), return rc);
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, msbd_event_fill_rel_i32(w, name_key, start), return rc);
    rc = sf_binary_writer_write_ascii(w, event->name ? event->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 4); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, msbd_event_fill_rel_i32(w, base_key, start), return rc);
    rc = sf_binary_writer_write_i32(w, event->part_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->region_index); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, msbd_event_fill_rel_i32(w, type_key, start), return rc);
    return msbd_event_write_type_data(w, event);
}

static sf_result_t msbd_event_write_entry(sf_binary_writer_t *w, const void *entry,
                                          size_t index, void *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbd_event_t *event = (const sf_msbd_event_t *)entry;
    return msbd_event_write_one(w, &event->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbd_event_param_write(sf_binary_writer_t *w, const sf_msbd_t *msbd) {
    if (!w || !msbd || msbd->event_count < 0) return SF_ERR_INVALID_ARG;
    return msb_legacy_entry_list_write(w, "EVENT_PARAM_ST", "MsbdNextList1", msbd->events,
                                       (size_t)msbd->event_count, sizeof(*msbd->events),
                                       msbd_event_write_entry, NULL);
}
