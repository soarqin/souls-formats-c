/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "souls_formats/sf_kf4.h"
#include "souls_formats/sf_kuon.h"
#include "souls_formats/sf_mwc.h"
#include "souls_formats/sf_legacy_misc.h"
#include "souls_formats/sf_common.h"

#include "unity.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const uint8_t KF4_DAT_HEADER_EMPTY[0x40] = {
    0x00, 0x80, 0x04, 0x1E,
    0x00, 0x00, 0x00, 0x00,
};

static void test_kf4_dat_is_function(void) {
    TEST_ASSERT_TRUE(sf_kf4_dat_is(KF4_DAT_HEADER_EMPTY, sizeof(KF4_DAT_HEADER_EMPTY)));

    uint8_t bad_magic[0x40];
    memset(bad_magic, 0, sizeof(bad_magic));
    bad_magic[0] = 'F'; bad_magic[1] = 'O'; bad_magic[2] = 'O'; bad_magic[3] = '!';
    TEST_ASSERT_FALSE(sf_kf4_dat_is(bad_magic, sizeof(bad_magic)));

    const uint8_t too_short[] = {0x00, 0x80};
    TEST_ASSERT_FALSE(sf_kf4_dat_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_kf4_dat_is(NULL, 0));
}

static void test_kf4_dat_read_empty(void) {
    sf_kf4_dat_t *dat = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kf4_dat_read_from_memory(
        &dat, KF4_DAT_HEADER_EMPTY, sizeof(KF4_DAT_HEADER_EMPTY), NULL));
    TEST_ASSERT_NOT_NULL(dat);
    TEST_ASSERT_EQUAL_size_t(0, sf_kf4_dat_file_count(dat));
    sf_kf4_dat_destroy(dat);
}

static void test_kf4_dat_read_one_file(void) {
    uint8_t bytes[0x40 + 0x40 + 4];
    memset(bytes, 0, sizeof(bytes));

    bytes[0] = 0x00; bytes[1] = 0x80; bytes[2] = 0x04; bytes[3] = 0x1E;
    bytes[4] = 0x01; bytes[5] = 0x00; bytes[6] = 0x00; bytes[7] = 0x00;

    const char *name = "hello.bin";
    memcpy(&bytes[0x40], name, strlen(name));

    uint32_t fsize = 4u;
    memcpy(&bytes[0x40 + 0x34], &fsize, sizeof(fsize));
    uint32_t padded = 4u;
    memcpy(&bytes[0x40 + 0x38], &padded, sizeof(padded));
    uint32_t offset = 0x80u;
    memcpy(&bytes[0x40 + 0x3C], &offset, sizeof(offset));

    bytes[0x80] = 0xDE; bytes[0x81] = 0xAD; bytes[0x82] = 0xBE; bytes[0x83] = 0xEF;

    sf_kf4_dat_t *dat = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kf4_dat_read_from_memory(
        &dat, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(dat);
    TEST_ASSERT_EQUAL_size_t(1, sf_kf4_dat_file_count(dat));

    const char *got_name = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kf4_dat_get_file_name(dat, 0, &got_name));
    TEST_ASSERT_EQUAL_STRING("hello.bin", got_name);

    const uint8_t *data = NULL;
    size_t data_size = 0;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kf4_dat_get_file_data(dat, 0, &data, &data_size));
    TEST_ASSERT_EQUAL_size_t(4u, data_size);
    TEST_ASSERT_EQUAL_UINT8(0xDE, data[0]);
    TEST_ASSERT_EQUAL_UINT8(0xAD, data[1]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, data[2]);
    TEST_ASSERT_EQUAL_UINT8(0xEF, data[3]);

    sf_kf4_dat_destroy(dat);
}

static void test_kf4_om2_read(void) {
    uint8_t bytes[16];
    memset(bytes, 0, sizeof(bytes));
    uint32_t file_size = 0x12345678u;
    memcpy(bytes, &file_size, sizeof(file_size));

    sf_kf4_om2_t *om2 = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kf4_om2_read_from_memory(
        &om2, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(om2);
    TEST_ASSERT_EQUAL_INT32(0x12345678, sf_kf4_om2_file_size(om2));
    sf_kf4_om2_destroy(om2);
}

static void test_kuon_bnd_is_function(void) {
    const uint8_t valid[16] = {
        'B', 'N', 'D', 0x00,
        0xC8, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    TEST_ASSERT_TRUE(sf_kuon_bnd_is(valid, sizeof(valid)));

    const uint8_t bad_magic[16] = { 'X', 'X', 'X', 'X' };
    TEST_ASSERT_FALSE(sf_kuon_bnd_is(bad_magic, sizeof(bad_magic)));

    const uint8_t too_short[] = { 'B', 'N', 'D', 0x00 };
    TEST_ASSERT_FALSE(sf_kuon_bnd_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_kuon_bnd_is(NULL, 0));
}

static void test_kuon_bnd_read_empty(void) {
    const uint8_t bytes[16] = {
        'B', 'N', 'D', 0x00,
        0xC8, 0x00, 0x00, 0x00,
        0x10, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    sf_kuon_bnd_t *bnd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_kuon_bnd_read_from_memory(
        &bnd, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(bnd);
    TEST_ASSERT_EQUAL_INT32(200, sf_kuon_bnd_file_version(bnd));
    TEST_ASSERT_EQUAL_size_t(0, sf_kuon_bnd_file_count(bnd));
    sf_kuon_bnd_destroy(bnd);
}

static void test_mwc_mmd_is_function(void) {
    const uint8_t valid[] = { 'M', 'M', 'D', 0x00, 0x00, 0x00, 0x00, 0x00 };
    TEST_ASSERT_TRUE(sf_mwc_mmd_is(valid, sizeof(valid)));

    const uint8_t bad[] = { 'O', 'T', 'R', 0x00 };
    TEST_ASSERT_FALSE(sf_mwc_mmd_is(bad, sizeof(bad)));

    const uint8_t too_short[] = { 'M', 'M' };
    TEST_ASSERT_FALSE(sf_mwc_mmd_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_mwc_mmd_is(NULL, 0));
}

static void test_mwc_mmd_read(void) {
    const uint8_t bytes[] = { 'M', 'M', 'D', 0x00, 0x01, 0x02, 0x03, 0x04 };
    sf_mwc_mmd_t *mmd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mwc_mmd_read_from_memory(
        &mmd, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(mmd);
    sf_mwc_mmd_destroy(mmd);
}

static void test_mwc_otr_is_function(void) {
    const uint8_t valid[] = { 'O', 'T', 'R', 0x00, 0x00, 0x00, 0x00, 0x00 };
    TEST_ASSERT_TRUE(sf_mwc_otr_is(valid, sizeof(valid)));

    const uint8_t bad[] = { 'M', 'M', 'D', 0x00 };
    TEST_ASSERT_FALSE(sf_mwc_otr_is(bad, sizeof(bad)));

    TEST_ASSERT_FALSE(sf_mwc_otr_is(NULL, 0));
}

static void test_mwc_otr_read(void) {
    const uint8_t bytes[] = { 'O', 'T', 'R', 0x00, 0x01, 0x02, 0x03, 0x04 };
    sf_mwc_otr_t *otr = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mwc_otr_read_from_memory(
        &otr, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(otr);
    sf_mwc_otr_destroy(otr);
}

static void test_mgf_is_function(void) {
    const uint8_t valid[] = {
        'M', 'G', 'F', 'L',
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    TEST_ASSERT_TRUE(sf_mgf_is(valid, sizeof(valid)));

    const uint8_t bad[] = { 'M', 'G', 'F', 'X' };
    TEST_ASSERT_FALSE(sf_mgf_is(bad, sizeof(bad)));

    const uint8_t too_short[] = { 'M', 'G' };
    TEST_ASSERT_FALSE(sf_mgf_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_mgf_is(NULL, 0));
}

static void test_mgf_read_empty(void) {
    const uint8_t bytes[] = {
        'M', 'G', 'F', 'L',
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };

    sf_mgf_t *mgf = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_mgf_read_from_memory(
        &mgf, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(mgf);
    TEST_ASSERT_EQUAL_INT32(1, sf_mgf_unk04(mgf));
    TEST_ASSERT_EQUAL_size_t(0, sf_mgf_file_count(mgf));
    sf_mgf_destroy(mgf);
}

static void test_ddl_is_function(void) {
    const uint8_t valid[] = { 'D', 'D', 'L', 0x00, 0x00, 0x00, 0x00, 0x00 };
    TEST_ASSERT_TRUE(sf_ddl_is(valid, sizeof(valid)));

    const uint8_t bad[] = { 'D', 'D', 'L', 'X' };
    TEST_ASSERT_FALSE(sf_ddl_is(bad, sizeof(bad)));

    const uint8_t too_short[] = { 'D' };
    TEST_ASSERT_FALSE(sf_ddl_is(too_short, sizeof(too_short)));

    TEST_ASSERT_FALSE(sf_ddl_is(NULL, 0));
}

static void test_ddl_read(void) {
    const uint8_t bytes[] = { 'D', 'D', 'L', 0x00, 0x20, 0x4E, 0x00, 0x00 };
    sf_ddl_t *ddl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_OK, sf_ddl_read_from_memory(
        &ddl, bytes, sizeof(bytes), NULL));
    TEST_ASSERT_NOT_NULL(ddl);
    sf_ddl_destroy(ddl);
}

static void test_bad_magic_rejected(void) {
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'X'; buf[1] = 'X'; buf[2] = 'X'; buf[3] = 'X';

    sf_kf4_dat_t *dat = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_kf4_dat_read_from_memory(&dat, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(dat);

    sf_kuon_bnd_t *bnd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_kuon_bnd_read_from_memory(&bnd, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(bnd);

    sf_mwc_mmd_t *mmd = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_mwc_mmd_read_from_memory(&mmd, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(mmd);

    sf_mwc_otr_t *otr = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_mwc_otr_read_from_memory(&otr, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(otr);

    sf_mgf_t *mgf = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_mgf_read_from_memory(&mgf, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(mgf);

    sf_ddl_t *ddl = NULL;
    TEST_ASSERT_EQUAL_INT(SF_ERR_BAD_MAGIC,
        sf_ddl_read_from_memory(&ddl, buf, sizeof(buf), NULL));
    TEST_ASSERT_NULL(ddl);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_kf4_dat_is_function);
    RUN_TEST(test_kf4_dat_read_empty);
    RUN_TEST(test_kf4_dat_read_one_file);
    RUN_TEST(test_kf4_om2_read);
    RUN_TEST(test_kuon_bnd_is_function);
    RUN_TEST(test_kuon_bnd_read_empty);
    RUN_TEST(test_mwc_mmd_is_function);
    RUN_TEST(test_mwc_mmd_read);
    RUN_TEST(test_mwc_otr_is_function);
    RUN_TEST(test_mwc_otr_read);
    RUN_TEST(test_mgf_is_function);
    RUN_TEST(test_mgf_read_empty);
    RUN_TEST(test_ddl_is_function);
    RUN_TEST(test_ddl_read);
    RUN_TEST(test_bad_magic_rejected);
    return UNITY_END();
}
