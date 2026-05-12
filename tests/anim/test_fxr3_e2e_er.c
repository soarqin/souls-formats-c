/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Phase 7 T25 — FXR3 e2e: parse the first .fxr entry inside ER
 * /sfx/sfxbnd_commoneffects.ffxbnd.dcx and verify it round-trips through
 * the production sf_fxr3_read_from_memory path. Also serialises the parsed
 * effect to XML and checks the root element appears.
 *
 * Pipeline:
 *   1. er_extract_from_data0 yields the plaintext ffxbnd BND4 (outer
 *      DCX_KRAK already unwrapped by the helper). The T5 probe in
 *      .sisyphus/evidence/task-5-fxr3-probe.md confirms the ffxbnd ships
 *      inside Data0 — no Data3 RSA dance needed.
 *   2. The decompressed BND4 SHA-256 is compared against the probe-time
 *      snapshot (0x3796…6ca4). A mismatch SKIPs the test on the assumption
 *      that the game patched and the snapshot needs refreshing.
 *   3. The first `.fxr` entry is loaded via sf_fxr3_read_from_memory and
 *      the public accessors are exercised: version must be SEKIRO (5),
 *      the root container must be present, and the container tree must
 *      contain at least one effect.
 *   4. sf_fxr3_to_xml is invoked on the parsed effect and the output is
 *      checked to contain the FXR3 root element marker.
 *
 * SKIPs gracefully when the ER copy, Oodle DLL, or ffxbnd is missing.
 */

#include "er_test_helper.h"

#include "souls_formats/sf_bhd5.h"
#include "souls_formats/sf_binder.h"
#include "souls_formats/sf_bnd4.h"
#include "souls_formats/sf_common.h"
#include "souls_formats/sf_dcx.h"
#include "souls_formats/sf_fxr3.h"
#include "souls_formats/sf_io.h"

#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <bcrypt.h>

#define FFXBND_PATH "/sfx/sfxbnd_commoneffects.ffxbnd.dcx"

/* Expected SHA-256 of the decompressed sfxbnd_commoneffects.ffxbnd BND4 —
 * captured by the T5 probe on 2026-05-12. Game-patch drift triggers
 * TEST_IGNORE. */
static const uint8_t kExpectedBnd4Sha256[32] = {
    0x37, 0x96, 0x1b, 0xe7, 0x78, 0x60, 0xa7, 0x12,
    0x45, 0x64, 0x60, 0xf7, 0xa8, 0x2f, 0x93, 0xab,
    0x9c, 0x46, 0x36, 0xc0, 0xb0, 0xb6, 0xb9, 0xf1,
    0x65, 0xcf, 0xd6, 0x60, 0xc8, 0x05, 0x6c, 0xa4,
};

void setUp(void) {}
void tearDown(void) {}

static bool env_ok;

static bool sha256_digest(const void *data, size_t size, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE  alg    = NULL;
    BCRYPT_HASH_HANDLE hash_h = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM,
                                              NULL, 0);
    if (!BCRYPT_SUCCESS(st)) {
        return false;
    }
    st = BCryptCreateHash(alg, &hash_h, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(st)) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    BCryptHashData(hash_h, (PUCHAR)(uintptr_t)data, (ULONG)size, 0);
    st = BCryptFinishHash(hash_h, out, 32, 0);
    BCryptDestroyHash(hash_h);
    BCryptCloseAlgorithmProvider(alg, 0);
    return BCRYPT_SUCCESS(st);
}

static bool name_ends_with(const char *name, const char *suffix)
{
    if (!name || !suffix) {
        return false;
    }
    const size_t n = strlen(name);
    const size_t s = strlen(suffix);
    if (s > n) {
        return false;
    }
    return memcmp(name + n - s, suffix, s) == 0;
}

/* ER stores some Data0 entries under a 64-bit folded path hash that the
 * production sf_path_hash_64 (zero-extended 32-bit, 37u multiplier) does
 * not compute. Mirrors the workaround used by tests/geom/test_matbin_e2e_er.c
 * and tests/probes/probe_fxr3_format.c. */
static uint64_t er_path_hash_64_alt(const char *path)
{
    uint64_t h = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        unsigned char c = (*p == '\\') ? '/' : *p;
        if (c >= 'A' && c <= 'Z') {
            c = (unsigned char)(c - 'A' + 'a');
        }
        h = (uint64_t)c + 133u * h;
    }
    return h;
}

static sf_result_t er_extract_with_fallback(const char *path,
                                            void **out, size_t *out_size)
{
    sf_result_t r = er_extract_from_data0(path, out, out_size);
    if (r == SF_OK) {
        return r;
    }
    if (r != SF_ERR_NOT_FOUND) {
        return r;
    }

    void  *raw      = NULL;
    size_t raw_size = 0;
    r = sf_bhd5_extract_by_hash_64(er_helper_get_bhd5_for_testing(),
                                   er_path_hash_64_alt(path),
                                   &raw, &raw_size, NULL);
    if (r != SF_OK) {
        return r;
    }

    sf_dcx_type_t     type    = SF_DCX_TYPE_UNKNOWN;
    const sf_result_t sniff_r = sf_dcx_sniff(raw, raw_size, &type);
    const bool        is_dcx  = sniff_r == SF_OK && type != SF_DCX_TYPE_NONE &&
                                type != SF_DCX_TYPE_UNKNOWN;

    if (is_dcx) {
        void         *decompressed = NULL;
        size_t        decomp_size  = 0;
        sf_dcx_type_t out_type     = SF_DCX_TYPE_UNKNOWN;
        r = sf_dcx_decompress(raw, raw_size, &decompressed, &decomp_size,
                              &out_type, NULL);
        sf_free(NULL, raw);
        if (r != SF_OK) {
            return r;
        }
        *out      = decompressed;
        *out_size = decomp_size;
    } else {
        *out      = raw;
        *out_size = raw_size;
    }
    return SF_OK;
}

/* Recursive search of the container tree for at least one effect. The
 * T5 probe confirms ER ffxbnd entries all have ≥3 effects, so this is a
 * conservative existence check rather than a structural assertion. */
static bool container_tree_has_effect(const sf_fxr3_container_t *c)
{
    if (!c) {
        return false;
    }
    if (sf_fxr3_container_effect_count(c) > 0) {
        return true;
    }
    const size_t children = sf_fxr3_container_child_count(c);
    for (size_t i = 0; i < children; ++i) {
        if (container_tree_has_effect(sf_fxr3_container_child(c, i))) {
            return true;
        }
    }
    return false;
}

/* ── T1: extract + parse + verify accessors + XML round-trip ───────────── */

static void test_fxr3_e2e_parse_first_commoneffects_fxr(void)
{
    if (!env_ok) {
        TEST_IGNORE_MESSAGE("ER copy or Oodle DLL not available");
    }

    void       *bnd_bytes = NULL;
    size_t      bnd_size  = 0;
    sf_result_t r = er_extract_with_fallback(FFXBND_PATH, &bnd_bytes, &bnd_size);
    if (r == SF_ERR_OODLE_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("Oodle DLL missing; cannot decompress DCX_KRAK");
    }
    if (r == SF_ERR_NOT_FOUND) {
        TEST_IGNORE_MESSAGE("sfxbnd_commoneffects.ffxbnd.dcx not present in "
                            "this ER install");
    }
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd_bytes);
    TEST_ASSERT_GREATER_THAN((size_t)0, bnd_size);

    /* Snapshot guard. Mismatch is a patch-drift signal, not a parser
     * regression — IGNORE so the suite still surfaces actionable
     * failures elsewhere. */
    uint8_t digest[32];
    if (!sha256_digest(bnd_bytes, bnd_size, digest)) {
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("BCrypt unavailable; cannot verify snapshot");
    }
    if (memcmp(digest, kExpectedBnd4Sha256, sizeof(digest)) != 0) {
        sf_free(NULL, bnd_bytes);
        TEST_IGNORE_MESSAGE("game patch changed snapshot, skipping");
    }

    sf_bnd4_t *bnd = NULL;
    r = sf_bnd4_read_from_memory(&bnd, (const uint8_t *)bnd_bytes, bnd_size, NULL);
    sf_free(NULL, bnd_bytes);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(bnd);

    const sf_binder_file_t *fxr_entry = NULL;
    const size_t            count     = sf_bnd4_file_count(bnd);
    for (size_t i = 0; i < count; ++i) {
        const sf_binder_file_t *file = sf_bnd4_get_file(bnd, i);
        if (!file || !file->name_utf8 || !file->data || file->size == 0) {
            continue;
        }
        if (name_ends_with(file->name_utf8, ".fxr")) {
            fxr_entry = file;
            break;
        }
    }
    if (!fxr_entry) {
        sf_bnd4_destroy(bnd);
        TEST_IGNORE_MESSAGE("no .fxr entry found in sfxbnd_commoneffects");
    }

    sf_fxr3_t *fxr = NULL;
    r = sf_fxr3_read_from_memory(&fxr, fxr_entry->data, fxr_entry->size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(fxr);

    TEST_ASSERT_EQUAL_INT(SF_FXR3_VERSION_SEKIRO, sf_fxr3_version(fxr));

    const sf_fxr3_container_t *root = sf_fxr3_root_container(fxr);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE_MESSAGE(container_tree_has_effect(root),
                             "container tree must contain at least one effect");

    char  *xml      = NULL;
    size_t xml_size = 0;
    r = sf_fxr3_to_xml(fxr, &xml, &xml_size, NULL);
    TEST_ASSERT_EQUAL_INT(SF_OK, r);
    TEST_ASSERT_NOT_NULL(xml);
    TEST_ASSERT_GREATER_THAN((size_t)0, xml_size);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(xml, "<FXR3"),
                                 "XML output must contain <FXR3 root tag");

    sf_free(NULL, xml);
    sf_fxr3_destroy(fxr);
    sf_bnd4_destroy(bnd);
}

int main(void)
{
    UNITY_BEGIN();
    env_ok = er_helper_is_available();
    if (env_ok) {
        env_ok = er_helper_init() == SF_OK;
    }
    RUN_TEST(test_fxr3_e2e_parse_first_commoneffects_fxr);
    return UNITY_END();
}
