/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ESD writer. Mirrors SoulsFormats/Formats/ESD.cs Write().
 */

#include "souls_formats/sf_esd.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <stdio.h>
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

typedef struct esd_condition_ref {
    const struct sf_esd_condition *condition;
    int32_t group_index;
    int32_t index_in_group;
    int64_t offset;
} esd_condition_ref_t;

typedef struct esd_command_ref {
    const struct sf_esd_command_call *command;
} esd_command_ref_t;

typedef struct esd_state_ref {
    const struct sf_esd_state *state;
    int32_t group_index;
    int32_t state_index;
    int64_t offset;
    int64_t dummy_offset;
    int64_t condition_offsets_offset;
    int64_t entry_cmds_offset;
    int64_t exit_cmds_offset;
    int64_t while_cmds_offset;
} esd_state_ref_t;

typedef struct esd_write_ctx {
    const sf_esd_t *esd;
    const sf_allocator_t *alloc;
    sf_binary_writer_t *bw;
    int64_t data_start;
    int32_t varint_size;
    int32_t state_size;
    int32_t condition_size;
    esd_state_ref_t *states;
    int32_t state_count;
    esd_condition_ref_t *conditions;
    int32_t condition_count;
    esd_command_ref_t *commands;
    int32_t command_count;
    int32_t command_arg_count;
} esd_write_ctx_t;

static sf_result_t esd_i64_to_i32(int64_t value, int32_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value < INT32_MIN || value > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t esd_size_to_i32(size_t value, int32_t *out) {
    SF_CHECK_ARG(out != NULL);
    if (value > (size_t)INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    *out = (int32_t)value;
    return SF_OK;
}

static sf_result_t esd_write_varint(sf_binary_writer_t *bw, bool long_format, int64_t value) {
    sf_binary_writer_set_varint_long(bw, long_format);
    return sf_binary_writer_write_varint(bw, value);
}

static sf_result_t esd_reserve_varint(sf_binary_writer_t *bw, bool long_format, const char *name) {
    sf_binary_writer_set_varint_long(bw, long_format);
    return sf_binary_writer_reserve_varint(bw, name);
}

static sf_result_t esd_fill_varint(sf_binary_writer_t *bw, bool long_format, const char *name,
                                   int64_t value) {
    sf_binary_writer_set_varint_long(bw, long_format);
    return sf_binary_writer_fill_varint(bw, name, value);
}

static void esd_name(char *buf, size_t size, const char *prefix, int32_t a, int32_t b,
                     const char *suffix) {
    (void)snprintf(buf, size, "%s%d_%d_%s", prefix, a, b, suffix);
}

static sf_result_t esd_append_condition(esd_write_ctx_t *ctx,
                                        const struct sf_esd_condition *condition,
                                        int32_t group_index) {
    for (int32_t i = 0; i < ctx->condition_count; i++) {
        if (ctx->conditions[i].condition == condition && ctx->conditions[i].group_index == group_index) {
            return SF_OK;
        }
    }
    if (ctx->condition_count == INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    esd_condition_ref_t *new_refs = sf_xalloc(ctx->alloc,
        (size_t)(ctx->condition_count + 1) * sizeof(*new_refs));
    if (!new_refs) return SF_ERR_OOM;
    if (ctx->condition_count > 0) {
        memcpy(new_refs, ctx->conditions, (size_t)ctx->condition_count * sizeof(*new_refs));
    }
    sf_xfree(ctx->alloc, ctx->conditions);
    ctx->conditions = new_refs;
    ctx->conditions[ctx->condition_count].condition = condition;
    ctx->conditions[ctx->condition_count].group_index = group_index;
    ctx->conditions[ctx->condition_count].index_in_group = 0;
    for (int32_t i = 0; i < ctx->condition_count; i++) {
        if (ctx->conditions[i].group_index == group_index) ctx->conditions[ctx->condition_count].index_in_group++;
    }
    ctx->conditions[ctx->condition_count].offset = 0;
    ctx->condition_count++;

    for (int32_t i = 0; i < condition->subcondition_count; i++) {
        sf_result_t r = esd_append_condition(ctx, &condition->subconditions[i], group_index);
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t esd_append_command(esd_write_ctx_t *ctx,
                                      const struct sf_esd_command_call *command) {
    if (ctx->command_count == INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    if (command->arg_count < 0 || command->arg_count > INT32_MAX - ctx->command_arg_count) {
        return SF_ERR_OUT_OF_RANGE;
    }
    esd_command_ref_t *new_refs = sf_xalloc(ctx->alloc,
        (size_t)(ctx->command_count + 1) * sizeof(*new_refs));
    if (!new_refs) return SF_ERR_OOM;
    if (ctx->command_count > 0) {
        memcpy(new_refs, ctx->commands, (size_t)ctx->command_count * sizeof(*new_refs));
    }
    sf_xfree(ctx->alloc, ctx->commands);
    ctx->commands = new_refs;
    ctx->commands[ctx->command_count].command = command;
    ctx->command_count++;
    ctx->command_arg_count += command->arg_count;
    return SF_OK;
}

static sf_result_t esd_collect_layout(esd_write_ctx_t *ctx) {
    int32_t stored_states = 0;
    for (int32_t g = 0; g < ctx->esd->group_count; g++) {
        if (ctx->esd->groups[g].state_count < 0) return SF_ERR_OUT_OF_RANGE;
        if (ctx->esd->groups[g].state_count > INT32_MAX - stored_states) return SF_ERR_OUT_OF_RANGE;
        stored_states += ctx->esd->groups[g].state_count;
        if (ctx->esd->groups[g].state_count > 1) {
            if (stored_states == INT32_MAX) return SF_ERR_OUT_OF_RANGE;
            stored_states++;
        }
    }
    ctx->state_count = stored_states;
    if (stored_states > 0) {
        ctx->states = sf_xalloc(ctx->alloc, (size_t)stored_states * sizeof(*ctx->states));
        if (!ctx->states) return SF_ERR_OOM;
        memset(ctx->states, 0, (size_t)stored_states * sizeof(*ctx->states));
    }

    int32_t state_ref = 0;
    for (int32_t g = 0; g < ctx->esd->group_count; g++) {
        const esd_group_t *group = &ctx->esd->groups[g];
        for (int32_t s = 0; s < group->state_count; s++) {
            ctx->states[state_ref].state = &group->states[s];
            ctx->states[state_ref].group_index = g;
            ctx->states[state_ref].state_index = s;
            state_ref++;
            for (int32_t c = 0; c < group->states[s].condition_count; c++) {
                sf_result_t r = esd_append_condition(ctx, &group->states[s].conditions[c], g);
                if (r != SF_OK) return r;
            }
        }
        if (group->state_count > 1) state_ref++;
    }
    return SF_OK;
}

static int64_t esd_find_state_offset(const esd_write_ctx_t *ctx, int32_t group_index,
                                     int64_t state_id) {
    for (int32_t i = 0; i < ctx->state_count; i++) {
        if (ctx->states[i].state && ctx->states[i].group_index == group_index &&
            ctx->states[i].state->id == state_id) {
            return ctx->states[i].offset;
        }
    }
    return INT64_MIN;
}

static int64_t esd_find_condition_offset(const esd_write_ctx_t *ctx,
                                         const struct sf_esd_condition *condition,
                                         int32_t group_index) {
    for (int32_t i = 0; i < ctx->condition_count; i++) {
        if (ctx->conditions[i].condition == condition && ctx->conditions[i].group_index == group_index) {
            return ctx->conditions[i].offset;
        }
    }
    return INT64_MIN;
}

static esd_state_ref_t *esd_find_state_ref(esd_write_ctx_t *ctx, int32_t group_index,
                                           int32_t state_index) {
    for (int32_t i = 0; i < ctx->state_count; i++) {
        if (ctx->states[i].state && ctx->states[i].group_index == group_index &&
            ctx->states[i].state_index == state_index) {
            return &ctx->states[i];
        }
    }
    return NULL;
}

static sf_result_t esd_write_state_header(esd_write_ctx_t *ctx, int32_t g, int32_t s,
                                          const struct sf_esd_state *state,
                                          bool reserve_offsets) {
    bool lf = ctx->esd->long_format;
    sf_result_t r = esd_write_varint(ctx->bw, lf, state->id);
    char name[64];
    esd_name(name, sizeof(name), "state", g, s, "conds");
    if (r == SF_OK) r = reserve_offsets ? esd_reserve_varint(ctx->bw, lf, name)
                                        : esd_write_varint(ctx->bw, lf, 0);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->condition_count);
    esd_name(name, sizeof(name), "state", g, s, "entry");
    if (r == SF_OK) r = reserve_offsets ? esd_reserve_varint(ctx->bw, lf, name)
                                        : esd_write_varint(ctx->bw, lf, -1);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->entry_cmd_count);
    esd_name(name, sizeof(name), "state", g, s, "exit");
    if (r == SF_OK) r = reserve_offsets ? esd_reserve_varint(ctx->bw, lf, name)
                                        : esd_write_varint(ctx->bw, lf, -1);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->exit_cmd_count);
    esd_name(name, sizeof(name), "state", g, s, "while");
    if (r == SF_OK) r = reserve_offsets ? esd_reserve_varint(ctx->bw, lf, name)
                                        : esd_write_varint(ctx->bw, lf, -1);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->while_cmd_count);
    return r;
}

static sf_result_t esd_write_state_header_values(esd_write_ctx_t *ctx,
                                                 const esd_state_ref_t *ref) {
    const struct sf_esd_state *state = ref->state;
    bool lf = ctx->esd->long_format;
    sf_result_t r = esd_write_varint(ctx->bw, lf, state->id);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, ref->condition_offsets_offset);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->condition_count);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, ref->entry_cmds_offset);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->entry_cmd_count);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, ref->exit_cmds_offset);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->exit_cmd_count);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, ref->while_cmds_offset);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, state->while_cmd_count);
    return r;
}

static sf_result_t esd_write_command_header(esd_write_ctx_t *ctx,
                                            const struct sf_esd_command_call *command,
                                            int32_t index) {
    char name[64];
    (void)snprintf(name, sizeof(name), "cmd%d_args", index);
    sf_result_t r = sf_binary_writer_write_i32(ctx->bw, command->bank);
    if (r == SF_OK) r = sf_binary_writer_write_i32(ctx->bw, command->id);
    if (r == SF_OK) r = esd_reserve_varint(ctx->bw, ctx->esd->long_format, name);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, ctx->esd->long_format, command->arg_count);
    if (r == SF_OK) r = esd_append_command(ctx, command);
    return r;
}

static sf_result_t esd_write_command_list(esd_write_ctx_t *ctx,
                                          const struct sf_esd_command_call *commands,
                                          int32_t count, const char *reserve_name) {
    bool lf = ctx->esd->long_format;
    sf_result_t r = SF_OK;
    if (count == 0) return esd_fill_varint(ctx->bw, lf, reserve_name, -1);
    r = esd_fill_varint(ctx->bw, lf, reserve_name, sf_binary_writer_position(ctx->bw) - ctx->data_start);
    for (int32_t i = 0; r == SF_OK && i < count; i++) {
        r = esd_write_command_header(ctx, &commands[i], ctx->command_count);
    }
    return r;
}

static sf_result_t esd_write_state_command_lists(esd_write_ctx_t *ctx, int32_t g, int32_t s,
                                                 const struct sf_esd_state *state) {
    char name[64];
    esd_state_ref_t *ref = esd_find_state_ref(ctx, g, s);
    if (!ref) return SF_ERR_NOT_FOUND;
    esd_name(name, sizeof(name), "state", g, s, "entry");
    ref->entry_cmds_offset = state->entry_cmd_count == 0 ? -1 : sf_binary_writer_position(ctx->bw) - ctx->data_start;
    sf_result_t r = esd_write_command_list(ctx, state->entry_cmds, state->entry_cmd_count, name);
    esd_name(name, sizeof(name), "state", g, s, "exit");
    ref->exit_cmds_offset = state->exit_cmd_count == 0 ? -1 : sf_binary_writer_position(ctx->bw) - ctx->data_start;
    if (r == SF_OK) r = esd_write_command_list(ctx, state->exit_cmds, state->exit_cmd_count, name);
    esd_name(name, sizeof(name), "state", g, s, "while");
    ref->while_cmds_offset = state->while_cmd_count == 0 ? -1 : sf_binary_writer_position(ctx->bw) - ctx->data_start;
    if (r == SF_OK) r = esd_write_command_list(ctx, state->while_cmds, state->while_cmd_count, name);
    return r;
}

static sf_result_t esd_write_condition_header(esd_write_ctx_t *ctx, esd_condition_ref_t *ref) {
    bool lf = ctx->esd->long_format;
    const struct sf_esd_condition *condition = ref->condition;
    int64_t target_offset = -1;
    if (condition->target_state != -1) {
        target_offset = esd_find_state_offset(ctx, ref->group_index, condition->target_state);
        if (target_offset == INT64_MIN) return SF_ERR_NOT_FOUND;
    }
    sf_result_t r = esd_write_varint(ctx->bw, lf, target_offset);
    char name[64];
    esd_name(name, sizeof(name), "cond", ref->group_index, ref->index_in_group, "pass");
    if (r == SF_OK) r = esd_reserve_varint(ctx->bw, lf, name);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, condition->pass_cmd_count);
    esd_name(name, sizeof(name), "cond", ref->group_index, ref->index_in_group, "subs");
    if (r == SF_OK) r = esd_reserve_varint(ctx->bw, lf, name);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, condition->subcondition_count);
    esd_name(name, sizeof(name), "cond", ref->group_index, ref->index_in_group, "eval");
    if (r == SF_OK) r = esd_reserve_varint(ctx->bw, lf, name);
    int32_t eval_size = 0;
    if (r == SF_OK) r = esd_size_to_i32(condition->evaluator_size, &eval_size);
    if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, eval_size);
    return r;
}

static sf_result_t esd_write_condition_command_list(esd_write_ctx_t *ctx,
                                                    const esd_condition_ref_t *ref) {
    char name[64];
    esd_name(name, sizeof(name), "cond", ref->group_index, ref->index_in_group, "pass");
    return esd_write_command_list(ctx, ref->condition->pass_cmds, ref->condition->pass_cmd_count, name);
}

static sf_result_t esd_write_state_condition_offsets(esd_write_ctx_t *ctx, int32_t g, int32_t s,
                                                     const struct sf_esd_state *state) {
    char name[64];
    esd_state_ref_t *ref = esd_find_state_ref(ctx, g, s);
    if (!ref) return SF_ERR_NOT_FOUND;
    ref->condition_offsets_offset = sf_binary_writer_position(ctx->bw) - ctx->data_start;
    esd_name(name, sizeof(name), "state", g, s, "conds");
    sf_result_t r = esd_fill_varint(ctx->bw, ctx->esd->long_format, name,
                                    ref->condition_offsets_offset);
    for (int32_t i = 0; r == SF_OK && i < state->condition_count; i++) {
        int64_t offset = esd_find_condition_offset(ctx, &state->conditions[i], g);
        if (offset == INT64_MIN) return SF_ERR_NOT_FOUND;
        r = esd_write_varint(ctx->bw, ctx->esd->long_format, offset);
    }
    return r;
}

static sf_result_t esd_write_condition_offsets(esd_write_ctx_t *ctx, const esd_condition_ref_t *ref) {
    char name[64];
    esd_name(name, sizeof(name), "cond", ref->group_index, ref->index_in_group, "subs");
    sf_result_t r = SF_OK;
    if (ref->condition->subcondition_count == 0) {
        return esd_fill_varint(ctx->bw, ctx->esd->long_format, name, -1);
    }
    r = esd_fill_varint(ctx->bw, ctx->esd->long_format, name,
                        sf_binary_writer_position(ctx->bw) - ctx->data_start);
    for (int32_t i = 0; r == SF_OK && i < ref->condition->subcondition_count; i++) {
        int64_t offset = esd_find_condition_offset(ctx, &ref->condition->subconditions[i], ref->group_index);
        if (offset == INT64_MIN) return SF_ERR_NOT_FOUND;
        r = esd_write_varint(ctx->bw, ctx->esd->long_format, offset);
    }
    return r;
}

static sf_result_t esd_write_command_arg_tables(esd_write_ctx_t *ctx) {
    bool lf = ctx->esd->long_format;
    for (int32_t i = 0; i < ctx->command_count; i++) {
        char name[64];
        (void)snprintf(name, sizeof(name), "cmd%d_args", i);
        sf_result_t r = esd_fill_varint(ctx->bw, lf, name,
                                        sf_binary_writer_position(ctx->bw) - ctx->data_start);
        for (int32_t j = 0; r == SF_OK && j < ctx->commands[i].command->arg_count; j++) {
            (void)snprintf(name, sizeof(name), "cmd%d_%d_bytecode", i, j);
            r = esd_reserve_varint(ctx->bw, lf, name);
            int32_t arg_size = 0;
            if (r == SF_OK) r = esd_size_to_i32(ctx->commands[i].command->args[j].size, &arg_size);
            if (r == SF_OK) r = esd_write_varint(ctx->bw, lf, arg_size);
        }
        if (r != SF_OK) return r;
    }
    return SF_OK;
}

static sf_result_t esd_write_evaluators_and_bytecode(esd_write_ctx_t *ctx) {
    bool lf = ctx->esd->long_format;
    for (int32_t i = 0; i < ctx->condition_count; i++) {
        char name[64];
        esd_name(name, sizeof(name), "cond", ctx->conditions[i].group_index,
                 ctx->conditions[i].index_in_group, "eval");
        sf_result_t r = esd_fill_varint(ctx->bw, lf, name,
                                        sf_binary_writer_position(ctx->bw) - ctx->data_start);
        if (r == SF_OK && ctx->conditions[i].condition->evaluator_size > 0) {
            r = sf_binary_writer_write_bytes(ctx->bw, ctx->conditions[i].condition->evaluator_bytes,
                                             ctx->conditions[i].condition->evaluator_size);
        }
        if (r != SF_OK) return r;
    }
    for (int32_t i = 0; i < ctx->command_count; i++) {
        for (int32_t j = 0; j < ctx->commands[i].command->arg_count; j++) {
            char name[64];
            (void)snprintf(name, sizeof(name), "cmd%d_%d_bytecode", i, j);
            sf_result_t r = esd_fill_varint(ctx->bw, lf, name,
                                            sf_binary_writer_position(ctx->bw) - ctx->data_start);
            if (r == SF_OK && ctx->commands[i].command->args[j].size > 0) {
                r = sf_binary_writer_write_bytes(ctx->bw, ctx->commands[i].command->args[j].bytes,
                                                 ctx->commands[i].command->args[j].size);
            }
            if (r != SF_OK) return r;
        }
    }
    return SF_OK;
}

static sf_result_t esd_write(sf_binary_writer_t *bw, const sf_esd_t *esd,
                             const sf_allocator_t *alloc) {
    if (esd->dark_souls_count < 1 || esd->dark_souls_count > 3) return SF_ERR_UNSUPPORTED_VERSION;
    esd_write_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.esd = esd;
    ctx.alloc = alloc;
    ctx.bw = bw;
    ctx.varint_size = esd->long_format ? 8 : 4;
    ctx.state_size = esd->long_format ? 0x48 : 0x24;
    ctx.condition_size = esd->long_format ? 0x38 : 0x1C;

    sf_result_t r = esd_collect_layout(&ctx);
    if (r != SF_OK) goto cleanup;

    r = sf_binary_writer_write_bytes(bw, esd->long_format ? "fsSL" : "fSSL", 4);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->dark_souls_count);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->dark_souls_count);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0x54);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "DataSize");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 6);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->long_format ? 0x48 : 0x2C);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->long_format ? 0x20 : 0x10);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->group_count);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, ctx.state_size);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, ctx.state_count);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, ctx.condition_size);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "ConditionCount");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->long_format ? 0x18 : 0x10);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "CommandCallCount");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, esd->long_format ? 0x10 : 0x8);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "CommandArgCount");
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "ConditionOffsetsOffset");
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "ConditionOffsetsCount");
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "NameBlockOffset");
    int32_t name_len = esd->name ? (int32_t)strlen(esd->name) + 1 : 0;
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, name_len);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "UnkOffset1");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_reserve_i32(bw, "UnkOffset2");
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r != SF_OK) goto cleanup;

    ctx.data_start = sf_binary_writer_position(bw);
    r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK && esd->long_format) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = esd_reserve_varint(bw, esd->long_format, "StateGroupsOffset");
    if (r == SF_OK) r = esd_write_varint(bw, esd->long_format, esd->group_count);
    if (r == SF_OK) r = esd_reserve_varint(bw, esd->long_format, "NameOffset");
    if (r == SF_OK) r = esd_write_varint(bw, esd->long_format, name_len);
    int64_t unk_null = esd->dark_souls_count == 1 ? 0 : -1;
    if (r == SF_OK) r = esd_write_varint(bw, esd->long_format, unk_null);
    if (r == SF_OK) r = esd_write_varint(bw, esd->long_format, unk_null);
    if (r != SF_OK) goto cleanup;

    r = esd_fill_varint(bw, esd->long_format, "StateGroupsOffset",
                        esd->group_count == 0 ? -1 : sf_binary_writer_position(bw) - ctx.data_start);
    for (int32_t g = 0; r == SF_OK && g < esd->group_count; g++) {
        char n1[64], n2[64];
        (void)snprintf(n1, sizeof(n1), "group%d_states1", g);
        (void)snprintf(n2, sizeof(n2), "group%d_states2", g);
        r = esd_write_varint(bw, esd->long_format, esd->groups[g].id);
        if (r == SF_OK) r = esd_reserve_varint(bw, esd->long_format, n1);
        if (r == SF_OK) r = esd_write_varint(bw, esd->long_format, esd->groups[g].state_count);
        if (r == SF_OK) r = esd_reserve_varint(bw, esd->long_format, n2);
    }
    int32_t state_ref = 0;
    for (int32_t g = 0; r == SF_OK && g < esd->group_count; g++) {
        char n1[64], n2[64];
        int64_t states_rel = sf_binary_writer_position(bw) - ctx.data_start;
        (void)snprintf(n1, sizeof(n1), "group%d_states1", g);
        (void)snprintf(n2, sizeof(n2), "group%d_states2", g);
        r = esd_fill_varint(bw, esd->long_format, n1, states_rel);
        if (r == SF_OK) r = esd_fill_varint(bw, esd->long_format, n2, states_rel);
        int32_t first_ref = state_ref;
        for (int32_t s = 0; r == SF_OK && s < esd->groups[g].state_count; s++) {
            ctx.states[state_ref].offset = sf_binary_writer_position(bw) - ctx.data_start;
            r = esd_write_state_header(&ctx, g, s, &esd->groups[g].states[s], true);
            state_ref++;
        }
        if (r == SF_OK && esd->groups[g].state_count > 1) {
            ctx.states[first_ref].dummy_offset = sf_binary_writer_position(bw) - ctx.data_start;
            r = sf_binary_writer_write_pattern(bw, (size_t)ctx.state_size, 0);
            state_ref++;
        }
    }
    if (r != SF_OK) goto cleanup;

    r = sf_binary_writer_fill_i32(bw, "ConditionCount", ctx.condition_count);
    for (int32_t i = 0; r == SF_OK && i < ctx.condition_count; i++) {
        ctx.conditions[i].offset = sf_binary_writer_position(bw) - ctx.data_start;
        r = esd_write_condition_header(&ctx, &ctx.conditions[i]);
    }
    for (int32_t g = 0; r == SF_OK && g < esd->group_count; g++) {
        for (int32_t s = 0; r == SF_OK && s < esd->groups[g].state_count; s++) {
            r = esd_write_state_command_lists(&ctx, g, s, &esd->groups[g].states[s]);
        }
        for (int32_t i = 0; r == SF_OK && i < ctx.condition_count; i++) {
            if (ctx.conditions[i].group_index == g) r = esd_write_condition_command_list(&ctx, &ctx.conditions[i]);
        }
    }
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "CommandCallCount", ctx.command_count);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "CommandArgCount", ctx.command_arg_count);
    if (r == SF_OK) r = esd_write_command_arg_tables(&ctx);

    int64_t cond_offsets_rel = sf_binary_writer_position(bw) - ctx.data_start;
    int32_t cond_offsets_rel32 = 0;
    int32_t cond_offsets_count = 0;
    if (r == SF_OK) r = esd_i64_to_i32(cond_offsets_rel, &cond_offsets_rel32);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "ConditionOffsetsOffset", cond_offsets_rel32);
    for (int32_t g = 0; r == SF_OK && g < esd->group_count; g++) {
        for (int32_t s = 0; r == SF_OK && s < esd->groups[g].state_count; s++) {
            r = esd_write_state_condition_offsets(&ctx, g, s, &esd->groups[g].states[s]);
            cond_offsets_count += esd->groups[g].states[s].condition_count;
        }
        for (int32_t i = 0; r == SF_OK && i < ctx.condition_count; i++) {
            if (ctx.conditions[i].group_index == g) {
                r = esd_write_condition_offsets(&ctx, &ctx.conditions[i]);
                cond_offsets_count += ctx.conditions[i].condition->subcondition_count;
            }
        }
    }
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "ConditionOffsetsCount", cond_offsets_count);
    if (r == SF_OK) r = esd_write_evaluators_and_bytecode(&ctx);

    int64_t name_block_rel = sf_binary_writer_position(bw) - ctx.data_start;
    int32_t name_block_rel32 = 0;
    if (r == SF_OK) r = esd_i64_to_i32(name_block_rel, &name_block_rel32);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "NameBlockOffset", name_block_rel32);
    if (r == SF_OK && esd->name == NULL) {
        r = esd_fill_varint(bw, esd->long_format, "NameOffset", -1);
    } else if (r == SF_OK) {
        r = sf_binary_writer_pad(bw, 2);
        if (r == SF_OK) r = esd_fill_varint(bw, esd->long_format, "NameOffset",
                                            sf_binary_writer_position(bw) - ctx.data_start);
        if (r == SF_OK) r = sf_binary_writer_write_utf16(bw, esd->name, true);
    }
    int64_t end_rel = sf_binary_writer_position(bw) - ctx.data_start;
    int32_t end_rel32 = 0;
    if (r == SF_OK) r = esd_i64_to_i32(end_rel, &end_rel32);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "UnkOffset1", end_rel32);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "UnkOffset2", end_rel32);
    if (r == SF_OK) r = sf_binary_writer_fill_i32(bw, "DataSize", end_rel32);
    if (r == SF_OK && esd->dark_souls_count == 1) r = sf_binary_writer_pad(bw, 4);
    if (r == SF_OK && esd->dark_souls_count == 2) r = sf_binary_writer_pad(bw, 0x10);

    if (r == SF_OK) {
        for (int32_t i = 0; i < ctx.state_count; i++) {
            if (ctx.states[i].state == NULL || ctx.states[i].dummy_offset <= 0) continue;
            r = sf_binary_writer_step_in(bw, ctx.data_start + ctx.states[i].dummy_offset);
            if (r == SF_OK) {
                r = esd_write_state_header_values(&ctx, &ctx.states[i]);
            }
            if (r == SF_OK) r = sf_binary_writer_step_out(bw);
            if (r != SF_OK) break;
        }
    }

cleanup:
    sf_xfree(alloc, ctx.states);
    sf_xfree(alloc, ctx.conditions);
    sf_xfree(alloc, ctx.commands);
    return r;
}

sf_result_t sf_esd_write_to_memory(const sf_esd_t *esd, uint8_t **out_data, size_t *out_size,
                                   const sf_allocator_t *alloc) {
    SF_CHECK_ARG(esd != NULL && out_data != NULL && out_size != NULL);
    *out_data = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc);

    sf_ostream_t *stream = NULL;
    sf_result_t r = sf_ostream_open_memory(&stream, alloc);
    if (r != SF_OK) return r;

    sf_binary_writer_t *bw = NULL;
    r = sf_binary_writer_create(&bw, stream, false, alloc);
    if (r != SF_OK) {
        sf_ostream_close(stream);
        return r;
    }

    r = esd_write(bw, esd, alloc);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, out_data, out_size);
    sf_binary_writer_destroy(bw);
    sf_ostream_close(stream);
    return r;
}
