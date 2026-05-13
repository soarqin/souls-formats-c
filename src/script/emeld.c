/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Upstream reference: SoulsFormats/Formats/EMELD.cs
 */

#include "souls_formats/sf_emeld.h"
#include "souls_formats/sf_emevd.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

struct sf_emeld {
    const sf_allocator_t *alloc;
    sf_emevd_format_t     format;
    sf_emeld_event_t     *events;
    size_t                event_count;
    size_t                event_capacity;
};

static bool emeld_format_supported(sf_emevd_format_t f) {
    return f == SF_EMEVD_FORMAT_DARK_SOULS_1 ||
           f == SF_EMEVD_FORMAT_DARK_SOULS_1_BE ||
           f == SF_EMEVD_FORMAT_BLOODBORNE;
}

static void emeld_format_flags(sf_emevd_format_t f, bool *big_endian, bool *is64) {
    *big_endian = (f == SF_EMEVD_FORMAT_DARK_SOULS_1_BE);
    *is64       = (f >= SF_EMEVD_FORMAT_BLOODBORNE);
}

sf_result_t sf_emeld_create(sf_emeld_t **out, sf_emevd_format_t format,
                            const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (!emeld_format_supported(format)) return SF_ERR_INVALID_ARG;
    alloc = sf_alloc_or_default(alloc);

    sf_emeld_t *emeld = (sf_emeld_t *)sf_xalloc(alloc, sizeof(*emeld));
    if (!emeld) return SF_ERR_OOM;
    memset(emeld, 0, sizeof(*emeld));
    emeld->alloc  = alloc;
    emeld->format = format;
    *out = emeld;
    return SF_OK;
}

void sf_emeld_destroy(sf_emeld_t *emeld) {
    if (!emeld) return;
    if (emeld->events) {
        for (size_t i = 0; i < emeld->event_count; i++) {
            sf_xfree(emeld->alloc, emeld->events[i].name);
        }
        sf_xfree(emeld->alloc, emeld->events);
    }
    sf_xfree(emeld->alloc, emeld);
}

bool sf_emeld_is(const void *bytes, size_t size) {
    if (!bytes || size < 4) return false;
    const uint8_t *p = (const uint8_t *)bytes;
    return p[0] == 'E' && p[1] == 'L' && p[2] == 'D' && p[3] == 0;
}

sf_emevd_format_t sf_emeld_get_format(const sf_emeld_t *emeld) {
    return emeld ? emeld->format : SF_EMEVD_FORMAT_DARK_SOULS_1;
}

size_t sf_emeld_event_count(const sf_emeld_t *emeld) {
    return emeld ? emeld->event_count : 0u;
}

sf_result_t sf_emeld_get_event(const sf_emeld_t *emeld, size_t index,
                               const sf_emeld_event_t **out) {
    SF_CHECK_ARG(emeld != NULL && out != NULL);
    if (index >= emeld->event_count) return SF_ERR_OUT_OF_RANGE;
    *out = &emeld->events[index];
    return SF_OK;
}

static sf_result_t emeld_reserve_one(sf_emeld_t *emeld) {
    if (emeld->event_count < emeld->event_capacity) return SF_OK;
    size_t new_cap = emeld->event_capacity ? emeld->event_capacity * 2u : 8u;
    if (new_cap > SIZE_MAX / sizeof(sf_emeld_event_t)) return SF_ERR_OUT_OF_RANGE;
    sf_emeld_event_t *new_buf = (sf_emeld_event_t *)sf_xrealloc(
        emeld->alloc, emeld->events,
        emeld->event_capacity * sizeof(sf_emeld_event_t),
        new_cap * sizeof(sf_emeld_event_t));
    if (!new_buf) return SF_ERR_OOM;
    emeld->events         = new_buf;
    emeld->event_capacity = new_cap;
    return SF_OK;
}

sf_result_t sf_emeld_add_event(sf_emeld_t *emeld, int64_t id, const char *name) {
    SF_CHECK_ARG(emeld != NULL && name != NULL);
    TRY(emeld_reserve_one(emeld));

    char *dup = sf_strdup(emeld->alloc, name);
    if (!dup) return SF_ERR_OOM;

    emeld->events[emeld->event_count].id   = id;
    emeld->events[emeld->event_count].name = dup;
    emeld->event_count++;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t emeld_read_event(sf_binary_reader_t *r,
                                    sf_emevd_format_t format,
                                    int64_t strings_offset,
                                    sf_emeld_event_t *out,
                                    const sf_allocator_t *alloc) {
    int64_t id          = 0;
    int64_t name_offset = 0;
    TRY(sf_binary_reader_read_varint(r, &id));
    TRY(sf_binary_reader_read_varint(r, &name_offset));
    if (format < SF_EMEVD_FORMAT_BLOODBORNE) {
        TRY(sf_binary_reader_assert_i32_one(r, 0));
    }

    char *name = NULL;
    TRY(sf_binary_reader_get_utf16(r, strings_offset + name_offset, &name, NULL));

    out->id   = id;
    out->name = name;
    (void)alloc;
    return SF_OK;
}

sf_result_t sf_emeld_read_from_memory(sf_emeld_t **out, const void *bytes,
                                      size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (size < 4) return SF_ERR_TRUNCATED;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t       *s     = NULL;
    sf_binary_reader_t *r     = NULL;
    sf_emeld_t         *emeld = NULL;
    sf_result_t         err   = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    uint8_t magic[4] = {0, 0, 0, 0};
    err = sf_binary_reader_read_bytes(r, magic, sizeof(magic));
    if (err != SF_OK) goto done;
    static const uint8_t expected_magic[4] = {'E', 'L', 'D', 0};
    if (memcmp(magic, expected_magic, sizeof(expected_magic)) != 0) {
        err = SF_ERR_BAD_MAGIC; goto done;
    }

    bool big_endian = false;
    err = sf_binary_reader_read_bool(r, &big_endian); if (err != SF_OK) goto done;

    int8_t is64_flag = 0;
    const int8_t varint_flags[2] = {0, -1};
    err = sf_binary_reader_assert_i8(r, 2, varint_flags, &is64_flag);
    if (err != SF_OK) goto done;
    const bool is64_bit = (is64_flag == -1);

    err = sf_binary_reader_assert_u8_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_u8_one(r, 0); if (err != SF_OK) goto done;

    sf_binary_reader_set_big_endian(r, big_endian);
    sf_binary_reader_set_varint_long(r, is64_bit);

    err = sf_binary_reader_assert_i16_one(r, (int16_t)0x65); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i16_one(r, (int16_t)0xCC); if (err != SF_OK) goto done;

    int32_t file_size_ignored = 0;
    err = sf_binary_reader_read_i32(r, &file_size_ignored); if (err != SF_OK) goto done;

    sf_emevd_format_t format;
    if      (!big_endian && !is64_bit) format = SF_EMEVD_FORMAT_DARK_SOULS_1;
    else if ( big_endian && !is64_bit) format = SF_EMEVD_FORMAT_DARK_SOULS_1_BE;
    else if (!big_endian &&  is64_bit) format = SF_EMEVD_FORMAT_BLOODBORNE;
    else { err = SF_ERR_UNSUPPORTED_VERSION; goto done; }

    int64_t event_count_v   = 0;
    int64_t events_offset   = 0;
    int64_t ignored         = 0;
    int64_t strings_length  = 0;
    int64_t strings_offset  = 0;
    err = sf_binary_reader_read_varint(r, &event_count_v); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &events_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_varint_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &ignored); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_varint_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &ignored); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &strings_length); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &strings_offset); if (err != SF_OK) goto done;
    if (!is64_bit) {
        err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
        err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
    }

    if (event_count_v < 0) { err = SF_ERR_OUT_OF_RANGE; goto done; }
#if SIZE_MAX < INT64_MAX
    if ((uint64_t)event_count_v > (uint64_t)SIZE_MAX) { err = SF_ERR_OUT_OF_RANGE; goto done; }
#endif
    const size_t event_count = (size_t)event_count_v;
    (void)strings_length;

    err = sf_emeld_create(&emeld, format, alloc);
    if (err != SF_OK) goto done;

    if (event_count > 0) {
        if (event_count > SIZE_MAX / sizeof(sf_emeld_event_t)) {
            err = SF_ERR_OUT_OF_RANGE; goto done;
        }
        emeld->events = (sf_emeld_event_t *)sf_xalloc(
            alloc, event_count * sizeof(sf_emeld_event_t));
        if (!emeld->events) { err = SF_ERR_OOM; goto done; }
        memset(emeld->events, 0, event_count * sizeof(sf_emeld_event_t));
        emeld->event_capacity = event_count;

        err = sf_binary_reader_step_in(r, events_offset);
        if (err != SF_OK) goto done;
        for (size_t i = 0; i < event_count; i++) {
            err = emeld_read_event(r, format, strings_offset, &emeld->events[i], alloc);
            if (err != SF_OK) {
                (void)sf_binary_reader_step_out(r);
                goto done;
            }
            emeld->event_count = i + 1u;
        }
        err = sf_binary_reader_step_out(r);
        if (err != SF_OK) goto done;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_emeld_destroy(emeld); return err; }
    *out = emeld;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t emeld_write_body(sf_binary_writer_t *bw, const sf_emeld_t *emeld) {
    bool big_endian = false, is64_bit = false;
    emeld_format_flags(emeld->format, &big_endian, &is64_bit);

    static const uint8_t magic[4] = {'E', 'L', 'D', 0};
    TRY(sf_binary_writer_write_bytes(bw, magic, sizeof(magic)));
    TRY(sf_binary_writer_write_bool(bw, big_endian));
    TRY(sf_binary_writer_write_i8(bw, is64_bit ? (int8_t)-1 : (int8_t)0));
    TRY(sf_binary_writer_write_u8(bw, 0));
    TRY(sf_binary_writer_write_u8(bw, 0));

    sf_binary_writer_set_big_endian(bw, big_endian);
    sf_binary_writer_set_varint_long(bw, is64_bit);

    TRY(sf_binary_writer_write_i16(bw, (int16_t)0x65));
    TRY(sf_binary_writer_write_i16(bw, (int16_t)0xCC));
    TRY(sf_binary_writer_reserve_i32(bw, "FileSize"));

    if (emeld->event_count > (size_t)INT64_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(sf_binary_writer_write_varint(bw, (int64_t)emeld->event_count));
    TRY(sf_binary_writer_reserve_varint(bw, "EventsOffset"));
    TRY(sf_binary_writer_write_varint(bw, 0));
    TRY(sf_binary_writer_reserve_varint(bw, "Offset2"));
    TRY(sf_binary_writer_write_varint(bw, 0));
    TRY(sf_binary_writer_reserve_varint(bw, "Offset3"));
    TRY(sf_binary_writer_reserve_varint(bw, "StringsLength"));
    TRY(sf_binary_writer_reserve_varint(bw, "StringsOffset"));
    if (!is64_bit) {
        TRY(sf_binary_writer_write_i32(bw, 0));
        TRY(sf_binary_writer_write_i32(bw, 0));
    }

    TRY(sf_binary_writer_fill_varint(bw, "EventsOffset", sf_binary_writer_position(bw)));
    char namebuf[40];
    for (size_t i = 0; i < emeld->event_count; i++) {
        TRY(sf_binary_writer_write_varint(bw, emeld->events[i].id));
        snprintf(namebuf, sizeof(namebuf), "Event%zuNameOffset", i);
        TRY(sf_binary_writer_reserve_varint(bw, namebuf));
        if (!is64_bit) {
            TRY(sf_binary_writer_write_i32(bw, 0));
        }
    }

    TRY(sf_binary_writer_fill_varint(bw, "Offset2", sf_binary_writer_position(bw)));
    TRY(sf_binary_writer_fill_varint(bw, "Offset3", sf_binary_writer_position(bw)));

    const int64_t strings_offset = sf_binary_writer_position(bw);
    TRY(sf_binary_writer_fill_varint(bw, "StringsOffset", strings_offset));
    for (size_t i = 0; i < emeld->event_count; i++) {
        snprintf(namebuf, sizeof(namebuf), "Event%zuNameOffset", i);
        const int64_t pos = sf_binary_writer_position(bw);
        TRY(sf_binary_writer_fill_varint(bw, namebuf, pos - strings_offset));
        TRY(sf_binary_writer_write_utf16(bw, emeld->events[i].name, true));
    }
    const int64_t pre_pad = sf_binary_writer_position(bw);
    const int64_t pad_rem = (pre_pad - strings_offset) % 0x10;
    if (pad_rem > 0) {
        TRY(sf_binary_writer_write_pattern(bw, (size_t)(0x10 - pad_rem), 0));
    }
    const int64_t strings_end = sf_binary_writer_position(bw);
    TRY(sf_binary_writer_fill_varint(bw, "StringsLength", strings_end - strings_offset));

    const int64_t file_size = sf_binary_writer_position(bw);
    if (file_size > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(sf_binary_writer_fill_i32(bw, "FileSize", (int32_t)file_size));
    return SF_OK;
}

sf_result_t sf_emeld_write_to_memory(const sf_emeld_t *emeld, uint8_t **out_data,
                                     size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(emeld != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t       *os  = NULL;
    sf_binary_writer_t *bw  = NULL;
    sf_result_t         err = SF_OK;

    err = sf_ostream_open_memory(&os, alloc);
    if (err != SF_OK) return err;

    bool big_endian = false, is64_bit = false;
    emeld_format_flags(emeld->format, &big_endian, &is64_bit);

    err = sf_binary_writer_create(&bw, os, false, alloc);
    if (err != SF_OK) { sf_ostream_close(os); return err; }

    err = emeld_write_body(bw, emeld);
    if (err != SF_OK) {
        sf_binary_writer_destroy(bw);
        sf_ostream_close(os);
        return err;
    }

    err = sf_binary_writer_finish_bytes(bw, out_data, out_size);
    sf_ostream_close(os);
    return err;
}
