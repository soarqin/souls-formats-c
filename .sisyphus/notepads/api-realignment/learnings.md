# API Realignment Learnings

## Project Context
- WSL2 + MinGW-w64 cross-compile: `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug`
- Run tests: `ctest --test-dir build-mingw --output-on-failure`
- All public symbols: `sf_` prefix, `_t` suffix for types, `SF_` for constants
- No stdio (fopen); use sf_istream_* / sf_ostream_* / Win32 directly
- Project is -Werror — every warning is fatal

## Upstream Reference
- Upstream at: `/home/soar/src/SoulsFormatsNEXT` (read-only)
- Utilities: `SoulsFormats/Utilities/{IO,Text,Cryptography,Compression}/`
- Formats: `SoulsFormats/Formats/`

## API Conventions
- Every public function: `sf_result_t` return; output via pointer params
- Allocator: `const sf_allocator_t *alloc` (NULL = default)
- Opaque types: forward-declared; never expose sizeof
- SF_API decoration required on all public symbols

## Wave Status
- Wave 1: IN PROGRESS (Tasks 1-7, all parallel)

## Task 5: POLICY.md Creation
- Established 13 core mapping policies for C# to C conversion.
- Verbatim headings are critical for automated QA and cross-referencing.
- Status legend symbols (`✓`, `~`, `✗`, `+`, `未实现`, `_skipped_`) are now standardized.

## Task 2: Golden Hash Baseline (2026-05-10)
- BCrypt SHA-256 uses same provider as MD5 (`BCRYPT_SHA256_ALGORITHM`, 32-byte digest).
- Capture-then-pin workflow: write test with placeholder zeros → run, capture printed C-array → paste into header → re-run, all PASS.
- `print_and_check()` helper prints the actual hash in header-paste-ready form on EVERY run (PASS or FAIL), so the evidence file doubles as the canonical baseline transcript.
- DCX zstd output IS deterministic at fixed library version (zstd 1.5.7 vendored via CPM). Any zstd bump must re-baseline.
- Fixture `GOLDEN_INPUT_64` is the bytes 0x00..0x3F — picked for redundancy (compresses well) without being degenerate.
- Header MUST be guarded `#ifndef SOULS_FORMATS_TESTS_GOLDEN_HASHES_H` and use `static const` (file-scope) so the include is safe per TU.
- `target_include_directories(... PRIVATE tests/golden)` lets the test source `#include "golden_hashes.h"` without a path prefix.

## Task 2 follow-up (Sisyphus-Junior, 2026-05-10)
- The previous learning about `target_include_directories(... PRIVATE tests/golden)` is *not* what was actually done. No extra include path was added; gcc's default behaviour for `#include "..."` (search the directory of the including translation unit first) resolves `"golden_hashes.h"` because the test source is `tests/golden/capture_golden.c`. Confirmed: clean build with no `-I tests/golden` anywhere.
- LSP/clangd background diagnostics can get *stuck* on a stale view of an aggressively rewritten header (kept reporting a redefinition at a line that no longer existed). Independent verifications used: `grep -c`, `ast_grep_search`, `lsp_diagnostics` directly on the header (returned 0 issues), and the actual gcc build (no warnings). When clangd disagrees with all four, trust gcc.
- Subtle hex-typo trap: when manually copy-editing a 64-char hex string, a single transposed digit can survive line-length checks (still 64 chars). Use a Python diff between header and text mirror as the authoritative check — `awk` length checks alone are insufficient.

## Task 12 — HashHelper / sf_is_prime (2026-05-10)

- Upstream `IsPrime` (HashHelper.cs:24-40) uses `int i; i*i <= candidate`. C# implicitly promotes both sides to `long` for `int <= uint` comparison, so the comparison is 64-bit even though `i` itself is 32-bit. To match this behaviour cleanly in C without signed-overflow UB, used `uint64_t i` for the loop counter — observable behaviour is identical for every `uint32_t` candidate.
- `<stdbool.h>` must be included from `sf_hash.h` since the header now exposes a `bool` return type. `sf_common.h` does not pull it in transitively.
- Test file already established a `k_*_cases[]` table-with-section-comments pattern in `test_golden_values`; reused that pattern for `k_prime_cases[]` to keep the file's style coherent.
- A scaffolding hook inserted a duplicate `main` and split-out test functions after my `main`; had to remove them before the build would succeed. Watch for this on future test-file additions.
- The `util-cryptography-hash-helper.md` mapping doc already existed (referenced from README) — only needed verification, no recreation.

## Task 10 — SFEncoding mapping / round-trip identity (2026-05-10)

- Upstream `SFEncoding` is only four static `Encoding` fields at lines 13, 18, 23, and 28, initialized in the static constructor to ASCII, shift-jis, Unicode LE, and BigEndianUnicode respectively.
- Existing public C encoding APIs already cover all four encodings as converter pairs; UTF-16 names use the established `utf16le` / `utf16be` spelling rather than inserting another underscore.
- The drift checklist currently has no `SFEncoding` entries to tick; Task 10 only needed the row-level mapping doc and additional round-trip identity tests.

## Task 11 — PathHelper / sf_path (2026-05-10)

- `PathHelper.GetRealExtension` first strips a terminal `.dcx` before taking the inner extension; the returned slice must end at the `.dcx` dot, not the original string end (`bar.flver.dcx` → `.flver`, not `.flver.dcx`).
- `PathHelper.GetRealFileName` uses basename-only semantics, then strips `.dcx` plus the inner extension when present (`/path/bar.flver.dcx` → `bar`).
- Windows test executables accepted `/tmp/sf_test_path/...` with direct Win32 `CreateDirectoryW` / `CreateFileW`; no stdio is needed for fixture setup or backup verification.

## Task 18 — Oodle public enums / version typing (2026-05-10)

- Upstream Oodle exposes exactly seven `OodleLZ_*` enum types in `Oodle.cs`: `Compressor`, `CompressionLevel`, `CheckCRC`, `Decode_ThreadPhase`, `FuzzSafe`, `Profile`, and `Verbosity`; no eighth `OodleLZ_*` enum exists at the pinned commit.
- `OodleLZ_CompressOptions` C# `[MarshalAs(UnmanagedType.Bool)] bool` fields marshal as 4-byte Win32 BOOL values, so the public C mirror should use `int32_t` for `seek_chunk_reset` and `send_quantum_crcs`, matching the existing internal loader layout.
- Upstream `OodleVersion` enum ordinals are `Oodle9 = 0`, `Oodle8 = 1`, `Oodle6 = 2`; the C API intentionally reports DLL version numbers (`9`, `8`, `6`) plus `UNKNOWN = 0` because `sf_oodle_version()` is a loaded-version query rather than an upstream object discriminator.

## Task 18 — Oodle public enums (2026-05-10)

- Upstream OodleLZ boolean fields use `[MarshalAs(UnmanagedType.Bool)]`, which is a 4-byte Win32 BOOL in the sequential P/Invoke struct; the public C `sf_oodle_lz_compress_options_t` therefore uses `int32_t` for `seek_chunk_reset` and `send_quantum_crcs`, not C `bool`.
- Current loader keeps the existing DLL search order 9 → 8 → 6 and exposes `sf_oodle_version_t` without exposing raw Oodle handles or per-version interop symbols.
- Build verification exposed a pre-existing duplicate `ext_end` declaration in `src/core/path.c`; removing the stale declaration restored the MinGW build before Oodle verification.

## Task 8 — BinaryReaderEx realignment (2026-05-10)

- Upstream `BinaryReaderEx.IsFlexible` only gates `AssertValue<T>`-based primitive/varint asserts; enum reads and string/pattern assertions are separate upstream code paths. C mirrors this with per-reader `is_flexible` copied from a global default at reader creation.
- C mapping for upstream generic `ReadEnum8/16/32/64<TEnum>` uses width-specific raw integer option lists; signed enum values are represented by their two's-complement raw bytes/words.
- `Read11_11_10Vector3` maps cleanly to `sf_binary_reader_read_11_11_10_vec3`; removing the old `read_vec3_11_11_10` symbol requires updating tests and documenting the breaking change.

## Task 9 — BinaryWriterEx realignment (2026-05-10)

- Upstream `BinaryWriterEx` reserves placeholders in-band as `0xFE` bytes, then keys fills by `(name, typeName)`; the C writer keeps equivalent per-type reservation identity via internal type tags (`bool`, `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `varint32`, `varint64`, `f32`, `f64`).
- C writer finish semantics now mirror the upstream 3-mode split: `to_array` snapshots without closing the writer, `finish` verifies and closes the writer handle, and `finish_bytes` snapshots then verifies/closes. The borrowed `sf_ostream_t` remains caller-owned so tests can still inspect bytes after writer close.
- `sf_binary_writer_write_pattern` is a clean break from the old `sf_binary_writer_pattern` name; tests and implementation should not retain aliases, to keep upstream `WritePattern` mapping unambiguous.

## Task 15 — DCX tagged-union realignment (2026-05-10)

- Upstream `DCX.Is(...)` only checks `DCP\0` / `DCX\0` file wrappers; plain zlib is supported by `Decompress` but must return false from the public `sf_dcx_is_from_*` family.
- `DefaultType` has duplicate `Type` ordinals upstream, so the C `sf_dcx_default_type_t` uses distinct values and the factory helper maps each game to the documented preset.
- KRAK files store compression level but not the Oodle compressor enum; decompression reconstructs `SF_OODLE_LZ_COMPRESSOR_KRAKEN`, matching upstream default constructor behavior.

## Task 19 — RegulationDecryptor public API (2026-05-10)

- Upstream `EncryptERNRRegulation` (RegulationDecryptor.cs:117-120) calls `EncryptRegulationWithKey(path, bnd, RegulationKey.EldenRing)` — note `EldenRing`, not `EldenRingNightreign`. The C wrapper `sf_regulation_encrypt_ernr()` faithfully mirrors this bit-identical behavior so that files produced by the C port match upstream-tooling output byte-for-byte. Decryption wrappers all forward to their matching key (no quirk). Documented as `✗ deviation` in the mapping table because the wrapper name implies Nightreign while the underlying key is EldenRing — preserve this on every future "fix" attempt.
- Added `test_ernr_encrypt_uses_er_key_quirk` as a regression guard: asserts `encrypt_ernr() == encrypt(..., ELDEN_RING, ...)` AND `encrypt_ernr() != encrypt(..., ELDEN_RING_NIGHTREIGN, ...)`. The double-assertion catches both forward-matching drift and accidental "fixes" that would silently break upstream byte compatibility.
- The internal `sfi_regulation_decrypt`/`sfi_regulation_encrypt` helpers and `sfi_regulation_game_t` enum (`SFI_REGULATION_DS3`/`_ER`/`_AC6`/`_NIGHTREIGN`) were removed entirely. Only the test file referenced them, and it was rewritten on top of the public API. Cross-TU access to the key bytes still flows through `sfi_regulation_key()` which now takes `sf_regulation_key_t`.
- Round-trip test pads a 33-byte plaintext (intentionally not a multiple of 16) so the test exercises the PKCS#7 padding boundary on encrypt and the "Epic Encryption Technology" zero-padding on decrypt. Each key gets its own dedicated `test_roundtrip_*` so a per-key regression is identifiable in the Unity output.
- DLL exports verified via `x86_64-w64-mingw32-objdump -p libsouls_formats.dll`: all 10 `sf_regulation_*` symbols present (`decrypt`, `encrypt`, plus 4 decrypt + 4 encrypt wrappers). The `sf_regulation_key_t` enum has no runtime export (compile-time-only type).
- Header doc-comment placeholder for Phase 3 BND4 overloads documents the future API surface (`sf_regulation_*_bnd4(const wchar_t *path, sf_bnd4_t **out, ...)`) so callers know byte-buffer-level today is intentional, not an oversight.
- ZlibHelper and ZstdHelper are mapped as internal-only extensions since they are implementation details of DCX and no public API is exposed for them.

## Task 13 — SFUtil / sf_get_decompressed_reader (2026-05-10)

- Upstream `SFUtil.GetDecompressedBinaryReader` only invokes `DCX.Decompress`
  when `DCX.Is` returns true. `DCX.Is` matches `DCP\0` / `DCX\0` magic only —
  it explicitly does NOT match plain zlib (0x78 0xDA). So plain-zlib bytes
  fall through to the borrow path with `NoCompressionInfo`. Faithful port
  of `sf_get_decompressed_reader` therefore uses `sf_dcx_is_from_buffer`
  (which mirrors `DCX.Is`), not the broader `sf_dcx_sniff` (which also
  recognises plain zlib). Test sub-case 1 must use a DCX/DCP-magic-prefixed
  variant (e.g. `SF_DCX_TYPE_DCX_DFLT` via the
  `DCX_DFLT_11000_44_9` preset) — using `SF_DCX_TYPE_ZLIB` here would land in
  the borrow path and the test would assert that `new_reader == in_reader`,
  not `!=`.
- `sf_binary_reader_t` previously could only borrow its istream. Adding
  `sf_binary_reader_create_from_memory` required two new fields
  (`bool owns_stream`, `void *owned_buffer`) plus a destroy-time fan-out:
  `sf_istream_close(stream)` if owned, then `sf_xfree(alloc, owned_buffer)`
  if non-NULL. Existing `sf_binary_reader_create()` is unchanged because the
  new fields are zero-initialised by `memset`.
- Public API addition was made to `sf_io.h` (per task constraint — no new
  `sf_util.h`). Implementation lives in `src/core/sf_util.c` so that the
  upstream→ours module boundary stays one-to-one. `sf_io.h` now `#include`s
  `sf_dcx.h` so that `sf_dcx_compression_info_t` is visible at the SFUtil
  signature; this is acceptable because the IO header was already heavy
  with downstream dependencies.
- Verified DLL exports both `sf_binary_reader_create_from_memory` and
  `sf_get_decompressed_reader` via
  `x86_64-w64-mingw32-objdump -p libsouls_formats.dll | grep ...`.
- Full ctest suite remains 17/17 PASS after the change; no golden-hash
  regressions observed.
### TPF & ENFL Mapping (Phase 3)
- TPF is a multi-file texture container that supports various platforms (PC, Xbox360, PS3, PS4, Switch, etc.).
- TPF transports DDS data opaquely; the C implementation should focus on the container format and metadata, not DDS pixel decoding.
- ENFL (EntryFileList) is a mysterious format used in BB and DS3 for asset loading based on map location.
- ENFL uses Zlib compression for its internal data block.
- Both formats are targeted for Phase 3 implementation.
### Binder Family Mapping (2026-05-10)
- Consolidated Binder common types (BinderFile, BinderFileHeader, BinderHashTable, IBinder, BinderReader) into a single mapping doc.
- BND3/4 and BXF3/4 follow a consistent pattern with their respective readers.
- BHD5 is a standalone format at the root level, with complex bucket/file header structures.
- All mappings marked as '未实现' as per Phase 3 requirements.
### EMEVD and ESD Mapping (2026-05-10)
- EMEVD format varies significantly between games (DS1/2 vs BB vs DS3 vs Sekiro) based on bitness, endianness, and version flags.
- ESD uses a 'LongFormat' flag to switch between 32-bit and 64-bit offsets/sizes.
- Both formats rely heavily on bytecode (EMEVD instructions, ESD evaluators/arguments) which will require careful implementation in Phase 5.
- EMEVD uses a 'Layer' system for ceremony-specific instructions, which involves a complex offset-based lookup in the file structure.

### MTD and MATBIN Mapping (2026-05-10)
- MTD is the legacy material format used in Sekiro and earlier games. It uses a block-based binary format with Shift-JIS strings.
- MATBIN is the modern material format introduced in Elden Ring. It uses UTF-16 strings and a more flat structure compared to MTD's nested blocks.
- Both formats share similar concepts: ShaderPath, Params, and Textures/Samplers.
- MTD's `Texture` has an `Extended` flag for Sekiro-specific data (Path and UnkFloats).
- MATBIN's `Param` and `Sampler` use 64-bit offsets for strings, reflecting the move to 64-bit engines.
## PARAM Family Mapping (Phase 4)
- PARAM and PARAMDEF are complex formats with multiple contributing files.
- PARAMDEF supports XML serialization which is critical for community tools.
- FMG supports multiple versions (DeS, DS1/2, DS3/BB) and optional MD5 hashing.
- PARAMTDF is a simple plaintext format for enums.
- Bitfield handling in PARAM rows is intricate and depends on PARAMDEF field types and bit sizes.

## FLVER Family Mapping (2026-05-10)
- FLVER2 upstream source is located at `Formats/FLVER/FLVER2/`, not at the root of Formats.
- Discovered 13 contributing files for FLVER2 mapping.
- Vertex decoding logic in `Vertex.cs` reveals version-dependent `uvFactor`: 1024 for < 0x2000E, 2048 for >= 0x2000E.
- Armored Core VI (AC6) uses a unique normalization for normals when stored as `UShort4`.
- Edge compression (`EdgeGeom`) is deeply integrated into both `VertexBuffer` and `FaceSet` (via `EdgeIndexGroup`/`EdgeIndexBuffer`).
- `LayoutType` (format) and `LayoutSemantic` (purpose) define the vertex structure; `LayoutType` values are non-contiguous (e.g., 0-3, 16-24, 26, 45-47, 240).
### Phase 7: TAE and FXR3 Mapping
- TAE (Time Act Editor) has a complex hierarchical structure: TAE -> Animation -> Event/EventGroup.
- TAE uses a Template system for event parameters, which is XML-driven in upstream.
- FXR3 (Rainbow Stone FXR) is an SFX definition format with a state-machine-like structure (StateMap -> State -> Condition).
- FXR3 supports XML serialization via FXR3EnhancedSerialization, which will be important for tool parity.
- Both formats are planned for Phase 7 implementation.

## MSB Family Mapping (Task 25)
- MSB formats share a common base structure (Models, Events, Regions, Routes, Parts) but differ significantly in the specific types and data structures within those categories.
- MSBE (Elden Ring) and MSBVI (AC6) use 64-bit offsets and have more complex inheritance/struct patterns compared to MSBS (Sekiro).
- MSBVI (AC6) implementation in SoulsFormatsNEXT is based on Smithbox and includes additional params like Layers.
- Disambiguation of names is a common pattern across all MSB formats to handle duplicate entry names.
