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
