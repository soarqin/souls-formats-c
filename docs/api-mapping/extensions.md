# API Extensions

This document tracks symbols and features in `souls-formats-c` that have no direct counterpart in the upstream `SoulsFormatsNEXT` library. These extensions are typically added for C-idiomatic memory management, performance optimizations, or platform-specific requirements.

| Symbol | Module | Rationale | Lifecycle | Source loc |
| :--- | :--- | :--- | :--- | :--- |
| `sf_dcx_unwrap` | `sf_dcx.h` | Convenience to probe+decompress in one call | stable | `include/souls_formats/sf_dcx.h` |
| `sf_ostream_detach_buffer` | `sf_io.h` | Lets caller steal the write buffer without copying | stable | `include/souls_formats/sf_io.h` |
| `sf_oodle_set_search_path` | `sf_oodle.h` | Override DLL search path (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_load` | `sf_oodle.h` | Explicit load trigger (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_unload` | `sf_oodle.h` | Explicit unload (no upstream equivalent) | stable | `include/souls_formats/sf_oodle.h` |
| `sf_oodle_version` | `sf_oodle.h` | Query currently loaded Oodle DLL major version | stable | `include/souls_formats/sf_oodle.h` |
| `sf_istream_t + sf_ostream_t dual-layer stream` | `sf_io.h` | Layered abstraction over Win32 handles + memory; upstream BinaryReaderEx wraps a .NET Stream directly | stable | `include/souls_formats/sf_io.h` |
| `sf_path_hash_64` | `sf_hash.h` | Extension. Upstream `HashHelper.FromPathHash` returns `uint` (32-bit). BHD5 ER+ stores hash as `UInt64` on disk via implicit cast (`BHD5.cs:474`). We expose a named 64-bit wrapper for consumer clarity. Same algorithm, no functional divergence — just a widening cast. | stable | `include/souls_formats/sf_hash.h` |
| `sfi_dds_parse_header` | extension | Upstream `DDS.cs` is `_skipped_` (full pixel decoder out of scope); we add minimal header-only reader to derive TPF Texture metadata without depending on full DDS class. | internal | `src/internal/dds_header.h` |
| `sfi_rsa_decrypt_pkcs1` | extension | Upstream `BHD5.Read` punts on RSA: "Must already be decrypted, if applicable." (`BHD5.cs:105`). We integrate a Win32 CNG raw-RSA wrapper (BCryptEncrypt + BCRYPT_PAD_NONE on the public key) so the BHD5 reader can consume on-disk encrypted `.bhd` directly. PEM input accepts both X.509 SPKI and PKCS#1 RSAPublicKey forms; the four shipped game keys are PKCS#1. Internal-only — never exported via DLL. | internal | `src/crypto/rsa_cng.h` |
| `sf_bhd5_game_t` (4 v1 values) | extension | Upstream enum (`BHD5.Game`) covers DarkSouls1 / DarkSoulsRemastered / DarkSouls2 / DarkSouls3 / EldenRing. v1 of this library targets only Sekiro / EldenRing / Nightreign / ArmoredCore6, so `sf_bhd5_game_t` enumerates exactly those four. DS1/DS1R/DS2/DS3 are deferred to v2+ per project charter (`legacy.md`). | stable | `include/souls_formats/sf_bhd5.h` |
