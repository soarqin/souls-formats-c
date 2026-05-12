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
| `sfi_tpf_headerize` (PC arm only) | extension | Upstream `Headerizer.Headerize` (`Headerizer.cs:18`) covers Xbox360/Xbox1/PS3/PS4/PS5 swizzling + DDS reconstruction. v1 of this library targets PC FromSoftware games (Sekiro, Elden Ring, Nightreign, AC6); the PC arm degenerates to a copy. Calling `sfi_tpf_headerize` for any non-PC `sf_tpf_platform_t` returns `SF_ERR_UNSUPPORTED_VERSION`. Console arms deferred to v2+. | internal | `src/archive/tpf_headerizer.h` |
| TPF DDS minimal scope | extension | `sf_tpf_t` carries DDS/GNF/etc. payloads opaquely. Only the on-disk metadata that round-trips PC TPFs is preserved; per-platform `TexHeader` and `FloatStruct` blocks are skipped past on read and emitted as zeros on write. Console TPF round-trip is best-effort and out of v1 scope. | stable | `include/souls_formats/sf_tpf.h` |
| `sfi_rsa_decrypt_pkcs1` | extension | Upstream `BHD5.Read` punts on RSA: "Must already be decrypted, if applicable." (`BHD5.cs:105`). We integrate a Win32 CNG raw-RSA wrapper (BCryptEncrypt + BCRYPT_PAD_NONE on the public key) so the BHD5 reader can consume on-disk encrypted `.bhd` directly. PEM input accepts both X.509 SPKI and PKCS#1 RSAPublicKey forms; the four shipped game keys are PKCS#1. Internal-only — never exported via DLL. | internal | `src/crypto/rsa_cng.h` |
| `sf_bhd5_game_t` (4 v1 values) | extension | Upstream enum (`BHD5.Game`) covers DarkSouls1 / DarkSoulsRemastered / DarkSouls2 / DarkSouls3 / EldenRing. v1 of this library targets only Sekiro / EldenRing / Nightreign / ArmoredCore6, so `sf_bhd5_game_t` enumerates exactly those four. DS1/DS1R/DS2/DS3 are deferred to v2+ per project charter (`legacy.md`). | stable | `include/souls_formats/sf_bhd5.h` |
| `sf_reverse_bits_u8` | `sf_io.h` | Bit-reversal helper for binder format flags (upstream uses bit-big-endian reads) | stable | `include/souls_formats/sf_io.h` |
| `sf_bhd5_open` | `sf_bhd5.h` | Two-path API to open .bhd and .bdt together; upstream `BHD5.Read` takes a stream and punts on the .bdt | stable | `include/souls_formats/sf_bhd5.h` |
| `er_load_param` | test helper | Decrypt regulation.bin → BND4 → extract param by suffix match | stable | `tests/param/test_param_er.c` |
| `er_load_msgbnd_entry` | test helper | Extract msgbnd entry from Data0 → BND4 | stable | `tests/fmg/test_fmg_er.c` |
| `sf_param_apply_mode_t` | `sf_param.h` | 3-mode apply (UNCONDITIONAL/SOMEWHAT_CAREFUL/CAREFUL) folds 8 upstream Apply variants | stable | `include/souls_formats/sf_param.h` |
| `SF_EMEVD_FORMAT_ELDEN_RING/ARMORED_CORE_VI/NIGHTREIGN` | `sf_emevd.h` | Sekiro aliases for ER/AC6/Nightreign (pending probe confirmation) | stable | `include/souls_formats/sf_emevd.h` |
| `sf_paramdef_get_index` | `sf_paramdef.h` | Paramdex XML `<Index>` element; binary returns -1 | stable | `include/souls_formats/sf_paramdef.h` |
| `sf_paramdef_field_get_sort_id` | `sf_paramdef.h` | Paramdex XML `<SortID>` element; binary returns 0 | stable | `include/souls_formats/sf_paramdef.h` |

## Phase 6: Geometry + Material

### sf_flver2_decode_mesh
- **Type**: Extension (C-side only, no upstream counterpart)
- **Upstream Ref**: `Mesh.cs:244` (`GetFaces()`) — upstream only triangulates; does NOT decode vertex attributes
- **C API or behavior description**:
  ```c
  SF_API sf_result_t sf_flver2_decode_mesh(
      const sf_flver2_t *f, size_t mesh_index,
      sf_flver2_decoded_mesh_t *out, const sf_allocator_t *a);
  ```
- **Rationale**: C consumers need typed position/normal/uv/bone arrays; upstream C# users access Vertex fields via LINQ/List — no C equivalent idiom exists, must provide helper
- **Impact**: Consumers of flver2 library get easy access to decoded vertex data

### FLVER2 big-endian byte-order refusal
- **Type**: Functional divergence (C-side is stricter than upstream)
- **Upstream Ref**: `FLVER2.cs:95` — upstream reads `AssertASCII("L\0", "B\0")` and supports BE
- **C API or behavior description**: If byte at offset 0x06 is `'B'` (0x42) → immediately return `SF_ERR_UNSUPPORTED_VERSION`, do not parse further
- **Rationale**: v1 target games (Sekiro/Elden Ring/Nightreign/AC6) are all x86_64 LE; BE support is upstream compat for PS3-era games, deferred to v2 legacy phase
- **Impact**: Upstream BE FLVER2 files cannot be read; write always emits LE

### FLVER2 EdgeCompression flag refusal
- **Type**: Functional divergence
- **Upstream Ref**: `FaceSet.cs:19` — FSFlags enum includes EdgeCompressed; upstream silently drops it on write
- **C API or behavior description**: Read: if FaceSet.Flags includes EdgeCompressed → `SF_ERR_UNSUPPORTED_VERSION`. Write: does not accept any Edge-related input
- **Rationale**: Edge geometry is explicitly OUT-of-scope for v1; silent data loss would be worse than a clear error
- **Impact**: EdgeCompressed FaceSets cannot be read or written in v1

## Phase 7: Animation + Effects

### TAE Template subsystem deferral
- **Type**: Functional divergence (C-side v1.1 does not implement)
- **Upstream Ref**: `Formats/TAE/Template.cs` (801 LOC); `TAE.cs:ApplyTemplate(Template, bool)`
- **C API**: None — `sf_tae_template_t`, `sf_tae_bank_template_t`, `sf_tae_event_template_t`, `sf_tae_param_template_t`, `sf_tae_param_type_t` are not exposed
- **Rationale**: Template is a friendly typed parameter access layer; v1.1 scope is round-trip only. Typed parameter access deferred to v1.2.
- **Impact**: `sf_tae_event_parameters()` returns opaque `uint8_t*` + size; consumers must parse parameter bytes themselves.

### mxml default allocator (no sf_allocator_t override)
- **Type**: C-style adaptation (known limitation)
- **Upstream Ref**: Upstream uses .NET XmlSerializer; allocation managed by GC
- **C API**: `sf_fxr3_to_xml(...)` / `sf_fxr3_from_xml(...)` use mxml's default `malloc/free` internally; the fxr3 object itself is controlled by `sf_allocator_t`
- **Rationale**: mxml 4.0.4 does not support thread-local allocator hooks; v1.1 does not bridge this.
- **Impact**: XML pipeline memory does not flow through the user allocator; XML strings must be freed via mxml-style `free()`.

### FXR3 XML round-trip equivalence policy
- **Type**: Functional adaptation
- **Upstream Ref**: `FXR3.cs:1488 FXR3ToXML` / `FXR3.cs:1480 XMLToFXR3` use .NET XmlSerializer
- **C API**: `sf_fxr3_to_xml` / `sf_fxr3_from_xml`
- **Rationale**: XmlSerializer output contains unstable whitespace / attribute ordering / namespace declarations; C-side cannot produce byte-equal output.
- **Impact**: Tests use structural in-memory equality (XML write → re-read → field-by-field comparison), not byte-equal raw XML comparison.

### TAE format coverage limited to SDT in v1.1
- **Type**: Functional divergence
- **Upstream Ref**: `TAE.cs:202-260` — 7 TAEFormat branches (DS1/SOTFS/DS3/SDT/DES/DESR/AC6-TBD)
- **C API**: `sf_tae_read_from_memory` returns `SF_ERR_UNSUPPORTED_VERSION` when version ≠ 0x1000D
- **Rationale**: v1.1 target games (Sekiro / Elden Ring / Nightreign) all use SDT format; legacy formats deferred to v2. AC6 TBD pending probe.
- **Impact**: DS1 / SOTFS / DS3 / BB / DES / DESR / AC6 (TBD) `.tae` files cannot be read.
