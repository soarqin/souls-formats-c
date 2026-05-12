/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Elden Ring MSBE EventParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBE/EventParam.cs
 */

#include "msbe_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h" /* IWYU pragma: keep */

#include <stdio.h>
#include <string.h>

enum {
    MSBE_EVENT_TREASURE            = 4,
    MSBE_EVENT_GENERATOR           = 5,
    MSBE_EVENT_OBJ_ACT             = 7,
    MSBE_EVENT_NAVMESH             = 10,
    MSBE_EVENT_PSEUDO_MULTIPLAYER  = 12,
    MSBE_EVENT_PLATOON_INFO        = 15,
    MSBE_EVENT_PATROL_INFO         = 20,
    MSBE_EVENT_MOUNT               = 21,
    MSBE_EVENT_SIGN_POOL           = 23,
    MSBE_EVENT_RETRY_POINT         = 24,
    MSBE_EVENT_AREA_TEAM           = 25,
    MSBE_EVENT_OTHER               = UINT32_MAX,
};

static bool msbe_event_type_is_known(uint32_t type) {
    switch (type) {
    case MSBE_EVENT_TREASURE:
    case MSBE_EVENT_GENERATOR:
    case MSBE_EVENT_OBJ_ACT:
    case MSBE_EVENT_NAVMESH:
    case MSBE_EVENT_PSEUDO_MULTIPLAYER:
    case MSBE_EVENT_PLATOON_INFO:
    case MSBE_EVENT_PATROL_INFO:
    case MSBE_EVENT_MOUNT:
    case MSBE_EVENT_SIGN_POOL:
    case MSBE_EVENT_RETRY_POINT:
    case MSBE_EVENT_AREA_TEAM:
    case MSBE_EVENT_OTHER:
        return true;
    default:
        return false;
    }
}

static bool msbe_event_has_type_data(uint32_t type) {
    return type != MSBE_EVENT_OTHER;
}

void msbe_event_param_free(sf_msbe_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msbe_event_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbe_event_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0;
    int64_t base_data_offset = 0;
    int64_t type_data_offset = 0;
    int64_t unk3_offset = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset);      if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->event_id);    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type);        if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->other_id);    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk14);       if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &base_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_data_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &unk3_offset);      if (rc != SF_OK) return rc;

    if (!msbe_event_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || base_data_offset == 0 || unk3_offset == 0) return SF_ERR_BAD_MAGIC;
    if (msbe_event_has_type_data(out->type) != (type_data_offset != 0)) return SF_ERR_BAD_MAGIC;
    return sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbe_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbe_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->event_count = count;
    out->events = NULL;
    if (count == 0) return SF_OK;

    out->events = (sf_msbe_event_t *)sf_xalloc(a, (size_t)count * sizeof(*out->events));
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, (size_t)count * sizeof(*out->events));
    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbe_event_read_one(r, entry_offsets[i], &out->events[i].data, a);
            if (rc != SF_OK) break;
        }
    }
    sf_xfree(a, entry_offsets);
    return rc;
}

static sf_result_t msbe_event_write_type_data(sf_binary_writer_t *w, uint32_t type) {
    (void)type;
    for (int i = 0; i < 16; i++) {
        sf_result_t rc = sf_binary_writer_write_i32(w, 0);
        if (rc != SF_OK) return rc;
    }
    return SF_OK;
}

static sf_result_t msbe_event_write_one(sf_binary_writer_t *w, const msbe_event_t *event,
                                        int32_t id, int32_t index) {
    char name_key[32];
    char base_key[32];
    char type_key[32];
    char unk_key[32];
    snprintf(name_key, sizeof name_key, "MsbeEventName%d", index);
    snprintf(base_key, sizeof base_key, "MsbeEventBase%d", index);
    snprintf(type_key, sizeof type_key, "MsbeEventType%d", index);
    snprintf(unk_key, sizeof unk_key, "MsbeEventUnk%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_key), return rc);
    rc = sf_binary_writer_write_i32(w, event->event_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, event->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->other_id != -1 ? event->other_id : id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->unk14); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, base_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, type_key), return rc);
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, unk_key), return rc);

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, event->name ? event->name : "", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, base_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, 0); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;

    if (msbe_event_has_type_data(event->type)) {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_key, sf_binary_writer_position(w) - start), return rc);
        rc = msbe_event_write_type_data(w, event->type); if (rc != SF_OK) return rc;
    } else {
        SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, type_key, 0), return rc);
    }
    rc = sf_binary_writer_pad(w, event->type == MSBE_EVENT_PSEUDO_MULTIPLAYER ? 4 : 8); if (rc != SF_OK) return rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, unk_key, sf_binary_writer_position(w) - start), return rc);
    for (int i = 0; i < 8; i++) {
        rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc;
    }
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t msbe_event_write_entry(sf_binary_writer_t *w,
                                          const void         *entry,
                                          size_t              index,
                                          void               *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbe_event_t *event = (const sf_msbe_event_t *)entry;
    return msbe_event_write_one(w, &event->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbe_event_param_write(sf_binary_writer_t *w, const sf_msbe_t *msbe) {
    if (!w || !msbe) return SF_ERR_INVALID_ARG;
    return msb_entry_list_write(w, 73, "EVENT_PARAM_ST", "MsbeNextList1", msbe->events,
                                (size_t)msbe->event_count, sizeof(*msbe->events),
                                msbe_event_write_entry, NULL);
}
