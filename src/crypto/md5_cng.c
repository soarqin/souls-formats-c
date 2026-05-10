/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "crypto/md5_cng.h"
#include "internal/sf_internal.h"

#include <windows.h>
#include <bcrypt.h>

sf_result_t sfi_md5_hash(const void *data, size_t size, uint8_t out[16]) {
    SF_CHECK_ARG((data || size == 0u) && out);
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_MD5_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(st)) return SF_ERR_CRYPTO;
    st = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
    if (BCRYPT_SUCCESS(st) && size > 0u) {
        st = BCryptHashData(hash, (PUCHAR)data, (ULONG)size, 0);
    }
    if (BCRYPT_SUCCESS(st)) st = BCryptFinishHash(hash, out, 16, 0);
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return BCRYPT_SUCCESS(st) ? SF_OK : SF_ERR_CRYPTO;
}
