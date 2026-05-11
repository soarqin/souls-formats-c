#include "unity.h"
#include "script/esd_bytecode.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t k_simple_eval[] = {
    0x01, 0x42, 0x00, 0x00, 0x00,  /* PUSH_INT 0x42 */
    0x01, 0x10, 0x00, 0x00, 0x00,  /* PUSH_INT 0x10 */
    0x0A,                           /* ADD */
    0x00                            /* END */
};

static void test_esd_bytecode_decode_simple(void) {
    sf_esd_bytecode_tree_t *tree = NULL;
    sf_result_t res = sf_esd_bytecode_decode(k_simple_eval, sizeof(k_simple_eval), &tree, NULL);
    TEST_ASSERT_EQUAL(SF_OK, res);
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT_EQUAL(4, tree->node_count);

    TEST_ASSERT_EQUAL(SF_ESD_OP_PUSH_INT, tree->nodes[0].opcode);
    TEST_ASSERT_EQUAL(0x42, tree->nodes[0].int_value);

    TEST_ASSERT_EQUAL(SF_ESD_OP_PUSH_INT, tree->nodes[1].opcode);
    TEST_ASSERT_EQUAL(0x10, tree->nodes[1].int_value);

    TEST_ASSERT_EQUAL(SF_ESD_OP_ADD, tree->nodes[2].opcode);

    TEST_ASSERT_EQUAL(SF_ESD_OP_END, tree->nodes[3].opcode);

    sf_esd_bytecode_tree_destroy(tree, NULL);
}

static void test_esd_bytecode_decode_unknown(void) {
    uint8_t bytes[] = {
        0x01, 0x05, 0x00, 0x00, 0x00, /* PUSH_INT 5 */
        0x99, 0xAA, 0xBB,             /* UNKNOWN */
    };
    sf_esd_bytecode_tree_t *tree = NULL;
    sf_result_t res = sf_esd_bytecode_decode(bytes, sizeof(bytes), &tree, NULL);
    TEST_ASSERT_EQUAL(SF_OK, res);
    TEST_ASSERT_NOT_NULL(tree);
    TEST_ASSERT_EQUAL(2, tree->node_count);

    TEST_ASSERT_EQUAL(SF_ESD_OP_PUSH_INT, tree->nodes[0].opcode);
    TEST_ASSERT_EQUAL(5, tree->nodes[0].int_value);

    TEST_ASSERT_EQUAL(SF_ESD_OP_UNKNOWN, tree->nodes[1].opcode);
    TEST_ASSERT_EQUAL(3, tree->nodes[1].raw_size);
    TEST_ASSERT_EQUAL(0x99, tree->nodes[1].raw_bytes[0]);
    TEST_ASSERT_EQUAL(0xAA, tree->nodes[1].raw_bytes[1]);
    TEST_ASSERT_EQUAL(0xBB, tree->nodes[1].raw_bytes[2]);

    sf_esd_bytecode_tree_destroy(tree, NULL);
}

static void test_esd_bytecode_decode_truncated(void) {
    uint8_t bytes[] = {
        0x01, 0x05, 0x00 /* PUSH_INT truncated */
    };
    sf_esd_bytecode_tree_t *tree = NULL;
    sf_result_t res = sf_esd_bytecode_decode(bytes, sizeof(bytes), &tree, NULL);
    TEST_ASSERT_EQUAL(SF_ERR_TRUNCATED, res);
    TEST_ASSERT_NULL(tree);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esd_bytecode_decode_simple);
    RUN_TEST(test_esd_bytecode_decode_unknown);
    RUN_TEST(test_esd_bytecode_decode_truncated);
    return UNITY_END();
}
