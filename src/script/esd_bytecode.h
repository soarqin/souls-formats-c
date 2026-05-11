#pragma once

#include "souls_formats/sf_common.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sf_esd_opcode {
    SF_ESD_OP_UNKNOWN    = -1,
    SF_ESD_OP_END        = 0x00,
    SF_ESD_OP_PUSH_INT   = 0x01,
    SF_ESD_OP_CALL       = 0x05,
    SF_ESD_OP_NEGATE     = 0x06,
    SF_ESD_OP_NOT        = 0x07,
    SF_ESD_OP_ABS        = 0x08,
    SF_ESD_OP_ADD        = 0x0A,
    SF_ESD_OP_SUB        = 0x0B,
    SF_ESD_OP_MUL        = 0x0C,
    SF_ESD_OP_DIV        = 0x0D,
    SF_ESD_OP_MOD        = 0x0E,
    SF_ESD_OP_EQ         = 0x23,
    SF_ESD_OP_NEQ        = 0x24,
    SF_ESD_OP_GT         = 0x25,
    SF_ESD_OP_GE         = 0x26,
    SF_ESD_OP_LT         = 0x27,
    SF_ESD_OP_LE         = 0x28,
    SF_ESD_OP_AND        = 0x40,
    SF_ESD_OP_OR         = 0x41,
    SF_ESD_OP_PUSH_FLOAT = 0x48,
    SF_ESD_OP_LOAD_REG   = 0x7C,
} sf_esd_opcode_t;

typedef struct sf_esd_bytecode_node {
    sf_esd_opcode_t opcode;
    int32_t  int_value;
    float    float_value;
    int16_t  call_id;
    int32_t  reg_index;
    uint8_t *raw_bytes;
    size_t   raw_size;
    int32_t  child_count;
    struct sf_esd_bytecode_node **children;
} sf_esd_bytecode_node_t;

typedef struct sf_esd_bytecode_tree {
    sf_esd_bytecode_node_t *root;
    int32_t node_count;
    sf_esd_bytecode_node_t *nodes;
    const sf_allocator_t *alloc;
} sf_esd_bytecode_tree_t;

sf_result_t sf_esd_bytecode_decode(const uint8_t *bytes, size_t size,
                                    sf_esd_bytecode_tree_t **out_tree,
                                    const sf_allocator_t *alloc);

sf_result_t sf_esd_bytecode_encode(const sf_esd_bytecode_tree_t *tree,
                                    uint8_t **out_bytes, size_t *out_size,
                                    const sf_allocator_t *alloc);

void sf_esd_bytecode_tree_destroy(sf_esd_bytecode_tree_t *tree, const sf_allocator_t *alloc);

#ifdef __cplusplus
}
#endif
