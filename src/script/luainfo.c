/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Upstream reference: SoulsFormats/Formats/LUAINFO.cs
 */

#include "souls_formats/sf_luainfo.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _e = (expr); if (_e != SF_OK) return _e; } while (0)

struct sf_luainfo {
    const sf_allocator_t *alloc;
    bool                  big_endian;
    bool                  long_format;
    sf_luainfo_goal_t   **goals;
    size_t                goal_count;
    size_t                goal_capacity;
};

/*===========================================================================
 * Goal allocation helpers
 *===========================================================================*/

static void luainfo_goal_free(const sf_allocator_t *alloc, sf_luainfo_goal_t *g) {
    if (!g) return;
    sf_xfree(alloc, g->name);
    sf_xfree(alloc, g->logic_interrupt_name);
    sf_xfree(alloc, g);
}

static sf_result_t luainfo_goal_new(const sf_allocator_t *alloc, int32_t id,
                                    const char *name, bool battle_interrupt,
                                    bool logic_interrupt,
                                    const char *logic_interrupt_name,
                                    sf_luainfo_goal_t **out) {
    sf_luainfo_goal_t *g = (sf_luainfo_goal_t *)sf_xalloc(alloc, sizeof(*g));
    if (!g) return SF_ERR_OOM;
    memset(g, 0, sizeof(*g));
    g->id               = id;
    g->battle_interrupt = battle_interrupt;
    g->logic_interrupt  = logic_interrupt;

    g->name = sf_strdup(alloc, name);
    if (!g->name) { luainfo_goal_free(alloc, g); return SF_ERR_OOM; }

    if (logic_interrupt_name) {
        g->logic_interrupt_name = sf_strdup(alloc, logic_interrupt_name);
        if (!g->logic_interrupt_name) { luainfo_goal_free(alloc, g); return SF_ERR_OOM; }
    }

    *out = g;
    return SF_OK;
}

static sf_result_t luainfo_reserve_one(sf_luainfo_t *info) {
    if (info->goal_count < info->goal_capacity) return SF_OK;
    size_t new_cap = info->goal_capacity ? info->goal_capacity * 2u : 8u;
    if (new_cap > SIZE_MAX / sizeof(sf_luainfo_goal_t *)) return SF_ERR_OUT_OF_RANGE;
    sf_luainfo_goal_t **new_buf = (sf_luainfo_goal_t **)sf_xrealloc(
        info->alloc, info->goals,
        info->goal_capacity * sizeof(sf_luainfo_goal_t *),
        new_cap * sizeof(sf_luainfo_goal_t *));
    if (!new_buf) return SF_ERR_OOM;
    info->goals         = new_buf;
    info->goal_capacity = new_cap;
    return SF_OK;
}

/*===========================================================================
 * Lifecycle + accessors
 *===========================================================================*/

sf_result_t sf_luainfo_create(sf_luainfo_t **out, bool big_endian,
                              bool long_format, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);

    sf_luainfo_t *info = (sf_luainfo_t *)sf_xalloc(alloc, sizeof(*info));
    if (!info) return SF_ERR_OOM;
    memset(info, 0, sizeof(*info));
    info->alloc       = alloc;
    info->big_endian  = big_endian;
    info->long_format = long_format;
    *out = info;
    return SF_OK;
}

void sf_luainfo_destroy(sf_luainfo_t *info) {
    if (!info) return;
    if (info->goals) {
        for (size_t i = 0; i < info->goal_count; i++) {
            luainfo_goal_free(info->alloc, info->goals[i]);
        }
        sf_xfree(info->alloc, info->goals);
    }
    sf_xfree(info->alloc, info);
}

bool sf_luainfo_is(const void *bytes, size_t size) {
    if (size < 4 || bytes == NULL) return false;
    const unsigned char *p = (const unsigned char *)bytes;
    return p[0] == 'L' && p[1] == 'U' && p[2] == 'A' && p[3] == 'I';
}

bool sf_luainfo_big_endian (const sf_luainfo_t *info) {
    return info ? info->big_endian : false;
}

bool sf_luainfo_long_format(const sf_luainfo_t *info) {
    return info ? info->long_format : false;
}

size_t sf_luainfo_goal_count(const sf_luainfo_t *info) {
    return info ? info->goal_count : 0u;
}

sf_result_t sf_luainfo_get_goal(const sf_luainfo_t *info, size_t index,
                                const sf_luainfo_goal_t **out) {
    SF_CHECK_ARG(info != NULL && out != NULL);
    if (index >= info->goal_count) return SF_ERR_OUT_OF_RANGE;
    *out = info->goals[index];
    return SF_OK;
}

sf_result_t sf_luainfo_add_goal(sf_luainfo_t *info, int32_t id, const char *name,
                                bool battle_interrupt, bool logic_interrupt,
                                const char *logic_interrupt_name) {
    SF_CHECK_ARG(info != NULL && name != NULL);
    TRY(luainfo_reserve_one(info));

    sf_luainfo_goal_t *g = NULL;
    TRY(luainfo_goal_new(info->alloc, id, name, battle_interrupt,
                         logic_interrupt, logic_interrupt_name, &g));
    info->goals[info->goal_count++] = g;
    return SF_OK;
}

/*===========================================================================
 * Read
 *===========================================================================*/

static sf_result_t luainfo_read_goal(sf_binary_reader_t *r, bool long_format,
                                     const sf_allocator_t *alloc,
                                     sf_luainfo_goal_t **out) {
    int32_t  id            = 0;
    bool     battle        = false;
    bool     logic         = false;
    int16_t  pad           = 0;
    int64_t  name_off      = 0;
    int64_t  interrupt_off = 0;

    TRY(sf_binary_reader_read_i32(r, &id));
    if (long_format) {
        TRY(sf_binary_reader_read_bool(r, &battle));
        TRY(sf_binary_reader_read_bool(r, &logic));
        TRY(sf_binary_reader_read_i16(r, &pad));
        if (pad != 0) return SF_ERR_UNSUPPORTED_VERSION;
        TRY(sf_binary_reader_read_i64(r, &name_off));
        TRY(sf_binary_reader_read_i64(r, &interrupt_off));
    } else {
        uint32_t name_off32      = 0;
        uint32_t interrupt_off32 = 0;
        TRY(sf_binary_reader_read_u32(r, &name_off32));
        TRY(sf_binary_reader_read_u32(r, &interrupt_off32));
        TRY(sf_binary_reader_read_bool(r, &battle));
        TRY(sf_binary_reader_read_bool(r, &logic));
        TRY(sf_binary_reader_read_i16(r, &pad));
        if (pad != 0) return SF_ERR_UNSUPPORTED_VERSION;
        name_off      = (int64_t)name_off32;
        interrupt_off = (int64_t)interrupt_off32;
    }

    char *name_utf8      = NULL;
    char *interrupt_utf8 = NULL;
    sf_result_t err;
    if (long_format) {
        err = sf_binary_reader_get_utf16(r, name_off, &name_utf8, NULL);
        if (err != SF_OK) return err;
        if (interrupt_off != 0) {
            err = sf_binary_reader_get_utf16(r, interrupt_off, &interrupt_utf8, NULL);
            if (err != SF_OK) { sf_xfree(alloc, name_utf8); return err; }
        }
    } else {
        err = sf_binary_reader_get_shift_jis(r, name_off, &name_utf8, NULL);
        if (err != SF_OK) return err;
        if (interrupt_off != 0) {
            err = sf_binary_reader_get_shift_jis(r, interrupt_off, &interrupt_utf8, NULL);
            if (err != SF_OK) { sf_xfree(alloc, name_utf8); return err; }
        }
    }

    sf_luainfo_goal_t *g = (sf_luainfo_goal_t *)sf_xalloc(alloc, sizeof(*g));
    if (!g) {
        sf_xfree(alloc, name_utf8);
        sf_xfree(alloc, interrupt_utf8);
        return SF_ERR_OOM;
    }
    g->id                   = id;
    g->battle_interrupt     = battle;
    g->logic_interrupt      = logic;
    g->name                 = name_utf8;
    g->logic_interrupt_name = interrupt_utf8;

    *out = g;
    return SF_OK;
}

sf_result_t sf_luainfo_read_from_memory(sf_luainfo_t **out, const void *bytes,
                                        size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || bytes != NULL));
    *out = NULL;
    if (!sf_luainfo_is(bytes, size)) return SF_ERR_BAD_MAGIC;
    alloc = sf_alloc_or_default(alloc);

    sf_istream_t       *s    = NULL;
    sf_binary_reader_t *r    = NULL;
    sf_luainfo_t       *info = NULL;
    sf_result_t         err  = SF_OK;

    err = sf_istream_open_memory(&s, bytes, size, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_reader_create(&r, s, false, alloc);
    if (err != SF_OK) { sf_istream_close(s); return err; }

    err = sf_binary_reader_assert_ascii(r, "LUAI");
    if (err != SF_OK) goto done;

    int32_t version = 0;
    err = sf_binary_reader_read_i32(r, &version);
    if (err != SF_OK) goto done;
    bool big_endian;
    if (version == 1) {
        big_endian = false;
    } else if (version == 0x01000000) {
        big_endian = true;
    } else {
        err = SF_ERR_UNSUPPORTED_VERSION;
        goto done;
    }
    sf_binary_reader_set_big_endian(r, big_endian);

    int32_t goal_count_signed = 0;
    err = sf_binary_reader_read_i32(r, &goal_count_signed);
    if (err != SF_OK) goto done;
    if (goal_count_signed < 0) { err = SF_ERR_UNSUPPORTED_VERSION; goto done; }

    int32_t header_pad = 0;
    err = sf_binary_reader_read_i32(r, &header_pad);
    if (err != SF_OK) goto done;
    if (header_pad != 0) { err = SF_ERR_UNSUPPORTED_VERSION; goto done; }

    bool long_format;
    if (goal_count_signed == 0) {
        err = SF_ERR_UNSUPPORTED_VERSION;
        goto done;
    } else if (goal_count_signed >= 2) {
        int32_t probe = 0;
        err = sf_binary_reader_get_i32(r, 0x24, &probe);
        if (err != SF_OK) goto done;
        long_format = (probe == 0);
    } else {
        /* goal_count == 1 */
        int32_t probe_long  = 0;
        int32_t probe_short = 0;
        err = sf_binary_reader_get_i32(r, 0x18, &probe_long);
        if (err != SF_OK) goto done;
        if (probe_long == 0x10 + 0x18 * 1) {
            long_format = true;
        } else {
            err = sf_binary_reader_get_i32(r, 0x14, &probe_short);
            if (err != SF_OK) goto done;
            if (probe_short == 0x10 + 0x10 * 1) {
                long_format = false;
            } else {
                err = SF_ERR_UNSUPPORTED_VERSION;
                goto done;
            }
        }
    }

    err = sf_luainfo_create(&info, big_endian, long_format, alloc);
    if (err != SF_OK) goto done;

    size_t goal_count = (size_t)goal_count_signed;
    for (size_t i = 0; i < goal_count; i++) {
        sf_luainfo_goal_t *g = NULL;
        err = luainfo_read_goal(r, long_format, alloc, &g);
        if (err != SF_OK) goto done;

        err = luainfo_reserve_one(info);
        if (err != SF_OK) { luainfo_goal_free(alloc, g); goto done; }
        info->goals[info->goal_count++] = g;
    }

done:
    sf_binary_reader_destroy(r);
    sf_istream_close(s);
    if (err != SF_OK) { sf_luainfo_destroy(info); return err; }
    *out = info;
    return SF_OK;
}

/*===========================================================================
 * Write
 *===========================================================================*/

static sf_result_t luainfo_write_goal(sf_binary_writer_t *bw, bool long_format,
                                      size_t index, const sf_luainfo_goal_t *g) {
    char name_label[64];
    char interrupt_label[64];
    snprintf(name_label,      sizeof(name_label),      "NameOffset%zu", index);
    snprintf(interrupt_label, sizeof(interrupt_label), "LogicInterruptNameOffset%zu", index);

    TRY(sf_binary_writer_write_i32(bw, g->id));
    if (long_format) {
        TRY(sf_binary_writer_write_bool(bw, g->battle_interrupt));
        TRY(sf_binary_writer_write_bool(bw, g->logic_interrupt));
        TRY(sf_binary_writer_write_i16(bw, 0));
        TRY(sf_binary_writer_reserve_i64(bw, name_label));
        TRY(sf_binary_writer_reserve_i64(bw, interrupt_label));
    } else {
        TRY(sf_binary_writer_reserve_u32(bw, name_label));
        TRY(sf_binary_writer_reserve_u32(bw, interrupt_label));
        TRY(sf_binary_writer_write_bool(bw, g->battle_interrupt));
        TRY(sf_binary_writer_write_bool(bw, g->logic_interrupt));
        TRY(sf_binary_writer_write_i16(bw, 0));
    }
    return SF_OK;
}

static sf_result_t luainfo_write_goal_strings(sf_binary_writer_t *bw,
                                              bool long_format, size_t index,
                                              const sf_luainfo_goal_t *g) {
    char name_label[64];
    char interrupt_label[64];
    snprintf(name_label,      sizeof(name_label),      "NameOffset%zu", index);
    snprintf(interrupt_label, sizeof(interrupt_label), "LogicInterruptNameOffset%zu", index);

    if (long_format) {
        int64_t name_pos = sf_binary_writer_position(bw);
        TRY(sf_binary_writer_fill_i64(bw, name_label, name_pos));
        TRY(sf_binary_writer_write_utf16(bw, g->name, true));
        if (g->logic_interrupt_name == NULL) {
            TRY(sf_binary_writer_fill_i64(bw, interrupt_label, 0));
        } else {
            int64_t interrupt_pos = sf_binary_writer_position(bw);
            TRY(sf_binary_writer_fill_i64(bw, interrupt_label, interrupt_pos));
            TRY(sf_binary_writer_write_utf16(bw, g->logic_interrupt_name, true));
        }
    } else {
        int64_t name_pos = sf_binary_writer_position(bw);
        if ((uint64_t)name_pos > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
        TRY(sf_binary_writer_fill_u32(bw, name_label, (uint32_t)name_pos));
        TRY(sf_binary_writer_write_shift_jis(bw, g->name, true));
        if (g->logic_interrupt_name == NULL) {
            TRY(sf_binary_writer_fill_u32(bw, interrupt_label, 0u));
        } else {
            int64_t interrupt_pos = sf_binary_writer_position(bw);
            if ((uint64_t)interrupt_pos > UINT32_MAX) return SF_ERR_OUT_OF_RANGE;
            TRY(sf_binary_writer_fill_u32(bw, interrupt_label, (uint32_t)interrupt_pos));
            TRY(sf_binary_writer_write_shift_jis(bw, g->logic_interrupt_name, true));
        }
    }
    return SF_OK;
}

static sf_result_t luainfo_write_body(sf_binary_writer_t *bw, const sf_luainfo_t *info) {
    TRY(sf_binary_writer_write_ascii(bw, "LUAI", false));
    TRY(sf_binary_writer_write_i32(bw, 1));
    if (info->goal_count > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    TRY(sf_binary_writer_write_i32(bw, (int32_t)info->goal_count));
    TRY(sf_binary_writer_write_i32(bw, 0));

    for (size_t i = 0; i < info->goal_count; i++) {
        TRY(luainfo_write_goal(bw, info->long_format, i, info->goals[i]));
    }
    for (size_t i = 0; i < info->goal_count; i++) {
        TRY(luainfo_write_goal_strings(bw, info->long_format, i, info->goals[i]));
    }

    TRY(sf_binary_writer_pad(bw, 0x10));
    return SF_OK;
}

sf_result_t sf_luainfo_write_to_memory(const sf_luainfo_t *info, uint8_t **out_data,
                                       size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(info != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t       *os  = NULL;
    sf_binary_writer_t *bw  = NULL;
    sf_result_t         err = SF_OK;

    err = sf_ostream_open_memory(&os, alloc);
    if (err != SF_OK) return err;
    err = sf_binary_writer_create(&bw, os, info->big_endian, alloc);
    if (err != SF_OK) { sf_ostream_close(os); return err; }

    err = luainfo_write_body(bw, info);
    if (err != SF_OK) {
        sf_binary_writer_destroy(bw);
        sf_ostream_close(os);
        return err;
    }

    err = sf_binary_writer_finish_bytes(bw, out_data, out_size);
    sf_ostream_close(os);
    return err;
}
