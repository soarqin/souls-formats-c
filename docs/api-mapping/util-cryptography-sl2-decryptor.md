# SL2Decryptor — API Mapping

**Upstream files**: `SoulsFormats/Utilities/Cryptography/SL2Decryptor.cs`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

`SL2Decryptor` exposes the byte-level AES-CBC + MD5 envelope used by DS2 /
DS2 Scholar of the First Sin / DS3 save files (`.sl2`). The C port
mirrors the upstream API one-to-one with the standard out-param /
allocator / `sf_result_t` adaptations.

| Upstream signature | Upstream loc | Kind | Our API | Status | Notes |
|---|---|---|---|---|---|
| `static byte[] ds2SaveKey = { 0xB7, 0xFD, 0x46, 0x3E, 0x4A, 0x9C, 0x11, 0x02, 0xDF, 0x17, 0x39, 0xE5, 0xF3, 0xB2, 0xA5, 0x0F }` | `SL2Decryptor.cs:12` | private static field | `g_sl2_ds2_key[16]` (file-static in `src/crypto/sl2.c`) | `✓ aligned` | Identical 16-byte key constant. |
| `static byte[] scholarSaveKey = { 0x59, 0x9F, 0x9B, 0x69, 0x96, 0x40, 0xA5, 0x52, 0x36, 0xEE, 0x2D, 0x70, 0x83, 0x5E, 0xC7, 0x44 }` | `SL2Decryptor.cs:13` | private static field | `g_sl2_scholar_key[16]` (file-static in `src/crypto/sl2.c`) | `✓ aligned` | Identical 16-byte key constant. |
| `static byte[] ds3SaveKey = { 0xFD, 0x46, 0x4D, 0x69, 0x5E, 0x69, 0xA3, 0x9A, 0x10, 0xE3, 0x19, 0xA7, 0xAC, 0xE8, 0xB7, 0xFA }` | `SL2Decryptor.cs:14` | private static field | `g_sl2_ds3_key[16]` (file-static in `src/crypto/sl2.c`) | `✓ aligned` | Identical 16-byte key constant. |
| `static byte[] GetDS2SaveKey()` | `SL2Decryptor.cs:19` | static method | `sf_sl2_get_ds2_key(const uint8_t **out_key_16) → sf_result_t` | `✗ deviation` | C-style adaptation: returns a borrowed pointer to a static 16-byte buffer instead of a heap-allocated copy. Callers MUST NOT free the pointer. See `docs/api-mapping/POLICY.md` §"Borrowed pointers". |
| `static byte[] GetScholarSaveKey()` | `SL2Decryptor.cs:27` | static method | `sf_sl2_get_scholar_key(const uint8_t **out_key_16) → sf_result_t` | `✗ deviation` | Same as above. |
| `static byte[] GetDS3SaveKey()` | `SL2Decryptor.cs:35` | static method | `sf_sl2_get_ds3_key(const uint8_t **out_key_16) → sf_result_t` | `✗ deviation` | Same as above. |
| `static byte[] DecryptSL2File(byte[] encrypted, byte[] key)` | `SL2Decryptor.cs:43` | static method | `sf_sl2_decrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc) → sf_result_t` | `✓ aligned` | Skips the leading 16-byte hash (documentation-only in upstream), reads next 16 bytes as IV, AES-128-CBC-decrypts the remainder with `PaddingMode.None`. Out-param ownership: `*out` is heap-owned by the caller and freed with `sf_free(alloc, *out)`. |
| `static byte[] EncryptSL2File(byte[] decrypted, byte[] key)` | `SL2Decryptor.cs:76` | static method | `sf_sl2_encrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc) → sf_result_t` | `✓ aligned` | Generates a random 16-byte IV via `BCryptGenRandom`, AES-128-CBC-encrypts the input with `PaddingMode.None`, then prepends the MD5 hash of `IV || ciphertext` (32-byte envelope header). Layout: `[0..16) = MD5; [16..32) = IV; [32..) = ciphertext`. Out-param ownership: heap-owned. |

## Constraints enforced by the C API

* `sf_sl2_decrypt`: `in_size` MUST be at least 32 (16-byte hash + 16-byte
  IV) AND `(in_size - 32)` MUST be a multiple of 16. Returns
  `SF_ERR_INVALID_ARG` otherwise.
* `sf_sl2_encrypt`: `in_size` MUST be a multiple of 16 (AES block size,
  matching upstream's `PaddingMode.None`). Returns `SF_ERR_INVALID_ARG`
  otherwise.
* All five entry points return `SF_ERR_INVALID_ARG` on `NULL` pointer
  inputs (other than `alloc`, where `NULL` selects the default
  malloc/free allocator per project convention).
* The leading MD5 hash is **not** verified by `sf_sl2_decrypt` — this
  matches upstream behaviour (the C# implementation also leaves the
  hash check up to the caller, and the official save layout uses the
  hash as integrity rather than authentication).

## Synthetic test coverage

`tests/crypto/test_sl2_kat.c` exercises:

1. **Key bytes KAT** — `sf_sl2_get_ds2_key` returns the exact 16 bytes
   listed at `SL2Decryptor.cs:12`. (Spot-check; the other two getters
   are exercised via the round-trip cases below.)
2. **DS2 round-trip** — encrypt + decrypt of a synthetic 256-byte
   payload using the DS2 key.
3. **Scholar round-trip** — same with the Scholar key.
4. **DS3 round-trip** — same with the DS3 key.
5. **Argument validation** — non-multiple-of-16 input lengths are
   rejected.

Real `.sl2` save files are **not** committed to the repository per
project policy; only synthetic fixtures are used.
