/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Raw RSA "decrypt" via Win32 CNG, used by BHD5 to unwrap sign-encrypted
 * archive headers. We accept both X.509 SubjectPublicKeyInfo PEMs (the
 * ubiquitous "BEGIN PUBLIC KEY" form) and PKCS#1 RSAPublicKey PEMs
 * (the "BEGIN RSA PUBLIC KEY" form used by every shipped game key).
 *
 * Strategy:
 *   1. Strip the PEM armor → DER bytes via CryptStringToBinaryA.
 *   2. Hand-parse the DER to extract (modulus n, publicExponent e).
 *      We do not call CryptDecodeObjectEx because the BHD5 game keys are
 *      PKCS#1 RSAPublicKey blobs, which CNG's standard decoders (X509
 *      SPKI / RSA_CSP_PUBLICKEYBLOB) do not natively accept; the modular
 *      ASN.1 we need is trivial enough to parse directly.
 *   3. Build a BCRYPT_RSAKEY_BLOB (header + e + n) and BCryptImportKeyPair
 *      with BCRYPT_RSAPUBLIC_BLOB.
 *   4. BCryptEncrypt with BCRYPT_PAD_NONE — raw modular exponentiation
 *      using the public key. In classical RSA terms the game has
 *      "signed" the archive header with its private key (s = m^d mod n)
 *      and we recover m by computing (s^e mod n), which on Win32 CNG is
 *      the public-key BCryptEncrypt primitive, not BCryptDecrypt.
 *   5. Strip any leading 0x00 padding bytes the result may carry.
 */

#include "crypto/rsa_cng.h"
#include "internal/sf_internal.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <string.h>

#ifndef BCRYPT_RSAPUBLIC_MAGIC
#define BCRYPT_RSAPUBLIC_MAGIC 0x31415352
#endif

typedef struct asn1_view {
    const uint8_t *p;
    size_t         n;
} asn1_view_t;

static int asn1_read_tag_len(asn1_view_t *v, uint8_t want_tag,
                             const uint8_t **content, size_t *content_len) {
    if (v->n < 2u) return 0;
    uint8_t tag = v->p[0];
    if (tag != want_tag) return 0;
    size_t  hdr = 2u;
    size_t  len = v->p[1];
    if (len & 0x80u) {
        size_t nlen = len & 0x7Fu;
        if (nlen == 0u || nlen > 4u || v->n < 2u + nlen) return 0;
        len = 0;
        for (size_t i = 0; i < nlen; i++) len = (len << 8) | v->p[2u + i];
        hdr = 2u + nlen;
    }
    if (v->n < hdr + len) return 0;
    *content     = v->p + hdr;
    *content_len = len;
    v->p += hdr + len;
    v->n -= hdr + len;
    return 1;
}

static int asn1_skip_integer_leading_zero(const uint8_t **p, size_t *n) {
    while (*n > 0u && (*p)[0] == 0x00u) { (*p)++; (*n)--; }
    return *n > 0u;
}

static int parse_rsa_public_key_pkcs1(const uint8_t *der, size_t der_len,
                                      const uint8_t **n_out, size_t *n_len_out,
                                      const uint8_t **e_out, size_t *e_len_out) {
    asn1_view_t v = { der, der_len };
    const uint8_t *seq; size_t seq_len;
    if (!asn1_read_tag_len(&v, 0x30u, &seq, &seq_len)) return 0;
    asn1_view_t inner = { seq, seq_len };
    const uint8_t *n; size_t nl;
    const uint8_t *e; size_t el;
    if (!asn1_read_tag_len(&inner, 0x02u, &n, &nl)) return 0;
    if (!asn1_read_tag_len(&inner, 0x02u, &e, &el)) return 0;
    if (!asn1_skip_integer_leading_zero(&n, &nl)) return 0;
    if (!asn1_skip_integer_leading_zero(&e, &el)) return 0;
    *n_out = n; *n_len_out = nl;
    *e_out = e; *e_len_out = el;
    return 1;
}

static int parse_rsa_public_key_x509(const uint8_t *der, size_t der_len,
                                     const uint8_t **n_out, size_t *n_len_out,
                                     const uint8_t **e_out, size_t *e_len_out) {
    asn1_view_t v = { der, der_len };
    const uint8_t *spki; size_t spki_len;
    if (!asn1_read_tag_len(&v, 0x30u, &spki, &spki_len)) return 0;
    asn1_view_t inner = { spki, spki_len };
    const uint8_t *alg_ignored; size_t alg_ignored_len;
    const uint8_t *bitstr;      size_t bitstr_len;
    if (!asn1_read_tag_len(&inner, 0x30u, &alg_ignored, &alg_ignored_len)) return 0;
    if (!asn1_read_tag_len(&inner, 0x03u, &bitstr, &bitstr_len)) return 0;
    if (bitstr_len < 1u || bitstr[0] != 0x00u) return 0;
    return parse_rsa_public_key_pkcs1(bitstr + 1u, bitstr_len - 1u,
                                      n_out, n_len_out, e_out, e_len_out);
}

static sf_result_t parse_pem_to_modulus_exponent(const char *pem,
                                                 const sf_allocator_t *alloc,
                                                 uint8_t **der_out, DWORD *der_len_out,
                                                 const uint8_t **n_out, size_t *n_len_out,
                                                 const uint8_t **e_out, size_t *e_len_out) {
    DWORD der_len = 0;
    if (!CryptStringToBinaryA(pem, 0, CRYPT_STRING_BASE64HEADER,
                              NULL, &der_len, NULL, NULL) || der_len == 0u) {
        return SF_ERR_CRYPTO;
    }
    uint8_t *der = (uint8_t *)sf_xalloc(alloc, der_len);
    if (!der) return SF_ERR_OOM;
    if (!CryptStringToBinaryA(pem, 0, CRYPT_STRING_BASE64HEADER,
                              der, &der_len, NULL, NULL)) {
        sf_xfree(alloc, der);
        return SF_ERR_CRYPTO;
    }
    if (parse_rsa_public_key_pkcs1(der, der_len, n_out, n_len_out, e_out, e_len_out) ||
        parse_rsa_public_key_x509 (der, der_len, n_out, n_len_out, e_out, e_len_out)) {
        *der_out     = der;
        *der_len_out = der_len;
        return SF_OK;
    }
    sf_xfree(alloc, der);
    return SF_ERR_CRYPTO;
}

static sf_result_t import_rsa_public_key(BCRYPT_ALG_HANDLE alg,
                                         const uint8_t *n, size_t n_len,
                                         const uint8_t *e, size_t e_len,
                                         const sf_allocator_t *alloc,
                                         BCRYPT_KEY_HANDLE *out_key) {
    size_t blob_size = sizeof(BCRYPT_RSAKEY_BLOB) + e_len + n_len;
    uint8_t *blob = (uint8_t *)sf_xalloc(alloc, blob_size);
    if (!blob) return SF_ERR_OOM;
    BCRYPT_RSAKEY_BLOB *hdr = (BCRYPT_RSAKEY_BLOB *)blob;
    hdr->Magic       = BCRYPT_RSAPUBLIC_MAGIC;
    hdr->BitLength   = (ULONG)(n_len * 8u);
    hdr->cbPublicExp = (ULONG)e_len;
    hdr->cbModulus   = (ULONG)n_len;
    hdr->cbPrime1    = 0;
    hdr->cbPrime2    = 0;
    memcpy(blob + sizeof(BCRYPT_RSAKEY_BLOB), e, e_len);
    memcpy(blob + sizeof(BCRYPT_RSAKEY_BLOB) + e_len, n, n_len);

    NTSTATUS st = BCryptImportKeyPair(alg, NULL, BCRYPT_RSAPUBLIC_BLOB,
                                      out_key, blob, (ULONG)blob_size, 0);
    sf_xfree(alloc, blob);
    return BCRYPT_SUCCESS(st) ? SF_OK : SF_ERR_CRYPTO;
}

sf_result_t sfi_rsa_decrypt_pkcs1(const char *pem_public_key,
                                  const uint8_t *in, size_t in_size,
                                  uint8_t **out, size_t *out_size,
                                  const sf_allocator_t *alloc) {
    SF_CHECK_ARG(pem_public_key && in && in_size > 0u && out && out_size);

    uint8_t *der = NULL;
    DWORD    der_len = 0;
    const uint8_t *n = NULL, *e = NULL;
    size_t   n_len = 0, e_len = 0;

    sf_result_t r = parse_pem_to_modulus_exponent(pem_public_key, alloc,
                                                   &der, &der_len,
                                                   &n, &n_len, &e, &e_len);
    if (r != SF_OK) return r;
    if (in_size != n_len) {
        sf_xfree(alloc, der);
        return SF_ERR_INVALID_ARG;
    }

    BCRYPT_ALG_HANDLE alg = NULL;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_RSA_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(st)) { sf_xfree(alloc, der); return SF_ERR_CRYPTO; }

    BCRYPT_KEY_HANDLE key = NULL;
    r = import_rsa_public_key(alg, n, n_len, e, e_len, alloc, &key);
    sf_xfree(alloc, der);
    if (r != SF_OK) { BCryptCloseAlgorithmProvider(alg, 0); return r; }

    uint8_t *raw = (uint8_t *)sf_xalloc(alloc, n_len);
    if (!raw) {
        BCryptDestroyKey(key);
        BCryptCloseAlgorithmProvider(alg, 0);
        return SF_ERR_OOM;
    }
    ULONG written = 0;
    st = BCryptEncrypt(key, (PUCHAR)in, (ULONG)in_size, NULL, NULL, 0,
                       raw, (ULONG)n_len, &written, BCRYPT_PAD_NONE);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!BCRYPT_SUCCESS(st) || written == 0u) {
        sf_xfree(alloc, raw);
        return SF_ERR_CRYPTO;
    }

    size_t lead = 0;
    while (lead < written && raw[lead] == 0x00u) lead++;
    size_t kept = (size_t)written - lead;
    if (kept == 0u) {
        sf_xfree(alloc, raw);
        return SF_ERR_CRYPTO;
    }
    uint8_t *trimmed = (uint8_t *)sf_xalloc(alloc, kept);
    if (!trimmed) {
        sf_xfree(alloc, raw);
        return SF_ERR_OOM;
    }
    memcpy(trimmed, raw + lead, kept);
    sf_xfree(alloc, raw);

    *out      = trimmed;
    *out_size = kept;
    return SF_OK;
}
