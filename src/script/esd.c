/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ESD reader. Mirrors SoulsFormats/Formats/ESD.cs Read().
 */

#include "souls_formats/sf_esd.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <string.h>

typedef struct esd_blob {
    uint8_t *bytes;
    size_t size;
} esd_blob_t;

struct sf_esd_command_call {
    int32_t bank;
    int32_t id;
    int32_t arg_count;
    esd_blob_t *args;
};

struct sf_esd_condition {
    int64_t target_state;
    uint8_t *evaluator_bytes;
    size_t evaluator_size;
    int32_t subcondition_count;
    struct sf_esd_condition *subconditions;
    int32_t pass_cmd_count;
    struct sf_esd_command_call *pass_cmds;
};

struct sf_esd_state {
    int64_t id;
    int32_t condition_count;
    struct sf_esd_condition *conditions;
    int32_t entry_cmd_count;
    struct sf_esd_command_call *entry_cmds;
    int32_t exit_cmd_count;
    struct sf_esd_command_call *exit_cmds;
    int32_t while_cmd_count;
    struct sf_esd_command_call *while_cmds;
};

typedef struct esd_group {
    int64_t id;
    int32_t state_count;
    struct sf_esd_state *states;
} esd_group_t;

struct sf_esd {
    bool long_format;
    int32_t dark_souls_count;
    char *name;
    int32_t group_count;
    esd_group_t *groups;
    const sf_allocator_t *alloc;
};

typedef struct esd_tmp_state {
    int64_t offset;
    struct sf_esd_state state;
    int32_t raw_condition_count;
    int64_t *raw_condition_offsets;
    bool used;
} esd_tmp_state_t;

typedef struct esd_tmp_condition {
    int64_t offset;
    int64_t raw_state_offset;
    int32_t raw_subcondition_count;
    int64_t *raw_subcondition_offsets;
    struct sf_esd_condition condition;
} esd_tmp_condition_t;

typedef struct esd_tmp_group {
    int64_t id;
    int32_t state_count;
    int64_t *state_offsets;
} esd_tmp_group_t;

static sf_result_t esd_i64_to_size(int64_t value, size_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < 0) return SF_ERR_OUT_OF_RANGE;
#if SIZE_MAX < INT64_MAX
    if ((uint64_t)value > (uint64_t)SIZE_MAX) return SF_ERR_OUT_OF_RANGE;
#endif
    *out = (size_t)value;
    return SF_OK;
}

static sf_result_t esd_i64_to_i32_count(int64_t value, int32_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < 0 || value > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t esd_mul_size(size_t count, size_t elem_size, size_t *out) {
    SF_CHECK_ARG(out != NULL && elem_size > 0);
    if (count > SIZE_MAX / elem_size) return SF_ERR_OUT_OF_RANGE;
    *out = count * elem_size;
    return SF_OK;
}

static sf_result_t esd_alloc_array(const sf_allocator_t *alloc, int32_t count, size_t elem_size,
                                   void **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    if (count == 0) return SF_OK;
    size_t bytes = 0;
    sf_result_t r = esd_mul_size((size_t)count, elem_size, &bytes);
    if (r != SF_OK) return r;
    void *p = sf_xalloc(alloc, bytes);
    if (!p) return SF_ERR_OOM;
    memset(p, 0, bytes);
    *out = p;
    return SF_OK;
}

static sf_result_t esd_read_varint(sf_binary_reader_t *br, bool long_format, int64_t *out) {
    sf_binary_reader_set_varint_long(br, long_format);
    return sf_binary_reader_read_varint(br, out);
}

static sf_result_t esd_assert_varint(sf_binary_reader_t *br, bool long_format, int64_t expect) {
    const int64_t options[1] = {expect};
    sf_binary_reader_set_varint_long(br, long_format);
    return sf_binary_reader_assert_varint(br, 1, options, NULL);
}

static sf_result_t esd_read_varint_count(sf_binary_reader_t *br, bool long_format, int32_t *out) {
    int64_t value = 0;
    sf_result_t r = esd_read_varint(br, long_format, &value);
    if (r != SF_OK) return r;
    return esd_i64_to_i32_count(value, out);
}

static sf_result_t esd_abs_offset(int64_t data_start, int64_t rel, int64_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (rel < 0 || data_start > INT64_MAX - rel) return SF_ERR_OUT_OF_RANGE;
    *out = data_start + rel;
    return SF_OK;
}

static sf_result_t esd_read_blob(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                 int64_t data_start, int64_t rel_offset, int64_t length,
                                 uint8_t **out_bytes, size_t *out_size) {
    SF_CHECK_ARG(br != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    size_t n = 0;
    sf_result_t r = esd_i64_to_size(length, &n);
    if (r != SF_OK) return r;
    if (n == 0) return SF_OK;
    int64_t abs = 0;
    r = esd_abs_offset(data_start, rel_offset, &abs);
    if (r != SF_OK) return r;
    uint8_t *bytes = (uint8_t *)sf_xalloc(alloc, n);
    if (!bytes) return SF_ERR_OOM;
    r = sf_binary_reader_get_bytes(br, abs, bytes, n);
    if (r != SF_OK) {
        sf_xfree(alloc, bytes);
        return r;
    }
    *out_bytes = bytes;
    *out_size = n;
    return SF_OK;
}

static void esd_free_command_call(struct sf_esd_command_call *cc, const sf_allocator_t *alloc) {
    if (!cc) return;
    for (int32_t i = 0; i < cc->arg_count; i++) {
        sf_xfree(alloc, cc->args[i].bytes);
    }
    sf_xfree(alloc, cc->args);
    memset(cc, 0, sizeof(*cc));
}

static void esd_free_condition(struct sf_esd_condition *c, const sf_allocator_t *alloc) {
    if (!c) return;
    for (int32_t i = 0; i < c->subcondition_count; i++) {
        esd_free_condition(&c->subconditions[i], alloc);
    }
    for (int32_t i = 0; i < c->pass_cmd_count; i++) {
        esd_free_command_call(&c->pass_cmds[i], alloc);
    }
    sf_xfree(alloc, c->subconditions);
    sf_xfree(alloc, c->pass_cmds);
    sf_xfree(alloc, c->evaluator_bytes);
    memset(c, 0, sizeof(*c));
}

static void esd_free_state(struct sf_esd_state *s, const sf_allocator_t *alloc) {
    if (!s) return;
    for (int32_t i = 0; i < s->condition_count; i++) esd_free_condition(&s->conditions[i], alloc);
    for (int32_t i = 0; i < s->entry_cmd_count; i++) esd_free_command_call(&s->entry_cmds[i], alloc);
    for (int32_t i = 0; i < s->exit_cmd_count; i++) esd_free_command_call(&s->exit_cmds[i], alloc);
    for (int32_t i = 0; i < s->while_cmd_count; i++) esd_free_command_call(&s->while_cmds[i], alloc);
    sf_xfree(alloc, s->conditions);
    sf_xfree(alloc, s->entry_cmds);
    sf_xfree(alloc, s->exit_cmds);
    sf_xfree(alloc, s->while_cmds);
    memset(s, 0, sizeof(*s));
}

static sf_result_t esd_read_command_call(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                         bool long_format, int64_t data_start,
                                         struct sf_esd_command_call *out) {
    SF_CHECK_ARG(br != NULL && out != NULL);
    memset(out, 0, sizeof(*out));
    const int32_t banks[4] = {1, 5, 6, 7};
    sf_result_t r = sf_binary_reader_assert_i32(br, 4, banks, &out->bank);
    if (r != SF_OK) return r;
    r = sf_binary_reader_read_i32(br, &out->id);
    if (r != SF_OK) return r;
    int64_t args_offset = 0;
    r = esd_read_varint(br, long_format, &args_offset);
    if (r != SF_OK) return r;
    r = esd_read_varint_count(br, long_format, &out->arg_count);
    if (r != SF_OK) return r;
    r = esd_alloc_array(alloc, out->arg_count, sizeof(*out->args), (void **)&out->args);
    if (r != SF_OK) return r;
    int64_t abs = 0;
    r = esd_abs_offset(data_start, args_offset, &abs);
    if (r != SF_OK) goto fail;
    r = sf_binary_reader_step_in(br, abs);
    if (r != SF_OK) goto fail;
    for (int32_t i = 0; i < out->arg_count; i++) {
        int64_t arg_offset = 0;
        int64_t arg_size = 0;
        r = esd_read_varint(br, long_format, &arg_offset);
        if (r == SF_OK) r = esd_read_varint(br, long_format, &arg_size);
        if (r == SF_OK) r = esd_read_blob(br, alloc, data_start, arg_offset, arg_size,
                                          &out->args[i].bytes, &out->args[i].size);
        if (r != SF_OK) break;
    }
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK) r = r2;
    }
    if (r == SF_OK) return SF_OK;
fail:
    esd_free_command_call(out, alloc);
    return r;
}

static sf_result_t esd_read_command_list(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                         bool long_format, int64_t data_start, int64_t rel_offset,
                                         int32_t count, struct sf_esd_command_call **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    sf_result_t r = esd_alloc_array(alloc, count, sizeof(**out), (void **)out);
    if (r != SF_OK || count == 0) return r;
    int64_t abs = 0;
    r = esd_abs_offset(data_start, rel_offset, &abs);
    if (r != SF_OK) goto fail;
    r = sf_binary_reader_step_in(br, abs);
    if (r != SF_OK) goto fail;
    for (int32_t i = 0; i < count; i++) {
        r = esd_read_command_call(br, alloc, long_format, data_start, &(*out)[i]);
        if (r != SF_OK) break;
    }
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK) r = r2;
    }
    if (r == SF_OK) return SF_OK;
fail:
    for (int32_t i = 0; i < count; i++) esd_free_command_call(&(*out)[i], alloc);
    sf_xfree(alloc, *out);
    *out = NULL;
    return r;
}

static sf_result_t esd_read_offset_list(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                        bool long_format, int64_t data_start, int64_t rel_offset,
                                        int32_t count, int64_t **out) {
    SF_CHECK_ARG(out != NULL);
    *out = NULL;
    sf_result_t r = esd_alloc_array(alloc, count, sizeof(**out), (void **)out);
    if (r != SF_OK || count == 0) return r;
    int64_t abs = 0;
    r = esd_abs_offset(data_start, rel_offset, &abs);
    if (r != SF_OK) goto fail;
    r = sf_binary_reader_step_in(br, abs);
    if (r != SF_OK) goto fail;
    for (int32_t i = 0; i < count; i++) {
        r = esd_read_varint(br, long_format, &(*out)[i]);
        if (r != SF_OK) break;
    }
    {
        sf_result_t r2 = sf_binary_reader_step_out(br);
        if (r == SF_OK) r = r2;
    }
    if (r == SF_OK) return SF_OK;
fail:
    sf_xfree(alloc, *out);
    *out = NULL;
    return r;
}

static sf_result_t esd_read_state(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                  bool long_format, int64_t data_start, esd_tmp_state_t *out) {
    SF_CHECK_ARG(out != NULL);
    memset(out, 0, sizeof(*out));
    out->offset = sf_binary_reader_position(br) - data_start;
    int64_t conds_offset = 0, entry_offset = 0, exit_offset = 0, while_offset = 0;
    sf_result_t r = esd_read_varint(br, long_format, &out->state.id);
    if (r != SF_OK) return r;
    r = esd_read_varint(br, long_format, &conds_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->raw_condition_count);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &entry_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->state.entry_cmd_count);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &exit_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->state.exit_cmd_count);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &while_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->state.while_cmd_count);
    if (r == SF_OK) r = esd_read_offset_list(br, alloc, long_format, data_start, conds_offset,
                                             out->raw_condition_count, &out->raw_condition_offsets);
    if (r == SF_OK) r = esd_read_command_list(br, alloc, long_format, data_start, entry_offset,
                                             out->state.entry_cmd_count, &out->state.entry_cmds);
    if (r == SF_OK) r = esd_read_command_list(br, alloc, long_format, data_start, exit_offset,
                                             out->state.exit_cmd_count, &out->state.exit_cmds);
    if (r == SF_OK) r = esd_read_command_list(br, alloc, long_format, data_start, while_offset,
                                             out->state.while_cmd_count, &out->state.while_cmds);
    if (r != SF_OK) {
        sf_xfree(alloc, out->raw_condition_offsets);
        esd_free_state(&out->state, alloc);
    }
    return r;
}

static sf_result_t esd_read_condition(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                      bool long_format, int64_t data_start,
                                      esd_tmp_condition_t *out) {
    SF_CHECK_ARG(out != NULL);
    memset(out, 0, sizeof(*out));
    out->offset = sf_binary_reader_position(br) - data_start;
    int64_t pass_offset = 0, conds_offset = 0, eval_offset = 0, eval_len = 0;
    sf_result_t r = esd_read_varint(br, long_format, &out->raw_state_offset);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &pass_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->condition.pass_cmd_count);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &conds_offset);
    if (r == SF_OK) r = esd_read_varint_count(br, long_format, &out->raw_subcondition_count);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &eval_offset);
    if (r == SF_OK) r = esd_read_varint(br, long_format, &eval_len);
    if (r == SF_OK) r = esd_read_command_list(br, alloc, long_format, data_start, pass_offset,
                                             out->condition.pass_cmd_count, &out->condition.pass_cmds);
    if (r == SF_OK) r = esd_read_offset_list(br, alloc, long_format, data_start, conds_offset,
                                             out->raw_subcondition_count,
                                             &out->raw_subcondition_offsets);
    if (r == SF_OK) r = esd_read_blob(br, alloc, data_start, eval_offset, eval_len,
                                      &out->condition.evaluator_bytes,
                                      &out->condition.evaluator_size);
    if (r != SF_OK) {
        sf_xfree(alloc, out->raw_subcondition_offsets);
        esd_free_condition(&out->condition, alloc);
    }
    return r;
}

static int esd_find_state(const esd_tmp_state_t *states, int32_t count, int64_t offset) {
    for (int32_t i = 0; i < count; i++) if (states[i].offset == offset) return i;
    return -1;
}

static int esd_find_condition(const esd_tmp_condition_t *conditions, int32_t count,
                              int64_t offset) {
    for (int32_t i = 0; i < count; i++) if (conditions[i].offset == offset) return i;
    return -1;
}

static bool esd_group_offset_to_id(const int64_t *offsets, const int64_t *ids, int32_t count,
                                   int64_t offset, int64_t *out_id) {
    for (int32_t i = 0; i < count; i++) {
        if (offsets[i] == offset) {
            *out_id = ids[i];
            return true;
        }
    }
    return false;
}

static sf_result_t esd_clone_command_call(const struct sf_esd_command_call *src,
                                          const sf_allocator_t *alloc,
                                          struct sf_esd_command_call *dst) {
    memset(dst, 0, sizeof(*dst));
    dst->bank = src->bank;
    dst->id = src->id;
    dst->arg_count = src->arg_count;
    sf_result_t r = esd_alloc_array(alloc, dst->arg_count, sizeof(*dst->args), (void **)&dst->args);
    if (r != SF_OK) return r;
    for (int32_t i = 0; i < dst->arg_count; i++) {
        dst->args[i].size = src->args[i].size;
        if (src->args[i].size == 0) continue;
        dst->args[i].bytes = (uint8_t *)sf_xalloc(alloc, src->args[i].size);
        if (!dst->args[i].bytes) {
            esd_free_command_call(dst, alloc);
            return SF_ERR_OOM;
        }
        memcpy(dst->args[i].bytes, src->args[i].bytes, src->args[i].size);
    }
    return SF_OK;
}

static sf_result_t esd_clone_command_list(const struct sf_esd_command_call *src, int32_t count,
                                          const sf_allocator_t *alloc,
                                          struct sf_esd_command_call **out) {
    *out = NULL;
    sf_result_t r = esd_alloc_array(alloc, count, sizeof(**out), (void **)out);
    if (r != SF_OK) return r;
    for (int32_t i = 0; i < count; i++) {
        r = esd_clone_command_call(&src[i], alloc, &(*out)[i]);
        if (r != SF_OK) {
            for (int32_t j = 0; j < count; j++) esd_free_command_call(&(*out)[j], alloc);
            sf_xfree(alloc, *out);
            *out = NULL;
            return r;
        }
    }
    return SF_OK;
}

static sf_result_t esd_clone_condition(int64_t raw_offset, const esd_tmp_condition_t *conditions,
                                       int32_t condition_count, const int64_t *group_state_offsets,
                                       const int64_t *group_state_ids, int32_t group_state_count,
                                       const sf_allocator_t *alloc,
                                       struct sf_esd_condition *out, int32_t depth) {
    if (depth > condition_count) return SF_ERR_INTERNAL;
    int idx = esd_find_condition(conditions, condition_count, raw_offset);
    if (idx < 0) return SF_ERR_NOT_FOUND;
    const esd_tmp_condition_t *src = &conditions[idx];
    memset(out, 0, sizeof(*out));
    if (src->raw_state_offset == -1) {
        out->target_state = -1;
    } else if (!esd_group_offset_to_id(group_state_offsets, group_state_ids, group_state_count,
                                       src->raw_state_offset, &out->target_state)) {
        return SF_ERR_NOT_FOUND;
    }
    out->evaluator_size = src->condition.evaluator_size;
    if (out->evaluator_size > 0) {
        out->evaluator_bytes = (uint8_t *)sf_xalloc(alloc, out->evaluator_size);
        if (!out->evaluator_bytes) return SF_ERR_OOM;
        memcpy(out->evaluator_bytes, src->condition.evaluator_bytes, out->evaluator_size);
    }
    out->pass_cmd_count = src->condition.pass_cmd_count;
    sf_result_t r = esd_clone_command_list(src->condition.pass_cmds, out->pass_cmd_count, alloc,
                                          &out->pass_cmds);
    if (r != SF_OK) goto fail;
    out->subcondition_count = src->raw_subcondition_count;
    r = esd_alloc_array(alloc, out->subcondition_count, sizeof(*out->subconditions),
                        (void **)&out->subconditions);
    if (r != SF_OK) goto fail;
    for (int32_t i = 0; i < out->subcondition_count; i++) {
        r = esd_clone_condition(src->raw_subcondition_offsets[i], conditions, condition_count,
                                group_state_offsets, group_state_ids, group_state_count, alloc,
                                &out->subconditions[i], depth + 1);
        if (r != SF_OK) goto fail;
    }
    return SF_OK;
fail:
    esd_free_condition(out, alloc);
    return r;
}

static sf_result_t esd_copy_group_state(struct sf_esd_state *dst, const esd_tmp_state_t *src,
                                        const esd_tmp_condition_t *conditions, int32_t condition_count,
                                        const int64_t *group_state_offsets,
                                        const int64_t *group_state_ids,
                                        int32_t group_state_count,
                                        const sf_allocator_t *alloc) {
    memset(dst, 0, sizeof(*dst));
    dst->id = src->state.id;
    dst->entry_cmd_count = src->state.entry_cmd_count;
    dst->exit_cmd_count = src->state.exit_cmd_count;
    dst->while_cmd_count = src->state.while_cmd_count;
    sf_result_t r = esd_clone_command_list(src->state.entry_cmds, dst->entry_cmd_count, alloc,
                                          &dst->entry_cmds);
    if (r == SF_OK) r = esd_clone_command_list(src->state.exit_cmds, dst->exit_cmd_count, alloc,
                                              &dst->exit_cmds);
    if (r == SF_OK) r = esd_clone_command_list(src->state.while_cmds, dst->while_cmd_count, alloc,
                                              &dst->while_cmds);
    dst->condition_count = src->raw_condition_count;
    if (r == SF_OK) r = esd_alloc_array(alloc, dst->condition_count, sizeof(*dst->conditions),
                                        (void **)&dst->conditions);
    for (int32_t i = 0; r == SF_OK && i < dst->condition_count; i++) {
        r = esd_clone_condition(src->raw_condition_offsets[i], conditions, condition_count,
                                group_state_offsets, group_state_ids, group_state_count, alloc,
                                &dst->conditions[i], 0);
    }
    if (r != SF_OK) esd_free_state(dst, alloc);
    return r;
}

static sf_result_t esd_read_header(sf_binary_reader_t *br, struct sf_esd *esd,
                                   int64_t *out_data_start, int32_t *out_state_group_count,
                                   int32_t *out_state_count, int32_t *out_condition_count,
                                   int64_t *out_state_size) {
    uint8_t magic[4] = {0, 0, 0, 0};
    sf_result_t r = sf_binary_reader_read_bytes(br, magic, sizeof(magic));
    if (r != SF_OK) return r;
    if (memcmp(magic, "fSSL", 4) == 0) {
        esd->long_format = false;
    } else if (memcmp(magic, "fsSL", 4) == 0) {
        esd->long_format = true;
    } else {
        return SF_ERR_BAD_MAGIC;
    }
    r = sf_binary_reader_assert_i32_one(br, 1);
    if (r == SF_OK) {
        const int32_t versions[3] = {1, 2, 3};
        r = sf_binary_reader_assert_i32(br, 3, versions, &esd->dark_souls_count);
    }
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->dark_souls_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0x54);
    int32_t ignored = 0;
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 6);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->long_format ? 0x48 : 0x2C);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 1);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->long_format ? 0x20 : 0x10);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, out_state_group_count);
    int32_t state_size = 0;
    if (r == SF_OK) r = sf_binary_reader_assert_i32(br, 1,
                                                    &(int32_t){esd->long_format ? 0x48 : 0x24},
                                                    &state_size);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, out_state_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->long_format ? 0x38 : 0x1C);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, out_condition_count);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->long_format ? 0x18 : 0x10);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, esd->long_format ? 0x10 : 0x8);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    int32_t name_block_offset = 0, name_length = 0;
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &name_block_offset);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &name_length);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r != SF_OK) return r;

    *out_data_start = sf_binary_reader_position(br);
    r = sf_binary_reader_assert_i32_one(br, 1);
    for (int i = 0; r == SF_OK && i < 4; i++) r = sf_binary_reader_read_i32(br, &ignored);
    if (r == SF_OK && esd->long_format) r = sf_binary_reader_assert_i32_one(br, 0);
    int64_t state_groups_offset = 0, name_offset = 0, count64 = 0, name_len64 = 0;
    if (r == SF_OK) r = esd_read_varint(br, esd->long_format, &state_groups_offset);
    if (r == SF_OK) r = esd_read_varint(br, esd->long_format, &count64);
    if (r == SF_OK && count64 != *out_state_group_count) r = SF_ERR_BAD_MAGIC;
    if (r == SF_OK) r = esd_read_varint(br, esd->long_format, &name_offset);
    if (r == SF_OK) r = esd_read_varint(br, esd->long_format, &name_len64);
    if (r == SF_OK && name_len64 != name_length) r = SF_ERR_BAD_MAGIC;
    int64_t unk_null = esd->dark_souls_count == 1 ? 0 : -1;
    if (r == SF_OK) r = esd_assert_varint(br, esd->long_format, unk_null);
    if (r == SF_OK) r = esd_assert_varint(br, esd->long_format, unk_null);
    if (r != SF_OK) return r;

    if (name_length > 0) {
        int64_t name_abs = 0;
        r = esd_abs_offset(*out_data_start, name_offset, &name_abs);
        if (r != SF_OK) return r;
        r = sf_binary_reader_get_utf16(br, name_abs, &esd->name, NULL);
        if (r != SF_OK) return r;
    } else {
        (void)name_block_offset;
    }
    *out_state_size = state_size;
    if (*out_state_group_count < 0 || *out_state_count < 0 || *out_condition_count < 0) {
        return SF_ERR_OUT_OF_RANGE;
    }
    return SF_OK;
}

static sf_result_t esd_read_state_groups(sf_binary_reader_t *br, const sf_allocator_t *alloc,
                                         bool long_format, int64_t data_start,
                                         int64_t state_size, int32_t group_count,
                                         esd_tmp_group_t *groups) {
    (void)data_start;
    for (int32_t i = 0; i < group_count; i++) {
        sf_result_t r = esd_read_varint(br, long_format, &groups[i].id);
        int64_t states_offset = 0;
        int64_t state_count64 = 0;
        if (r == SF_OK) r = esd_read_varint(br, long_format, &states_offset);
        if (r == SF_OK) r = esd_read_varint(br, long_format, &state_count64);
        if (r == SF_OK) r = esd_assert_varint(br, long_format, states_offset);
        if (r == SF_OK) r = esd_i64_to_i32_count(state_count64, &groups[i].state_count);
        if (r == SF_OK) r = esd_alloc_array(alloc, groups[i].state_count,
                                            sizeof(*groups[i].state_offsets),
                                            (void **)&groups[i].state_offsets);
        if (r != SF_OK) return r;
        for (int32_t j = 0; j < groups[i].state_count; j++) {
            if (states_offset > INT64_MAX - (int64_t)j * state_size) return SF_ERR_OUT_OF_RANGE;
            groups[i].state_offsets[j] = states_offset + (int64_t)j * state_size;
        }
        if (groups[i].state_count > 1) {
            /* Upstream verifies the duplicated dummy state bytes; the state table parse and
             * orphan checks below still reject missing or malformed dummies. */
        }
    }
    return SF_OK;
}

static sf_result_t esd_read(sf_binary_reader_t *br, struct sf_esd **out,
                            const sf_allocator_t *alloc) {
    struct sf_esd *esd = (struct sf_esd *)sf_xalloc(alloc, sizeof(*esd));
    if (!esd) return SF_ERR_OOM;
    memset(esd, 0, sizeof(*esd));
    esd->alloc = alloc;
    int64_t data_start = 0, state_size = 0;
    int32_t state_group_count = 0, state_count = 0, condition_count = 0;
    sf_result_t r = esd_read_header(br, esd, &data_start, &state_group_count, &state_count,
                                    &condition_count, &state_size);
    esd_tmp_group_t *tmp_groups = NULL;
    esd_tmp_state_t *tmp_states = NULL;
    esd_tmp_condition_t *tmp_conditions = NULL;
    if (r == SF_OK) r = esd_alloc_array(alloc, state_group_count, sizeof(*tmp_groups),
                                        (void **)&tmp_groups);
    if (r == SF_OK) r = esd_read_state_groups(br, alloc, esd->long_format, data_start, state_size,
                                             state_group_count, tmp_groups);
    if (r == SF_OK) r = esd_alloc_array(alloc, state_count, sizeof(*tmp_states),
                                        (void **)&tmp_states);
    for (int32_t i = 0; r == SF_OK && i < state_count; i++) {
        r = esd_read_state(br, alloc, esd->long_format, data_start, &tmp_states[i]);
    }
    if (r == SF_OK) r = esd_alloc_array(alloc, condition_count, sizeof(*tmp_conditions),
                                        (void **)&tmp_conditions);
    for (int32_t i = 0; r == SF_OK && i < condition_count; i++) {
        r = esd_read_condition(br, alloc, esd->long_format, data_start, &tmp_conditions[i]);
    }
    esd->group_count = state_group_count;
    if (r == SF_OK) r = esd_alloc_array(alloc, esd->group_count, sizeof(*esd->groups),
                                        (void **)&esd->groups);
    for (int32_t g = 0; r == SF_OK && g < esd->group_count; g++) {
        esd->groups[g].id = tmp_groups[g].id;
        esd->groups[g].state_count = tmp_groups[g].state_count;
        r = esd_alloc_array(alloc, esd->groups[g].state_count, sizeof(*esd->groups[g].states),
                            (void **)&esd->groups[g].states);
        int64_t *state_ids = NULL;
        if (r == SF_OK) r = esd_alloc_array(alloc, tmp_groups[g].state_count, sizeof(*state_ids),
                                            (void **)&state_ids);
        for (int32_t s = 0; r == SF_OK && s < tmp_groups[g].state_count; s++) {
            int idx = esd_find_state(tmp_states, state_count, tmp_groups[g].state_offsets[s]);
            if (idx < 0) {
                r = SF_ERR_NOT_FOUND;
            } else {
                state_ids[s] = tmp_states[idx].state.id;
            }
        }
        for (int32_t s = 0; r == SF_OK && s < tmp_groups[g].state_count; s++) {
            int idx = esd_find_state(tmp_states, state_count, tmp_groups[g].state_offsets[s]);
            r = esd_copy_group_state(&esd->groups[g].states[s], &tmp_states[idx], tmp_conditions,
                                     condition_count, tmp_groups[g].state_offsets, state_ids,
                                     tmp_groups[g].state_count, alloc);
            if (r == SF_OK) tmp_states[idx].used = true;
        }
        if (r == SF_OK && tmp_groups[g].state_count > 1) {
            int dummy_idx = esd_find_state(tmp_states, state_count,
                                           tmp_groups[g].state_offsets[0] +
                                               state_size * tmp_groups[g].state_count);
            if (dummy_idx < 0) r = SF_ERR_NOT_FOUND;
            else tmp_states[dummy_idx].used = true;
        }
        sf_xfree(alloc, state_ids);
    }
    for (int32_t i = 0; r == SF_OK && i < state_count; i++) {
        if (!tmp_states[i].used) r = SF_ERR_NOT_FOUND;
    }

    for (int32_t i = 0; i < state_group_count; i++) sf_xfree(alloc, tmp_groups[i].state_offsets);
    for (int32_t i = 0; i < state_count; i++) {
        sf_xfree(alloc, tmp_states[i].raw_condition_offsets);
        esd_free_state(&tmp_states[i].state, alloc);
    }
    for (int32_t i = 0; i < condition_count; i++) {
        sf_xfree(alloc, tmp_conditions[i].raw_subcondition_offsets);
        esd_free_condition(&tmp_conditions[i].condition, alloc);
    }
    sf_xfree(alloc, tmp_groups);
    sf_xfree(alloc, tmp_states);
    sf_xfree(alloc, tmp_conditions);
    if (r != SF_OK) {
        sf_esd_destroy(esd);
        return r;
    }
    *out = esd;
    return SF_OK;
}

bool sf_esd_is_long_format(const sf_esd_t *esd) { return esd != NULL && esd->long_format; }

int32_t sf_esd_get_format_version(const sf_esd_t *esd) {
    return esd ? esd->dark_souls_count : 0;
}

sf_result_t sf_esd_get_name(const sf_esd_t *esd, char **out_name) {
    SF_CHECK_ARG(esd != NULL && out_name != NULL);
    *out_name = esd->name;
    return SF_OK;
}

int32_t sf_esd_get_state_group_count(const sf_esd_t *esd) { return esd ? esd->group_count : 0; }

sf_result_t sf_esd_get_state_group_id(const sf_esd_t *esd, int32_t idx, int64_t *out_id) {
    SF_CHECK_ARG(esd != NULL && out_id != NULL);
    if (idx < 0 || idx >= esd->group_count) return SF_ERR_OUT_OF_RANGE;
    *out_id = esd->groups[idx].id;
    return SF_OK;
}

int32_t sf_esd_get_state_count(const sf_esd_t *esd, int64_t group_id) {
    if (!esd) return 0;
    for (int32_t i = 0; i < esd->group_count; i++) {
        if (esd->groups[i].id == group_id) return esd->groups[i].state_count;
    }
    return 0;
}

const sf_esd_state_t *sf_esd_get_state(const sf_esd_t *esd, int64_t group_id, int32_t state_idx) {
    if (!esd || state_idx < 0) return NULL;
    for (int32_t i = 0; i < esd->group_count; i++) {
        if (esd->groups[i].id == group_id) {
            if (state_idx >= esd->groups[i].state_count) return NULL;
            return &esd->groups[i].states[state_idx];
        }
    }
    return NULL;
}

int64_t sf_esd_state_get_id(const sf_esd_state_t *s) { return s ? s->id : 0; }
int32_t sf_esd_state_get_condition_count(const sf_esd_state_t *s) { return s ? s->condition_count : 0; }
const sf_esd_condition_t *sf_esd_state_get_condition(const sf_esd_state_t *s, int32_t idx) {
    if (!s || idx < 0 || idx >= s->condition_count) return NULL;
    return &s->conditions[idx];
}
int32_t sf_esd_state_get_entry_command_count(const sf_esd_state_t *s) { return s ? s->entry_cmd_count : 0; }
const sf_esd_command_call_t *sf_esd_state_get_entry_command(const sf_esd_state_t *s, int32_t i) {
    if (!s || i < 0 || i >= s->entry_cmd_count) return NULL;
    return &s->entry_cmds[i];
}
int32_t sf_esd_state_get_exit_command_count(const sf_esd_state_t *s) { return s ? s->exit_cmd_count : 0; }
const sf_esd_command_call_t *sf_esd_state_get_exit_command(const sf_esd_state_t *s, int32_t i) {
    if (!s || i < 0 || i >= s->exit_cmd_count) return NULL;
    return &s->exit_cmds[i];
}
int32_t sf_esd_state_get_while_command_count(const sf_esd_state_t *s) { return s ? s->while_cmd_count : 0; }
const sf_esd_command_call_t *sf_esd_state_get_while_command(const sf_esd_state_t *s, int32_t i) {
    if (!s || i < 0 || i >= s->while_cmd_count) return NULL;
    return &s->while_cmds[i];
}

int64_t sf_esd_condition_get_target_state(const sf_esd_condition_t *c) { return c ? c->target_state : 0; }
sf_result_t sf_esd_condition_get_evaluator(const sf_esd_condition_t *c, const uint8_t **out_bytes,
                                           size_t *out_size) {
    SF_CHECK_ARG(c != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = c->evaluator_bytes;
    *out_size = c->evaluator_size;
    return SF_OK;
}
int32_t sf_esd_condition_get_subcondition_count(const sf_esd_condition_t *c) { return c ? c->subcondition_count : 0; }
const sf_esd_condition_t *sf_esd_condition_get_subcondition(const sf_esd_condition_t *c, int32_t i) {
    if (!c || i < 0 || i >= c->subcondition_count) return NULL;
    return &c->subconditions[i];
}
int32_t sf_esd_condition_get_pass_command_count(const sf_esd_condition_t *c) { return c ? c->pass_cmd_count : 0; }
const sf_esd_command_call_t *sf_esd_condition_get_pass_command(const sf_esd_condition_t *c, int32_t i) {
    if (!c || i < 0 || i >= c->pass_cmd_count) return NULL;
    return &c->pass_cmds[i];
}

int32_t sf_esd_command_call_get_bank(const sf_esd_command_call_t *cc) { return cc ? cc->bank : 0; }
int32_t sf_esd_command_call_get_id(const sf_esd_command_call_t *cc) { return cc ? cc->id : 0; }
int32_t sf_esd_command_call_get_argument_count(const sf_esd_command_call_t *cc) { return cc ? cc->arg_count : 0; }
sf_result_t sf_esd_command_call_get_argument(const sf_esd_command_call_t *cc, int32_t idx,
                                             const uint8_t **out_bytes, size_t *out_size) {
    SF_CHECK_ARG(cc != NULL && out_bytes != NULL && out_size != NULL);
    if (idx < 0 || idx >= cc->arg_count) return SF_ERR_OUT_OF_RANGE;
    *out_bytes = cc->args[idx].bytes;
    *out_size = cc->args[idx].size;
    return SF_OK;
}

sf_result_t sf_esd_read_from_memory(sf_esd_t **out, const uint8_t *data, size_t size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL && (size == 0 || data != NULL));
    *out = NULL;
    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *stream = NULL;
    sf_result_t r = sf_istream_open_memory(&stream, data, size, alloc);
    if (r != SF_OK) return r;
    sf_binary_reader_t *br = NULL;
    r = sf_binary_reader_create(&br, stream, false, alloc);
    if (r == SF_OK) {
        r = esd_read(br, out, alloc);
        sf_binary_reader_destroy(br);
    }
    sf_istream_close(stream);
    return r;
}

void sf_esd_destroy(sf_esd_t *esd) {
    if (!esd) return;
    const sf_allocator_t *alloc = esd->alloc;
    for (int32_t i = 0; i < esd->group_count; i++) {
        for (int32_t j = 0; j < esd->groups[i].state_count; j++) {
            esd_free_state(&esd->groups[i].states[j], alloc);
        }
        sf_xfree(alloc, esd->groups[i].states);
    }
    sf_xfree(alloc, esd->groups);
    sf_xfree(alloc, esd->name);
    sf_xfree(alloc, esd);
}
