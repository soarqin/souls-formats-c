#include "esd_bytecode.h"
#include "souls_formats/sf_io.h"
#include "internal/sf_internal.h"
#include <limits.h>
#include <string.h>

typedef struct sf_esd_bytecode_buffer {
    uint8_t *bytes;
    size_t size;
    size_t capacity;
    const sf_allocator_t *alloc;
} sf_esd_bytecode_buffer_t;

static sf_result_t esd_bytecode_buffer_reserve(sf_esd_bytecode_buffer_t *buffer,
                                               size_t additional) {
    if (additional > SIZE_MAX - buffer->size) return SF_ERR_OUT_OF_RANGE;
    size_t needed = buffer->size + additional;
    if (needed <= buffer->capacity) return SF_OK;

    size_t new_capacity = buffer->capacity ? buffer->capacity : 32;
    while (new_capacity < needed) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = needed;
            break;
        }
        new_capacity *= 2;
    }

    uint8_t *new_bytes = sf_xalloc(buffer->alloc, new_capacity);
    if (!new_bytes) return SF_ERR_OOM;
    if (buffer->size > 0) memcpy(new_bytes, buffer->bytes, buffer->size);
    sf_xfree(buffer->alloc, buffer->bytes);
    buffer->bytes = new_bytes;
    buffer->capacity = new_capacity;
    return SF_OK;
}

static sf_result_t esd_bytecode_buffer_write(sf_esd_bytecode_buffer_t *buffer,
                                             const void *bytes, size_t size) {
    sf_result_t result = esd_bytecode_buffer_reserve(buffer, size);
    if (result != SF_OK) return result;
    if (size > 0) memcpy(buffer->bytes + buffer->size, bytes, size);
    buffer->size += size;
    return SF_OK;
}

static sf_result_t esd_bytecode_buffer_write_u8(sf_esd_bytecode_buffer_t *buffer, uint8_t value) {
    return esd_bytecode_buffer_write(buffer, &value, sizeof(value));
}

static sf_result_t esd_bytecode_encode_node(const sf_esd_bytecode_node_t *node,
                                            sf_esd_bytecode_buffer_t *buffer) {
    SF_CHECK_ARG(node != NULL && buffer != NULL);

    if (node->opcode == SF_ESD_OP_UNKNOWN) {
        return esd_bytecode_buffer_write(buffer, node->raw_bytes, node->raw_size);
    }

    sf_result_t result = esd_bytecode_buffer_write_u8(buffer, (uint8_t)node->opcode);
    if (result != SF_OK) return result;

    switch (node->opcode) {
        case SF_ESD_OP_PUSH_INT:
            result = esd_bytecode_buffer_write(buffer, &node->int_value, sizeof(node->int_value));
            break;
        case SF_ESD_OP_CALL:
            result = esd_bytecode_buffer_write(buffer, &node->call_id, sizeof(node->call_id));
            break;
        case SF_ESD_OP_PUSH_FLOAT:
            result = esd_bytecode_buffer_write(buffer, &node->float_value, sizeof(node->float_value));
            break;
        case SF_ESD_OP_LOAD_REG:
            result = esd_bytecode_buffer_write(buffer, &node->reg_index, sizeof(node->reg_index));
            break;
        default:
            break;
    }
    if (result != SF_OK) return result;

    for (int32_t i = 0; i < node->child_count; i++) {
        result = esd_bytecode_encode_node(node->children[i], buffer);
        if (result != SF_OK) return result;
    }
    return SF_OK;
}

sf_result_t sf_esd_bytecode_decode(const uint8_t *bytes, size_t size,
                                   sf_esd_bytecode_tree_t **out_tree,
                                   const sf_allocator_t *alloc) {
    if (!bytes || !out_tree) {
        return SF_ERR_INVALID_ARG;
    }

    alloc = sf_alloc_or_default(alloc);

    sf_esd_bytecode_tree_t *tree = sf_xalloc(alloc, sizeof(sf_esd_bytecode_tree_t));
    if (!tree) {
        return SF_ERR_OOM;
    }
    memset(tree, 0, sizeof(*tree));
    tree->alloc = alloc;
    tree->root = sf_xalloc(alloc, sizeof(sf_esd_bytecode_node_t));
    if (!tree->root) {
        sf_xfree(alloc, tree);
        return SF_ERR_OOM;
    }
    memset(tree->root, 0, sizeof(sf_esd_bytecode_node_t));

    size_t capacity = 16;
    tree->root->children = sf_xalloc(alloc, capacity * sizeof(sf_esd_bytecode_node_t *));
    if (!tree->root->children) {
        sf_xfree(alloc, tree->root);
        sf_xfree(alloc, tree);
        return SF_ERR_OOM;
    }

    size_t offset = 0;
    sf_result_t result = SF_OK;

    while (offset < size) {
        if ((size_t)tree->root->child_count >= capacity) {
            size_t new_capacity = capacity * 2;
            sf_esd_bytecode_node_t **new_children = sf_xalloc(alloc, new_capacity * sizeof(sf_esd_bytecode_node_t *));
            if (!new_children) {
                result = SF_ERR_OOM;
                break;
            }
            memcpy(new_children, tree->root->children, tree->root->child_count * sizeof(sf_esd_bytecode_node_t *));
            sf_xfree(alloc, tree->root->children);
            tree->root->children = new_children;
            capacity = new_capacity;
        }

        sf_esd_bytecode_node_t *node = sf_xalloc(alloc, sizeof(sf_esd_bytecode_node_t));
        if (!node) {
            result = SF_ERR_OOM;
            break;
        }
        memset(node, 0, sizeof(sf_esd_bytecode_node_t));
        tree->root->children[tree->root->child_count++] = node;

        uint8_t op = bytes[offset++];
        node->opcode = (sf_esd_opcode_t)op;

        switch (op) {
            case SF_ESD_OP_END:
            case SF_ESD_OP_NEGATE:
            case SF_ESD_OP_NOT:
            case SF_ESD_OP_ABS:
            case SF_ESD_OP_ADD:
            case SF_ESD_OP_SUB:
            case SF_ESD_OP_MUL:
            case SF_ESD_OP_DIV:
            case SF_ESD_OP_MOD:
            case SF_ESD_OP_EQ:
            case SF_ESD_OP_NEQ:
            case SF_ESD_OP_GT:
            case SF_ESD_OP_GE:
            case SF_ESD_OP_LT:
            case SF_ESD_OP_LE:
            case SF_ESD_OP_AND:
            case SF_ESD_OP_OR:
                break;

            case SF_ESD_OP_PUSH_INT:
                if (offset + 4 > size) {
                    result = SF_ERR_TRUNCATED;
                    goto cleanup;
                }
                memcpy(&node->int_value, bytes + offset, 4);
                offset += 4;
                break;

            case SF_ESD_OP_CALL:
                if (offset + 2 > size) {
                    result = SF_ERR_TRUNCATED;
                    goto cleanup;
                }
                memcpy(&node->call_id, bytes + offset, 2);
                offset += 2;
                break;

            case SF_ESD_OP_PUSH_FLOAT:
                if (offset + 4 > size) {
                    result = SF_ERR_TRUNCATED;
                    goto cleanup;
                }
                memcpy(&node->float_value, bytes + offset, 4);
                offset += 4;
                break;

            case SF_ESD_OP_LOAD_REG:
                if (offset + 4 > size) {
                    result = SF_ERR_TRUNCATED;
                    goto cleanup;
                }
                memcpy(&node->reg_index, bytes + offset, 4);
                offset += 4;
                break;

            default:
                node->opcode = SF_ESD_OP_UNKNOWN;
                node->raw_size = size - offset + 1;
                node->raw_bytes = sf_xalloc(alloc, node->raw_size);
                if (!node->raw_bytes) {
                    result = SF_ERR_OOM;
                    goto cleanup;
                }
                node->raw_bytes[0] = op;
                if (node->raw_size > 1) {
                    memcpy(node->raw_bytes + 1, bytes + offset, node->raw_size - 1);
                }
                offset = size;
                break;
        }
    }

cleanup:
    if (result != SF_OK) {
        sf_esd_bytecode_tree_destroy(tree, alloc);
    } else {
        tree->node_count = tree->root->child_count;
        if (tree->node_count > 0) {
            tree->nodes = sf_xalloc(alloc, (size_t)tree->node_count * sizeof(*tree->nodes));
            if (!tree->nodes) {
                sf_esd_bytecode_tree_destroy(tree, alloc);
                return SF_ERR_OOM;
            }
            for (int32_t i = 0; i < tree->node_count; i++) {
                tree->nodes[i] = *tree->root->children[i];
            }
        }
        *out_tree = tree;
    }
    return result;
}

static void sf_esd_bytecode_node_destroy(sf_esd_bytecode_node_t *node,
                                         const sf_allocator_t *alloc) {
    if (!node) return;
    for (int32_t i = 0; i < node->child_count; i++) {
        sf_esd_bytecode_node_destroy(node->children[i], alloc);
    }
    sf_xfree(alloc, node->children);
    sf_xfree(alloc, node->raw_bytes);
    sf_xfree(alloc, node);
}

void sf_esd_bytecode_tree_destroy(sf_esd_bytecode_tree_t *tree, const sf_allocator_t *alloc) {
    if (!tree) return;
    alloc = sf_alloc_or_default(alloc);
    sf_esd_bytecode_node_destroy(tree->root, alloc);
    sf_xfree(alloc, tree->nodes);
    sf_xfree(alloc, tree);
}

sf_result_t sf_esd_bytecode_encode(const sf_esd_bytecode_tree_t *tree,
                                    uint8_t **out_bytes, size_t *out_size,
                                    const sf_allocator_t *alloc) {
    SF_CHECK_ARG(tree != NULL && tree->root != NULL && out_bytes != NULL && out_size != NULL);
    *out_bytes = NULL;
    *out_size = 0;
    alloc = sf_alloc_or_default(alloc ? alloc : tree->alloc);

    sf_esd_bytecode_buffer_t buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.alloc = alloc;

    sf_result_t result = SF_OK;
    if (tree->node_count > 0 && tree->nodes != NULL) {
        for (int32_t i = 0; i < tree->node_count; i++) {
            result = esd_bytecode_encode_node(&tree->nodes[i], &buffer);
            if (result != SF_OK) break;
        }
    } else {
        for (int32_t i = 0; i < tree->root->child_count; i++) {
            result = esd_bytecode_encode_node(tree->root->children[i], &buffer);
            if (result != SF_OK) break;
        }
    }

    if (result != SF_OK) {
        sf_xfree(alloc, buffer.bytes);
        return result;
    }

    *out_bytes = buffer.bytes;
    *out_size = buffer.size;
    return SF_OK;
}
