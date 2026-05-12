/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — Sekiro MSBS RouteParam.
 *
 * Mirrors upstream:
 *   SoulsFormats/Formats/MSB/MSBS/RouteParam.cs
 */

#include "msbs_internal.h"

#include "internal/sf_internal.h"
#include "map/msb_internal.h" /* IWYU pragma: keep */

#include <stdio.h>
#include <string.h>

static bool msbs_route_type_is_known(uint32_t type) {
    switch (type) {
    case MSBS_ROUTE_MUFFLING_PORTAL_LINK:
    case MSBS_ROUTE_MUFFLING_BOX_LINK:
        return true;
    default:
        return false;
    }
}

void msbs_route_param_free(sf_msbs_route_t *routes, int32_t count, const sf_allocator_t *a) {
    if (!routes || count <= 0) return;
    for (int32_t i = 0; i < count; i++) {
        sf_xfree(a, routes[i].data.name);
        memset(&routes[i], 0, sizeof(routes[i]));
    }
}

static sf_result_t msbs_route_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                       msbs_route_t *out, const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->alloc = a;

    sf_istream_t *stream = sf_binary_reader_stream(r);
    sf_result_t rc = sf_istream_seek(stream, entry_offset);
    if (rc != SF_OK) return rc;

    int64_t name_offset = 0;
    uint32_t type = 0;
    int32_t id = 0;

    rc = sf_binary_reader_read_i64(r, &name_offset);    if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk08);     if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk0c);     if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_u32(r, &type);           if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &id);             if (rc != SF_OK) return rc;
    (void)id;
    rc = sf_binary_reader_assert_pattern(r, 0x68, 0x00); if (rc != SF_OK) return rc;

    if (!msbs_route_type_is_known(type)) return SF_ERR_UNSUPPORTED_VERSION;
    if (name_offset == 0) return SF_ERR_BAD_MAGIC;

    out->type = (msbs_route_type_t)type;

    rc = sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
    if (rc != SF_OK) return rc;

    return SF_OK;
}

sf_result_t msbs_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbs_t *out,
                                  const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;

    out->route_count = count;
    out->routes = NULL;
    if (count == 0) return SF_OK;

    size_t bytes = (size_t)count * sizeof(*out->routes);
    out->routes = (sf_msbs_route_t *)sf_xalloc(a, bytes);
    if (!out->routes) return SF_ERR_OOM;
    memset(out->routes, 0, bytes);

    int64_t *entry_offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*entry_offsets));
    if (!entry_offsets) return SF_ERR_OOM;

    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, entry_offsets);
    if (rc == SF_OK) {
        for (int32_t i = 0; i < count; i++) {
            rc = msbs_route_read_one(r, entry_offsets[i], &out->routes[i].data, a);
            if (rc != SF_OK) break;
        }
    }

    sf_xfree(a, entry_offsets);
    if (rc != SF_OK) {
        msbs_route_param_free(out->routes, count, a);
        sf_xfree(a, out->routes);
        out->routes = NULL;
        out->route_count = 0;
    }
    return rc;
}

static sf_result_t msbs_route_write_one(sf_binary_writer_t *w, const msbs_route_t *route,
                                        int32_t id, int32_t index) {
    char name_offset_key[32];
    snprintf(name_offset_key, sizeof name_offset_key, "MsbsRouteName%d", index);

    int64_t start = sf_binary_writer_position(w);
    sf_result_t rc;
    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_reserve_i64(w, name_offset_key), return rc);
    rc = sf_binary_writer_write_i32(w, route->unk08);         if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, route->unk0c);         if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_u32(w, (uint32_t)route->type); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, id);                   if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_pattern(w, 0x68, 0x00);       if (rc != SF_OK) return rc;

    SF_RESERVE_FILL_PAIR(rc, sf_binary_writer_fill_i64(w, name_offset_key, sf_binary_writer_position(w) - start), return rc);
    rc = sf_binary_writer_write_utf16(w, route->name ? route->name : "", true);
    if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

static sf_result_t msbs_route_write_entry(sf_binary_writer_t *w,
                                          const void         *entry,
                                          size_t              index,
                                          void               *ctx) {
    (void)ctx;
    if (index > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    const sf_msbs_route_t *route = (const sf_msbs_route_t *)entry;
    return msbs_route_write_one(w, &route->data, (int32_t)index, (int32_t)index);
}

sf_result_t msbs_route_param_write(sf_binary_writer_t *w, const sf_msbs_t *msbs) {
    if (!w || !msbs || msbs->route_count < 0) return SF_ERR_INVALID_ARG;
    return msb_entry_list_write(w, 35, "ROUTE_PARAM_ST", "MsbsNextList3", msbs->routes,
                                (size_t)msbs->route_count, sizeof(*msbs->routes),
                                msbs_route_write_entry, NULL);
}
