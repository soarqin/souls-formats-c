/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * souls-formats-c — TAE (Time Act Editor) SDT format implementation.
 *
 * Mirrors:
 *   SoulsFormats/Formats/TAE/TAE.cs
 *   SoulsFormats/Formats/TAE/Animation.cs
 *   SoulsFormats/Formats/TAE/Event.cs
 *   SoulsFormats/Formats/TAE/EventGroup.cs
 *
 * Wave 2 (T11-T13) implements the actual read/write logic.
 */

#include "effects/tae_internal.h"
#include "internal/sf_internal.h"
#include "souls_formats/sf_io.h"

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tae_animation_record {
    int64_t id;
    int64_t body_offset;
    int64_t event_headers_offset;
    int64_t event_groups_offset;
    int64_t times_offset;
    int64_t anim_file_offset;
    int32_t event_count;
    int32_t event_group_count;
    int32_t times_count;
} tae_animation_record_t;

typedef struct tae_event_record {
    int64_t header_offset;
    int64_t event_data_offset;
    int64_t parameters_offset;
} tae_event_record_t;

typedef struct tae_time_offset {
    float value;
    int64_t offset;
} tae_time_offset_t;

static sf_result_t tae_i64_to_size(int64_t value, size_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < 0)
        return SF_ERR_OUT_OF_RANGE;
#if SIZE_MAX < INT64_MAX
    if ((uint64_t)value > (uint64_t)SIZE_MAX)
        return SF_ERR_OUT_OF_RANGE;
#endif
    *out = (size_t)value;
    return SF_OK;
}

static sf_result_t tae_size_to_i64(size_t value, int64_t *out) {
    SF_CHECK_ARG(out != NULL);
#if SIZE_MAX > INT64_MAX
    if (value > (size_t)INT64_MAX)
        return SF_ERR_OUT_OF_RANGE;
#endif
    *out = (int64_t)value;
    return SF_OK;
}

static sf_result_t tae_position_to_i32(int64_t value, int32_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < INT32_MIN || value > INT32_MAX)
        return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t tae_mul_size(size_t count, size_t elem_size, size_t *out) {
    SF_CHECK_ARG(out != NULL && elem_size > 0);
    if (count > SIZE_MAX / elem_size)
        return SF_ERR_OUT_OF_RANGE;
    *out = count * elem_size;
    return SF_OK;
}

static sf_result_t tae_alloc_array(const sf_allocator_t *alloc, size_t count, size_t elem_size,
                                   void **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (count == 0)
        return SF_OK;
    size_t bytes = 0;
    sf_result_t r = tae_mul_size(count, elem_size, &bytes);
    if (r != SF_OK)
        return r;
    void *p = sf_xalloc(alloc, bytes);
    if (!p)
        return SF_ERR_OOM;
    memset(p, 0, bytes);
    *out = p;
    return SF_OK;
}

static sf_result_t tae_make_name(char *buf, size_t buf_size, const char *prefix, size_t i,
                                 size_t j) {
    int n = snprintf(buf, buf_size, "%s%zu:%zu", prefix, i, j);
    if (n < 0 || (size_t)n >= buf_size)
        return SF_ERR_INTERNAL;
    return SF_OK;
}

static sf_result_t tae_make_name1(char *buf, size_t buf_size, const char *prefix, size_t i) {
    int n = snprintf(buf, buf_size, "%s%zu", prefix, i);
    if (n < 0 || (size_t)n >= buf_size)
        return SF_ERR_INTERNAL;
    return SF_OK;
}

static sf_result_t tae_read_header(sf_binary_reader_t *br, sf_tae_t *t,
                                   tae_animation_record_t **out_records) {
    SF_CHECK_ARG(br != NULL && t != NULL && out_records != NULL);
    *out_records = NULL;

    sf_result_t r = sf_binary_reader_assert_ascii(br, "TAE ");
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_u8_one(br, 0);
    if (r != SF_OK)
        return r;
    sf_binary_reader_set_big_endian(br, false);
    r = sf_binary_reader_assert_u8_one(br, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_u8_one(br, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_u8_one(br, 0xFF);
    if (r != SF_OK)
        return r;
    sf_binary_reader_set_varint_long(br, true);

    int32_t version = 0;
    r = sf_binary_reader_read_i32(br, &version);
    if (r != SF_OK)
        return r;
    if (version != 0x1000D)
        return SF_ERR_UNSUPPORTED_VERSION;
    t->format = SF_TAE_FORMAT_SDT;

    int32_t file_size = 0;
    r = sf_binary_reader_read_i32(br, &file_size);
    if (r != SF_OK)
        return r;
    (void)file_size;
    r = sf_binary_reader_assert_varint_one(br, 0x40);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0x50);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0x80);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_varint(br, &t->event_bank);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0);
    if (r != SF_OK)
        return r;

    r = sf_binary_reader_read_bytes(br, t->flags, sizeof(t->flags));
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_u8_one(br, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_u8_one(br, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_pattern(br, 6, 0);
    if (r != SF_OK)
        return r;

    r = sf_binary_reader_read_i32(br, &t->id);
    if (r != SF_OK)
        return r;
    int32_t anim_count_i32 = 0;
    r = sf_binary_reader_read_i32(br, &anim_count_i32);
    if (r != SF_OK)
        return r;
    if (anim_count_i32 < 0)
        return SF_ERR_OUT_OF_RANGE;
    t->animation_count = (size_t)anim_count_i32;

    int64_t anims_offset = 0;
    int64_t anim_groups_offset = 0;
    int64_t ignored = 0;
    r = sf_binary_reader_read_varint(br, &anims_offset);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_varint(br, &anim_groups_offset);
    if (r != SF_OK)
        return r;
    (void)anim_groups_offset;
    r = sf_binary_reader_assert_varint_one(br, 0xA0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_varint(br, &ignored);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_varint(br, &ignored);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0x90);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_i32_one(br, t->id);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_i32_one(br, t->id);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0x50);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_i64_one(br, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0xB0);
    if (r != SF_OK)
        return r;

    int64_t skeleton_name_offset = 0;
    int64_t sib_name_offset = 0;
    r = sf_binary_reader_read_varint(br, &skeleton_name_offset);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_varint(br, &sib_name_offset);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_assert_varint_one(br, 0);
    if (r != SF_OK)
        return r;

    if (skeleton_name_offset != 0) {
        r = sf_binary_reader_get_utf16(br, skeleton_name_offset, &t->skeleton_name, NULL);
        if (r != SF_OK)
            return r;
    }
    if (sib_name_offset != 0) {
        r = sf_binary_reader_get_utf16(br, sib_name_offset, &t->sib_name, NULL);
        if (r != SF_OK)
            return r;
    }

    if (t->animation_count == 0)
        return SF_OK;
    tae_animation_record_t *records = NULL;
    r = tae_alloc_array(t->alloc, t->animation_count, sizeof(*records), (void **)&records);
    if (r != SF_OK)
        return r;

    r = sf_binary_reader_step_in(br, anims_offset);
    if (r != SF_OK) {
        sf_xfree(t->alloc, records);
        return r;
    }
    for (size_t i = 0; i < t->animation_count; i++) {
        r = sf_binary_reader_read_varint(br, &records[i].id);
        if (r == SF_OK)
            r = sf_binary_reader_read_varint(br, &records[i].body_offset);
        if (r != SF_OK)
            break;
    }
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK)
            r = r2;
    }
    if (r != SF_OK) {
        sf_xfree(t->alloc, records);
        return r;
    }

    for (size_t i = 0; i < t->animation_count; i++) {
        r = sf_binary_reader_step_in(br, records[i].body_offset);
        if (r != SF_OK)
            break;
        r = sf_binary_reader_read_varint(br, &records[i].event_headers_offset);
        if (r == SF_OK)
            r = sf_binary_reader_read_varint(br, &records[i].event_groups_offset);
        if (r == SF_OK)
            r = sf_binary_reader_read_varint(br, &records[i].times_offset);
        if (r == SF_OK)
            r = sf_binary_reader_read_varint(br, &records[i].anim_file_offset);
        if (r == SF_OK)
            r = sf_binary_reader_read_i32(br, &records[i].event_count);
        if (r == SF_OK)
            r = sf_binary_reader_read_i32(br, &records[i].event_group_count);
        if (r == SF_OK)
            r = sf_binary_reader_read_i32(br, &records[i].times_count);
        if (r == SF_OK)
            r = sf_binary_reader_assert_i32_one(br, 0);
        {
            sf_result_t r2 = sf_binary_reader_step_out(br);
            if (r == SF_OK)
                r = r2;
        }
        if (r != SF_OK)
            break;
        if (records[i].event_count < 0 || records[i].event_group_count < 0 ||
            records[i].times_count < 0) {
            r = SF_ERR_OUT_OF_RANGE;
            break;
        }
    }
    if (r != SF_OK) {
        sf_xfree(t->alloc, records);
        return r;
    }

    *out_records = records;
    return SF_OK;
}

static sf_result_t tae_read_mini_header(sf_binary_reader_t *br, sf_tae_animation_t *anim,
                                        int64_t anim_file_offset, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && anim != NULL);
    sf_result_t r = sf_binary_reader_step_in(br, anim_file_offset);
    if (r != SF_OK)
        return r;

    uint32_t type = 0;
    const uint32_t types[2] = {SF_TAE_MINI_HEADER_STANDARD, SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM};
    r = sf_binary_reader_read_enum_32(br, 2, types, &type);
    if (r == SF_OK)
        r = sf_binary_reader_assert_i32_one(br, 0);

    int64_t offset_offset_pos = sf_binary_reader_position(br);
    int32_t actual_offset_offset = 0;
    int64_t possible_offsets[2] = {offset_offset_pos + 8, 0};
    if (r == SF_OK)
        r = sf_binary_reader_assert_i32(br, 2, (const int32_t[]){(int32_t)possible_offsets[0], 0},
                                        &actual_offset_offset);
    if (r == SF_OK)
        r = sf_binary_reader_read_i32(br, &(int32_t){0});

    anim->mini_header.type = (sf_tae_anim_mini_header_type_t)type;
    anim->mini_header.is_null_header = (actual_offset_offset == 0);
    if (r == SF_OK && !anim->mini_header.is_null_header) {
        r = sf_binary_reader_step_in(br, actual_offset_offset);
        if (r == SF_OK) {
            int64_t anim_file_name_offset = 0;
            r = sf_binary_reader_read_varint(br, &anim_file_name_offset);
            if (r == SF_OK) {
                if (type == SF_TAE_MINI_HEADER_STANDARD) {
                    uint8_t v = 0;
                    r = sf_binary_reader_read_u8(br, &v);
                    anim->mini_header.payload.standard.is_loop_by_default = (v != 0);
                    if (r == SF_OK)
                        r = sf_binary_reader_read_u8(br, &v);
                    anim->mini_header.payload.standard.imports_hkx = (v != 0);
                    if (r == SF_OK)
                        r = sf_binary_reader_read_u8(br, &v);
                    anim->mini_header.payload.standard.allow_delay_load = (v != 0);
                    if (r == SF_OK)
                        r = sf_binary_reader_read_u8(br, &v);
                    if (r == SF_OK)
                        r = sf_binary_reader_read_i32(
                            br, &anim->mini_header.payload.standard.import_hkx_source_anim_id);
                } else {
                    r = sf_binary_reader_read_i32(
                        br, &anim->mini_header.payload.import_other.import_from_anim_id);
                    if (r == SF_OK)
                        r = sf_binary_reader_read_i32(
                            br, &anim->mini_header.payload.import_other.unknown);
                }
            }
            if (r == SF_OK)
                r = sf_binary_reader_assert_varint_one(br, 0);
            if (r == SF_OK)
                r = sf_binary_reader_assert_varint_one(br, 0);
            if (r == SF_OK && anim_file_name_offset > 0 &&
                anim_file_name_offset < sf_binary_reader_length(br)) {
                r = sf_binary_reader_get_utf16(br, anim_file_name_offset, &anim->anim_file_name,
                                               NULL);
            }
            if (r == SF_OK && !anim->anim_file_name) {
                anim->anim_file_name = sf_strdup(alloc, "");
                if (!anim->anim_file_name)
                    r = SF_ERR_OOM;
            }
            {
                sf_result_t r2 = sf_binary_reader_step_out(br);
                if (r == SF_OK)
                    r = r2;
            }
        }
    }

    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK)
            r = r2;
    }
    return r;
}

static sf_result_t tae_read_event(sf_binary_reader_t *br, sf_tae_event_t *ev,
                                  tae_event_record_t *rec) {
    SF_CHECK_ARG(br != NULL && ev != NULL && rec != NULL);
    rec->header_offset = sf_binary_reader_position(br);
    int64_t start_time_offset = 0;
    int64_t end_time_offset = 0;
    sf_result_t r = sf_binary_reader_read_varint(br, &start_time_offset);
    if (r == SF_OK)
        r = sf_binary_reader_read_varint(br, &end_time_offset);
    if (r == SF_OK)
        r = sf_binary_reader_read_varint(br, &rec->event_data_offset);
    if (r == SF_OK)
        r = sf_binary_reader_get_f32(br, start_time_offset, &ev->start_time);
    if (r == SF_OK)
        r = sf_binary_reader_get_f32(br, end_time_offset, &ev->end_time);
    if (r != SF_OK)
        return r;

    r = sf_binary_reader_step_in(br, rec->event_data_offset);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_i32(br, &ev->type);
    if (r == SF_OK)
        r = sf_binary_reader_read_i32(br, &ev->unk04);
    int64_t expected_param_offset = sf_binary_reader_position(br) + 8;
    int64_t param_options[2] = {expected_param_offset, 0};
    if (r == SF_OK)
        r = sf_binary_reader_assert_varint(br, 2, param_options, NULL);
    rec->parameters_offset = expected_param_offset;
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK)
            r = r2;
    }
    return r;
}

static sf_result_t tae_read_parameter_bytes(sf_binary_reader_t *br, sf_tae_event_t *ev,
                                            int64_t start, int64_t end,
                                            const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && ev != NULL);
    if (end < start)
        return SF_ERR_OUT_OF_RANGE;
    size_t size = 0;
    sf_result_t r = tae_i64_to_size(end - start, &size);
    if (r != SF_OK)
        return r;
    ev->parameters_size = size;
    if (size == 0)
        return SF_OK;
    ev->parameters = (uint8_t *)sf_xalloc(alloc, size);
    if (!ev->parameters)
        return SF_ERR_OOM;
    r = sf_binary_reader_get_bytes(br, start, ev->parameters, size);
    if (r != SF_OK) {
        sf_xfree(alloc, ev->parameters);
        ev->parameters = NULL;
        ev->parameters_size = 0;
    }
    return r;
}

static sf_result_t tae_read_event_group(sf_binary_reader_t *br, sf_tae_event_group_t *group,
                                        const tae_event_record_t *event_records, size_t event_count,
                                        const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && group != NULL);
    int64_t entry_count = 0;
    int64_t values_offset = 0;
    int64_t type_offset = 0;
    sf_result_t r = sf_binary_reader_read_varint(br, &entry_count);
    if (r == SF_OK)
        r = sf_binary_reader_read_varint(br, &values_offset);
    if (r == SF_OK)
        r = sf_binary_reader_read_varint(br, &type_offset);
    if (r == SF_OK)
        r = sf_binary_reader_assert_varint_one(br, 0);
    if (r != SF_OK)
        return r;
    r = tae_i64_to_size(entry_count, &group->member_count);
    if (r != SF_OK)
        return r;

    r = sf_binary_reader_step_in(br, type_offset);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_read_i32(br, &group->group_type);
    if (r == SF_OK)
        r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK)
        r = sf_binary_reader_assert_varint_one(br, 0);
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK)
            r = r2;
    }
    if (r != SF_OK)
        return r;

    r = tae_alloc_array(alloc, group->member_count, sizeof(*group->members),
                        (void **)&group->members);
    if (r != SF_OK)
        return r;
    r = sf_binary_reader_step_in(br, values_offset);
    if (r != SF_OK)
        return r;
    for (size_t i = 0; i < group->member_count; i++) {
        int32_t header_offset = 0;
        r = sf_binary_reader_read_i32(br, &header_offset);
        if (r != SF_OK)
            break;
        group->members[i] = -1;
        for (size_t j = 0; j < event_count; j++) {
            if (event_records[j].header_offset == header_offset) {
                group->members[i] = (int32_t)j;
                break;
            }
        }
        if (group->members[i] < 0) {
            r = SF_ERR_NOT_FOUND;
            break;
        }
    }
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK)
            r = r2;
    }
    return r;
}

static sf_result_t tae_read_animation(sf_binary_reader_t *br, const tae_animation_record_t *records,
                                      size_t index, size_t count, sf_tae_animation_t **out,
                                      const sf_allocator_t *alloc) {
    SF_CHECK_ARG(br != NULL && records != NULL && out != NULL);
    *out = NULL;
    const tae_animation_record_t *rec = &records[index];
    sf_tae_animation_t *anim = (sf_tae_animation_t *)sf_xalloc(alloc, sizeof(*anim));
    if (!anim)
        return SF_ERR_OOM;
    memset(anim, 0, sizeof(*anim));
    anim->id = rec->id;

    sf_result_t r = tae_read_mini_header(br, anim, rec->anim_file_offset, alloc);
    if (r != SF_OK)
        goto fail;

    r = tae_i64_to_size(rec->event_count, &anim->event_count);
    if (r != SF_OK)
        goto fail;
    r = tae_alloc_array(alloc, anim->event_count, sizeof(*anim->events), (void **)&anim->events);
    if (r != SF_OK)
        goto fail;

    tae_event_record_t *event_records = NULL;
    r = tae_alloc_array(alloc, anim->event_count, sizeof(*event_records), (void **)&event_records);
    if (r != SF_OK)
        goto fail;

    if (anim->event_count > 0) {
        r = sf_binary_reader_step_in(br, rec->event_headers_offset);
        if (r != SF_OK)
            goto event_fail;
        for (size_t i = 0; i < anim->event_count; i++) {
            anim->events[i] = (sf_tae_event_t *)sf_xalloc(alloc, sizeof(*anim->events[i]));
            if (!anim->events[i]) {
                r = SF_ERR_OOM;
                break;
            }
            memset(anim->events[i], 0, sizeof(*anim->events[i]));
            r = tae_read_event(br, anim->events[i], &event_records[i]);
            if (r != SF_OK)
                break;
        }
        {
            sf_result_t r2 = sf_binary_reader_step_out(br);
            if (r == SF_OK)
                r = r2;
        }
        if (r != SF_OK)
            goto event_fail;

        for (size_t i = 0; i < anim->event_count; i++) {
            int64_t end = 0;
            if (i + 1 < anim->event_count) {
                end = event_records[i + 1].event_data_offset;
            } else if (rec->event_groups_offset != 0) {
                end = rec->event_groups_offset;
            } else if (index + 1 < count) {
                end = records[index + 1].anim_file_offset;
            } else {
                end = sf_binary_reader_length(br);
            }
            r = tae_read_parameter_bytes(br, anim->events[i], event_records[i].parameters_offset,
                                         end, alloc);
            if (r != SF_OK)
                goto event_fail;
        }
    }

    r = tae_i64_to_size(rec->event_group_count, &anim->event_group_count);
    if (r != SF_OK)
        goto event_fail;
    r = tae_alloc_array(alloc, anim->event_group_count, sizeof(*anim->event_groups),
                        (void **)&anim->event_groups);
    if (r != SF_OK)
        goto event_fail;
    if (anim->event_group_count > 0) {
        r = sf_binary_reader_step_in(br, rec->event_groups_offset);
        if (r != SF_OK)
            goto event_fail;
        for (size_t i = 0; i < anim->event_group_count; i++) {
            anim->event_groups[i] =
                (sf_tae_event_group_t *)sf_xalloc(alloc, sizeof(*anim->event_groups[i]));
            if (!anim->event_groups[i]) {
                r = SF_ERR_OOM;
                break;
            }
            memset(anim->event_groups[i], 0, sizeof(*anim->event_groups[i]));
            r = tae_read_event_group(br, anim->event_groups[i], event_records, anim->event_count,
                                     alloc);
            if (r != SF_OK)
                break;
        }
        {
            sf_result_t r2 = sf_binary_reader_step_out(br);
            if (r == SF_OK)
                r = r2;
        }
    }

event_fail:
    sf_xfree(alloc, event_records);
    if (r != SF_OK)
        goto fail;
    *out = anim;
    return SF_OK;

fail:
    if (anim) {
        sf_xfree(alloc, anim->anim_file_name);
        for (size_t i = 0; i < anim->event_count; i++) {
            if (anim->events && anim->events[i]) {
                sf_xfree(alloc, anim->events[i]->parameters);
                sf_xfree(alloc, anim->events[i]);
            }
        }
        for (size_t i = 0; i < anim->event_group_count; i++) {
            if (anim->event_groups && anim->event_groups[i]) {
                sf_xfree(alloc, anim->event_groups[i]->members);
                sf_xfree(alloc, anim->event_groups[i]);
            }
        }
        sf_xfree(alloc, anim->events);
        sf_xfree(alloc, anim->event_groups);
        sf_xfree(alloc, anim);
    }
    return r;
}

SF_API sf_result_t sf_tae_read_from_memory(sf_tae_t **out, const void *bytes, size_t size,
                                           const sf_allocator_t *a) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    a = sf_alloc_or_default(a);

    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, bytes, size, a);
    if (r != SF_OK)
        return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, a);
    if (r != SF_OK) {
        sf_istream_close(stream);
        return r;
    }

    sf_tae_t *t = (sf_tae_t *)sf_xalloc(a, sizeof(*t));
    if (!t) {
        sf_binary_reader_destroy(br);
        sf_istream_close(stream);
        return SF_ERR_OOM;
    }
    memset(t, 0, sizeof(*t));
    t->alloc = a;

    tae_animation_record_t *records = NULL;
    r = tae_read_header(br, t, &records);
    if (r != SF_OK)
        goto fail;
    r = tae_alloc_array(a, t->animation_count, sizeof(*t->animations), (void **)&t->animations);
    if (r != SF_OK)
        goto fail;
    for (size_t i = 0; i < t->animation_count; i++) {
        r = tae_read_animation(br, records, i, t->animation_count, &t->animations[i], a);
        if (r != SF_OK)
            goto fail;
    }

    sf_xfree(a, records);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    *out = t;
    return SF_OK;

fail:
    sf_xfree(a, records);
    sf_binary_reader_destroy(br);
    sf_istream_close(stream);
    sf_tae_destroy(t);
    return r;
}

static sf_result_t tae_write_header(sf_binary_writer_t *bw, const sf_tae_t *t) {
    int64_t anim_count = 0;
    sf_result_t r = tae_size_to_i64(t->animation_count, &anim_count);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_bytes(bw, "TAE ", 4);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_bool(bw, false);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_u8(bw, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_u8(bw, 0);
    if (r != SF_OK)
        return r;
    sf_binary_writer_set_varint_long(bw, true);
    r = sf_binary_writer_write_u8(bw, 0xFF);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_i32(bw, 0x1000D);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, "FileSize"), return r);
    r = sf_binary_writer_write_varint(bw, 0x40);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0x50);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0x80);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, t->event_bank);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_bytes(bw, t->flags, sizeof(t->flags));
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_u8(bw, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_u8(bw, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_pattern(bw, 6, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_i32(bw, t->id);
    if (r != SF_OK)
        return r;
    if (anim_count > INT32_MAX)
        return SF_ERR_OUT_OF_RANGE;
    r = sf_binary_writer_write_i32(bw, (int32_t)anim_count);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "AnimsOffset"), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "AnimGroupsHeaderOffset"), return r);
    r = sf_binary_writer_write_varint(bw, 0xA0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, anim_count);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "FirstAnimOffset"), return r);
    r = sf_binary_writer_write_varint(bw, 1);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0x90);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_i32(bw, t->id);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_i32(bw, t->id);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0x50);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_i64(bw, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0xB0);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "SkeletonName"), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "SibName"), return r);
    r = sf_binary_writer_write_varint(bw, 0);
    if (r != SF_OK)
        return r;
    return sf_binary_writer_write_varint(bw, 0);
}

static sf_result_t tae_write_optional_utf16(sf_binary_writer_t *bw, const char *reserve_name,
                                            const char *value) {
    sf_result_t r;
    if (!value)
        return sf_binary_writer_fill_varint(bw, reserve_name, 0);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, reserve_name, sf_binary_writer_position(bw)), return r);
    if (value[0] == '\0')
        return SF_OK;
    r = sf_binary_writer_write_utf16(bw, value, true);
    if (r != SF_OK)
        return r;
    return sf_binary_writer_pad(bw, 0x10);
}

static sf_result_t tae_write_animation_table(sf_binary_writer_t *bw, const sf_tae_t *t) {
    sf_result_t r;
    if (t->animation_count == 0) {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "AnimsOffset", 0), return r);
    } else {
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "AnimsOffset", sf_binary_writer_position(bw)), return r);
        for (size_t i = 0; i < t->animation_count; i++) {
            char name[64];
            r = tae_make_name1(name, sizeof(name), "AnimationOffset", i);
            if (r != SF_OK)
                return r;
            r = sf_binary_writer_write_varint(bw, t->animations[i]->id);
            if (r != SF_OK)
                return r;
            SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        }
    }

    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "AnimGroupsHeaderOffset", sf_binary_writer_position(bw)), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "TopAnimGroupsCount"), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, "TopAnimGroupsOffset"), return r);
    size_t group_count = 0;
    int64_t group_start = sf_binary_writer_position(bw);
    for (size_t i = 0; i < t->animation_count; i++) {
        r = sf_binary_writer_write_i32(bw, (int32_t)t->animations[i]->id);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, (int32_t)t->animations[i]->id);
        if (r != SF_OK)
            return r;
        char name[64];
        r = tae_make_name1(name, sizeof(name), "TopAnimationOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        group_count++;
    }
    int64_t group_count_i64 = 0;
    r = tae_size_to_i64(group_count, &group_count_i64);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "TopAnimGroupsCount", group_count_i64), return r);
    return sf_binary_writer_fill_varint(bw, "TopAnimGroupsOffset",
                                        group_count == 0 ? 0 : group_start);
}

static sf_result_t tae_write_animation_bodies(sf_binary_writer_t *bw, const sf_tae_t *t) {
    sf_result_t r;
    if (t->animation_count == 0)
        return sf_binary_writer_fill_varint(bw, "FirstAnimOffset", 0);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, "FirstAnimOffset", sf_binary_writer_position(bw)), return r);
    for (size_t i = 0; i < t->animation_count; i++) {
        const sf_tae_animation_t *anim = t->animations[i];
        char name[64];
        r = tae_make_name1(name, sizeof(name), "AnimationOffset", i);
        if (r != SF_OK)
            return r;
        int64_t body_pos = sf_binary_writer_position(bw);
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, body_pos), return r);
        r = tae_make_name1(name, sizeof(name), "TopAnimationOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, body_pos), return r);
        r = tae_make_name1(name, sizeof(name), "EventHeadersOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        r = tae_make_name1(name, sizeof(name), "EventGroupHeadersOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        r = tae_make_name1(name, sizeof(name), "TimesOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        r = tae_make_name1(name, sizeof(name), "AnimFileOffset", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        if (anim->event_count > INT32_MAX || anim->event_group_count > INT32_MAX)
            return SF_ERR_OUT_OF_RANGE;
        r = sf_binary_writer_write_i32(bw, (int32_t)anim->event_count);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, (int32_t)anim->event_group_count);
        if (r != SF_OK)
            return r;
        r = tae_make_name1(name, sizeof(name), "TimesCount", i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_i32(bw, name), return r);
        r = sf_binary_writer_write_i32(bw, 0);
        if (r != SF_OK)
            return r;
    }
    return SF_OK;
}

static sf_result_t tae_write_anim_file(sf_binary_writer_t *bw, const sf_tae_animation_t *anim,
                                       size_t i) {
    char name[64];
    sf_result_t r = tae_make_name1(name, sizeof(name), "AnimFileOffset", i);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
    r = sf_binary_writer_write_varint(bw, anim->mini_header.type);
    if (r != SF_OK)
        return r;
    r = tae_make_name1(name, sizeof(name), "AnimFileNameOffsetOffset", i);
    if (r != SF_OK)
        return r;
    if (anim->mini_header.is_null_header) {
        return sf_binary_writer_write_varint(bw, 0);
    }
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
    r = tae_make_name1(name, sizeof(name), "AnimFileNameOffset", i);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    if (anim->mini_header.type == SF_TAE_MINI_HEADER_STANDARD) {
        const sf_tae_anim_mini_header_standard_t *h = &anim->mini_header.payload.standard;
        r = sf_binary_writer_write_bool(bw, h->is_loop_by_default);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_bool(bw, h->imports_hkx);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_bool(bw, h->allow_delay_load);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_u8(bw, 0);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, h->import_hkx_source_anim_id);
        if (r != SF_OK)
            return r;
    } else if (anim->mini_header.type == SF_TAE_MINI_HEADER_IMPORT_OTHER_ANIM) {
        r = sf_binary_writer_write_i32(bw,
                                       anim->mini_header.payload.import_other.import_from_anim_id);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, anim->mini_header.payload.import_other.unknown);
        if (r != SF_OK)
            return r;
    } else {
        return SF_ERR_UNSUPPORTED_VERSION;
    }
    r = sf_binary_writer_write_varint(bw, 0);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_write_varint(bw, 0);
    if (r != SF_OK)
        return r;
    r = tae_make_name1(name, sizeof(name), "AnimFileNameOffset", i);
    if (r != SF_OK)
        return r;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
    if (anim->anim_file_name && anim->anim_file_name[0] != '\0') {
        r = sf_binary_writer_write_utf16(bw, anim->anim_file_name, true);
        if (r != SF_OK)
            return r;
        return sf_binary_writer_pad(bw, 0x10);
    }
    return sf_binary_writer_write_i16(bw, 0);
}

static int tae_float_compare(const void *a, const void *b) {
    const float fa = *(const float *)a;
    const float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static sf_result_t tae_collect_times(const sf_tae_animation_t *anim, const sf_allocator_t *alloc,
                                     float **out_times, size_t *out_count) {
    *out_times = NULL;
    *out_count = 0;
    if (anim->event_count == 0)
        return SF_OK;
    if (anim->event_count > SIZE_MAX / (2 * sizeof(float)))
        return SF_ERR_OUT_OF_RANGE;
    size_t cap = anim->event_count * 2;
    float *times = (float *)sf_xalloc(alloc, cap * sizeof(*times));
    if (!times)
        return SF_ERR_OOM;
    for (size_t i = 0; i < anim->event_count; i++) {
        times[*out_count] = anim->events[i]->start_time;
        (*out_count)++;
        times[*out_count] = anim->events[i]->end_time;
        (*out_count)++;
    }
    qsort(times, *out_count, sizeof(*times), tae_float_compare);
    size_t unique = 0;
    for (size_t i = 0; i < *out_count; i++) {
        if (unique == 0 || times[i] != times[unique - 1])
            times[unique++] = times[i];
    }
    *out_count = unique;
    *out_times = times;
    return SF_OK;
}

static int64_t tae_find_time_offset(const tae_time_offset_t *times, size_t count, float value) {
    for (size_t i = 0; i < count; i++)
        if (times[i].value == value)
            return times[i].offset;
    return -1;
}

static sf_result_t tae_write_times(sf_binary_writer_t *bw, const sf_tae_animation_t *anim,
                                   size_t anim_index, const sf_allocator_t *alloc,
                                   tae_time_offset_t **out_offsets, size_t *out_count) {
    *out_offsets = NULL;
    *out_count = 0;
    float *times = NULL;
    sf_result_t r = tae_collect_times(anim, alloc, &times, out_count);
    if (r != SF_OK)
        return r;
    char name[64];
    r = tae_make_name1(name, sizeof(name), "TimesCount", anim_index);
    if (r != SF_OK)
        goto cleanup;
    if (*out_count > INT32_MAX) {
        r = SF_ERR_OUT_OF_RANGE;
        goto cleanup;
    }
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_i32(bw, name, (int32_t)*out_count), goto cleanup);
    r = tae_make_name1(name, sizeof(name), "TimesOffset", anim_index);
    if (r != SF_OK)
        goto cleanup;
    SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, *out_count == 0 ? 0 : sf_binary_writer_position(bw)), goto cleanup);
    r = tae_alloc_array(alloc, *out_count, sizeof(**out_offsets), (void **)out_offsets);
    if (r != SF_OK)
        goto cleanup;
    for (size_t i = 0; i < *out_count; i++) {
        (*out_offsets)[i].value = times[i];
        (*out_offsets)[i].offset = sf_binary_writer_position(bw);
        r = sf_binary_writer_write_f32(bw, times[i]);
        if (r != SF_OK)
            goto cleanup;
    }
    r = sf_binary_writer_pad(bw, 0x10);
cleanup:
    sf_xfree(alloc, times);
    if (r != SF_OK) {
        sf_xfree(alloc, *out_offsets);
        *out_offsets = NULL;
        *out_count = 0;
    }
    return r;
}

static sf_result_t tae_write_event_headers(sf_binary_writer_t *bw, const sf_tae_animation_t *anim,
                                           size_t anim_index, const tae_time_offset_t *times,
                                           size_t time_count, int64_t *event_header_offsets) {
    char name[64];
    sf_result_t r = tae_make_name1(name, sizeof(name), "EventHeadersOffset", anim_index);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_fill_varint(bw, name,
                                     anim->event_count == 0 ? 0 : sf_binary_writer_position(bw));
    if (r != SF_OK)
        return r;
    for (size_t i = 0; i < anim->event_count; i++) {
        event_header_offsets[i] = sf_binary_writer_position(bw);
        int64_t start_offset = tae_find_time_offset(times, time_count, anim->events[i]->start_time);
        int64_t end_offset = tae_find_time_offset(times, time_count, anim->events[i]->end_time);
        if (start_offset < 0 || end_offset < 0)
            return SF_ERR_INTERNAL;
        r = sf_binary_writer_write_varint(bw, start_offset);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_varint(bw, end_offset);
        if (r != SF_OK)
            return r;
        r = tae_make_name(name, sizeof(name), "EventDataOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
    }
    return SF_OK;
}

static sf_result_t tae_write_event_data(sf_binary_writer_t *bw, const sf_tae_animation_t *anim,
                                        size_t anim_index) {
    for (size_t i = 0; i < anim->event_count; i++) {
        const sf_tae_event_t *ev = anim->events[i];
        char name[64];
        sf_result_t r = tae_make_name(name, sizeof(name), "EventDataOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_i32(bw, ev->type);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, ev->unk04);
        if (r != SF_OK)
            return r;
        bool weird_no_param_offset = (ev->type == 943);
        r = tae_make_name(name, sizeof(name), "EventParamsOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        if (weird_no_param_offset) {
            r = sf_binary_writer_write_varint(bw, 0);
            if (r != SF_OK)
                return r;
        } else {
            SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
            SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        }
        r = sf_binary_writer_write_bytes(bw, ev->parameters, ev->parameters_size);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_pad(bw, 0x10);
        if (r != SF_OK)
            return r;
    }
    return SF_OK;
}

static sf_result_t tae_write_event_group_headers(sf_binary_writer_t *bw,
                                                 const sf_tae_animation_t *anim,
                                                 size_t anim_index) {
    char name[64];
    sf_result_t r = tae_make_name1(name, sizeof(name), "EventGroupHeadersOffset", anim_index);
    if (r != SF_OK)
        return r;
    r = sf_binary_writer_fill_varint(
        bw, name, anim->event_group_count == 0 ? 0 : sf_binary_writer_position(bw));
    if (r != SF_OK)
        return r;
    for (size_t i = 0; i < anim->event_group_count; i++) {
        const sf_tae_event_group_t *g = anim->event_groups[i];
        int64_t member_count = 0;
        r = tae_size_to_i64(g->member_count, &member_count);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_varint(bw, member_count);
        if (r != SF_OK)
            return r;
        r = tae_make_name(name, sizeof(name), "EventGroupValuesOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        r = tae_make_name(name, sizeof(name), "EventGroupTypeOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_reserve_varint(bw, name), return r);
        r = sf_binary_writer_write_varint(bw, 0);
        if (r != SF_OK)
            return r;
    }
    return SF_OK;
}

static sf_result_t tae_write_event_group_data(sf_binary_writer_t *bw,
                                              const sf_tae_animation_t *anim, size_t anim_index,
                                              const int64_t *event_header_offsets) {
    for (size_t i = 0; i < anim->event_group_count; i++) {
        const sf_tae_event_group_t *g = anim->event_groups[i];
        char name[64];
        sf_result_t r = tae_make_name(name, sizeof(name), "EventGroupTypeOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        r = sf_binary_writer_write_i32(bw, g->group_type);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_i32(bw, 0);
        if (r != SF_OK)
            return r;
        r = sf_binary_writer_write_varint(bw, 0);
        if (r != SF_OK)
            return r;
        r = tae_make_name(name, sizeof(name), "EventGroupValuesOffset", anim_index, i);
        if (r != SF_OK)
            return r;
        SF_RESERVE_FILL_PAIR(r, sf_binary_writer_fill_varint(bw, name, sf_binary_writer_position(bw)), return r);
        for (size_t j = 0; j < g->member_count; j++) {
            if (g->members[j] < 0 || (size_t)g->members[j] >= anim->event_count)
                return SF_ERR_OUT_OF_RANGE;
            int32_t offset = 0;
            r = tae_position_to_i32(event_header_offsets[g->members[j]], &offset);
            if (r != SF_OK)
                return r;
            r = sf_binary_writer_write_i32(bw, offset);
            if (r != SF_OK)
                return r;
        }
        r = sf_binary_writer_pad(bw, 0x10);
        if (r != SF_OK)
            return r;
    }
    return SF_OK;
}

static sf_result_t tae_write_all_animation_data(sf_binary_writer_t *bw, const sf_tae_t *t,
                                                const sf_allocator_t *alloc) {
    for (size_t i = 0; i < t->animation_count; i++) {
        const sf_tae_animation_t *anim = t->animations[i];
        sf_result_t r = tae_write_anim_file(bw, anim, i);
        if (r != SF_OK)
            return r;
        tae_time_offset_t *time_offsets = NULL;
        size_t time_count = 0;
        r = tae_write_times(bw, anim, i, alloc, &time_offsets, &time_count);
        if (r != SF_OK)
            return r;
        int64_t *event_header_offsets = NULL;
        r = tae_alloc_array(alloc, anim->event_count, sizeof(*event_header_offsets),
                            (void **)&event_header_offsets);
        if (r == SF_OK)
            r = tae_write_event_headers(bw, anim, i, time_offsets, time_count,
                                        event_header_offsets);
        if (r == SF_OK)
            r = tae_write_event_data(bw, anim, i);
        if (r == SF_OK)
            r = tae_write_event_group_headers(bw, anim, i);
        if (r == SF_OK)
            r = tae_write_event_group_data(bw, anim, i, event_header_offsets);
        sf_xfree(alloc, event_header_offsets);
        sf_xfree(alloc, time_offsets);
        if (r != SF_OK)
            return r;
    }
    return SF_OK;
}

SF_API sf_result_t sf_tae_write_to_memory(const sf_tae_t *t, void **out_bytes, size_t *out_size,
                                          const sf_allocator_t *a) {
    SF_CHECK_ARG(t != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    if (t->format != SF_TAE_FORMAT_SDT)
        return SF_ERR_UNSUPPORTED_VERSION;
    a = sf_alloc_or_default(a);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, a);
    if (r != SF_OK)
        return r;
    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, a);
    if (r != SF_OK) {
        sf_ostream_close(stream);
        return r;
    }

    r = tae_write_header(bw, t);
    if (r == SF_OK)
        r = tae_write_optional_utf16(bw, "SkeletonName", t->skeleton_name);
    if (r == SF_OK)
        r = tae_write_optional_utf16(bw, "SibName", t->sib_name);
    if (r == SF_OK)
        r = tae_write_animation_table(bw, t);
    if (r == SF_OK)
        r = tae_write_animation_bodies(bw, t);
    if (r == SF_OK)
        r = tae_write_all_animation_data(bw, t, a);
    if (r == SF_OK) {
        int32_t file_size = 0;
        r = tae_position_to_i32(sf_binary_writer_position(bw), &file_size);
        if (r == SF_OK)
            r = sf_binary_writer_fill_i32(bw, "FileSize", file_size);
    }
    if (r == SF_OK)
        r = sf_binary_writer_finish_bytes(bw, (uint8_t **)out_bytes, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}

SF_API void sf_tae_destroy(sf_tae_t *t) {
    if (!t)
        return;
    const sf_allocator_t *alloc = t->alloc;
    sf_xfree(alloc, t->skeleton_name);
    sf_xfree(alloc, t->sib_name);
    for (size_t i = 0; i < t->animation_count; i++) {
        sf_tae_animation_t *anim = t->animations ? t->animations[i] : NULL;
        if (!anim)
            continue;
        sf_xfree(alloc, anim->anim_file_name);
        for (size_t j = 0; j < anim->event_count; j++) {
            sf_tae_event_t *ev = anim->events ? anim->events[j] : NULL;
            if (!ev)
                continue;
            sf_xfree(alloc, ev->parameters);
            sf_xfree(alloc, ev);
        }
        for (size_t j = 0; j < anim->event_group_count; j++) {
            sf_tae_event_group_t *group = anim->event_groups ? anim->event_groups[j] : NULL;
            if (!group)
                continue;
            sf_xfree(alloc, group->members);
            sf_xfree(alloc, group);
        }
        sf_xfree(alloc, anim->events);
        sf_xfree(alloc, anim->event_groups);
        sf_xfree(alloc, anim);
    }
    sf_xfree(alloc, t->animations);
    sf_xfree(alloc, t);
}

SF_API sf_tae_format_t sf_tae_format(const sf_tae_t *t) {
    return t ? t->format : SF_TAE_FORMAT_SDT;
}

SF_API int32_t sf_tae_id(const sf_tae_t *t) {
    return t ? t->id : 0;
}

SF_API const char *sf_tae_skeleton_name(const sf_tae_t *t) {
    return t ? t->skeleton_name : NULL;
}

SF_API const char *sf_tae_sib_name(const sf_tae_t *t) {
    return t ? t->sib_name : NULL;
}

SF_API int64_t sf_tae_event_bank(const sf_tae_t *t) {
    return t ? t->event_bank : 0;
}

SF_API size_t sf_tae_animation_count(const sf_tae_t *t) {
    return t ? t->animation_count : 0;
}

SF_API const sf_tae_animation_t *sf_tae_animation(const sf_tae_t *t, size_t i) {
    return (t && i < t->animation_count) ? t->animations[i] : NULL;
}

SF_API int64_t sf_tae_animation_id(const sf_tae_animation_t *a) {
    return a ? a->id : 0;
}

SF_API const sf_tae_anim_mini_header_t *sf_tae_animation_mini_header(const sf_tae_animation_t *a) {
    return a ? &a->mini_header : NULL;
}

SF_API size_t sf_tae_animation_event_count(const sf_tae_animation_t *a) {
    return a ? a->event_count : 0;
}

SF_API const sf_tae_event_t *sf_tae_animation_event(const sf_tae_animation_t *a, size_t i) {
    return (a && i < a->event_count) ? a->events[i] : NULL;
}

SF_API size_t sf_tae_animation_event_group_count(const sf_tae_animation_t *a) {
    return a ? a->event_group_count : 0;
}

SF_API const sf_tae_event_group_t *sf_tae_animation_event_group(const sf_tae_animation_t *a,
                                                                size_t i) {
    return (a && i < a->event_group_count) ? a->event_groups[i] : NULL;
}

SF_API float sf_tae_event_start_time(const sf_tae_event_t *e) {
    return e ? e->start_time : 0.0f;
}

SF_API float sf_tae_event_end_time(const sf_tae_event_t *e) {
    return e ? e->end_time : 0.0f;
}

SF_API int32_t sf_tae_event_type(const sf_tae_event_t *e) {
    return e ? e->type : 0;
}

SF_API const uint8_t *sf_tae_event_parameters(const sf_tae_event_t *e, size_t *out_size) {
    if (out_size)
        *out_size = e ? e->parameters_size : 0;
    return e ? e->parameters : NULL;
}

SF_API int32_t sf_tae_event_group_type(const sf_tae_event_group_t *g) {
    return g ? g->group_type : 0;
}

SF_API size_t sf_tae_event_group_member_count(const sf_tae_event_group_t *g) {
    return g ? g->member_count : 0;
}

SF_API int32_t sf_tae_event_group_member(const sf_tae_event_group_t *g, size_t i) {
    return (g && i < g->member_count) ? g->members[i] : 0;
}
