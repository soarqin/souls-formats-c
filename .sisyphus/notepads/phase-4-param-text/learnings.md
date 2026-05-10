# Phase 4 Learnings

## Project Conventions (from Phase 0-3)

- All public symbols prefixed `sf_`, types end `_t`, constants `SF_<CAT>_<NAME>`
- All fallible APIs return `sf_result_t`, output via pointer params
- Every "create" API takes `const sf_allocator_t *alloc` (NULL = default malloc/free)
- UTF-8 on boundary; Win32 MultiByteToWideChar/WideCharToMultiByte internally
- All public types are opaque forward-declared pointers
- Every public symbol decorated with `SF_API`
- `_Static_assert` after every enum table to catch drift
- Build: MinGW-w64 cross from WSL2, `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake`
- Tests: Unity (ThrowTheSwitch), `sf_add_test()` in tests/CMakeLists.txt
- Upstream reference: `/home/soar/src/SoulsFormatsNEXT` (read-only, pinned commit `9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a`)

## Phase 4 Specific

- FormatFlags1/2 MUST be `typedef uint8_t` + constants (NOT C enum) — C11 enum defaults to int (4 bytes)
- `_Static_assert` on enum: use COUNT or VALUE assertions, NOT sizeof (C11 enum is int by default)
- PARAMDEF 9 FormatVersions: 0/101/102/103/104/106/201/202/203 — read all, write only v104/v106/v201/v202/v203
- VersionAware PARAMDEF cannot be written (mirrors upstream PARAMDEF.cs:191-192)
- Bit-packing: MUST mirror Row.cs:236-244 `(64 - bitSize - bitOffset)` shift LITERALLY — no "beautification"
- PARAMTDF: naive Trim('"') parsing only — no escape sequences, no BOM, no comments
- FMG: MD5 prefix detected by byte[0] != 0 — do NOT verify hash, just read/write
- EMEVD: 5 game variants + ER/AC6/Nightreign as Sekiro aliases (pending Wave 0 probe confirmation)
- PARAM apply: 3 modes (UNCONDITIONAL/SOMEWHAT_CAREFUL/CAREFUL) — NOT 8 upstream variants
- Cell typed getters: exactly 13 (one per DefType) — no more, no less
- Test data: `/mnt/c/Games/ELDEN RING/Game/`, `/home/soar/dev/paramdex/`, `~/dev/oodle/`
- mxml library already in deps (used in Phase 3 for XML)

## T0.1 — Pre-Flight Probe (Wave 0, 2026-05-11)

- Result code SF_ERR_OOM (not SF_ERR_OUT_OF_MEMORY) — Phase 1 enum naming convention
- `sf_bnd4_read_from_memory` accepts DCX-wrapped BND4 transparently (auto-unwraps)
- Empirical PARAM data confirmed: SpEffectParam entry in ER 1.16+ regulation.bin is
  `N:\GR\data\Param\param\GameParam\merged\DLC02\SpEffectParam.param` — DLC02 path prefix
  reflects "merged" DLC override layout, NOT the vanilla `N:\GR\data\Param\param\GameParam\` location
- Decrypted regulation.bin is 2,036,256 bytes for ER 1.16 (input 2,036,272 = +16 IV bytes)
- Decrypted regulation begins with `44 43 58 00` ("DCX\0") — inner BND4 is itself DCX-wrapped
- `sf_bhd5_open` returns SF_ERR_OUT_OF_RANGE in current dev environment despite RSA decrypt working
  (test_rsa_real_eldenring_data0 passes); this prevents Data0-dependent probes
  (EMEVD, FMG) from running. Pre-existing baseline issue — all Phase 3 e2e tests skip the same way.
- Test compile-time path injection pattern: use `SF_E2E_REPO_DIR=L"${CMAKE_SOURCE_DIR}"` to let
  tests write outputs back into the repo at known locations
- Each probe in a multi-probe test SHOULD gate on its own preconditions (regulation_available
  vs data0_available), so partial environments yield partial empirical data
