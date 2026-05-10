# Phase 3 Learnings

## Project Conventions

### Build System
- Build: `cmake --build build-mingw`
- Test: `ctest --test-dir build-mingw --output-on-failure`
- Toolchain: MinGW-w64 cross-compile (`cmake/toolchain-mingw-w64.cmake`)
- Platform: Windows PE binaries, run via WSL interop (binfmt_misc)
- Compiler flags: `-Werror` — ALL warnings are fatal

### API Conventions
- Prefix: every public symbol starts `sf_`, every type ends `_t`, constants are `SF_<CAT>_<NAME>`
- Error: all fallible APIs return `sf_result_t`, output via pointer parameters
- Memory: every create API takes `const sf_allocator_t *alloc` (NULL = default malloc/free)
- Strings: UTF-8 boundary, internal Win32 with MultiByteToWideChar/WideCharToMultiByte
- Opacity: all public types are forward-declared opaque pointers
- `SF_API`: every public symbol in public header must have this decoration
- `_Static_assert`: add one after every enum table to catch drift

### Phase 3 Specific Constraints
- BHD5: streaming-only — Data0.bdt NEVER loaded into RAM
- BHD5: SHA hashes stored verbatim, never recomputed
- BHD5: RSA-encrypted header auto-detected by first 4 bytes (not "BHD5" magic)
- Binder: Format/FileFlags are `typedef uint8_t`, NOT enum (C11 enum is int-sized)
- Binder: `BinderFile.id` is `int32_t`, NOT `int64_t`
- Binder: `compression_info` is `sf_dcx_compression_info_t`, NOT `sf_dcx_type_t`
- Hash table: re-derived at WRITE time, never pre-computed on add
- Round-trip: synthetic = byte-equal; real ER e2e = content-equal only
- Per AGENTS.md: no fopen/fread/fwrite — use sf_istream_t/sf_ostream_t
- Internal helpers: prefix `sfi_*` (NOT `sf_*`), NOT exported via SF_API

### Test Data
- Upstream reference: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/`
- ER game data: `/mnt/c/Games/ELDEN RING/Game/Data0.bhd`
- Oodle DLL: `~/dev/oodle/`
- E2E tests SKIP gracefully (TEST_IGNORE_MESSAGE) when data missing

### Source Layout
- Public headers: `include/souls_formats/sf_*.h`
- Internal headers: `src/<area>/*.h` (NOT under include/)
- Internal helpers: `src/internal/*.{h,c}` or `src/<area>/<name>.{h,c}`
- Tests: `tests/<area>/test_*.c` registered via `sf_add_test()` in `tests/CMakeLists.txt`
- CMakeLists.txt: `SF_PUBLIC_HEADERS` and `SF_SOURCES` lists need updating

## Phase 3 Wave Structure
- Wave 0: T0a/T0b/T0c/T0d/T0e — all parallel, prerequisites
- Wave 1: T1/T3/T4/T5 (parallel) → T2 (after T0a+T1)
- Wave 2: T6/T7/T8/T9/T10/T11/T12 (parallel, after Wave 1) → T13 (after all)
- Wave 3: T14 → T15-T18 parallel → T19 parallel → T20 sequential
- Final: F1/F2/F3/F4 parallel

## Task T0b — sfi_aes_decrypt_ecb_buffer (2026-05-10)
- Reused cached `g_aes_ecb` BCRYPT_ALG_HANDLE via existing `aes_alg()` helper — no per-call provider open.
- BCrypt ECB mode requires NO IV: `BCryptDecrypt(hkey, buf, n, NULL, NULL, 0, buf, n, &done, 0)`. Critical: passing IV would silently shift to CBC-like behaviour.
- BCrypt supports in-place transform when in == out; verified by NIST CAVP vector.
- size==0 short-circuits to SF_OK to avoid 0-length BCryptDecrypt edge case.
- Multi-block correctness validated by encrypting via `sfi_aes_ecb_block` in a loop, then decrypting the concatenation in one `sfi_aes_decrypt_ecb_buffer` call.
- DLL export check (`objdump -p ... | grep -c sfi_aes_decrypt_ecb_buffer == 0`) is the canonical way to confirm an internal helper stays out of the public ABI.

## T0c — sf_path_hash_64 for ER+ BHD5 (2026-05-10)

- **Pattern**: 64-bit hash wrapper is purely a documented widening cast — `return (uint64_t)sf_path_hash(utf8_path);`. No separate algorithm; mirrors upstream's implicit C# `uint→ulong` at `BHD5.cs:474`.
- **DLL export verified**: `x86_64-w64-mingw32-objdump -p libsouls_formats.dll | grep 'sf_path_hash'` shows both `sf_path_hash` (slot 254) and `sf_path_hash_64` (slot 255) — confirms `SF_API` decoration is sufficient for export when `SF_BUILD_DLL` is set on the shared target.
- **Test fixture reuse**: `k_cases` table for 32-bit goldens can be re-iterated for 64-bit equivalence — no need to duplicate vectors. Same array serves both `test_golden_values` and `test_sf_path_hash_64_equivalence`.
- **Deferred-validation pattern**: For tests that need a downstream phase (BHD5 + er_helper for T15), use `RUN_TEST(...)` + `TEST_IGNORE_MESSAGE("requires Phase 3 BHD5 + er_helper");` placeholder. ctest reports `9 Tests 0 Failures 1 Ignored OK` — the placeholder is registered but skipped, ready to be activated when the dependency lands.
- **api-mapping rows**: Extension cast that has no upstream symbol gets row in BOTH `util-cryptography-hash-helper.md` (with status `+ extension`, upstream loc citing the implicit cast site) AND `extensions.md` (with full rationale). The two files have overlapping but complementary purposes.

## T0e — sfi_dds_parse_header (2026-05-10)

**Task**: Minimal internal-only DDS header parser for TPF metadata derivation.
**Status**: PASS — 3/3 sub-tests, full suite 18/18, DLL exports unchanged at 280.

**Key learnings**:
- Internal-only modules go under `src/internal/` and are added to `SF_SOURCES` in
  the **top-level** `CMakeLists.txt` (not `tests/CMakeLists.txt`). Both static
  and shared lib targets share `SF_SOURCES`.
- Tests for internal modules `#include "internal/dds_header.h"` directly — the
  test target inherits `target_include_directories(... PRIVATE ${CMAKE_SOURCE_DIR}/src)`
  via `sf_add_test()`, so the `internal/` prefix resolves to `src/internal/`.

## T10 — BHD5 streaming reader (2026-05-10)

- Modern v1 BHD5 parses cleanly with the upstream 64-bit layout: after magic/endian/flags/version/fileSize, read `bucketCount:int64`, `bucketsOffset:int64`, then `saltLength:int32 + salt`.
- For 64-bit buckets, preserve the upstream `unknownFlag == 1` field between `count:int32` and `fileHeadersOffset:int64`; synthetic fixtures must include it or parser alignment breaks.
- Inline AES data follows upstream `AESKey.Read`: exactly 16 key bytes, then `rangeCount:int32`, then `rangeCount` pairs of `int64` start/end offsets. No per-game AES constants are involved.
- Windows wide `swprintf` format strings in MinGW tests should use `%ls` for wide strings. Using `%s` caused temp BHD/BDT paths to collide/truncate and made BDT bytes overwrite the synthetic BHD fixture.
- DDS header layout (per task spec, verified working):
  - magic at off 0 (4 bytes, `'DDS '` = LE 0x20534444)
  - dwSize at off 4 (must be 124)
  - dwFlags at off 8 (DDSD_DEPTH = 0x800000 gates dwDepth read)
  - dwDepth at off 24, dwMipMapCount at off 28
  - dwFourCC at off 84 (LE 0x30315844 = "DX10" triggers extension)
  - dwCaps2 at off 112 (DDSCAPS2_CUBEMAP = 0x200)
  - DX10 extension dxgiFormat at off 128 (only if size >= 148)
- No `SF_API` decoration on internal symbols → DLL export count is unchanged,
  proving the symbol stays internal-only.

## T0d (2026-05-10): RSA wrapper + BHD5 PEM keys

### Win32 CNG raw RSA: use BCryptEncrypt, NOT BCryptDecrypt
The "BCryptDecrypt with public key" path returns SF_ERR_CRYPTO. In classical
RSA terms the game has **signed** the archive header with the private key
(`s = m^d mod n`); recovering `m` requires `s^e mod n`, which on CNG is
**BCryptEncrypt** with the public key + `BCRYPT_PAD_NONE`. This took one
debug iteration to discover.

### PEM format dual-support
The four shipped game keys (extracted from Nordgaren/UXM-Selective-Unpack)
are PKCS#1 `-----BEGIN RSA PUBLIC KEY-----`, NOT X.509 SubjectPublicKeyInfo.
Win32 `CryptDecodeObjectEx` does not natively decode PKCS#1, so we hand-parse
the trivial ASN.1 DER (SEQUENCE { INTEGER n, INTEGER e }) and build a
`BCRYPT_RSAKEY_BLOB` directly. The same parser also strips the X.509 SPKI
wrapper to support standard `-----BEGIN PUBLIC KEY-----` PEMs (used by the
hermetic test vector path).

### FromSoft public exponents are weird
Most of these keys use non-standard public exponents (e.g. Nightreign's `e`
is 5 bytes, Sekiro `Data1` is 4 bytes 0x2A48F927). Be sure to handle
arbitrary `e_len` rather than assuming 0x010001.

### Hermetic test vectors > runtime openssl shell-out
The original task brief suggested `system("openssl genrsa ...")` at test
time, but WSL2/MinGW cross-compile means the test .exe runs under Win32
binfmt-interop where /tmp paths and PATH lookup for openssl.exe are flaky.
Solution: generate vectors once via openssl on the host, embed as static C
arrays (PEM + plaintext + ciphertext). Test is now fully hermetic.

### crypt32 is required for CryptStringToBinaryA
Added `crypt32` to the `_sf_configure_library` link line (alongside
`bcrypt`). The test target also gets crypt32 via `target_link_libraries(...
PRIVATE crypt32)` after `sf_add_test(...)`.

## T1 — sf_binder.h shared types + helpers (2026-05-10)

### What landed
- `include/souls_formats/sf_binder.h` — public header
- `src/archive/sf_binder.c` — 10 helper impls
- Wired into `souls_formats.h` umbrella + `CMakeLists.txt`
- Build: 0 errors / 0 warnings under `-Werror`
- DLL exports: 10 new `sf_binder_*` symbols (objdump-confirmed)
- Tests: existing 19/19 still pass (no regressions)

### Patterns reinforced
- `typedef uint8_t` + `#define` is the right idiom for byte-sized C-side
  representations of upstream `[Flags] enum : byte` types — C11 enum is
  int-sized, which would silently change struct layout on serialization.
- Per-bit `_Static_assert` (one for each constant + drift message) is
  preferred to a single `sizeof(...) == 1` assertion: it catches both
  size drift AND value drift in one place.
- `sscanf("%2d%c%d%c%d", ...)` is a clean drop-in for the upstream
  regex `(\d\d)(\w)(\d+)(\w)(\d+)` because greedy `%d` stops at the
  next non-digit character (the letter), no separator needed.
- `snprintf(buf, 9, "%02d%c%d%c%d", ...)` followed by zero-fill
  reproduces C# `String.PadRight(8, ' ')` byte-exact.

### Watch-outs (caught during T1)
- Be careful with `*/` in C block comments. The phrase
  "Has*/ForceBigEndian" inside a banner comment closed the comment
  prematurely. Caught only by `lsp_diagnostics`, not by clangd hover.
  Audit comments that paraphrase upstream method names containing slashes.
- clangd standalone-header diagnostics flag missing-header errors and
  unknown `SF_API` because clangd reads headers without compile_commands
  context. These are noise; the ground truth is `cmake --build`.

## T2 — binder_common.c

* `Binder.ReadFormat` / `Binder.WriteFormat` are **NOT bijective on all 256
  byte values when bit_big_endian = false**. The heuristic in ReadFormat
  (`(raw & 1) != 0 && (raw & 0x80) == 0`) only round-trips correctly with
  the inverse condition in WriteFormat (`ForceBigEndian(f) && Flag6(f)`).
  The mismatch between these two predicates means f-values like 0x80
  (Flag7 alone) are NOT recoverable. Real BND/BXF format bytes always
  include either bit0 (BigEndian) or another marker bit, so this is a
  non-issue in practice — but the test must either restrict to legitimate
  formats OR set bit_big_endian=true (where both helpers are pass-through).

* `BinderHashTable.Write` uses **integer division** (`files.Count / 7`)
  for its starting prime, NOT ceil. The PLAN.md text "ceil(n/7)" is
  misleading. Concrete values: prime(7)=2, prime(49)=7, prime(100)=17,
  prime(1000)=149.

* The PLAN's stated test expectations `prime(100)=15, prime(1000)=143`
  are wrong: 15 = 3·5 and 143 = 11·13 are composite. Tests must use the
  upstream-derived values 17 and 149.

* `Binder.GetBND4FileHeaderSize(Names1)` = 0x20, NOT 0x24. Names1 alone
  doesn't have the IDs bit, so the IDs+4 contribution is absent:
  0x10 + 4(offset) + 4(name) + 8(Names1-extra) = 0x20.

* The task spec listed `sfi_binder_read_format(sf_binary_reader_t *br)`
  with no bit_big_endian param; this had to be added (`bool
  bit_big_endian` second param) for upstream parity. AGENTS.md §5.x
  STRICT UPSTREAM REFERENCE takes precedence over the task spec when
  they disagree on signatures.

* Internal helpers must include `#include <stdio.h>` for `snprintf`.
  Forgetting it yields `-Wimplicit-function-declaration` under clang.

* `sf_binary_reader_get_shift_jis` / `_get_utf16` accept NULL for the
  `out_len_bytes` argument — use that to avoid `-Wunused-but-set` on
  the length variable.

## T4 + T5 (skeleton creation, 2026-05-10)

- The existing `SF_BUILD_EXAMPLES` option in top-level `CMakeLists.txt` (line 13)
  defaults to OFF. Wired `add_subdirectory(examples)` under `if(SF_BUILD_EXAMPLES)`
  to respect that pattern; verified the example builds cleanly when reconfigured
  with `-DSF_BUILD_EXAMPLES=ON`.
- `tests/e2e/CMakeLists.txt` is intentionally not yet pulled in via
  `add_subdirectory(e2e)` from `tests/CMakeLists.txt` — the file only declares
  path macros for future tasks (T14-T18). The `.c` and `.h` stubs are not
  compiled into anything yet, so the build is a no-op for T4 alone.
- Helper for warnings is named `sf_apply_warnings(<target>)` (defined in
  `cmake/compiler_warnings.cmake`); the test helper for sanitizers is
  `sf_apply_sanitizers(<target>)`.
- Stubs return `SF_ERR_INTERNAL` (last entry before `SF_RESULT_COUNT_` in
  `sf_common.h`). This avoids triggering the existing `SF_OK` semantics check
  but is clearly distinct from real error codes like `SF_ERR_INVALID_ARG`.

## 2026-05-10 — T7 BND4 port

- BND4 header mirrors upstream `BND4.cs`: offset `0x0A` stores `!BitBigEndian`, not `BitBigEndian`; read/write must invert.
- BND4 hash table is written/asserted only for `Extended == 4`; other accepted values (`0`, `1`, `0x80`) keep `HashTableOffset == 0`.
- The PC-save `Format.Names1` corner case is already centralized in `sfi_binder4_*_file_header`: an extra int32 ID plus zero padding round-trips even without the normal `IDs` bit.
