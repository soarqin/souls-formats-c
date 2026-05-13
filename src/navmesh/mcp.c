/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — MCP navigation volumes (DeS/DS1).
 *
 * Mirrors:
 *   SoulsFormats/Formats/MCP.cs
 */

#include "souls_formats/sf_mcp.h"

#include "internal/sf_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sf_mcp_room {
    uint32_t  map_id;
    int32_t   local_index;
    sf_vec3_t bbox_min;
    sf_vec3_t bbox_max;
    int32_t  *connected_indices;
    size_t    connected_count;
    size_t    connected_capacity;
};

struct sf_mcp {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    int32_t               unk04;
    sf_mcp_room_t        *rooms;
    size_t                room_count;
    size_t                room_capacity;
};

static void mcp_room_release(sf_mcp_room_t *room, const sf_allocator_t *alloc) {
    if (!room) return;
    sf_xfree(alloc, room->connected_indices);
    memset(room, 0, sizeof(*room));
}

void sf_mcp_destroy(sf_mcp_t *mcp) {
    if (!mcp) return;
    const sf_allocator_t *alloc = mcp->alloc;
    for (size_t i = 0; i < mcp->room_count; i++) mcp_room_release(&mcp->rooms[i], alloc);
    sf_xfree(alloc, mcp->rooms);
    sf_xfree(alloc, mcp);
}

sf_result_t sf_mcp_create_empty(sf_mcp_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_mcp_t *mcp = (sf_mcp_t *)sf_xalloc(alloc, sizeof(*mcp));
    if (!mcp) return SF_ERR_OOM;
    memset(mcp, 0, sizeof(*mcp));
    mcp->alloc = alloc;
    *out = mcp;
    return SF_OK;
}

static sf_result_t mcp_reserve_rooms(sf_mcp_t *mcp, size_t needed) {
    if (needed <= mcp->room_capacity) return SF_OK;
    size_t cap = mcp->room_capacity ? mcp->room_capacity * 2 : 8;
    if (cap < needed) cap = needed;
    size_t old_bytes = mcp->room_capacity * sizeof(sf_mcp_room_t);
    size_t new_bytes = cap * sizeof(sf_mcp_room_t);
    sf_mcp_room_t *rooms =
        (sf_mcp_room_t *)sf_xrealloc(mcp->alloc, mcp->rooms, old_bytes, new_bytes);
    if (!rooms) return SF_ERR_OOM;
    mcp->rooms = rooms;
    mcp->room_capacity = cap;
    return SF_OK;
}

sf_result_t sf_mcp_append_room(sf_mcp_t *mcp, sf_mcp_room_t **out_room) {
    SF_CHECK_ARG(mcp != NULL);
    sf_result_t r = mcp_reserve_rooms(mcp, mcp->room_count + 1);
    if (r != SF_OK) return r;
    sf_mcp_room_t *room = &mcp->rooms[mcp->room_count++];
    memset(room, 0, sizeof(*room));
    if (out_room) *out_room = room;
    return SF_OK;
}

bool    sf_mcp_big_endian(const sf_mcp_t *mcp) { return mcp ? mcp->big_endian : false; }
void    sf_mcp_set_big_endian(sf_mcp_t *mcp, bool be) { if (mcp) mcp->big_endian = be; }
int32_t sf_mcp_unk04(const sf_mcp_t *mcp) { return mcp ? mcp->unk04 : 0; }
void    sf_mcp_set_unk04(sf_mcp_t *mcp, int32_t v) { if (mcp) mcp->unk04 = v; }

size_t sf_mcp_room_count(const sf_mcp_t *mcp) { return mcp ? mcp->room_count : 0; }

const sf_mcp_room_t *sf_mcp_room(const sf_mcp_t *mcp, size_t i) {
    if (!mcp || i >= mcp->room_count) return NULL;
    return &mcp->rooms[i];
}
sf_mcp_room_t *sf_mcp_room_mut(sf_mcp_t *mcp, size_t i) {
    if (!mcp || i >= mcp->room_count) return NULL;
    return &mcp->rooms[i];
}

uint32_t sf_mcp_room_map_id     (const sf_mcp_room_t *r) { return r ? r->map_id : 0; }
int32_t  sf_mcp_room_local_index(const sf_mcp_room_t *r) { return r ? r->local_index : 0; }
sf_vec3_t sf_mcp_room_bbox_min(const sf_mcp_room_t *r) {
    sf_vec3_t z = {0};
    return r ? r->bbox_min : z;
}
sf_vec3_t sf_mcp_room_bbox_max(const sf_mcp_room_t *r) {
    sf_vec3_t z = {0};
    return r ? r->bbox_max : z;
}
size_t  sf_mcp_room_connected_count(const sf_mcp_room_t *r) {
    return r ? r->connected_count : 0;
}
int32_t sf_mcp_room_connected_index(const sf_mcp_room_t *r, size_t i) {
    if (!r || i >= r->connected_count) return 0;
    return r->connected_indices[i];
}

void sf_mcp_room_set_map_id     (sf_mcp_room_t *r, uint32_t v) { if (r) r->map_id = v; }
void sf_mcp_room_set_local_index(sf_mcp_room_t *r, int32_t  v) { if (r) r->local_index = v; }
void sf_mcp_room_set_bbox_min   (sf_mcp_room_t *r, sf_vec3_t v) { if (r) r->bbox_min = v; }
void sf_mcp_room_set_bbox_max   (sf_mcp_room_t *r, sf_vec3_t v) { if (r) r->bbox_max = v; }

sf_result_t sf_mcp_room_append_connected_index(sf_mcp_room_t *r, int32_t idx) {
    SF_CHECK_ARG(r != NULL);
    if (r->connected_count + 1 > r->connected_capacity) {
        size_t cap = r->connected_capacity ? r->connected_capacity * 2 : 4;
        if (cap < r->connected_count + 1) cap = r->connected_count + 1;
        size_t old_bytes = r->connected_capacity * sizeof(int32_t);
        size_t new_bytes = cap * sizeof(int32_t);
        int32_t *arr = (int32_t *)sf_xrealloc(NULL, r->connected_indices, old_bytes, new_bytes);
        if (!arr) return SF_ERR_OOM;
        r->connected_indices = arr;
        r->connected_capacity = cap;
    }
    r->connected_indices[r->connected_count++] = idx;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t mcp_read_room(sf_binary_reader_t *br, sf_mcp_room_t *room,
                                 const sf_allocator_t *alloc) {
    int32_t index_count = 0;
    int32_t indices_offset = 0;
    sf_result_t r = sf_binary_reader_read_u32(br, &room->map_id);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &room->local_index);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &index_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &indices_offset);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &room->bbox_min);
    if (r == SF_OK) r = sf_binary_reader_read_vec3(br, &room->bbox_max);
    if (r != SF_OK) return r;
    if (index_count < 0 || indices_offset < 0) return SF_ERR_OUT_OF_RANGE;

    if (index_count > 0) {
        int32_t *arr = (int32_t *)sf_xalloc(alloc, (size_t)index_count * sizeof(int32_t));
        if (!arr) return SF_ERR_OOM;
        r = sf_binary_reader_step_in(br, indices_offset);
        if (r == SF_OK) r = sf_binary_reader_read_i32s(br, (size_t)index_count, arr);
        if (r == SF_OK) r = sf_binary_reader_step_out(br);
        if (r != SF_OK) { sf_xfree(alloc, arr); return r; }
        room->connected_indices = arr;
        room->connected_count = (size_t)index_count;
        room->connected_capacity = (size_t)index_count;
    }
    return SF_OK;
}

sf_result_t sf_mcp_read_from_memory(sf_mcp_t **out, const void *data,
                                    size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(data != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;

    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, true, alloc);
    if (r != SF_OK) {
        sf_istream_close(stream);
        return r;
    }

    sf_mcp_t *mcp = NULL;
    r = sf_mcp_create_empty(&mcp, alloc);
    if (r != SF_OK) goto cleanup;

    const int32_t magic_opts[2] = { 2, 0x2000000 };
    int32_t magic = 0;
    r = sf_binary_reader_assert_i32(br, 2, magic_opts, &magic);
    if (r != SF_OK) goto cleanup;
    mcp->big_endian = (magic == 2);
    sf_binary_reader_set_big_endian(br, mcp->big_endian);

    int32_t room_count = 0;
    int32_t rooms_offset = 0;
    r = sf_binary_reader_read_i32(br, &mcp->unk04);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &room_count);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &rooms_offset);
    if (r != SF_OK) goto cleanup;
    if (room_count < 0 || rooms_offset < 0) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }

    r = mcp_reserve_rooms(mcp, (size_t)room_count);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_reader_step_in(br, rooms_offset);
    if (r != SF_OK) goto cleanup;

    for (int32_t i = 0; i < room_count; i++) {
        sf_mcp_room_t *room = NULL;
        r = sf_mcp_append_room(mcp, &room);
        if (r != SF_OK) break;
        r = mcp_read_room(br, room, alloc);
        if (r != SF_OK) break;
    }
    sf_binary_reader_step_out(br);
    if (r != SF_OK) goto cleanup;

    *out = mcp;
    mcp = NULL;

cleanup:
    if (mcp) sf_mcp_destroy(mcp);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    return r;
}

/*===========================================================================
 * Write
 *===========================================================================*/

sf_result_t sf_mcp_write_to_memory(const sf_mcp_t *mcp,
                                   void **out_data, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(mcp != NULL);
    SF_CHECK_ARG(out_data != NULL);
    SF_CHECK_ARG(out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    if (mcp->room_count > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, mcp->big_endian, alloc);
    if (r != SF_OK) {
        sf_ostream_close(stream);
        return r;
    }

    int32_t *indices_offsets = NULL;
    if (mcp->room_count > 0) {
        indices_offsets = (int32_t *)sf_xalloc(alloc, mcp->room_count * sizeof(int32_t));
        if (!indices_offsets) { r = SF_ERR_OOM; goto cleanup; }
        for (size_t i = 0; i < mcp->room_count; i++) indices_offsets[i] = 0;
    }

    r = sf_binary_writer_write_i32(bw, 2);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, mcp->unk04);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)mcp->room_count);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "RoomsOffset");
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < mcp->room_count; i++) {
        const sf_mcp_room_t *room = &mcp->rooms[i];
        int64_t pos = sf_binary_writer_position(bw);
        if (pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        indices_offsets[i] = (int32_t)pos;
        if (room->connected_count > (size_t)INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        r = sf_binary_writer_write_i32s(bw, room->connected_count, room->connected_indices);
        if (r != SF_OK) goto cleanup;
    }

    int64_t rooms_pos = sf_binary_writer_position(bw);
    if (rooms_pos > INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
    r = sf_binary_writer_fill_i32(bw, "RoomsOffset", (int32_t)rooms_pos);
    if (r != SF_OK) goto cleanup;

    for (size_t i = 0; i < mcp->room_count; i++) {
        const sf_mcp_room_t *room = &mcp->rooms[i];
        if (room->connected_count > (size_t)INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; goto cleanup; }
        r = sf_binary_writer_write_u32(bw, room->map_id);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, room->local_index);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, (int32_t)room->connected_count);
        if (r == SF_OK) r = sf_binary_writer_write_i32(bw, indices_offsets[i]);
        if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, room->bbox_min);
        if (r == SF_OK) r = sf_binary_writer_write_vec3(bw, room->bbox_max);
        if (r != SF_OK) goto cleanup;
    }

    uint8_t *bytes = NULL;
    size_t   bytes_len = 0;
    r = sf_binary_writer_finish_bytes(bw, &bytes, &bytes_len);
    bw = NULL;
    if (r != SF_OK) goto cleanup;
    *out_data = bytes;
    *out_size = bytes_len;

cleanup:
    sf_xfree(alloc, indices_offsets);
    if (bw) sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
