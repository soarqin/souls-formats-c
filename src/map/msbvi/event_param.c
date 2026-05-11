/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core VI MSBVI EventParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBVI/EventParam.cs
 */

#include "msbvi_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

static bool msbvi_event_type_is_known(uint32_t type) {
    return type == 4 || type == 5 || type == 9 || type == 15 || type == 20 || type == 24 ||
           type == UINT32_MAX;
}

void msbvi_event_param_free(sf_msbvi_event_t *events, int32_t count, const sf_allocator_t *a) {
    if (!events || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, events[i].data.name);
        memset(&events[i], 0, sizeof(events[i]));
    }
}

static sf_result_t msbvi_event_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                        msbvi_event_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;
    sf_result_t rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset);
    if (rc != SF_OK) return rc;
    int64_t name_offset = 0, common_offset = 0, type_offset = 0;
    int32_t zero = 0, part_index = 0, region_index = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset);       if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->event_id);     if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &out->type);         if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->type_index);   if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &zero);              if (rc != SF_OK) return rc;
    if (zero != 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_read_i64(r, &common_offset);     if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i64(r, &type_offset);       if (rc != SF_OK) return rc;
    if (!msbvi_event_type_is_known(out->type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0 || common_offset == 0) return SF_ERR_BAD_MAGIC;
    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;
    rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset + common_offset);
    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &part_index);       if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &region_index);     if (rc != SF_OK) return rc;
    (void)part_index;
    (void)region_index;
    rc = sf_binary_reader_read_i32(r, &out->entity_id);   if (rc != SF_OK) return rc;
    int8_t minus_one = 0;
    uint8_t b0 = 0, b1 = 0, b2 = 0;
    rc = sf_binary_reader_read_i8(r, &minus_one);         if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &b0);                if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &b1);                if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u8(r, &b2);                if (rc != SF_OK) return rc;
    if (minus_one != -1 || b0 != 0 || b1 != 0 || b2 != 0) return SF_ERR_BAD_MAGIC;
    (void)type_offset;
    return SF_OK;
}

sf_result_t msbvi_event_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->event_count = count;
    out->events = NULL;
    if (count == 0) return SF_OK;
    out->events = (sf_msbvi_event_t *)sf_xalloc(a, (size_t)count * sizeof(*out->events));
    if (!out->events) return SF_ERR_OOM;
    memset(out->events, 0, (size_t)count * sizeof(*out->events));
    int64_t *offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, offsets);
    if (rc == SF_OK) for (int32_t i = 0; i < count; i++) {
        rc = msbvi_event_read_one(r, offsets[i], &out->events[i].data, a);
        if (rc != SF_OK) break;
    }
    sf_xfree(a, offsets);
    return rc;
}

static sf_result_t msbvi_event_write_one(sf_binary_writer_t *w, const msbvi_event_t *event,
                                         int32_t id) {
    (void)id;
    int64_t start = sf_binary_writer_position(w);
    char name_res[32], common_res[32], type_res[32];
    snprintf(name_res, sizeof name_res, "MsbviEventName%lld", (long long)start);
    snprintf(common_res, sizeof common_res, "MsbviEventCommon%lld", (long long)start);
    snprintf(type_res, sizeof type_res, "MsbviEventType%lld", (long long)start);
    sf_result_t rc;
    rc = sf_binary_writer_reserve_i64(w, name_res);          if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->event_id);     if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, event->type);         if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->type_index);   if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, 0);                   if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, common_res);        if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, type_res);          if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, event->name ? event->name : "", true);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, common_res, sf_binary_writer_position(w) - start);
    if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1);              if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1);              if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, event->entity_id); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0xff);             if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0);                if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0);                if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u8(w, 0);                if (rc != SF_OK) return rc;
    return sf_binary_writer_fill_i64(w, type_res, 0);
}

sf_result_t msbvi_event_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi) {
    if (!w || !msbvi) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 52); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbvi->event_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbviNameOff1"); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->event_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviEventOff%d", i); rc = sf_binary_writer_reserve_i64(w, n); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_reserve_i64(w, "MsbviNextList1"); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_fill_i64(w, "MsbviNameOff1", sf_binary_writer_position(w)); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "EVENT_PARAM_ST", true); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->event_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviEventOff%d", i); rc = sf_binary_writer_fill_i64(w, n, sf_binary_writer_position(w)); if (rc != SF_OK) return rc; rc = msbvi_event_write_one(w, &msbvi->events[i].data, i); if (rc != SF_OK) return rc; }
    return SF_OK;
}
