/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_edd.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

struct sf_edd {
    const sf_allocator_t *alloc;
    bool long_format;
    int32_t unk80;
    int32_t unk_b0[4];

    char **strings;
    size_t string_count;

    sf_edd_function_spec_t *function_specs;
    size_t function_spec_count;

    sf_edd_command_spec_t *command_specs;
    size_t command_spec_count;
};

sf_result_t sf_edd_create(sf_edd_t **out, bool long_format, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_edd_t *e = (sf_edd_t *)sf_xalloc(alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    memset(e, 0, sizeof(*e));
    e->alloc = alloc;
    e->long_format = long_format;
    *out = e;
    return SF_OK;
}

void sf_edd_destroy(sf_edd_t *e) {
    if (!e) return;
    if (e->strings) {
        for (size_t i = 0; i < e->string_count; i++) {
            sf_xfree(e->alloc, e->strings[i]);
        }
        sf_xfree(e->alloc, e->strings);
    }
    sf_xfree(e->alloc, e->function_specs);
    sf_xfree(e->alloc, e->command_specs);
    sf_xfree(e->alloc, e);
}

bool sf_edd_long_format(const sf_edd_t *e) { return e ? e->long_format : false; }
int32_t sf_edd_unk80(const sf_edd_t *e) { return e ? e->unk80 : 0; }

size_t sf_edd_function_spec_count(const sf_edd_t *e) {
    return e ? e->function_spec_count : 0u;
}

sf_result_t sf_edd_get_function_spec(const sf_edd_t *e, size_t index,
                                     sf_edd_function_spec_t *out) {
    SF_CHECK_ARG(e != NULL && out != NULL);
    if (index >= e->function_spec_count) return SF_ERR_OUT_OF_RANGE;
    *out = e->function_specs[index];
    return SF_OK;
}

size_t sf_edd_command_spec_count(const sf_edd_t *e) {
    return e ? e->command_spec_count : 0u;
}

sf_result_t sf_edd_get_command_spec(const sf_edd_t *e, size_t index,
                                    sf_edd_command_spec_t *out) {
    SF_CHECK_ARG(e != NULL && out != NULL);
    if (index >= e->command_spec_count) return SF_ERR_OUT_OF_RANGE;
    *out = e->command_specs[index];
    return SF_OK;
}

static sf_result_t edd_read_strings(sf_edd_t *e, sf_binary_reader_t *r, int64_t data_start,
                                    size_t string_count) {
    if (string_count == 0) return SF_OK;
    e->strings = (char **)sf_xalloc(e->alloc, string_count * sizeof(char *));
    if (!e->strings) return SF_ERR_OOM;
    memset(e->strings, 0, string_count * sizeof(char *));
    e->string_count = string_count;

    for (size_t i = 0; i < string_count; i++) {
        int64_t string_offset = 0;
        int64_t unused = 0;
        TRY(sf_binary_reader_read_varint(r, &string_offset));
        TRY(sf_binary_reader_read_varint(r, &unused));
        char *str = NULL;
        TRY(sf_binary_reader_get_utf16(r, data_start + string_offset, &str, NULL));
        e->strings[i] = str;
    }
    return SF_OK;
}

static sf_result_t edd_read_function_specs(sf_edd_t *e, sf_binary_reader_t *r,
                                           size_t count) {
    if (count == 0) return SF_OK;
    e->function_specs = (sf_edd_function_spec_t *)sf_xalloc(
        e->alloc, count * sizeof(sf_edd_function_spec_t));
    if (!e->function_specs) return SF_ERR_OOM;
    memset(e->function_specs, 0, count * sizeof(sf_edd_function_spec_t));
    e->function_spec_count = count;

    for (size_t i = 0; i < count; i++) {
        sf_edd_function_spec_t *fs = &e->function_specs[i];
        int16_t name_index = 0;
        TRY(sf_binary_reader_read_i32(r, &fs->id));
        TRY(sf_binary_reader_read_i16(r, &name_index));
        TRY(sf_binary_reader_read_u8(r, &fs->unk06));
        TRY(sf_binary_reader_read_u8(r, &fs->unk07));
        if (name_index < 0 || (size_t)name_index >= e->string_count) {
            return SF_ERR_BAD_MAGIC;
        }
        fs->name = e->strings[name_index];
    }
    return SF_OK;
}

static sf_result_t edd_read_conditions(sf_binary_reader_t *r, size_t count) {
    for (size_t i = 0; i < count; i++) {
        TRY(sf_binary_reader_assert_varint_one(r, -1));
        TRY(sf_binary_reader_assert_varint_one(r, 0));
    }
    return SF_OK;
}

static sf_result_t edd_read_command_specs(sf_edd_t *e, sf_binary_reader_t *r,
                                          size_t count) {
    if (count == 0) return SF_OK;
    e->command_specs = (sf_edd_command_spec_t *)sf_xalloc(
        e->alloc, count * sizeof(sf_edd_command_spec_t));
    if (!e->command_specs) return SF_ERR_OOM;
    memset(e->command_specs, 0, count * sizeof(sf_edd_command_spec_t));
    e->command_spec_count = count;

    for (size_t i = 0; i < count; i++) {
        sf_edd_command_spec_t *cs = &e->command_specs[i];
        int16_t name_index = 0;
        TRY(sf_binary_reader_read_varint(r, &cs->id));
        TRY(sf_binary_reader_assert_varint_one(r, -1));
        TRY(sf_binary_reader_assert_i32_one(r, 0));
        TRY(sf_binary_reader_read_i16(r, &name_index));
        TRY(sf_binary_reader_read_i16(r, &cs->unk0e));
        if (name_index < 0 || (size_t)name_index >= e->string_count) {
            return SF_ERR_BAD_MAGIC;
        }
        cs->name = e->strings[name_index];
    }
    return SF_OK;
}

static sf_result_t edd_read_commands(sf_binary_reader_t *r, size_t count,
                                     size_t string_count) {
    for (size_t i = 0; i < count; i++) {
        int16_t name_index = 0;
        TRY(sf_binary_reader_read_i16(r, &name_index));
        TRY(sf_binary_reader_assert_u8_one(r, 1));
        TRY(sf_binary_reader_assert_u8_one(r, 0xFF));
        if (name_index < 0 || (size_t)name_index >= string_count) {
            return SF_ERR_BAD_MAGIC;
        }
    }
    return SF_OK;
}

static sf_result_t edd_read_pass_commands(sf_binary_reader_t *r, size_t count,
                                          bool long_format) {
    for (size_t i = 0; i < count; i++) {
        if (long_format) {
            int64_t command_id = 0, command_offset = 0;
            TRY(sf_binary_reader_read_i64(r, &command_id));
            TRY(sf_binary_reader_read_i64(r, &command_offset));
        } else {
            int32_t command_id = 0, command_offset = 0;
            TRY(sf_binary_reader_read_i32(r, &command_id));
            TRY(sf_binary_reader_read_i32(r, &command_offset));
        }
    }
    return SF_OK;
}

static sf_result_t edd_skip(sf_binary_reader_t *r, int64_t bytes) {
    return sf_binary_reader_skip(r, bytes);
}

sf_result_t sf_edd_read_from_memory(sf_edd_t **out, const void *bytes, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;

    sf_istream_t *s = NULL;
    sf_binary_reader_t *r = NULL;
    sf_edd_t *e = NULL;
    sf_result_t err = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    char magic[5] = {0};
    err = sf_binary_reader_read_bytes(r, magic, 4);
    if (err != SF_OK) goto done;

    bool long_format;
    if (memcmp(magic, "fSSL", 4) == 0) {
        long_format = false;
    } else if (memcmp(magic, "fsSL", 4) == 0) {
        long_format = true;
    } else {
        err = SF_ERR_BAD_MAGIC;
        goto done;
    }
    sf_binary_reader_set_varint_long(r, long_format);

    err = sf_edd_create(&e, long_format, alloc);
    if (err != SF_OK) goto done;

    err = sf_binary_reader_assert_i32_one(r, 1); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 1); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 1); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0x7C); if (err != SF_OK) goto done;
    int32_t data_size = 0;
    err = sf_binary_reader_read_i32(r, &data_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 11); if (err != SF_OK) goto done;

    int32_t string_count = 0;
    int32_t function_spec_count = 0;
    int32_t condition_count = 0;
    int32_t command_spec_count = 0;
    int32_t command_count = 0;
    int32_t pass_command_count = 0;
    int32_t state_count = 0;
    int32_t machine_count = 0;
    int32_t condition_size = long_format ? 0x10 : 8;
    int32_t command_size = 4;
    int32_t pass_command_size = long_format ? 0x10 : 8;
    int32_t state_size = long_format ? 0x78 : 0x3C;
    int32_t machine_size = long_format ? 0x48 : 0x30;

    err = sf_binary_reader_assert_i32_one(r, long_format ? 0x58 : 0x34); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 1); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, long_format ? 0x10 : 8); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &string_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 4); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 8); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &function_spec_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, condition_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &condition_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, long_format ? 0x10 : 8); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, long_format ? 0x18 : 0x10); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &command_spec_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, command_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &command_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, pass_command_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &pass_command_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, state_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &state_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, machine_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &machine_count); if (err != SF_OK) goto done;

    int32_t strings_offset = 0;
    err = sf_binary_reader_read_i32(r, &strings_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, strings_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32(r, &e->unk80); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, data_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, data_size); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, 0); if (err != SF_OK) goto done;

    int64_t data_start = sf_binary_reader_position(r);

    err = sf_binary_reader_assert_varint_one(r, 0); if (err != SF_OK) goto done;
    int64_t command_spec_offset = 0, function_spec_offset = 0, machine_offset = 0;
    err = sf_binary_reader_read_varint(r, &command_spec_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_varint_one(r, command_spec_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &function_spec_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_varint_one(r, function_spec_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_varint(r, &machine_offset); if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_i32_one(r, machine_count); if (err != SF_OK) goto done;
    err = sf_binary_reader_read_i32s(r, 4, e->unk_b0); if (err != SF_OK) goto done;
    if (long_format) {
        err = sf_binary_reader_assert_i32_one(r, 0);
        if (err != SF_OK) goto done;
    }
    err = sf_binary_reader_assert_varint_one(r, long_format ? 0x58 : 0x34);
    if (err != SF_OK) goto done;
    err = sf_binary_reader_assert_varint_one(r, string_count);
    if (err != SF_OK) goto done;

    if (string_count < 0 || function_spec_count < 0 || condition_count < 0 ||
        command_spec_count < 0 || command_count < 0 || pass_command_count < 0 ||
        state_count < 0 || machine_count < 0) {
        err = SF_ERR_BAD_MAGIC;
        goto done;
    }

    (void)command_spec_offset;
    (void)function_spec_offset;
    (void)machine_offset;

    err = edd_read_strings(e, r, data_start, (size_t)string_count);
    if (err != SF_OK) goto done;

    err = edd_read_function_specs(e, r, (size_t)function_spec_count);
    if (err != SF_OK) goto done;

    err = edd_read_conditions(r, (size_t)condition_count);
    if (err != SF_OK) goto done;

    err = edd_read_command_specs(e, r, (size_t)command_spec_count);
    if (err != SF_OK) goto done;

    err = edd_read_commands(r, (size_t)command_count, (size_t)string_count);
    if (err != SF_OK) goto done;

    if (long_format) {
        int64_t pos = sf_binary_reader_position(r);
        int64_t rel = pos - data_start;
        int64_t rem = rel % 8;
        if (rem != 0) {
            err = sf_binary_reader_skip(r, 8 - rem);
            if (err != SF_OK) goto done;
        }
    }

    err = edd_read_pass_commands(r, (size_t)pass_command_count, long_format);
    if (err != SF_OK) goto done;

    err = edd_skip(r, (int64_t)state_count * state_size);
    if (err != SF_OK) goto done;

    err = edd_skip(r, (int64_t)machine_count * machine_size);
    if (err != SF_OK) goto done;

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_edd_destroy(e); return err; }
    *out = e;
    return SF_OK;
}
