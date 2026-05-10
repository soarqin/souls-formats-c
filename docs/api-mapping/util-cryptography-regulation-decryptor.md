# RegulationDecryptor — API Mapping

**Upstream files**: `SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs`
**Pinned commit**: `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`

| Upstream signature | Upstream loc (file:line) | Kind | Our API | Status | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `public enum RegulationKey { DarkSouls3 = 0, EldenRing = 1, ArmoredCore6 = 2, EldenRingNightreign = 3 }` | `RegulationDecryptor.cs:16` | enum | `sf_regulation_key_t` | ✓ aligned | C exposes the same four ordinals: `SF_REGULATION_KEY_DARK_SOULS_3 = 0`, `SF_REGULATION_KEY_ELDEN_RING = 1`, `SF_REGULATION_KEY_ARMORED_CORE_6 = 2`, `SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN = 3`. |
| `private static readonly Dictionary<RegulationKey, byte[]> RegulationKeyDictionary` | `RegulationDecryptor.cs:40` | field | `_skipped_` | _skipped_ | Internal key table; raw key bytes are never exposed by the C API. The four 256-bit keys live in `src/crypto/regulation_keys.c` and are looked up via the internal `sfi_regulation_key()` helper. |
| `public static BND4 DecryptDS3Regulation(string path)` | `RegulationDecryptor.cs:51` | static method | `sf_regulation_decrypt_ds3` | ~ partial | Phase 2 byte-buffer overload only: `sf_result_t sf_regulation_decrypt_ds3(const uint8_t *in, size_t in_size, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc)`. Forwards to `sf_regulation_decrypt(..., SF_REGULATION_KEY_DARK_SOULS_3, ...)`. The BND4-aware path overload (`sf_regulation_decrypt_ds3_bnd4`) lands in Phase 3 once `sf_bnd4_t` is public. |
| `public static BND4 DecryptERRegulation(string path)` | `RegulationDecryptor.cs:59` | static method | `sf_regulation_decrypt_er` | ~ partial | Byte-buffer overload only; forwards to `sf_regulation_decrypt(..., SF_REGULATION_KEY_ELDEN_RING, ...)`. BND4 overload deferred to Phase 3. |
| `public static BND4 DecryptAC6Regulation(string path)` | `RegulationDecryptor.cs:67` | static method | `sf_regulation_decrypt_ac6` | ~ partial | Byte-buffer overload only; forwards to `sf_regulation_decrypt(..., SF_REGULATION_KEY_ARMORED_CORE_6, ...)`. BND4 overload deferred to Phase 3. |
| `public static BND4 DecryptERNRRegulation(string path)` | `RegulationDecryptor.cs:75` | static method | `sf_regulation_decrypt_ernr` | ~ partial | Byte-buffer overload only; forwards to `sf_regulation_decrypt(..., SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN, ...)`. BND4 overload deferred to Phase 3. |
| `public static BND4 DecryptBndWithKey(string path, RegulationKey key)` | `RegulationDecryptor.cs:83` | static method | `sf_regulation_decrypt` | ~ partial | Byte-buffer overload: `sf_result_t sf_regulation_decrypt(const uint8_t *in, size_t in_size, sf_regulation_key_t key, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc)`. Reads the leading 16-byte IV plus AES-256-CBC ciphertext (no padding, zero-padded to a 16-byte multiple) and returns the recovered plaintext. The BND4 deserialization side of upstream `DecryptBndWithKey` runs through `sf_bnd4_read()` once Phase 3 ships. |
| `public static void EncryptDS3Regulation(string path, BND4 bnd)` | `RegulationDecryptor.cs:93` | static method | `sf_regulation_encrypt_ds3` | ~ partial | Byte-buffer overload only: `sf_result_t sf_regulation_encrypt_ds3(const uint8_t *in, size_t in_size, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc)`. Forwards to `sf_regulation_encrypt(..., SF_REGULATION_KEY_DARK_SOULS_3, ...)`. BND4 overload deferred to Phase 3. |
| `public static void EncryptERRegulation(string path, BND4 bnd)` | `RegulationDecryptor.cs:101` | static method | `sf_regulation_encrypt_er` | ~ partial | Byte-buffer overload only; forwards to `sf_regulation_encrypt(..., SF_REGULATION_KEY_ELDEN_RING, ...)`. BND4 overload deferred to Phase 3. |
| `public static void EncryptAC6Regulation(string path, BND4 bnd)` | `RegulationDecryptor.cs:109` | static method | `sf_regulation_encrypt_ac6` | ~ partial | Byte-buffer overload only; forwards to `sf_regulation_encrypt(..., SF_REGULATION_KEY_ARMORED_CORE_6, ...)`. BND4 overload deferred to Phase 3. |
| `public static void EncryptERNRRegulation(string path, BND4 bnd)` | `RegulationDecryptor.cs:117` | static method | `sf_regulation_encrypt_ernr` | ✗ deviation | **Upstream quirk**: `EncryptERNRRegulation` calls `EncryptRegulationWithKey(path, bnd, RegulationKey.EldenRing)` — note `EldenRing`, not `EldenRingNightreign`. The C wrapper `sf_regulation_encrypt_ernr` faithfully mirrors this bit-identical behavior, forwarding to `sf_regulation_encrypt(..., SF_REGULATION_KEY_ELDEN_RING, ...)`. This is documented as a deviation only because the wrapper name suggests Nightreign, while the underlying key is the base Elden Ring key. Callers who want the true Nightreign key must call `sf_regulation_encrypt(..., SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN, ...)` directly. |
| `public static void EncryptRegulationWithKey(string path, BND4 bnd, RegulationKey key)` | `RegulationDecryptor.cs:125` | static method | `sf_regulation_encrypt` | ~ partial | Byte-buffer overload: `sf_result_t sf_regulation_encrypt(const uint8_t *in, size_t in_size, sf_regulation_key_t key, uint8_t **out, size_t *out_size, const sf_allocator_t *alloc)`. Generates a 16-byte zero IV, applies AES-256-CBC with PKCS#7 padding, and prepends the IV. The BND4 serialization side of upstream `EncryptRegulationWithKey` runs through `sf_bnd4_write()` once Phase 3 ships. |
| `private static byte[] EncryptByteArray(RegulationKey key, byte[] secret)` | `RegulationDecryptor.cs:133` | private method | `_skipped_` | _skipped_ | Implementation detail of `sf_regulation_encrypt`; uses Win32 BCrypt AES-256-CBC with PKCS#7 padding via `sfi_aes_encrypt_cbc()`. Not exposed publicly. |
| `private static byte[] DecryptByteArray(RegulationKey key, byte[] secret)` | `RegulationDecryptor.cs:162` | private method | `_skipped_` | _skipped_ | Implementation detail of `sf_regulation_decrypt`; uses Win32 BCrypt AES-256-CBC with no padding (zero-padded to 16-byte multiple) via `sfi_aes_decrypt_cbc()`. Not exposed publicly. |

## Notes on the Nightreign quirk

Upstream `EncryptERNRRegulation` (RegulationDecryptor.cs:117-120) invokes
`EncryptRegulationWithKey(path, bnd, RegulationKey.EldenRing)` — that is, it
encrypts ER Nightreign regulations with the **Elden Ring** key, not the
Nightreign key. This is presumably because the shipping ER:NR regulation
binaries were re-encrypted with the ER key during development.

The C port mirrors this bit-identically: `sf_regulation_encrypt_ernr()`
forwards to the generic `sf_regulation_encrypt()` with
`SF_REGULATION_KEY_ELDEN_RING`, **not** with
`SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN`. This preserves byte-level
compatibility with files produced by upstream tooling (e.g. WitchyBND,
modding pipelines using SoulsFormatsNEXT). Callers who want the true
Nightreign key must call the generic `sf_regulation_encrypt()` directly with
`SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN`.

In contrast, **decryption** wrappers all forward to their matching key:
`sf_regulation_decrypt_ernr()` correctly uses
`SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN`, matching upstream
`DecryptERNRRegulation`.

## Phase 3 follow-up

Once Phase 3 ships and `sf_bnd4_t` becomes a public type, additional BND4-
aware overloads will be added:

* `sf_regulation_decrypt_*_bnd4(const wchar_t *path, sf_bnd4_t **out, ...)`
* `sf_regulation_encrypt_*_bnd4(const wchar_t *path, const sf_bnd4_t *bnd, ...)`

These will mirror upstream's full `string path -> BND4` and
`string path, BND4 bnd -> void` signatures end-to-end. The byte-buffer
overloads documented above remain the lower-level primitive and continue to
be used by the BND4 overloads internally.
