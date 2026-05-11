/* SPDX-License-Identifier: GPL-3.0-or-later
 * souls-formats-c — Armored Core VI MSBVI RouteParam.
 * Mirrors SoulsFormats/Formats/MSB/MSBVI/RouteParam.cs
 */

#include "msbvi_internal.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <string.h>

void msbvi_route_param_free(sf_msbvi_route_t *routes, int32_t count, const sf_allocator_t *a) {
    if (!routes || count <= 0) return;
    for (int32_t i = 0; i < count; i++) { sf_xfree(a, routes[i].data.name); memset(&routes[i], 0, sizeof(routes[i])); }
}

static sf_result_t msbvi_route_read_one(sf_binary_reader_t *r, int64_t entry_offset,
                                        msbvi_route_t *out, const sf_allocator_t *a) {
    memset(out, 0, sizeof(*out)); out->alloc = a;
    sf_result_t rc = sf_istream_seek(sf_binary_reader_stream(r), entry_offset); if (rc != SF_OK) return rc;
    int64_t name_offset = 0; int32_t value = 0;
    rc = sf_binary_reader_read_i64(r, &name_offset); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk08); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &out->unk0c); if (rc != SF_OK) return rc;
    rc = sf_binary_reader_read_i32(r, &value); if (rc != SF_OK) return rc;
    if (value != -1 || name_offset == 0) return SF_ERR_BAD_MAGIC;
    for (int i = 0; i < 27; i++) { rc = sf_binary_reader_read_i32(r, &value); if (rc != SF_OK) return rc; if (value != 0) return SF_ERR_BAD_MAGIC; }
    return sf_binary_reader_get_utf16(r, entry_offset + name_offset, &out->name, NULL);
}

sf_result_t msbvi_route_param_read(sf_binary_reader_t *r, int32_t count, sf_msbvi_t *out,
                                   const sf_allocator_t *a) {
    if (!r || !out) return SF_ERR_INVALID_ARG;
    if (count < 0) return SF_ERR_OUT_OF_RANGE;
    out->route_count = count;
    out->routes = NULL;
    if (count == 0) return SF_OK;
    out->routes = (sf_msbvi_route_t *)sf_xalloc(a, (size_t)count * sizeof(*out->routes));
    if (!out->routes) return SF_ERR_OOM;
    memset(out->routes, 0, (size_t)count * sizeof(*out->routes));
    int64_t *offsets = (int64_t *)sf_xalloc(a, (size_t)count * sizeof(*offsets));
    if (!offsets) return SF_ERR_OOM;
    sf_result_t rc = sf_binary_reader_read_i64s(r, (size_t)count, offsets);
    if (rc == SF_OK) for (int32_t i = 0; i < count; i++) { rc = msbvi_route_read_one(r, offsets[i], &out->routes[i].data, a); if (rc != SF_OK) break; }
    sf_xfree(a, offsets);
    return rc;
}

static sf_result_t msbvi_route_write_one(sf_binary_writer_t *w, const msbvi_route_t *route) {
    int64_t start = sf_binary_writer_position(w); char name_res[32]; snprintf(name_res, sizeof name_res, "MsbviRouteName%lld", (long long)start);
    sf_result_t rc = sf_binary_writer_reserve_i64(w, name_res); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, route->unk08); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, route->unk0c); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, -1); if (rc != SF_OK) return rc;
    for (int i = 0; i < 27; i++) { rc = sf_binary_writer_write_i32(w, 0); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_fill_i64(w, name_res, sf_binary_writer_position(w) - start); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, route->name ? route->name : "", true); if (rc != SF_OK) return rc;
    return sf_binary_writer_pad(w, 8);
}

sf_result_t msbvi_route_param_write(sf_binary_writer_t *w, const sf_msbvi_t *msbvi) {
    if (!w || !msbvi) return SF_ERR_INVALID_ARG;
    sf_result_t rc;
    rc = sf_binary_writer_write_i32(w, 52); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_i32(w, msbvi->route_count + 1); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_reserve_i64(w, "MsbviNameOff3"); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->route_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviRouteOff%d", i); rc = sf_binary_writer_reserve_i64(w, n); if (rc != SF_OK) return rc; }
    rc = sf_binary_writer_reserve_i64(w, "MsbviNextList3"); if (rc != SF_OK) return rc; rc = sf_binary_writer_fill_i64(w, "MsbviNameOff3", sf_binary_writer_position(w)); if (rc != SF_OK) return rc;
    rc = sf_binary_writer_write_utf16(w, "ROUTE_PARAM_ST", true); if (rc != SF_OK) return rc; rc = sf_binary_writer_pad(w, 8); if (rc != SF_OK) return rc;
    for (int32_t i = 0; i < msbvi->route_count; i++) { char n[32]; snprintf(n, sizeof n, "MsbviRouteOff%d", i); rc = sf_binary_writer_fill_i64(w, n, sf_binary_writer_position(w)); if (rc != SF_OK) return rc; rc = msbvi_route_write_one(w, &msbvi->routes[i].data); if (rc != SF_OK) return rc; }
    return SF_OK;
}
