/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_ffxdlse.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TRY(expr) do { sf_result_t _r = (expr); if (_r != SF_OK) return _r; } while (0)

#define CN_EFFECT       "FXSerializableEffect"
#define CN_PARAM_LIST   "FXSerializableParamList"
#define CN_STATE_MAP    "FXSerializableStateMap"
#define CN_STATE        "FXSerializableState"
#define CN_ACTION       "FXSerializableAction"
#define CN_TRIGGER      "FXSerializableTrigger"
#define CN_PARAM        "FXSerializableParam"
#define CN_EVAL         "FXSerializableEvaluatable<dl_int32>"
#define CN_RESOURCE_SET "FXResourceSet"
#define CN_DLVECTOR     "DLVector"

#define V_EFFECT       5
#define V_PARAM_LIST   2
#define V_STATE_MAP    1
#define V_STATE        1
#define V_ACTION       1
#define V_TRIGGER      1
#define V_PARAM        2
#define V_EVAL         1
#define V_RESOURCE_SET 1

typedef struct ffx_class_names {
    const sf_allocator_t *alloc;
    char **names;
    size_t count;
    size_t cap;
} ffx_class_names_t;

struct sf_ffxdlse_evaluatable {
    const sf_allocator_t *alloc;
    sf_ffxdlse_evaluatable_opcode_t opcode;
    int32_t type_field;
    int32_t value;
    int32_t unk00;
    int32_t arg_index;
    sf_ffxdlse_evaluatable_t *operand;
    sf_ffxdlse_evaluatable_t *left;
    sf_ffxdlse_evaluatable_t *right;
};

struct sf_ffxdlse_param {
    const sf_allocator_t *alloc;
    sf_ffxdlse_param_type_t type;
    uint8_t *payload_after_type;
    size_t payload_after_type_size;
};

struct sf_ffxdlse_param_list {
    const sf_allocator_t *alloc;
    int32_t unk04;
    sf_ffxdlse_param_t **params;
    size_t count;
    size_t cap;
};

struct sf_ffxdlse_action {
    const sf_allocator_t *alloc;
    int32_t id;
    sf_ffxdlse_param_list_t *param_list;
};

struct sf_ffxdlse_trigger {
    const sf_allocator_t *alloc;
    int32_t state_index;
    sf_ffxdlse_evaluatable_t *evaluator;
};

struct sf_ffxdlse_state {
    const sf_allocator_t *alloc;
    sf_ffxdlse_action_t **actions;
    size_t action_count;
    size_t action_cap;
    sf_ffxdlse_trigger_t **triggers;
    size_t trigger_count;
    size_t trigger_cap;
};

struct sf_ffxdlse_state_map {
    const sf_allocator_t *alloc;
    sf_ffxdlse_state_t **states;
    size_t count;
    size_t cap;
};

struct sf_ffxdlse_resource_set {
    const sf_allocator_t *alloc;
    int32_t *vectors[SF_FFXDLSE_RES_VECTOR_COUNT];
    size_t counts[SF_FFXDLSE_RES_VECTOR_COUNT];
    size_t caps[SF_FFXDLSE_RES_VECTOR_COUNT];
};

struct sf_ffxdlse_effect {
    const sf_allocator_t *alloc;
    int32_t id;
    sf_ffxdlse_param_list_t *param_list1;
    sf_ffxdlse_param_list_t *param_list2;
    sf_ffxdlse_state_map_t *state_map;
    sf_ffxdlse_resource_set_t *resource_set;
};

struct sf_ffxdlse {
    const sf_allocator_t *alloc;
    ffx_class_names_t class_names;
    sf_ffxdlse_effect_t *effect;
};

static void cn_init(ffx_class_names_t *cn, const sf_allocator_t *alloc) {
    cn->alloc = alloc;
    cn->names = NULL;
    cn->count = 0;
    cn->cap = 0;
}

static void cn_destroy(ffx_class_names_t *cn) {
    if (!cn) return;
    for (size_t i = 0; i < cn->count; i++) sf_xfree(cn->alloc, cn->names[i]);
    sf_xfree(cn->alloc, cn->names);
    cn->names = NULL;
    cn->count = 0;
    cn->cap = 0;
}

static int cn_index_of(const ffx_class_names_t *cn, const char *name) {
    for (size_t i = 0; i < cn->count; i++) {
        if (strcmp(cn->names[i], name) == 0) return (int)i;
    }
    return -1;
}

static sf_result_t cn_append_owned(ffx_class_names_t *cn, char *name) {
    if (cn->count == cn->cap) {
        size_t new_cap = cn->cap == 0 ? 8u : cn->cap * 2u;
        char **new_names = (char **)sf_xalloc(cn->alloc, new_cap * sizeof(*new_names));
        if (!new_names) return SF_ERR_OOM;
        if (cn->names) {
            memcpy(new_names, cn->names, cn->count * sizeof(*new_names));
            sf_xfree(cn->alloc, cn->names);
        }
        cn->names = new_names;
        cn->cap = new_cap;
    }
    cn->names[cn->count++] = name;
    return SF_OK;
}

static sf_result_t cn_add(ffx_class_names_t *cn, const char *name) {
    if (cn_index_of(cn, name) >= 0) return SF_OK;
    size_t len = strlen(name);
    char *copy = (char *)sf_xalloc(cn->alloc, len + 1u);
    if (!copy) return SF_ERR_OOM;
    memcpy(copy, name, len + 1u);
    sf_result_t r = cn_append_owned(cn, copy);
    if (r != SF_OK) sf_xfree(cn->alloc, copy);
    return r;
}

static sf_result_t cn_copy(ffx_class_names_t *dst, const ffx_class_names_t *src) {
    for (size_t i = 0; i < src->count; i++) TRY(cn_add(dst, src->names[i]));
    return SF_OK;
}

static sf_result_t grow_ptrs(const sf_allocator_t *alloc, void ***items, size_t count,
                             size_t *cap) {
    if (count < *cap) return SF_OK;
    size_t new_cap = *cap == 0 ? 4u : *cap * 2u;
    void **new_items = (void **)sf_xalloc(alloc, new_cap * sizeof(*new_items));
    if (!new_items) return SF_ERR_OOM;
    if (*items) {
        memcpy(new_items, *items, count * sizeof(*new_items));
        sf_xfree(alloc, *items);
    }
    *items = new_items;
    *cap = new_cap;
    return SF_OK;
}

static sf_result_t grow_i32(const sf_allocator_t *alloc, int32_t **items, size_t count,
                            size_t *cap) {
    if (count < *cap) return SF_OK;
    size_t new_cap = *cap == 0 ? 4u : *cap * 2u;
    int32_t *new_items = (int32_t *)sf_xalloc(alloc, new_cap * sizeof(*new_items));
    if (!new_items) return SF_ERR_OOM;
    if (*items) {
        memcpy(new_items, *items, count * sizeof(*new_items));
        sf_xfree(alloc, *items);
    }
    *items = new_items;
    *cap = new_cap;
    return SF_OK;
}

static bool valid_param_type(int32_t type) {
    switch (type) {
        case 1: case 2: case 5: case 6: case 7: case 9: case 11: case 12:
        case 13: case 15: case 17: case 18: case 19: case 20: case 21:
        case 37: case 38: case 40: case 41: case 44: case 45: case 46:
        case 47: case 59: case 60: case 66: case 68: case 69: case 70:
        case 71: case 79: case 81: case 82: case 83: case 84: case 85:
        case 87:
            return true;
        default:
            return false;
    }
}

static bool valid_eval_opcode(int32_t opcode) {
    switch (opcode) {
        case 1: case 2: case 3: case 4: case 5: case 8: case 9: case 10:
        case 11: case 12: case 13: case 14: case 15: case 20: case 21:
        case 22: case 23: case 24:
            return true;
        default:
            return false;
    }
}

static bool eval_is_binary(sf_ffxdlse_evaluatable_opcode_t opcode) {
    return opcode == SF_FFXDLSE_EVAL_AND || opcode == SF_FFXDLSE_EVAL_OR ||
           opcode == SF_FFXDLSE_EVAL_GE || opcode == SF_FFXDLSE_EVAL_GT ||
           opcode == SF_FFXDLSE_EVAL_LE || opcode == SF_FFXDLSE_EVAL_LT ||
           opcode == SF_FFXDLSE_EVAL_EQ || opcode == SF_FFXDLSE_EVAL_NE;
}

static sf_result_t read_ser_header(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                   const char *expected_class, int32_t expected_version,
                                   int64_t *out_start, int32_t *out_length) {
    int16_t class_index = 0;
    int32_t version = 0;
    *out_start = sf_binary_reader_position(r);
    TRY(sf_binary_reader_read_i16(r, &class_index));
    TRY(sf_binary_reader_read_i32(r, &version));
    TRY(sf_binary_reader_read_i32(r, out_length));
    if (class_index <= 0 || (size_t)class_index > cn->count) return SF_ERR_INVALID_ARG;
    if (strcmp(cn->names[(size_t)class_index - 1u], expected_class) != 0) return SF_ERR_INVALID_ARG;
    if (version != expected_version || *out_length < 10) return SF_ERR_INVALID_ARG;
    return SF_OK;
}

static sf_result_t verify_end(sf_binary_reader_t *r, int64_t start, int32_t length) {
    return sf_binary_reader_position(r) == start + (int64_t)length ? SF_OK : SF_ERR_INVALID_ARG;
}

static sf_result_t write_ser_begin(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                   const char *class_name, int32_t version,
                                   int64_t *out_start, char *key, size_t key_size) {
    int idx = cn_index_of(cn, class_name);
    if (idx < 0) return SF_ERR_INVALID_ARG;
    *out_start = sf_binary_writer_position(w);
    snprintf(key, key_size, "FFXDLSE_%llX", (unsigned long long)*out_start);
    TRY(sf_binary_writer_write_i16(w, (int16_t)(idx + 1)));
    TRY(sf_binary_writer_write_i32(w, version));
    TRY(sf_binary_writer_reserve_i32(w, key));
    return SF_OK;
}

static sf_result_t write_ser_end(sf_binary_writer_t *w, const char *key, int64_t start) {
    int64_t end = sf_binary_writer_position(w);
    if (end < start || end - start > INT32_MAX) return SF_ERR_OUT_OF_RANGE;
    return sf_binary_writer_fill_i32(w, key, (int32_t)(end - start));
}

static sf_result_t read_dlvector(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                 const sf_allocator_t *alloc, int32_t **out, size_t *out_count) {
    int idx = cn_index_of(cn, CN_DLVECTOR);
    if (idx < 0) return SF_ERR_INVALID_ARG;
    TRY(sf_binary_reader_assert_i16_one(r, (int16_t)(idx + 1)));
    int32_t count = 0;
    TRY(sf_binary_reader_read_i32(r, &count));
    if (count < 0) return SF_ERR_INVALID_ARG;
    *out = NULL;
    *out_count = (size_t)count;
    if (count == 0) return SF_OK;
    *out = (int32_t *)sf_xalloc(alloc, (size_t)count * sizeof(**out));
    if (!*out) return SF_ERR_OOM;
    sf_result_t res = sf_binary_reader_read_i32s(r, (size_t)count, *out);
    if (res != SF_OK) {
        sf_xfree(alloc, *out);
        *out = NULL;
        *out_count = 0;
    }
    return res;
}

static sf_result_t write_dlvector(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                  const int32_t *items, size_t count) {
    int idx = cn_index_of(cn, CN_DLVECTOR);
    if (idx < 0 || count > (size_t)INT32_MAX) return SF_ERR_INVALID_ARG;
    TRY(sf_binary_writer_write_i16(w, (int16_t)(idx + 1)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)count));
    if (count > 0) TRY(sf_binary_writer_write_i32s(w, count, items));
    return SF_OK;
}

static void destroy_evaluatable(sf_ffxdlse_evaluatable_t *ev);
static void destroy_param(sf_ffxdlse_param_t *p);
static void destroy_param_list(sf_ffxdlse_param_list_t *pl);
static void destroy_action(sf_ffxdlse_action_t *a);
static void destroy_trigger(sf_ffxdlse_trigger_t *t);
static void destroy_state(sf_ffxdlse_state_t *s);
static void destroy_state_map(sf_ffxdlse_state_map_t *sm);
static void destroy_resource_set(sf_ffxdlse_resource_set_t *rs);
static void destroy_effect(sf_ffxdlse_effect_t *e);

static void destroy_evaluatable(sf_ffxdlse_evaluatable_t *ev) {
    if (!ev) return;
    destroy_evaluatable(ev->operand);
    destroy_evaluatable(ev->left);
    destroy_evaluatable(ev->right);
    sf_xfree(ev->alloc, ev);
}

static void destroy_param(sf_ffxdlse_param_t *p) {
    if (!p) return;
    sf_xfree(p->alloc, p->payload_after_type);
    sf_xfree(p->alloc, p);
}

static void destroy_param_list(sf_ffxdlse_param_list_t *pl) {
    if (!pl) return;
    for (size_t i = 0; i < pl->count; i++) destroy_param(pl->params[i]);
    sf_xfree(pl->alloc, pl->params);
    sf_xfree(pl->alloc, pl);
}

static void destroy_action(sf_ffxdlse_action_t *a) {
    if (!a) return;
    destroy_param_list(a->param_list);
    sf_xfree(a->alloc, a);
}

static void destroy_trigger(sf_ffxdlse_trigger_t *t) {
    if (!t) return;
    destroy_evaluatable(t->evaluator);
    sf_xfree(t->alloc, t);
}

static void destroy_state(sf_ffxdlse_state_t *s) {
    if (!s) return;
    for (size_t i = 0; i < s->action_count; i++) destroy_action(s->actions[i]);
    for (size_t i = 0; i < s->trigger_count; i++) destroy_trigger(s->triggers[i]);
    sf_xfree(s->alloc, s->actions);
    sf_xfree(s->alloc, s->triggers);
    sf_xfree(s->alloc, s);
}

static void destroy_state_map(sf_ffxdlse_state_map_t *sm) {
    if (!sm) return;
    for (size_t i = 0; i < sm->count; i++) destroy_state(sm->states[i]);
    sf_xfree(sm->alloc, sm->states);
    sf_xfree(sm->alloc, sm);
}

static void destroy_resource_set(sf_ffxdlse_resource_set_t *rs) {
    if (!rs) return;
    for (size_t i = 0; i < SF_FFXDLSE_RES_VECTOR_COUNT; i++) sf_xfree(rs->alloc, rs->vectors[i]);
    sf_xfree(rs->alloc, rs);
}

static void destroy_effect(sf_ffxdlse_effect_t *e) {
    if (!e) return;
    destroy_param_list(e->param_list1);
    destroy_param_list(e->param_list2);
    destroy_state_map(e->state_map);
    destroy_resource_set(e->resource_set);
    sf_xfree(e->alloc, e);
}

static sf_result_t create_param_list(const sf_allocator_t *alloc, sf_ffxdlse_param_list_t **out) {
    sf_ffxdlse_param_list_t *pl = (sf_ffxdlse_param_list_t *)sf_xalloc(alloc, sizeof(*pl));
    if (!pl) return SF_ERR_OOM;
    pl->alloc = alloc;
    pl->unk04 = 0;
    pl->params = NULL;
    pl->count = 0;
    pl->cap = 0;
    *out = pl;
    return SF_OK;
}

static sf_result_t create_action(const sf_allocator_t *alloc, sf_ffxdlse_action_t **out) {
    sf_ffxdlse_action_t *a = (sf_ffxdlse_action_t *)sf_xalloc(alloc, sizeof(*a));
    if (!a) return SF_ERR_OOM;
    a->alloc = alloc;
    a->id = 0;
    a->param_list = NULL;
    sf_result_t r = create_param_list(alloc, &a->param_list);
    if (r != SF_OK) { destroy_action(a); return r; }
    *out = a;
    return SF_OK;
}

static sf_result_t create_trigger(const sf_allocator_t *alloc, sf_ffxdlse_trigger_t **out) {
    sf_ffxdlse_trigger_t *t = (sf_ffxdlse_trigger_t *)sf_xalloc(alloc, sizeof(*t));
    if (!t) return SF_ERR_OOM;
    t->alloc = alloc;
    t->state_index = 0;
    t->evaluator = NULL;
    *out = t;
    return SF_OK;
}

static sf_result_t create_state(const sf_allocator_t *alloc, sf_ffxdlse_state_t **out) {
    sf_ffxdlse_state_t *s = (sf_ffxdlse_state_t *)sf_xalloc(alloc, sizeof(*s));
    if (!s) return SF_ERR_OOM;
    s->alloc = alloc;
    s->actions = NULL;
    s->action_count = 0;
    s->action_cap = 0;
    s->triggers = NULL;
    s->trigger_count = 0;
    s->trigger_cap = 0;
    *out = s;
    return SF_OK;
}

static sf_result_t create_state_map(const sf_allocator_t *alloc, sf_ffxdlse_state_map_t **out) {
    sf_ffxdlse_state_map_t *sm = (sf_ffxdlse_state_map_t *)sf_xalloc(alloc, sizeof(*sm));
    if (!sm) return SF_ERR_OOM;
    sm->alloc = alloc;
    sm->states = NULL;
    sm->count = 0;
    sm->cap = 0;
    *out = sm;
    return SF_OK;
}

static sf_result_t create_resource_set(const sf_allocator_t *alloc, sf_ffxdlse_resource_set_t **out) {
    sf_ffxdlse_resource_set_t *rs = (sf_ffxdlse_resource_set_t *)sf_xalloc(alloc, sizeof(*rs));
    if (!rs) return SF_ERR_OOM;
    rs->alloc = alloc;
    for (size_t i = 0; i < SF_FFXDLSE_RES_VECTOR_COUNT; i++) {
        rs->vectors[i] = NULL;
        rs->counts[i] = 0;
        rs->caps[i] = 0;
    }
    *out = rs;
    return SF_OK;
}

static sf_result_t create_effect(const sf_allocator_t *alloc, sf_ffxdlse_effect_t **out) {
    sf_ffxdlse_effect_t *e = (sf_ffxdlse_effect_t *)sf_xalloc(alloc, sizeof(*e));
    if (!e) return SF_ERR_OOM;
    e->alloc = alloc;
    e->id = 0;
    e->param_list1 = NULL;
    e->param_list2 = NULL;
    e->state_map = NULL;
    e->resource_set = NULL;
    sf_result_t r = create_param_list(alloc, &e->param_list1);
    if (r == SF_OK) r = create_param_list(alloc, &e->param_list2);
    if (r == SF_OK) r = create_state_map(alloc, &e->state_map);
    if (r == SF_OK) r = create_resource_set(alloc, &e->resource_set);
    if (r != SF_OK) { destroy_effect(e); return r; }
    *out = e;
    return SF_OK;
}

static sf_result_t create_eval(sf_ffxdlse_evaluatable_t **out,
                               sf_ffxdlse_evaluatable_opcode_t opcode,
                               const sf_allocator_t *alloc) {
    alloc = sf_alloc_or_default(alloc);
    sf_ffxdlse_evaluatable_t *ev = (sf_ffxdlse_evaluatable_t *)sf_xalloc(alloc, sizeof(*ev));
    if (!ev) return SF_ERR_OOM;
    ev->alloc = alloc;
    ev->opcode = opcode;
    ev->type_field = eval_is_binary(opcode) || opcode == SF_FFXDLSE_EVAL_NOT ? 1 : 3;
    ev->value = 0;
    ev->unk00 = 0;
    ev->arg_index = 0;
    ev->operand = NULL;
    ev->left = NULL;
    ev->right = NULL;
    *out = ev;
    return SF_OK;
}

static sf_result_t read_evaluatable(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                    const sf_allocator_t *alloc, sf_ffxdlse_evaluatable_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_EVAL, V_EVAL, &start, &length));
    int32_t opcode = 0;
    int32_t type_field = 0;
    TRY(sf_binary_reader_read_i32(r, &opcode));
    TRY(sf_binary_reader_read_i32(r, &type_field));
    if (!valid_eval_opcode(opcode)) return SF_ERR_INVALID_ARG;
    sf_ffxdlse_evaluatable_t *ev = NULL;
    TRY(create_eval(&ev, (sf_ffxdlse_evaluatable_opcode_t)opcode, alloc));
    ev->type_field = type_field;
    sf_result_t res = SF_OK;
    if (opcode == SF_FFXDLSE_EVAL_CONSTANT) {
        res = sf_binary_reader_read_i32(r, &ev->value);
    } else if (opcode == SF_FFXDLSE_EVAL_2 || opcode == SF_FFXDLSE_EVAL_3) {
        if ((res = sf_binary_reader_read_i32(r, &ev->unk00)) == SF_OK) {
            res = sf_binary_reader_read_i32(r, &ev->arg_index);
        }
    } else if (eval_is_binary((sf_ffxdlse_evaluatable_opcode_t)opcode)) {
        if ((res = read_evaluatable(r, cn, alloc, &ev->right)) == SF_OK) {
            res = read_evaluatable(r, cn, alloc, &ev->left);
        }
    } else if (opcode == SF_FFXDLSE_EVAL_NOT) {
        res = read_evaluatable(r, cn, alloc, &ev->operand);
    }
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_evaluatable(ev); return res; }
    *out = ev;
    return SF_OK;
}

static sf_result_t write_evaluatable(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                     const sf_ffxdlse_evaluatable_t *ev) {
    if (!ev) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_EVAL, V_EVAL, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)ev->opcode));
    TRY(sf_binary_writer_write_i32(w, ev->type_field));
    if (ev->opcode == SF_FFXDLSE_EVAL_CONSTANT) {
        TRY(sf_binary_writer_write_i32(w, ev->value));
    } else if (ev->opcode == SF_FFXDLSE_EVAL_2 || ev->opcode == SF_FFXDLSE_EVAL_3) {
        TRY(sf_binary_writer_write_i32(w, ev->unk00));
        TRY(sf_binary_writer_write_i32(w, ev->arg_index));
    } else if (eval_is_binary(ev->opcode)) {
        TRY(write_evaluatable(w, cn, ev->right));
        TRY(write_evaluatable(w, cn, ev->left));
    } else if (ev->opcode == SF_FFXDLSE_EVAL_NOT) {
        TRY(write_evaluatable(w, cn, ev->operand));
    }
    return write_ser_end(w, key, start);
}

static sf_result_t read_param(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                              const sf_allocator_t *alloc, sf_ffxdlse_param_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_PARAM, V_PARAM, &start, &length));
    int32_t type = 0;
    TRY(sf_binary_reader_read_i32(r, &type));
    if (!valid_param_type(type)) return SF_ERR_INVALID_ARG;
    int64_t end = start + (int64_t)length;
    int64_t pos = sf_binary_reader_position(r);
    if (end < pos) return SF_ERR_INVALID_ARG;
    size_t payload_size = (size_t)(end - pos);
    sf_ffxdlse_param_t *p = (sf_ffxdlse_param_t *)sf_xalloc(alloc, sizeof(*p));
    if (!p) return SF_ERR_OOM;
    p->alloc = alloc;
    p->type = (sf_ffxdlse_param_type_t)type;
    p->payload_after_type = NULL;
    p->payload_after_type_size = payload_size;
    if (payload_size > 0) {
        p->payload_after_type = (uint8_t *)sf_xalloc(alloc, payload_size);
        if (!p->payload_after_type) { destroy_param(p); return SF_ERR_OOM; }
        sf_result_t res = sf_binary_reader_read_bytes(r, p->payload_after_type, payload_size);
        if (res != SF_OK) { destroy_param(p); return res; }
    }
    TRY(verify_end(r, start, length));
    *out = p;
    return SF_OK;
}

static sf_result_t write_param(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                               const sf_ffxdlse_param_t *p) {
    if (!p) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_PARAM, V_PARAM, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)p->type));
    if (p->payload_after_type_size > 0) {
        TRY(sf_binary_writer_write_bytes(w, p->payload_after_type, p->payload_after_type_size));
    }
    return write_ser_end(w, key, start);
}

static sf_result_t read_param_list(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                   const sf_allocator_t *alloc, sf_ffxdlse_param_list_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_PARAM_LIST, V_PARAM_LIST, &start, &length));
    sf_ffxdlse_param_list_t *pl = NULL;
    TRY(create_param_list(alloc, &pl));
    int32_t count = 0;
    sf_result_t res = sf_binary_reader_read_i32(r, &count);
    if (res == SF_OK) res = sf_binary_reader_read_i32(r, &pl->unk04);
    if (res == SF_OK && count < 0) res = SF_ERR_INVALID_ARG;
    for (int32_t i = 0; res == SF_OK && i < count; i++) {
        res = grow_ptrs(alloc, (void ***)&pl->params, pl->count, &pl->cap);
        if (res == SF_OK) res = read_param(r, cn, alloc, &pl->params[pl->count]);
        if (res == SF_OK) pl->count++;
    }
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_param_list(pl); return res; }
    *out = pl;
    return SF_OK;
}

static sf_result_t write_param_list(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                    const sf_ffxdlse_param_list_t *pl) {
    if (!pl || pl->count > (size_t)INT32_MAX) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_PARAM_LIST, V_PARAM_LIST, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)pl->count));
    TRY(sf_binary_writer_write_i32(w, pl->unk04));
    for (size_t i = 0; i < pl->count; i++) TRY(write_param(w, cn, pl->params[i]));
    return write_ser_end(w, key, start);
}

static sf_result_t read_action(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                               const sf_allocator_t *alloc, sf_ffxdlse_action_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_ACTION, V_ACTION, &start, &length));
    sf_ffxdlse_action_t *a = NULL;
    TRY(create_action(alloc, &a));
    destroy_param_list(a->param_list);
    a->param_list = NULL;
    sf_result_t res = sf_binary_reader_read_i32(r, &a->id);
    if (res == SF_OK) res = read_param_list(r, cn, alloc, &a->param_list);
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_action(a); return res; }
    *out = a;
    return SF_OK;
}

static sf_result_t write_action(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                const sf_ffxdlse_action_t *a) {
    if (!a) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_ACTION, V_ACTION, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, a->id));
    TRY(write_param_list(w, cn, a->param_list));
    return write_ser_end(w, key, start);
}

static sf_result_t read_trigger(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                const sf_allocator_t *alloc, sf_ffxdlse_trigger_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_TRIGGER, V_TRIGGER, &start, &length));
    sf_ffxdlse_trigger_t *t = NULL;
    TRY(create_trigger(alloc, &t));
    sf_result_t res = sf_binary_reader_read_i32(r, &t->state_index);
    if (res == SF_OK) res = read_evaluatable(r, cn, alloc, &t->evaluator);
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_trigger(t); return res; }
    *out = t;
    return SF_OK;
}

static sf_result_t write_trigger(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                 const sf_ffxdlse_trigger_t *t) {
    if (!t || !t->evaluator) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_TRIGGER, V_TRIGGER, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, t->state_index));
    TRY(write_evaluatable(w, cn, t->evaluator));
    return write_ser_end(w, key, start);
}

static sf_result_t read_state(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                              const sf_allocator_t *alloc, sf_ffxdlse_state_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_STATE, V_STATE, &start, &length));
    sf_ffxdlse_state_t *s = NULL;
    TRY(create_state(alloc, &s));
    int32_t action_count = 0;
    int32_t trigger_count = 0;
    sf_result_t res = sf_binary_reader_read_i32(r, &action_count);
    if (res == SF_OK) res = sf_binary_reader_read_i32(r, &trigger_count);
    if (res == SF_OK && (action_count < 0 || trigger_count < 0)) res = SF_ERR_INVALID_ARG;
    for (int32_t i = 0; res == SF_OK && i < action_count; i++) {
        res = grow_ptrs(alloc, (void ***)&s->actions, s->action_count, &s->action_cap);
        if (res == SF_OK) res = read_action(r, cn, alloc, &s->actions[s->action_count]);
        if (res == SF_OK) s->action_count++;
    }
    for (int32_t i = 0; res == SF_OK && i < trigger_count; i++) {
        res = grow_ptrs(alloc, (void ***)&s->triggers, s->trigger_count, &s->trigger_cap);
        if (res == SF_OK) res = read_trigger(r, cn, alloc, &s->triggers[s->trigger_count]);
        if (res == SF_OK) s->trigger_count++;
    }
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_state(s); return res; }
    *out = s;
    return SF_OK;
}

static sf_result_t write_state(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                               const sf_ffxdlse_state_t *s) {
    if (!s || s->action_count > (size_t)INT32_MAX || s->trigger_count > (size_t)INT32_MAX) {
        return SF_ERR_INVALID_ARG;
    }
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_STATE, V_STATE, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)s->action_count));
    TRY(sf_binary_writer_write_i32(w, (int32_t)s->trigger_count));
    for (size_t i = 0; i < s->action_count; i++) TRY(write_action(w, cn, s->actions[i]));
    for (size_t i = 0; i < s->trigger_count; i++) TRY(write_trigger(w, cn, s->triggers[i]));
    return write_ser_end(w, key, start);
}

static sf_result_t read_state_map(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                  const sf_allocator_t *alloc, sf_ffxdlse_state_map_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_STATE_MAP, V_STATE_MAP, &start, &length));
    sf_ffxdlse_state_map_t *sm = NULL;
    TRY(create_state_map(alloc, &sm));
    int32_t count = 0;
    sf_result_t res = sf_binary_reader_read_i32(r, &count);
    if (res == SF_OK && count < 0) res = SF_ERR_INVALID_ARG;
    for (int32_t i = 0; res == SF_OK && i < count; i++) {
        res = grow_ptrs(alloc, (void ***)&sm->states, sm->count, &sm->cap);
        if (res == SF_OK) res = read_state(r, cn, alloc, &sm->states[sm->count]);
        if (res == SF_OK) sm->count++;
    }
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_state_map(sm); return res; }
    *out = sm;
    return SF_OK;
}

static sf_result_t write_state_map(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                   const sf_ffxdlse_state_map_t *sm) {
    if (!sm || sm->count > (size_t)INT32_MAX) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_STATE_MAP, V_STATE_MAP, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, (int32_t)sm->count));
    for (size_t i = 0; i < sm->count; i++) TRY(write_state(w, cn, sm->states[i]));
    return write_ser_end(w, key, start);
}

static sf_result_t read_resource_set(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                                     const sf_allocator_t *alloc, sf_ffxdlse_resource_set_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_RESOURCE_SET, V_RESOURCE_SET, &start, &length));
    sf_ffxdlse_resource_set_t *rs = NULL;
    TRY(create_resource_set(alloc, &rs));
    sf_result_t res = SF_OK;
    for (size_t i = 0; res == SF_OK && i < SF_FFXDLSE_RES_VECTOR_COUNT; i++) {
        res = read_dlvector(r, cn, alloc, &rs->vectors[i], &rs->counts[i]);
        rs->caps[i] = rs->counts[i];
    }
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_resource_set(rs); return res; }
    *out = rs;
    return SF_OK;
}

static sf_result_t write_resource_set(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                      const sf_ffxdlse_resource_set_t *rs) {
    if (!rs) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_RESOURCE_SET, V_RESOURCE_SET, &start, key, sizeof(key)));
    for (size_t i = 0; i < SF_FFXDLSE_RES_VECTOR_COUNT; i++) {
        TRY(write_dlvector(w, cn, rs->vectors[i], rs->counts[i]));
    }
    return write_ser_end(w, key, start);
}

static sf_result_t read_effect(sf_binary_reader_t *r, const ffx_class_names_t *cn,
                               const sf_allocator_t *alloc, sf_ffxdlse_effect_t **out) {
    int64_t start = 0;
    int32_t length = 0;
    TRY(read_ser_header(r, cn, CN_EFFECT, V_EFFECT, &start, &length));
    sf_ffxdlse_effect_t *e = NULL;
    TRY(create_effect(alloc, &e));
    destroy_param_list(e->param_list1); e->param_list1 = NULL;
    destroy_param_list(e->param_list2); e->param_list2 = NULL;
    destroy_state_map(e->state_map); e->state_map = NULL;
    destroy_resource_set(e->resource_set); e->resource_set = NULL;
    sf_result_t res = sf_binary_reader_assert_i32_one(r, 0);
    if (res == SF_OK) res = sf_binary_reader_read_i32(r, &e->id);
    if (res == SF_OK) res = sf_binary_reader_assert_i32_one(r, 0);
    if (res == SF_OK) res = sf_binary_reader_assert_i32_one(r, 0);
    if (res == SF_OK) res = sf_binary_reader_assert_i32_one(r, 2);
    if (res == SF_OK) res = sf_binary_reader_assert_i16_one(r, 0);
    if (res == SF_OK) res = sf_binary_reader_assert_i16_one(r, 2);
    if (res == SF_OK) res = sf_binary_reader_assert_i32_one(r, 0);
    if (res == SF_OK) res = read_param_list(r, cn, alloc, &e->param_list1);
    if (res == SF_OK) res = read_param_list(r, cn, alloc, &e->param_list2);
    if (res == SF_OK) res = read_state_map(r, cn, alloc, &e->state_map);
    if (res == SF_OK) res = read_resource_set(r, cn, alloc, &e->resource_set);
    if (res == SF_OK) res = sf_binary_reader_assert_u8_one(r, 0);
    if (res == SF_OK) res = verify_end(r, start, length);
    if (res != SF_OK) { destroy_effect(e); return res; }
    *out = e;
    return SF_OK;
}

static sf_result_t write_effect(sf_binary_writer_t *w, const ffx_class_names_t *cn,
                                const sf_ffxdlse_effect_t *e) {
    if (!e) return SF_ERR_INVALID_ARG;
    int64_t start = 0;
    char key[48];
    TRY(write_ser_begin(w, cn, CN_EFFECT, V_EFFECT, &start, key, sizeof(key)));
    TRY(sf_binary_writer_write_i32(w, 0));
    TRY(sf_binary_writer_write_i32(w, e->id));
    TRY(sf_binary_writer_write_i32(w, 0));
    TRY(sf_binary_writer_write_i32(w, 0));
    TRY(sf_binary_writer_write_i32(w, 2));
    TRY(sf_binary_writer_write_i16(w, 0));
    TRY(sf_binary_writer_write_i16(w, 2));
    TRY(sf_binary_writer_write_i32(w, 0));
    TRY(write_param_list(w, cn, e->param_list1));
    TRY(write_param_list(w, cn, e->param_list2));
    TRY(write_state_map(w, cn, e->state_map));
    TRY(write_resource_set(w, cn, e->resource_set));
    TRY(sf_binary_writer_write_u8(w, 0));
    return write_ser_end(w, key, start);
}

static sf_result_t collect_eval(ffx_class_names_t *cn, const sf_ffxdlse_evaluatable_t *ev) {
    if (!ev) return SF_ERR_INVALID_ARG;
    TRY(cn_add(cn, CN_EVAL));
    if (ev->operand) TRY(collect_eval(cn, ev->operand));
    if (ev->right) TRY(collect_eval(cn, ev->right));
    if (ev->left) TRY(collect_eval(cn, ev->left));
    return SF_OK;
}

static sf_result_t collect_param_list(ffx_class_names_t *cn, const sf_ffxdlse_param_list_t *pl) {
    TRY(cn_add(cn, CN_PARAM_LIST));
    if (pl && pl->count > 0) TRY(cn_add(cn, CN_PARAM));
    return SF_OK;
}

static sf_result_t collect_classes(const sf_ffxdlse_t *ffx, ffx_class_names_t *cn) {
    TRY(cn_copy(cn, &ffx->class_names));
    const sf_ffxdlse_effect_t *e = ffx->effect;
    TRY(cn_add(cn, CN_EFFECT));
    TRY(cn_add(cn, CN_DLVECTOR));
    TRY(collect_param_list(cn, e->param_list1));
    TRY(collect_param_list(cn, e->param_list2));
    TRY(cn_add(cn, CN_STATE_MAP));
    for (size_t i = 0; i < e->state_map->count; i++) {
        const sf_ffxdlse_state_t *s = e->state_map->states[i];
        TRY(cn_add(cn, CN_STATE));
        for (size_t j = 0; j < s->action_count; j++) {
            TRY(cn_add(cn, CN_ACTION));
            TRY(collect_param_list(cn, s->actions[j]->param_list));
        }
        for (size_t j = 0; j < s->trigger_count; j++) {
            TRY(cn_add(cn, CN_TRIGGER));
            TRY(collect_eval(cn, s->triggers[j]->evaluator));
        }
    }
    TRY(cn_add(cn, CN_RESOURCE_SET));
    TRY(cn_add(cn, CN_DLVECTOR));
    return SF_OK;
}

SF_API sf_result_t sf_ffxdlse_create(sf_ffxdlse_t **out, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    alloc = sf_alloc_or_default(alloc);
    sf_ffxdlse_t *ffx = (sf_ffxdlse_t *)sf_xalloc(alloc, sizeof(*ffx));
    if (!ffx) return SF_ERR_OOM;
    ffx->alloc = alloc;
    cn_init(&ffx->class_names, alloc);
    ffx->effect = NULL;
    sf_result_t r = create_effect(alloc, &ffx->effect);
    if (r != SF_OK) { sf_ffxdlse_destroy(ffx); return r; }
    *out = ffx;
    return SF_OK;
}

SF_API void sf_ffxdlse_destroy(sf_ffxdlse_t *ffx) {
    if (!ffx) return;
    destroy_effect(ffx->effect);
    cn_destroy(&ffx->class_names);
    sf_xfree(ffx->alloc, ffx);
}

SF_API bool sf_ffxdlse_is(const void *bytes, size_t size) {
    return bytes && size >= 4u && memcmp(bytes, "DLsE", 4u) == 0;
}

SF_API sf_result_t sf_ffxdlse_read_from_memory(sf_ffxdlse_t **out, const void *bytes,
                                               size_t size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    SF_CHECK_ARG(bytes != NULL || size == 0);
    if (!sf_ffxdlse_is(bytes, size)) return SF_ERR_BAD_MAGIC;
    alloc = sf_alloc_or_default(alloc);
    sf_istream_t *is = NULL;
    sf_binary_reader_t *br = NULL;
    sf_ffxdlse_t *ffx = NULL;
    sf_result_t r = sf_istream_open_memory(&is, bytes, size, alloc);
    if (r == SF_OK) r = sf_binary_reader_create(&br, is, false, alloc);
    if (r == SF_OK) r = sf_ffxdlse_create(&ffx, alloc);
    if (r == SF_OK) {
        TRY(sf_binary_reader_assert_u8_one(br, (uint8_t)'D'));
        TRY(sf_binary_reader_assert_u8_one(br, (uint8_t)'L'));
        TRY(sf_binary_reader_assert_u8_one(br, (uint8_t)'s'));
        TRY(sf_binary_reader_assert_u8_one(br, (uint8_t)'E'));
    }
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 1);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 3);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_u8_one(br, 0);
    if (r == SF_OK) r = sf_binary_reader_assert_i32_one(br, 1);
    int16_t class_count = 0;
    if (r == SF_OK) r = sf_binary_reader_read_i16(br, &class_count);
    if (r == SF_OK && class_count < 0) r = SF_ERR_INVALID_ARG;
    for (int16_t i = 0; r == SF_OK && i < class_count; i++) {
        int32_t len = 0;
        char *name = NULL;
        r = sf_binary_reader_read_i32(br, &len);
        if (r == SF_OK && len < 0) r = SF_ERR_INVALID_ARG;
        if (r == SF_OK) r = sf_binary_reader_read_ascii_n(br, (size_t)len, &name, NULL);
        if (r == SF_OK) r = cn_append_owned(&ffx->class_names, name);
        if (r != SF_OK) sf_xfree(alloc, name);
    }
    if (r == SF_OK) {
        destroy_effect(ffx->effect);
        ffx->effect = NULL;
        r = read_effect(br, &ffx->class_names, alloc, &ffx->effect);
    }
    if (br) sf_binary_reader_destroy(br);
    if (is) sf_istream_close(is);
    if (r != SF_OK) { sf_ffxdlse_destroy(ffx); return r; }
    *out = ffx;
    return SF_OK;
}

SF_API sf_result_t sf_ffxdlse_write_to_memory(const sf_ffxdlse_t *ffx, void **out_bytes,
                                              size_t *out_size, const sf_allocator_t *alloc) {
    SF_CHECK_ARG(ffx != NULL);
    SF_CHECK_ARG(out_bytes != NULL);
    SF_CHECK_ARG(out_size != NULL);
    alloc = sf_alloc_or_default(alloc);
    ffx_class_names_t cn;
    cn_init(&cn, alloc);
    sf_result_t r = collect_classes(ffx, &cn);
    sf_ostream_t *os = NULL;
    sf_binary_writer_t *bw = NULL;
    uint8_t *bytes = NULL;
    size_t size = 0;
    if (r == SF_OK) r = sf_ostream_open_memory(&os, alloc);
    if (r == SF_OK) r = sf_binary_writer_create(&bw, os, false, alloc);
    if (r == SF_OK) r = sf_binary_writer_write_bytes(bw, "DLsE", 4);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 1);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 3);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_u8(bw, 0);
    if (r == SF_OK) r = sf_binary_writer_write_i32(bw, 1);
    if (r == SF_OK && cn.count > (size_t)INT16_MAX) r = SF_ERR_OUT_OF_RANGE;
    if (r == SF_OK) r = sf_binary_writer_write_i16(bw, (int16_t)cn.count);
    for (size_t i = 0; r == SF_OK && i < cn.count; i++) {
        size_t len = strlen(cn.names[i]);
        if (len > (size_t)INT32_MAX) { r = SF_ERR_OUT_OF_RANGE; break; }
        r = sf_binary_writer_write_i32(bw, (int32_t)len);
        if (r == SF_OK) r = sf_binary_writer_write_ascii(bw, cn.names[i], false);
    }
    if (r == SF_OK) r = write_effect(bw, &cn, ffx->effect);
    if (r == SF_OK) r = sf_binary_writer_finish_bytes(bw, &bytes, &size);
    else if (bw) sf_binary_writer_destroy(bw);
    if (os) sf_ostream_close(os);
    cn_destroy(&cn);
    if (r != SF_OK) return r;
    *out_bytes = bytes;
    *out_size = size;
    return SF_OK;
}

SF_API sf_ffxdlse_effect_t *sf_ffxdlse_effect(const sf_ffxdlse_t *ffx) {
    return ffx ? ffx->effect : NULL;
}

SF_API int32_t sf_ffxdlse_effect_id(const sf_ffxdlse_effect_t *e) { return e ? e->id : 0; }
SF_API void sf_ffxdlse_effect_set_id(sf_ffxdlse_effect_t *e, int32_t id) { if (e) e->id = id; }
SF_API sf_ffxdlse_param_list_t *sf_ffxdlse_effect_param_list1(const sf_ffxdlse_effect_t *e) { return e ? e->param_list1 : NULL; }
SF_API sf_ffxdlse_param_list_t *sf_ffxdlse_effect_param_list2(const sf_ffxdlse_effect_t *e) { return e ? e->param_list2 : NULL; }
SF_API sf_ffxdlse_state_map_t *sf_ffxdlse_effect_state_map(const sf_ffxdlse_effect_t *e) { return e ? e->state_map : NULL; }
SF_API sf_ffxdlse_resource_set_t *sf_ffxdlse_effect_resource_set(const sf_ffxdlse_effect_t *e) { return e ? e->resource_set : NULL; }

SF_API int32_t sf_ffxdlse_param_list_unk04(const sf_ffxdlse_param_list_t *pl) { return pl ? pl->unk04 : 0; }
SF_API void sf_ffxdlse_param_list_set_unk04(sf_ffxdlse_param_list_t *pl, int32_t v) { if (pl) pl->unk04 = v; }
SF_API size_t sf_ffxdlse_param_list_count(const sf_ffxdlse_param_list_t *pl) { return pl ? pl->count : 0; }
SF_API sf_ffxdlse_param_t *sf_ffxdlse_param_list_at(const sf_ffxdlse_param_list_t *pl, size_t i) { return pl && i < pl->count ? pl->params[i] : NULL; }
SF_API sf_ffxdlse_param_type_t sf_ffxdlse_param_type(const sf_ffxdlse_param_t *p) { return p ? p->type : (sf_ffxdlse_param_type_t)0; }

SF_API size_t sf_ffxdlse_state_map_count(const sf_ffxdlse_state_map_t *sm) { return sm ? sm->count : 0; }
SF_API sf_ffxdlse_state_t *sf_ffxdlse_state_map_at(const sf_ffxdlse_state_map_t *sm, size_t i) { return sm && i < sm->count ? sm->states[i] : NULL; }
SF_API sf_result_t sf_ffxdlse_state_map_add(sf_ffxdlse_state_map_t *sm, sf_ffxdlse_state_t **out) {
    SF_CHECK_ARG(sm != NULL);
    TRY(grow_ptrs(sm->alloc, (void ***)&sm->states, sm->count, &sm->cap));
    sf_ffxdlse_state_t *s = NULL;
    TRY(create_state(sm->alloc, &s));
    sm->states[sm->count++] = s;
    if (out) *out = s;
    return SF_OK;
}

SF_API size_t sf_ffxdlse_state_action_count(const sf_ffxdlse_state_t *s) { return s ? s->action_count : 0; }
SF_API size_t sf_ffxdlse_state_trigger_count(const sf_ffxdlse_state_t *s) { return s ? s->trigger_count : 0; }
SF_API sf_ffxdlse_action_t *sf_ffxdlse_state_action_at(const sf_ffxdlse_state_t *s, size_t i) { return s && i < s->action_count ? s->actions[i] : NULL; }
SF_API sf_ffxdlse_trigger_t *sf_ffxdlse_state_trigger_at(const sf_ffxdlse_state_t *s, size_t i) { return s && i < s->trigger_count ? s->triggers[i] : NULL; }
SF_API sf_result_t sf_ffxdlse_state_add_action(sf_ffxdlse_state_t *s, sf_ffxdlse_action_t **out) {
    SF_CHECK_ARG(s != NULL);
    TRY(grow_ptrs(s->alloc, (void ***)&s->actions, s->action_count, &s->action_cap));
    sf_ffxdlse_action_t *a = NULL;
    TRY(create_action(s->alloc, &a));
    s->actions[s->action_count++] = a;
    if (out) *out = a;
    return SF_OK;
}
SF_API sf_result_t sf_ffxdlse_state_add_trigger(sf_ffxdlse_state_t *s, sf_ffxdlse_trigger_t **out) {
    SF_CHECK_ARG(s != NULL);
    TRY(grow_ptrs(s->alloc, (void ***)&s->triggers, s->trigger_count, &s->trigger_cap));
    sf_ffxdlse_trigger_t *t = NULL;
    TRY(create_trigger(s->alloc, &t));
    s->triggers[s->trigger_count++] = t;
    if (out) *out = t;
    return SF_OK;
}

SF_API int32_t sf_ffxdlse_action_id(const sf_ffxdlse_action_t *a) { return a ? a->id : 0; }
SF_API void sf_ffxdlse_action_set_id(sf_ffxdlse_action_t *a, int32_t id) { if (a) a->id = id; }
SF_API sf_ffxdlse_param_list_t *sf_ffxdlse_action_param_list(const sf_ffxdlse_action_t *a) { return a ? a->param_list : NULL; }

SF_API int32_t sf_ffxdlse_trigger_state_index(const sf_ffxdlse_trigger_t *t) { return t ? t->state_index : 0; }
SF_API void sf_ffxdlse_trigger_set_state_index(sf_ffxdlse_trigger_t *t, int32_t v) { if (t) t->state_index = v; }
SF_API sf_ffxdlse_evaluatable_t *sf_ffxdlse_trigger_evaluator(const sf_ffxdlse_trigger_t *t) { return t ? t->evaluator : NULL; }
SF_API sf_result_t sf_ffxdlse_trigger_set_evaluator(sf_ffxdlse_trigger_t *t, sf_ffxdlse_evaluatable_t *ev) {
    SF_CHECK_ARG(t != NULL);
    SF_CHECK_ARG(ev != NULL);
    destroy_evaluatable(t->evaluator);
    t->evaluator = ev;
    return SF_OK;
}

SF_API sf_result_t sf_ffxdlse_resource_set_count(const sf_ffxdlse_resource_set_t *rs,
                                                 sf_ffxdlse_resource_vector_t which,
                                                 size_t *out_count) {
    SF_CHECK_ARG(rs != NULL);
    SF_CHECK_ARG(out_count != NULL);
    if (which < 0 || which >= SF_FFXDLSE_RES_VECTOR_COUNT) return SF_ERR_OUT_OF_RANGE;
    *out_count = rs->counts[which];
    return SF_OK;
}
SF_API sf_result_t sf_ffxdlse_resource_set_at(const sf_ffxdlse_resource_set_t *rs,
                                              sf_ffxdlse_resource_vector_t which,
                                              size_t index, int32_t *out_value) {
    SF_CHECK_ARG(rs != NULL);
    SF_CHECK_ARG(out_value != NULL);
    if (which < 0 || which >= SF_FFXDLSE_RES_VECTOR_COUNT) return SF_ERR_OUT_OF_RANGE;
    if (index >= rs->counts[which]) return SF_ERR_OUT_OF_RANGE;
    *out_value = rs->vectors[which][index];
    return SF_OK;
}
SF_API sf_result_t sf_ffxdlse_resource_set_add(sf_ffxdlse_resource_set_t *rs,
                                               sf_ffxdlse_resource_vector_t which,
                                               int32_t value) {
    SF_CHECK_ARG(rs != NULL);
    if (which < 0 || which >= SF_FFXDLSE_RES_VECTOR_COUNT) return SF_ERR_OUT_OF_RANGE;
    TRY(grow_i32(rs->alloc, &rs->vectors[which], rs->counts[which], &rs->caps[which]));
    rs->vectors[which][rs->counts[which]++] = value;
    return SF_OK;
}

SF_API sf_ffxdlse_evaluatable_opcode_t sf_ffxdlse_evaluatable_opcode(
    const sf_ffxdlse_evaluatable_t *ev) {
    return ev ? ev->opcode : (sf_ffxdlse_evaluatable_opcode_t)0;
}
SF_API void sf_ffxdlse_evaluatable_destroy(sf_ffxdlse_evaluatable_t *ev) { destroy_evaluatable(ev); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_constant(sf_ffxdlse_evaluatable_t **out,
                                                          int32_t value,
                                                          const sf_allocator_t *alloc) {
    SF_CHECK_ARG(out != NULL);
    TRY(create_eval(out, SF_FFXDLSE_EVAL_CONSTANT, alloc));
    (*out)->value = value;
    return SF_OK;
}
SF_API sf_result_t sf_ffxdlse_evaluatable_create_current_tick(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_CURRENT_TICK, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_total_tick(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_TOTAL_TICK, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_child_exists(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_CHILD_EXISTS, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_parent_exists(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_PARENT_EXISTS, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_distance_from_camera(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_DISTANCE_FROM_CAMERA, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_create_emitters_stopped(sf_ffxdlse_evaluatable_t **out, const sf_allocator_t *alloc) { SF_CHECK_ARG(out != NULL); return create_eval(out, SF_FFXDLSE_EVAL_EMITTERS_STOPPED, alloc); }
SF_API sf_result_t sf_ffxdlse_evaluatable_constant_value(const sf_ffxdlse_evaluatable_t *ev,
                                                         int32_t *out_value) {
    SF_CHECK_ARG(ev != NULL);
    SF_CHECK_ARG(out_value != NULL);
    if (ev->opcode != SF_FFXDLSE_EVAL_CONSTANT) return SF_ERR_INVALID_ARG;
    *out_value = ev->value;
    return SF_OK;
}
