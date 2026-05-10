/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 1 QA — sf_path_hash matches upstream HashHelper.FromPathHash.
 */

#include "souls_formats/sf_hash.h"

#include "unity.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/*  Golden values were computed in Python with the algorithm transcribed from
 *  upstream HashHelper.FromPathHash:
 *
 *      h = path.lower().replace('\\','/')
 *      if not h.startswith('/'): h = '/' + h
 *      acc = 0
 *      for c in h: acc = (acc * 37 + ord(c)) & 0xFFFFFFFF
 *      return acc
 */
typedef struct {
    const char *path;
    uint32_t    expected;
} hash_case_t;

static const hash_case_t k_cases[] = {
    /*  Empty + minimal forms. */
    { "",                                              0x0000002fu },
    { "/",                                             0x0000002fu },
    { "a",                                             0x0000072cu },
    { "/a",                                            0x0000072cu },
    { "\\a",                                           0x0000072cu },

    /*  Case-folding. */
    { "Abc",                                           0x002668d9u },

    /*  All three forms (lower, upper-mixed, backslash) hash identically. */
    { "/chr/c0000.chrbnd",                             0xbb893839u },
    { "/CHR/c0000.ChrBnd",                             0xbb893839u },
    { "\\chr\\c0000.chrbnd",                           0xbb893839u },

    /*  Real Elden Ring asset paths used by Phase 3+ e2e tests. */
    { "/N:/GR/data/Param/Item.param",                  0x9bb75e55u },
    { "/event/m60_42_36_00.emevd.dcx",                 0xff7abc6bu },
    { "/map/mapstudio/m60_42_36_00.msb.dcx",           0x2fcc2a59u },
    { "/material/allmaterial.matbinbnd.dcx",           0xbe91a793u },
    { "/msg/engus/item.msgbnd.dcx",                    0x624f014fu },
    { "/sfx/sfxbnd_commoneffects.ffxbnd.dcx",          0xcfc5176bu },
    { "/chr/c0000.anibnd.dcx",                         0xf8630fb1u },
};

static void test_golden_values(void) {
    for (size_t i = 0; i < sizeof(k_cases) / sizeof(k_cases[0]); i++) {
        uint32_t got = sf_path_hash(k_cases[i].path);
        char msg[160];
        snprintf(msg, sizeof(msg), "case %zu: \"%s\" → 0x%08x (got 0x%08x)",
                 i, k_cases[i].path, k_cases[i].expected, got);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(k_cases[i].expected, got, msg);
    }
}

static void test_null_input(void) {
    /*  NULL is treated as empty string per upstream behaviour. */
    TEST_ASSERT_EQUAL_HEX32(0x0000002fu, sf_path_hash(NULL));
}

static void test_case_folding_only_ascii(void) {
    /*  "/AbCdEf" should fold to "/abcdef". */
    uint32_t low = sf_path_hash("/abcdef");
    uint32_t mix = sf_path_hash("/AbCdEf");
    uint32_t up  = sf_path_hash("/ABCDEF");
    TEST_ASSERT_EQUAL_HEX32(low, mix);
    TEST_ASSERT_EQUAL_HEX32(low, up);
}

static void test_slash_normalization(void) {
    /*  Backslash and forward slash hash the same. */
    uint32_t fwd = sf_path_hash("/foo/bar/baz");
    uint32_t bwd = sf_path_hash("\\foo\\bar\\baz");
    uint32_t mix = sf_path_hash("/foo\\bar/baz");
    TEST_ASSERT_EQUAL_HEX32(fwd, bwd);
    TEST_ASSERT_EQUAL_HEX32(fwd, mix);
}

static void test_leading_slash_inserted(void) {
    /*  "foo" should hash same as "/foo". */
    TEST_ASSERT_EQUAL_HEX32(sf_path_hash("/foo"), sf_path_hash("foo"));
    TEST_ASSERT_EQUAL_HEX32(sf_path_hash("/x"),   sf_path_hash("x"));
}

typedef struct {
    uint32_t candidate;
    bool     expected;
    const char *label;
} prime_case_t;

static const prime_case_t k_prime_cases[] = {
    /*  Boundary: < 2 is non-prime by definition. */
    { 0u,         false, "0 is not prime" },
    { 1u,         false, "1 is not prime" },
    /*  The only even prime. */
    { 2u,         true,  "2 is prime" },
    /*  Small odd primes. */
    { 3u,         true,  "3 is prime" },
    { 5u,         true,  "5 is prime" },
    { 7u,         true,  "7 is prime" },
    { 11u,        true,  "11 is prime" },
    { 13u,        true,  "13 is prime" },
    { 17u,        true,  "17 is prime" },
    { 19u,        true,  "19 is prime" },
    /*  Composites — even and odd. */
    { 4u,         false, "4 = 2*2" },
    { 6u,         false, "6 = 2*3" },
    { 8u,         false, "8 = 2^3" },
    { 9u,         false, "9 = 3*3" },
    { 15u,        false, "15 = 3*5" },
    { 21u,        false, "21 = 3*7" },
    { 25u,        false, "25 = 5*5" },
    { 49u,        false, "49 = 7*7" },
    /*  Larger primes used by hash-table sizing. */
    { 65537u,     true,  "65537 = F4 Fermat prime" },
    { 100003u,    true,  "100003 prime" },
    /*  Mersenne-adjacent composite. */
    { 65535u,     false, "65535 = 3*5*17*257" },
};

static void test_is_prime(void) {
    for (size_t i = 0; i < sizeof(k_prime_cases) / sizeof(k_prime_cases[0]); i++) {
        bool got = sf_is_prime(k_prime_cases[i].candidate);
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "case %zu: sf_is_prime(%u) expected %s, got %s — %s",
                 i, k_prime_cases[i].candidate,
                 k_prime_cases[i].expected ? "true" : "false",
                 got ? "true" : "false",
                 k_prime_cases[i].label);
        TEST_ASSERT_EQUAL_MESSAGE((int)k_prime_cases[i].expected, (int)got, msg);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_golden_values);
    RUN_TEST(test_null_input);
    RUN_TEST(test_case_folding_only_ascii);
    RUN_TEST(test_slash_normalization);
    RUN_TEST(test_leading_slash_inserted);
    RUN_TEST(test_is_prime);
    return UNITY_END();
}
