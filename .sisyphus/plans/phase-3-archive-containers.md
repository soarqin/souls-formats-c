# Phase 3 — Archive Containers (BND/BXF/BHD5/TPF/ENFL)

## TL;DR

> **Quick Summary**: Port the entire archive-container layer from upstream SoulsFormatsNEXT (~4,894 LOC of C# across 16 .cs files) to pure C, plus retro-fit 5 missing infrastructure pieces from Phases 1/2 + integrate RSA-encrypt-bhd-decryption + minimal DDS header reader. By the end, `er_extract_from_data0()` is the single helper every downstream phase (4-7) builds on.
>
> **Deliverables**:
> - Wave 0: 5 prerequisite infra files (`sf_reverse_bits_u8`, `sfi_aes_decrypt_ecb_buffer`, `sf_path_hash_64`, `sfi_rsa_*` + 4 game PEM keys, `sf_dds_parse_header`)
> - Wave 1: shared types `sf_binder.h` + `binder_common.c` + 4 game AES key constants + 2 skeletons
> - Wave 2: 7 format implementations (BND3/BND4/BXF3/BXF4 each with eager + Reader; BHD5 streaming-only; TPF PC-only; ENFL) + api-mapping doc updates
> - Wave 3: ER test helper impl + 4 ER e2e tests + CLI example + PLAN.md retrospective
> - Wave Final: 4 parallel verification reviews
>
> **Estimated Effort**: Large (~2 weeks)
> **Parallel Execution**: YES — 5 waves, max 8 concurrent in Wave 2
> **Critical Path**: T0d (RSA) → T0b (AES range) → T1 (binder.h) → T2 (binder_common) → T7 (BND4) ∥ T10 (BHD5) → T14 (er_helper) → T15 (keystone e2e) → F1-F4 → user okay

---

## Context

### Original Request
"编写下一阶段的计划" — Phase 3 next, per AGENTS.md status table.

### Interview Summary

**User Decisions Confirmed**:

| ID | Decision | Choice |
|---|---|---|
| D1 | BND/BXF Reader mirror scope | 全镜像 — Phase 3 ships eager + streaming Reader for ALL 4 formats (BND3/BND4/BXF3/BXF4) |
| D2 | BHD5 game key coverage | v1 4 games — Sekiro / ER / Nightreign / AC6 keys embedded |
| D3 | TPF Headerizer in scope? | YES — **but capped to PC-only** (refined per Metis findings) |
| D4 | Test infra strategy | Continue Phase 1/2 practice — synthetic test inline with each format task; ER e2e is a separate task per format |
| GAP-1 | BHD5 RSA-encrypt-bhd handling | **Phase 3 integrates RSA decrypt** with embedded community PEM keys |
| GAP-2 | BHD5 Game enum | Extend to 4 v1-target values (`SF_BHD5_GAME_SEKIRO/ELDENRING/NIGHTREIGN/ARMOREDCORE6`) — track in `extensions.md` |
| GAP-9/10 | TPF / DDS scope | **PC-only TPF + minimal `sf_dds_parse_header` (~50 LOC)** — PS3/PS4/Xbox/PS5 stubs return `SF_ERR_UNSUPPORTED_VERSION` |

**Research Findings**:
- Upstream LOC to port: ~4,894 (Binder common 860 + BND3 286 + BND4 352 + BXF3 864 + BXF4 979 + BHD5 746 + TPF 630 + ENFL 177).
- All 8 api-mapping docs already enumerate every "未实现" row to flip — total ~107 rows.
- Phase 1/2 infra reuse: `sf_get_decompressed_reader` ✅, `sf_dcx_*` ✅, `sf_path_hash` ✅, AES-128 ECB block / CBC ✅, `sf_binary_reader/writer_t` ✅, `sf_istream/ostream_t` ✅.
- **Phase 1 GAP**: `sf_reverse_bits_u8` not present; needed by `Binder.ReadFormat` / `WriteFormat` / `ReadFileFlags` / `WriteFileFlags`.
- **Phase 1 GAP**: `sf_path_hash_64` not present; ER+ uses 64-bit hash (different fold than 32-bit `sf_path_hash`).
- **Phase 2 GAP**: AES range-decrypt buffer helper not exposed; only single-block ECB and CBC are public.
- **Phase 2 GAP**: RSA-decrypt not implemented; Data0.bhd in vanilla ER install needs RSA unwrap before BHD5 magic.
- **Test data verified**: `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` first 4 bytes = `e1 0e 36 ab` (RSA-encrypted; community PEM key required).

### Metis Review

**Critical Gaps Surfaced** (all addressed):

1. ✅ Data0.bhd RSA-encrypted → Wave 0 T0d adds `sfi_rsa_decrypt` + 4 game PEM keys
2. ✅ `EndianHelper.ReverseBits` missing → Wave 0 T0a retro-fits to Phase 1
3. ✅ AES range-decrypt buffer missing → Wave 0 T0b retro-fits to Phase 2
4. ✅ ER 64-bit FileNameHash needs different algorithm → Wave 0 T0c adds `sf_path_hash_64`
5. ✅ `BinderFile.ID` is Int32 (not Int64) → Public API uses `int32_t id`
6. ✅ Per-entry compression preset must be preserved → `sf_dcx_compression_info_t compression_info` (not bare enum)
7. ✅ BND3 `Unk18` + `WriteFileHeadersEnd` were missing → Now mandatory in T6
8. ✅ BND4/BXF4 `Unk04` + `Unk05` + `Unicode` + `Extended` were missing → Now mandatory in T7/T9
9. ✅ Headerizer 919 LOC + DDS dependency → Capped to PC-only in T11
10. ✅ TPF Texture metadata derivation needs DDS header → `sf_dds_parse_header` minimal in Wave 0 T0e

**Per-Task Guardrails Locked**: see "Must NOT do" sections in each TODO item.

**Round-Trip Semantic** (locked): synthetic fixtures = **byte-equal**; real ER e2e = **content-equal** (FromSoft hash table layouts are non-deterministic vs our writer).

---

## Work Objectives

### Core Objective
Land the complete archive-container layer plus the `er_extract_from_data0()` test helper that all of Phases 4-7 e2e tests depend on. The keystone `test_bhd5_e2e_er` test must walk the full chain RSA → AES range → BHD5 lookup → DCX_KRAK → Oodle → BND4 magic verification on a real Elden Ring `Data0.bhd/bdt`.

### Concrete Deliverables

**Public headers** (added under `include/souls_formats/`):
- `sf_binder.h` — shared Format/FileFlags enums, `sf_binder_file_t`
- `sf_bnd3.h`, `sf_bnd4.h`, `sf_bxf3.h`, `sf_bxf4.h` (each with eager `sf_*_t` + streaming `sf_*_reader_t`)
- `sf_bhd5.h` (streaming-only)
- `sf_tpf.h` (PC-only Headerizer)
- `sf_enfl.h`

**Public infra** (Phase 1/2 retro-fits):
- `sf_io.h` adds `SF_API uint8_t sf_reverse_bits_u8(uint8_t b);`
- `sf_hash.h` adds `SF_API uint64_t sf_path_hash_64(const char *utf8_path);`
- (Internal-only) `src/crypto/aes_cng.h` adds `sfi_aes_decrypt_ecb_buffer`
- (Internal-only) `src/crypto/rsa_cng.{h,c}` adds `sfi_rsa_decrypt_pkcs1` + 4 game PEM key constants
- (Internal-only) `src/internal/dds_header.{h,c}` adds `sfi_dds_parse_header`

**Source files** (added under `src/archive/`):
- `binder_common.c` — full shared surface (timestamps, hash table, BinderFileHeader r/w, ReadFormat/WriteFormat helpers)
- `bnd3.c`, `bnd4.c`, `bxf3.c`, `bxf4.c` (each with eager + reader impl)
- `bhd5.c`, `bhd5_keys.c`
- `tpf.c`, `tpf_headerizer.c`
- `enfl.c`

**Tests** (added under `tests/`):
- `tests/archive/test_bnd3_synthetic.c`, `_bnd4_`, `_bxf3_`, `_bxf4_`, `_bhd5_`, `_tpf_`, `_enfl_`
- `tests/e2e/er_test_helper.{h,c}` — singleton `er_helper_init / er_extract_from_data0 / er_helper_shutdown`
- `tests/e2e/test_bhd5_e2e_er.c` — keystone (RSA → BHD5 → AES range → DCX_KRAK → BND4 magic)
- `tests/e2e/test_bnd4_e2e_er.c`, `_bxf4_e2e_er`, `_tpf_e2e_er`

**Examples**:
- `examples/sf_bnd_extract.c` — CLI: unpack a BND4 to disk

**Docs**:
- All 8 api-mapping docs (`format-{binder-common,bnd3,bnd4,bxf3,bxf4,bhd5,tpf,enfl}.md`) flip ~107 rows from `未实现` → `✓ aligned`
- `extensions.md` adds 5 extension rows: `sf_reverse_bits_u8`, `sf_path_hash_64`, `sf_bhd5_game_t` (4 values), `sf_bhd5_open(bhd, bdt)` two-path API, `sfi_dds_parse_header` minimal
- `POLICY.md` adds RSA-bhd-decryption deviation note + TPF Headerizer PC-only scope note + round-trip semantic note (synthetic byte-equal, real content-equal)
- `PLAN.md` Phase 3 boxes ticked with timestamp + concrete pass count
- `AGENTS.md` status table updated to `Phase 3 ✅ done`
- `docs/roadmap/README.md` Phase 3 row updated

### Definition of Done
- [ ] `cmake --build build-mingw` green with no warnings (project is `-Werror`)
- [ ] `ctest --test-dir build-mingw -L core` 5/5 PASS (Phase 0/1 regression — must remain green)
- [ ] `ctest --test-dir build-mingw -L 'compression|crypto'` 13/13 PASS (Phase 2 regression — must remain green; AES retro-fit T0b adds 1 sub-test)
- [ ] `ctest --test-dir build-mingw -L archive` all PASS — 7 synthetic + 4 ER e2e
- [ ] `ctest --test-dir build-mingw --output-on-failure` overall green
- [ ] `examples/sf_bnd_extract.exe` runs on `c0000.chrbnd.dcx` and dumps real entries to disk
- [ ] All 4 verification reviews (F1-F4) APPROVE; user explicit okay received

### Must Have
- All 7 archive formats (BND3/BND4/BXF3/BXF4/BHD5/TPF/ENFL) read+write; BND3/BND4/BXF3/BXF4 each have eager + Reader
- `er_extract_from_data0()` works on a vanilla ER install (RSA → BHD5 → AES → DCX_KRAK → BND4 byte stream)
- `sf_reverse_bits_u8` exposed via `sf_io.h` (Phase 1 retro-fit)
- `sf_path_hash_64` exposed via `sf_hash.h` (Phase 1 retro-fit)
- `sfi_aes_decrypt_ecb_buffer` (internal, Phase 2 retro-fit) — used by BHD5
- `sfi_rsa_decrypt_pkcs1` (internal) + 4 game PEM keys (Sekiro/ER/Nightreign/AC6)
- `sfi_dds_parse_header` (internal, ~50 LOC) — used by TPF
- BND3/BND4/BXF4 unknown fields (`Unk18`/`WriteFileHeadersEnd`/`Unk04`/`Unk05`/`Unicode`/`Extended`) preserved on round-trip
- Per-entry DCX type+preset preserved on round-trip via `sf_dcx_compression_info_t compression_info`
- `BinderFile.id` is `int32_t` (matches upstream Int32, sentinel -1)
- BND4 `Format=Names1` PC-save corner case handled (BinderFileHeader.cs:149-153)
- BHD5 `is64Bit` auto-detection at offset 0x14 (BHD5.cs:151-163)
- BHD5 32-byte SHA hash blob: read+stored verbatim, NOT recomputed on write
- BHD5 streaming-only — Data0.bdt opened via `sf_istream_t`, never loaded into RAM
- BHD5 per-game branching in FileHeader read/write (DS1 vs DS2+ vs DS3+ vs ER+) via switch on game enum
- Hash table: re-derived at write time using upstream's prime-search loop (`for p = ceil(count/7); p ≤ 100000; if IsPrime(p) break`)
- TPF DX10 cubemap fix at byte 0x8C (PC platform only, TPF.cs:357)
- TPF per-texture DCP_EDGE compression handled via `sf_dcx_compress` with `DcpEdgeCompressionInfo()`
- ENFL payload uses internal zlib (`ZlibHelper.ReadZlib`-equivalent path), NOT external DCX
- Round-trip semantics: synthetic fixtures byte-equal; real ER e2e content-equal (file count, lookup-by-hash, decompressed magic)
- All 8 api-mapping docs flipped to `✓ aligned`; 5 deviations tracked in `extensions.md`
- All e2e tests SKIP gracefully (with `TEST_IGNORE_MESSAGE`) if Data0.bhd is missing or first 4 bytes ≠ `e1 0e 36 ab` AND ≠ `BHD5`

### Must NOT Have (Guardrails)

> Locked from Metis review and AGENTS.md §5.x. Any violation is a REJECT in F1-F4 verification.

**Code-level forbidden patterns**:
- ❌ Computing SHA hashes on BHD5 write (must store verbatim)
- ❌ Loading Data0.bdt into memory (must use file-backed `sf_istream_t` + seek)
- ❌ Sorting the in-memory `Files` list (hash sort is write-time only)
- ❌ Eager hash table compute (must re-derive at write time)
- ❌ Bare `sf_dcx_type_t compression` (must be `sf_dcx_compression_info_t compression_info`)
- ❌ `int64_t id` (must be `int32_t id` mirroring upstream Int32)
- ❌ Skipping any unknown fields (`Unk*`, `WriteFileHeadersEnd`, `Unicode`, `Extended`)
- ❌ Assuming "BHD5" magic on Data0.bhd (must detect RSA-encrypted state via first 4 bytes)
- ❌ AES range-decrypt without bounds-checking against actual BDT length
- ❌ Sort-on-read for hash tables (silent reordering breaks round-trip)
- ❌ Implementing DDS pixel decoding in TPF (only header parse allowed)
- ❌ Implementing Xbox360/Xbone/PS5 Headerize paths (return `SF_ERR_UNSUPPORTED_VERSION`)
- ❌ Implementing PS3/PS4 Headerize write path beyond what's needed (capped to PC in v1)
- ❌ Asserting byte-equal round-trip on real ER files (semantics is content-equal only)

**Process-level forbidden patterns**:
- ❌ Per-file `// disable warning` or `#pragma warning` (`-Werror` violations must be fixed at source)
- ❌ AI slop: pseudo-helpers like `_helper_internal_v2`, redundant comments, dead code, generic names (`data`, `result`, `temp`)
- ❌ Defining a private DCX type per format (use `sf_dcx_compression_info_t` from `sf_dcx.h` directly)
- ❌ Skipping `bw.Pad(0x10)` before file data when `bytes_size > 0` (BinderFileHeader.cs:236)
- ❌ Stub `_Static_assert` (every enum needs one verifying `sizeof` and table alignment)
- ❌ Embedding game files in the test fixtures dir (only synthetic ≤4KB samples)
- ❌ Embedding `oo2core_*.dll` anywhere in the repo
- ❌ Use of `fopen/fread/fwrite` (must use `sf_istream_t`/`sf_ostream_t`)
- ❌ Reservation without matching Fill (every `sf_binary_writer_reserve_*` must be paired)
- ❌ Splitting a single format's work across multiple commits without all tests green at each commit
- ❌ Touching files outside the task's declared scope (cross-task contamination)

**Architectural forbidden patterns** (per AGENTS.md §5.x):
- ❌ Implementation that doesn't strictly mirror upstream `.cs` files (no guessing on wire formats)
- ❌ Public API divergence from upstream class shape (only allowed: snake_case, out-param errors, pointer ownership)
- ❌ Dynamic registry / runtime dispatch for backend selection (CMake-time selection only)
- ❌ Spawning a thread inside the library (single-context single-thread per PLAN.md §1.3)

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — all verification is agent-executed. No "user manually checks ...".

### Test Decision (D4 confirmed)
- **Infrastructure exists**: YES (Unity 2.6.1 already wired in Phase 0; `sf_add_test()` helper; `ctest` integrated)
- **Automated tests**: YES — synthetic round-trip inline with each format task; ER e2e as separate task per format
- **Framework**: Unity (ThrowTheSwitch)
- **Style**: tests-with-impl (synthetic round-trip is the format's acceptance criterion; e2e is layered separately)
- **Round-trip semantics**: synthetic fixtures = byte-equal; real ER files = content-equal (file count, lookup-by-hash, decompressed magic)

### QA Policy
Every task MUST include agent-executed QA scenarios (see TODO template). Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Library/Module tests** (the bulk of Phase 3): Use Bash with `ctest --test-dir build-mingw -R '^<test>$' --output-on-failure -V` — captures stdout to evidence dir
- **CLI examples**: Use `interactive_bash` (tmux) — runs `sf_bnd_extract.exe` via WSL interop, validates extracted files on disk
- **DLL exports**: Use Bash + `x86_64-w64-mingw32-objdump -p libsouls_formats.dll | grep -c 'sf_'` — counts exported `sf_*` symbols
- **No browser/UI required** for this phase

### Skip Behavior (mandatory)
Each ER e2e test must check preconditions and SKIP (not fail) gracefully when:
- `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` missing → `TEST_IGNORE_MESSAGE("ER copy not found")`
- Data0.bhd first 4 bytes are NOT `e1 0e 36 ab` AND NOT `BHD5` → `TEST_IGNORE_MESSAGE("Data0.bhd has unknown magic; UXM-unpack required or RSA key drift")`
- Oodle DLL missing under `/home/soar/dev/oodle/` → `TEST_IGNORE_MESSAGE("Oodle DLL not found")`

---

## Execution Strategy

### Parallel Execution Waves

> Maximize throughput by grouping independent tasks into parallel waves. Each wave completes before the next begins.

```
Wave 0 (PREREQUISITES — must land before any Wave 1+; all parallel):
├── T0a: sf_reverse_bits_u8 (Phase 1 retro-fit)              [quick]
├── T0b: sfi_aes_decrypt_ecb_buffer (Phase 2 retro-fit)      [quick]
├── T0c: sf_path_hash_64 (Phase 1 retro-fit, ER+ algorithm)  [unspecified-low]
├── T0d: RSA decrypt + 4 game PEM keys                       [unspecified-high]
└── T0e: sfi_dds_parse_header (~50 LOC minimal)              [quick]

Wave 1 (Foundation — depend on Wave 0; mostly parallel):
├── T1: sf_binder.h shared types        (depends T0a)        [quick]
├── T2: binder_common.c shared helpers  (depends T1)         [unspecified-high]
├── T3: bhd5_keys.c (4 game AES keys)   (depends T0d)        [quick]
├── T4: er_test_helper.h skeleton                            [quick]
└── T5: sf_bnd_extract.c skeleton                            [quick]

Wave 2 (Per-format implementation — depend on Wave 1; max parallel):
├── T6:  BND3 + synthetic test          (depends T1, T2)     [unspecified-high]
├── T7:  BND4 + synthetic test          (depends T1, T2)     [deep]
├── T8:  BXF3 + synthetic test          (depends T1, T2)     [unspecified-high]
├── T9:  BXF4 + synthetic test          (depends T1, T2)     [unspecified-high]
├── T10: BHD5 + synthetic test          (depends T0b, T0d, T3) [deep]
├── T11: TPF + Headerizer PC-only + synthetic (depends T0e, T1) [unspecified-high]
├── T12: ENFL + synthetic test                               [quick]
└── T13: api-mapping doc updates (8 docs flip to ✓ aligned)  [writing]

Wave 3 (Integration & e2e — depend on Wave 2):
├── T14: er_test_helper.c impl          (depends T7, T10)    [unspecified-high]
├── T15: test_bhd5_e2e_er (KEYSTONE)    (depends T10, T14)   [unspecified-high]
├── T16: test_bnd4_e2e_er               (depends T7, T14)    [unspecified-low]
├── T17: test_bxf4_e2e_er               (depends T9, T14)    [unspecified-low]
├── T18: test_tpf_e2e_er                (depends T7, T11, T14) [unspecified-low]
├── T19: sf_bnd_extract.c impl          (depends T7)         [quick]
└── T20: PLAN.md retro + AGENTS.md + roadmap README update   [writing]

Wave FINAL (Verification — 4 parallel reviews + user okay):
├── F1: Plan compliance audit           (oracle)
├── F2: Code quality review             (unspecified-high)
├── F3: Real manual QA (full ER chain)  (unspecified-high)
└── F4: Scope fidelity check            (deep)
→ Present consolidated results → wait for user explicit "okay" → mark Phase 3 done

Critical Path: T0d → T0b → T1 → T2 → T7 ∥ T10 → T14 → T15 → F1-F4 → user okay
Parallel Speedup: ~70% faster than sequential (Wave 2 has 8 concurrent tasks)
Max Concurrent: 8 (Wave 2)
```

### Dependency Matrix

| Task | Depends On | Blocks |
|---|---|---|
| T0a | (none — Phase 1 retro-fit) | T1, T2 |
| T0b | (none — Phase 2 retro-fit) | T10 |
| T0c | (none — Phase 1 retro-fit) | T10 (BHD5 uses 64-bit hash for ER+) |
| T0d | (none) | T3, T10 |
| T0e | (none) | T11 |
| T1 | T0a | T2, T6, T7, T8, T9, T11 |
| T2 | T0a, T1 | T6, T7, T8, T9 |
| T3 | T0d | T10 |
| T4 | (none — skeleton only) | T14 |
| T5 | (none — skeleton only) | T19 |
| T6 | T1, T2 | T13 |
| T7 | T1, T2 | T13, T14, T16, T18, T19 |
| T8 | T1, T2 | T13 |
| T9 | T1, T2 | T13, T17 |
| T10 | T0b, T0c, T0d, T3 | T13, T14, T15 |
| T11 | T0e, T1 | T13, T18 |
| T12 | T1 | T13 |
| T13 | T6-T12 | T20 |
| T14 | T7, T10 | T15, T16, T17, T18 |
| T15 | T10, T14 | T20 |
| T16 | T7, T14 | T20 |
| T17 | T9, T14 | T20 |
| T18 | T7, T11, T14 | T20 |
| T19 | T7 | T20 |
| T20 | T13, T15-T19 | F1-F4 |
| F1-F4 | T20 (and all impl tasks) | user okay |

### Agent Dispatch Summary

| Wave | Tasks | Agent Profiles |
|---|---|---|
| 0 | 5 | T0a/T0b/T0e → `quick`; T0c → `unspecified-low`; T0d → `unspecified-high` |
| 1 | 5 | T1/T3/T4/T5 → `quick`; T2 → `unspecified-high` |
| 2 | 8 | T6/T8/T9/T11 → `unspecified-high`; T7/T10 → `deep`; T12 → `quick`; T13 → `writing` |
| 3 | 7 | T14/T15 → `unspecified-high`; T16/T17/T18 → `unspecified-low`; T19 → `quick`; T20 → `writing` |
| Final | 4 | F1 → `oracle`; F2/F3 → `unspecified-high`; F4 → `deep` |

---

## TODOs

> Implementation + Test = ONE Task. Never separate.
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + QA Scenarios.
> **A task WITHOUT QA Scenarios is INCOMPLETE. No exceptions.**

- [x] T0a. **Add `sf_reverse_bits_u8` to sf_io.h (Phase 1 retro-fit)**

  **What to do**:
  - Add `SF_API uint8_t sf_reverse_bits_u8(uint8_t b);` declaration to `include/souls_formats/sf_io.h` (place near other byte-order utilities).
  - Implement in `src/core/binary_reader.c` (or split into a new `src/core/bitops.c` if cleaner; pick whichever matches existing layout).
  - Reference upstream: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/EndianHelper.cs` — `ReverseBits(byte b)` static method, bit-reverse via shift-and-mask sequence (1<->8, 2<->7, 3<->6, 4<->5).
  - Add 1 sub-test to `tests/core/test_binary_reader.c` (or new `test_bitops.c`): assert `sf_reverse_bits_u8(0x01) == 0x80`, `0xAB == 0xD5`, `0x00 == 0x00`, `0xFF == 0xFF`, `0x42 == 0x42`.
  - Update `docs/api-mapping/util-io-binary-reader-ex.md` to add a row for `EndianHelper.ReverseBits` → `sf_reverse_bits_u8` `✓ aligned`.

  **Must NOT do**:
  - Use a lookup table (LUT) — upstream uses arithmetic shifts; mirror exactly.
  - Inline this as a private static helper inside `binder_common.c` — it's general-purpose infra, must be public.
  - Forget `SF_API` decoration.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single function, ≤30 LOC, mechanical port from C# bit ops.
  - **Skills**: `[]`
    - No specialized skill needed.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0 (with T0b, T0c, T0d, T0e)
  - **Blocks**: T1, T2 (binder_common reads/writes Format byte using ReverseBits)
  - **Blocked By**: None — can start immediately

  **References**:
  - Pattern: `src/core/binary_reader.c:sf_binary_reader_assert_pattern` — example of small public utility implementation style
  - Pattern: `include/souls_formats/sf_io.h` near line 100 — where to place the public declaration
  - API: Upstream `EndianHelper.ReverseBits` in `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/EndianHelper.cs` — exact algorithm to mirror (verified path)
  - Test: `tests/core/test_binary_reader.c` — test file structure to mirror for the new sub-test
  - WHY each: Pattern files show C-idiomatic implementation style; upstream `.cs` is the canonical algorithm; test file shows Unity wiring pattern in this project.

  **Acceptance Criteria**:
  - [ ] `sf_reverse_bits_u8` declared in `sf_io.h` with `SF_API`
  - [ ] Implementation in `src/core/*.c`, ≤30 LOC
  - [ ] Sub-test asserts 5 known values pass
  - [ ] `cmake --build build-mingw` 0 warnings (`-Werror`)
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_binary_reader$' --output-on-failure` PASS
  - [ ] DLL export count increases by 1 (`objdump -p libsouls_formats.dll | grep -c 'sf_reverse_bits'` == 1)

  **QA Scenarios**:

  ```
  Scenario: Bit-reverse 5 known values
    Tool: Bash (`ctest`)
    Preconditions: build-mingw is configured; T0a impl + test merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_binary_reader 2>&1 | tee .sisyphus/evidence/task-T0a-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_binary_reader$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T0a-ctest.log
      3. grep -E 'PASS|FAIL' .sisyphus/evidence/task-T0a-ctest.log
    Expected Result: All sub-tests PASS, including new `test_sf_reverse_bits_u8_known_values` showing 5 passes
    Failure Indicators: Any "FAIL" line in ctest output; build warnings; unresolved symbol on DLL link
    Evidence: .sisyphus/evidence/task-T0a-build.log + .sisyphus/evidence/task-T0a-ctest.log

  Scenario: Symbol exported in DLL
    Tool: Bash (`objdump`)
    Preconditions: build-mingw produces `libsouls_formats.dll`
    Steps:
      1. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep 'sf_reverse_bits' | tee .sisyphus/evidence/task-T0a-objdump.log
      2. wc -l .sisyphus/evidence/task-T0a-objdump.log
    Expected Result: Output contains exactly 1 line referencing `sf_reverse_bits_u8`
    Failure Indicators: 0 matches (not exported) or >1 matches (duplicate)
    Evidence: .sisyphus/evidence/task-T0a-objdump.log
  ```

  **Evidence to Capture**: build log, ctest log, objdump log

  **Commit**: YES (groups with NONE)
  - Message: `core(io): add sf_reverse_bits_u8 (Phase 1 retro-fit for binder)`
  - Files: `include/souls_formats/sf_io.h`, `src/core/binary_reader.c` (or `src/core/bitops.c`), `tests/core/test_binary_reader.c`, `docs/api-mapping/util-io-binary-reader-ex.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T0b. **Expose `sfi_aes_decrypt_ecb_buffer` for BHD5 range decrypt (Phase 2 retro-fit)**

  **What to do**:
  - Add internal helper to `src/crypto/aes_cng.h` (header is internal-only, not under `include/souls_formats/`):
    ```c
    /* Decrypt N×16 bytes in-place using AES-128-ECB.
     * key: 16-byte AES key. buf: N×16 bytes (multiple-of-16 enforced via assert).
     * Returns SF_OK or SF_ERR_CRYPTO. Internal-only; not SF_API. */
    sf_result_t sfi_aes_decrypt_ecb_buffer(const uint8_t key[16],
                                           uint8_t *buf, size_t size);
    ```
  - Implement in `src/crypto/aes_cng.c` using existing `BCryptOpenAlgorithmProvider("AES")` + `BCRYPT_CHAIN_MODE_ECB`. Reuse the same algorithm provider lifetime pattern as `sfi_aes_decrypt_cbc`.
  - Add 1 sub-test to `tests/crypto/test_aes_kat.c` using a NIST CAVP AES-128-ECB vector (key + 32-byte plaintext + 32-byte ciphertext): decrypt 32 bytes in-place and assert match.
  - Bound assertion: `size % 16 == 0` (return `SF_ERR_INVALID_ARG` if not).

  **Must NOT do**:
  - Re-open the BCrypt algorithm handle per call (cache it via `static` + `init_once`).
  - Allow `size` not multiple of 16.
  - Expose this via `SF_API` — it's an internal helper for BHD5 only.
  - Allocate output buffer (in-place only).
  - Forget to set `BCRYPT_CHAIN_MODE_ECB` (default chaining mode is CBC and would silently corrupt).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Wraps existing AES infra; ≤60 LOC including bounds check.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T10 (BHD5 range decrypt requires this)
  - **Blocked By**: None

  **References**:
  - Pattern: `src/crypto/aes_cng.c:sfi_aes_decrypt_cbc` — existing CBC impl shows BCrypt provider pattern, error mapping, key handle lifecycle
  - Pattern: `src/crypto/aes_cng.h` — internal-only header (no SF_API, separated from public include/)
  - API: Upstream `BHD5.cs:689-700` `AESKey.Decrypt` — the consumer pattern; decrypts windows of 16-byte multiples in-place
  - Test: `tests/crypto/test_aes_kat.c` — Phase 2 already has NIST CAVP test pattern; mirror sub-test structure
  - External: NIST CAVP vectors at https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/block-ciphers — pick AES-128 ECB Encrypt vectors (KEY+PLAINTEXT+CIPHERTEXT triples, multiple-of-16-byte plaintext)
  - WHY each: Existing CBC impl is the closest analog; AESKey.Decrypt shows the exact consumer call shape; existing test file is the merge target.

  **Acceptance Criteria**:
  - [ ] `sfi_aes_decrypt_ecb_buffer` declared in `src/crypto/aes_cng.h` (internal)
  - [ ] Implementation cached BCrypt provider (no per-call open)
  - [ ] Sub-test in `test_aes_kat.c` decrypts 32-byte NIST vector, byte-equal to expected plaintext
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_aes_kat$' --output-on-failure` PASS (existing + 1 new sub-test)
  - [ ] DLL export count UNCHANGED (this is internal, not `SF_API`)

  **QA Scenarios**:

  ```
  Scenario: NIST CAVP vector decrypts in-place
    Tool: Bash (`ctest`)
    Preconditions: build-mingw configured; T0b impl + test merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_aes_kat 2>&1 | tee .sisyphus/evidence/task-T0b-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_aes_kat$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T0b-ctest.log
      3. grep -E 'test_aes_decrypt_ecb_buffer_kat' .sisyphus/evidence/task-T0b-ctest.log
    Expected Result: New sub-test `test_aes_decrypt_ecb_buffer_kat` reports PASS; existing AES-128-ECB/CBC/AES-256-CBC sub-tests still PASS
    Failure Indicators: FAIL line; byte-mismatch error; OOM
    Evidence: .sisyphus/evidence/task-T0b-build.log + .sisyphus/evidence/task-T0b-ctest.log

  Scenario: Bounds check rejects non-multiple-of-16 input
    Tool: Bash (in-test assertion)
    Preconditions: T0b impl merged
    Steps:
      1. The new sub-test must call sfi_aes_decrypt_ecb_buffer with size=15 and assert return == SF_ERR_INVALID_ARG
      2. Captured in same ctest log as scenario above
    Expected Result: bounds-check sub-assertion PASS
    Failure Indicators: SF_OK returned for size=15; segfault
    Evidence: .sisyphus/evidence/task-T0b-ctest.log (same file)

  Scenario: Internal symbol NOT exported
    Tool: Bash (`objdump`)
    Preconditions: build-mingw produces libsouls_formats.dll
    Steps:
      1. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -c 'sfi_aes_decrypt_ecb_buffer' | tee .sisyphus/evidence/task-T0b-objdump.log
    Expected Result: Output is "0" (internal symbol must NOT leak via DLL exports)
    Failure Indicators: Output > 0
    Evidence: .sisyphus/evidence/task-T0b-objdump.log
  ```

  **Evidence to Capture**: build log, ctest log, objdump log

  **Commit**: YES
  - Message: `crypto(aes): expose sfi_aes_decrypt_ecb_buffer for BHD5 range decrypt`
  - Files: `src/crypto/aes_cng.h`, `src/crypto/aes_cng.c`, `tests/crypto/test_aes_kat.c`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T0c. **Add `sf_path_hash_64` thin wrapper for ER+ BHD5 (Phase 1 retro-fit)**

  **What to do**:
  - **CORRECTED understanding** (verified against upstream): `BHD5.cs:472-478` reads a 64-bit `UInt64 FileNameHash`, but `BHD5.cs:432` says "Hash of the full file path using From's algorithm found in SFUtil.FromPathHash" — i.e. the **SAME 32-bit FromPathHash algorithm**, just zero-extended to 64 bits when stored on disk in ER+. There is no separate 64-bit algorithm in upstream `HashHelper.cs` (which contains ONLY `public static uint FromPathHash(string text)` returning `uint`).
  - Add `SF_API uint64_t sf_path_hash_64(const char *utf8_path);` declaration to `include/souls_formats/sf_hash.h`.
  - Implementation is a thin convenience wrapper: `return (uint64_t)sf_path_hash(utf8_path);` — same fold (`hashable.Aggregate(0u, (i, c) => i * 37u + c)` from `HashHelper.cs:18`), just widened to 64 bits. Same normalization rules (lowercase, backslash → forward slash, prefix `/` if absent, NULL → 0).
  - **Why expose as a separate function**: BHD5 ER+ FileHeader.Write (T10) emits `UInt64 FileNameHash`. Having a named 64-bit accessor makes consumer intent explicit and prevents accidental bit-truncation when stored in 64-bit fields. Internal calls in `bhd5.c` may use `(uint64_t)sf_path_hash(path)` directly OR `sf_path_hash_64(path)` interchangeably.
  - Add 1 sub-test to `tests/core/test_filename_hash.c`:
    1. **Equivalence vector**: assert `sf_path_hash_64(p) == (uint64_t)sf_path_hash(p)` for the existing 16 Phase 1 golden paths.
    2. **Real-BHD5 ground-truth verification** (cross-check, runs only when ER copy + Oodle present): after RSA-decrypting `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` (or via `er_helper_init` once T14 lands; until then use a saved fixture), parse out a few `FileHeader.FileNameHash` values from real bucket entries and assert `sf_path_hash_64(known_path) == observed_uint64_hash` for at least 3 well-known ER paths (e.g. `/chr/c0000.chrbnd.dcx`, `/map/mapstudio/m60_42_36_00.msb.dcx`, `/material/allmaterial.matbinbnd.dcx`). This sub-test is conditional — SKIP gracefully if env missing. **Until T10/T14 land**, this scenario is deferred to T15 keystone test as a cross-check; T0c lands with only the equivalence vector sub-test.
  - Update `docs/api-mapping/util-cryptography-hash-helper.md` adding new row: `sf_path_hash_64` mapping to `(ulong)HashHelper.FromPathHash` cast pattern used implicitly in BHD5 ER+ branch.
  - Add to `docs/api-mapping/extensions.md`: row "`sf_path_hash_64` — extension. Upstream `HashHelper.FromPathHash` returns `uint` (32-bit). BHD5 ER+ stores hash as `UInt64` on disk via implicit cast. We expose a named 64-bit wrapper for consumer clarity. **Same algorithm, no functional divergence** — just a widening cast."

  **Must NOT do**:
  - Invent a separate 64-bit fold algorithm — there isn't one in upstream.
  - Skip the equivalence sub-test asserting `sf_path_hash_64(p) == (uint64_t)sf_path_hash(p)`.
  - Add the real-BHD5 cross-check before T10/T14 are available; defer it to T15 keystone instead (or land as `TEST_IGNORE_MESSAGE("requires Phase 3 BHD5 + er_helper")` placeholder in T0c).
  - Forget normalization rules (already inherited via wrapper).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Algorithm-driven port; small but needs ground-truth verification (running Python harness against C# upstream).
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T10 (BHD5 ER+ branch uses 64-bit hash)
  - **Blocked By**: None

  **References**:
  - Pattern: `src/core/filename_hash.c:sf_path_hash` — existing 32-bit impl, copy normalization but change fold algorithm
  - Pattern: `tests/core/test_filename_hash.c` — Phase 1 has 16 golden 32-bit vectors; mirror style for 64-bit
  - API: Upstream `BHD5.cs:472-478` (ER+ branch in FileHeader.Read) — has `br.ReadUInt64()` for FileNameHash; check upstream `HashHelper.cs` or any 64-bit hash function called for these paths
  - WHY each: Existing 32-bit impl is the canonical normalization template; BHD5.cs ER+ branch is the consumer that demands this function; HashHelper.cs is where upstream algorithm constants live.

  **Acceptance Criteria**:
  - [ ] `sf_path_hash_64` declared in `sf_hash.h` with `SF_API`
  - [ ] Implementation is a thin `(uint64_t)sf_path_hash(p)` wrapper — verified by reading the source file (≤5 LOC of impl)
  - [ ] Equivalence sub-test: for each of the 16 existing Phase 1 golden paths, `sf_path_hash_64(p) == (uint64_t)sf_path_hash(p)` PASSES
  - [ ] Normalization parity with `sf_path_hash` (NULL, backslash, lowercase, prefix all tested via wrapper inheritance)
  - [ ] DLL export count increases by 1
  - [ ] `extensions.md` updated with extension row clarifying "same algorithm, widening cast" — NO claim of distinct algorithm
  - [ ] Real-BHD5 cross-check sub-test placeholder added but `TEST_IGNORE_MESSAGE` until T15 keystone validates

  **QA Scenarios**:

  ```
  Scenario: Equivalence wrapper test (16 golden vectors from Phase 1)
    Tool: Bash (`ctest`)
    Preconditions: build-mingw configured; T0c merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_filename_hash 2>&1 | tee .sisyphus/evidence/task-T0c-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_filename_hash$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T0c-ctest.log
      3. grep -E 'test_sf_path_hash_64_equivalence' .sisyphus/evidence/task-T0c-ctest.log
    Expected Result: New sub-test reports PASS asserting sf_path_hash_64(p) == (uint64_t)sf_path_hash(p) for all 16 golden paths; existing 32-bit hash sub-tests still PASS
    Failure Indicators: FAIL on any path; inequality between 64-bit and widened 32-bit
    Evidence: .sisyphus/evidence/task-T0c-build.log + .sisyphus/evidence/task-T0c-ctest.log

  Scenario: Normalization parity (NULL, slash, case)
    Tool: Bash (in-test assertion)
    Preconditions: T0c merged
    Steps:
      1. New sub-test asserts (matching existing Phase 1 `sf_path_hash` semantics — verified at `tests/core/test_filename_hash.c:67`):
         - sf_path_hash_64(NULL) == (uint64_t)0x0000002fu  (matches `sf_path_hash(NULL)` which folds the empty-path-prefix "/" into 0x2f)
         - sf_path_hash_64(NULL) == (uint64_t)sf_path_hash(NULL)  (wrapper equivalence)
         - sf_path_hash_64("/chr/c0000.chrbnd.dcx") == sf_path_hash_64("\\chr\\C0000.CHRBND.DCX")  (backslash + case folding)
         - sf_path_hash_64("chr/c0000.chrbnd.dcx") == sf_path_hash_64("/chr/c0000.chrbnd.dcx")  (auto-prefix slash)
      2. Captured in same ctest log
    Expected Result: 4 normalization sub-assertions PASS
    Failure Indicators: any inequality; segfault on NULL
    Evidence: .sisyphus/evidence/task-T0c-ctest.log (same file)

  Scenario: Implementation is genuinely a thin wrapper (anti-divergence guard)
    Tool: Bash (`grep` source)
    Preconditions: T0c merged
    Steps:
      1. wc -l src/core/filename_hash.c | tee .sisyphus/evidence/task-T0c-loc.log
      2. grep -E 'sf_path_hash_64' src/core/filename_hash.c | tee -a .sisyphus/evidence/task-T0c-loc.log
    Expected Result: Implementation function body is ≤ 5 LOC AND mentions `sf_path_hash` (the 32-bit function) — proves it's a wrapper, not a re-implementation
    Failure Indicators: 64-bit impl > 10 LOC OR doesn't reference sf_path_hash → indicates someone re-derived a separate algorithm
    Evidence: .sisyphus/evidence/task-T0c-loc.log
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `core(hash): add sf_path_hash_64 for ER+ BHD5 (64-bit fold)`
  - Files: `include/souls_formats/sf_hash.h`, `src/core/filename_hash.c`, `tests/core/test_filename_hash.c`, `docs/api-mapping/util-cryptography-hash-helper.md`, `docs/api-mapping/extensions.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T0d. **Add `sf_bhd5.h` skeleton + `sfi_rsa_decrypt_pkcs1` + 4 game PEM keys (BHD5 RSA unwrap layer)**

  **What to do**:
  - **NEW (per Momus review)**: Also create `include/souls_formats/sf_bhd5.h` SKELETON with the public types that downstream tasks (T3, T4, T10) reference — ensures every dependent task can `#include` it and compile cleanly:
    ```c
    /* sf_bhd5.h — Phase 3 BHD5 archive container.
     * T0d adds: opaque forward decl + sf_bhd5_game_t enum.
     * T10 adds: open/close/extract function declarations. */
    #include "sf_common.h"

    /* Opaque BHD5 reader handle. Defined in src/archive/bhd5.c (T10). */
    typedef struct sf_bhd5 sf_bhd5_t;

    /* Game enum for BHD5 key + format selection. v1 covers 4 target games.
     * Extension per docs/api-mapping/extensions.md (upstream uses 5-value DS1..ER set;
     * we add Sekiro/Nightreign/AC6 — they share ER's wire format with different keys). */
    typedef enum sf_bhd5_game {
        SF_BHD5_GAME_SEKIRO       = 0,
        SF_BHD5_GAME_ELDENRING    = 1,
        SF_BHD5_GAME_NIGHTREIGN   = 2,
        SF_BHD5_GAME_ARMOREDCORE6 = 3,
        SF_BHD5_GAME_COUNT_       /* drift-guard sentinel (not a real game) */
    } sf_bhd5_game_t;
    _Static_assert(SF_BHD5_GAME_COUNT_ == 4, "sf_bhd5_game_t drift");
    ```
  - Add `sf_bhd5.h` to `include/souls_formats/souls_formats.h` umbrella include list.
  - Create new internal-only header `src/crypto/rsa_cng.h`:
    ```c
    /* Decrypt RSA-encrypted block using NoPadding mode (which upstream community
     * uses — encrypt path is OAEP-equivalent but decrypt is raw modular exponentiation
     * with the public key, then strip leading zero padding). Signature mirrors AES helpers. */
    sf_result_t sfi_rsa_decrypt_pkcs1(const char *pem_public_key,
                                      const uint8_t *in, size_t in_size,
                                      uint8_t **out, size_t *out_size,
                                      const sf_allocator_t *alloc);
    ```
  - Implement in `src/crypto/rsa_cng.c` using Win32 CNG (`BCryptOpenAlgorithmProvider("RSA")` + `BCryptDecrypt` with `BCRYPT_PAD_NONE`). Parse the PEM key via `CryptStringToBinaryA` (CRYPT_STRING_BASE64HEADER) → `CryptDecodeObjectEx` (X509_PUBLIC_KEY_INFO) → import into BCrypt key handle.
  - Embed the 4 game public PEM keys as `static const char SF_BHD5_PEM_KEY_<GAME>[]` constants in `src/archive/bhd5_keys.c` (NOT `rsa_cng.c` — keep crypto generic, archive-specific keys live with archive). Keys are publicly known in the soulsmods community (see SoulsFormatsNEXT issue tracker / TKGP commits / community repos like UXM source).
  - **CROSS-TRANSLATION-UNIT ACCESS** (per Momus review): the `static const` PEM constants are file-local; T10/`bhd5.c` and `tests/crypto/test_rsa.c` cannot access them directly. T0d MUST add an internal accessor to `src/archive/bhd5_keys.h`:
    ```c
    /* Returns the embedded public PEM key for a game enum value, or NULL if unknown.
     * Pointer is owned by the static data segment; do not free. */
    const char *sfi_bhd5_get_pem_key(sf_bhd5_game_t game);
    ```
    Implementation in `bhd5_keys.c` is a switch returning the matching `static const char *`. **T3** later adds the parallel `sfi_bhd5_get_aes_key` accessor for AES keys — T0d only adds `sfi_bhd5_get_pem_key`.
  - Game→key mapping: SF_BHD5_GAME_SEKIRO / SF_BHD5_GAME_ELDENRING / SF_BHD5_GAME_NIGHTREIGN / SF_BHD5_GAME_ARMOREDCORE6 each maps to its known public PEM. AC6 + Nightreign may use the same key as ER on shipped builds — implementing agent must verify via test fixture, not assume.
  - Add `tests/crypto/test_rsa.c` with TWO sub-tests:
    1. **Throwaway-keypair functional test**: at test fixture build (via `add_custom_command`), generate a fresh test keypair via openssl. Encrypt plaintext with the test PRIVATE key (raw modular exp via `openssl rsautl -sign -raw`). Then load the test PUBLIC PEM into `sfi_rsa_decrypt_pkcs1` and assert byte-equal round-trip back to the original plaintext (after leading-zero stripping). This validates the C wrapper / CNG plumbing without needing a real game's private key. SKIP if openssl missing.
    2. **Real-BHD5 magic-byte test**: read first 256 bytes of `/mnt/c/Games/ELDEN RING/Game/Data0.bhd`; call `sfi_rsa_decrypt_pkcs1` with `SF_BHD5_PEM_KEY_ELDENRING`; assert decrypted bytes start with "BHD5" (the only observable success indicator without the game's private key). SKIP if ER copy not present.
  - **NOTE**: We CANNOT round-trip with public-key-only on a real game key because that would require From's private key. The throwaway keypair test isolates the C wrapper correctness; the real-BHD5 test validates the embedded PEM is correct for the production ER build.
  - Add to `docs/api-mapping/extensions.md`: row "`sfi_rsa_decrypt_pkcs1` + game PEM keys — extension. Upstream BHD5 punts on RSA layer ('Must already be decrypted, if applicable.'); we integrate it for autonomy."

  **Must NOT do**:
  - Embed PRIVATE keys — only PUBLIC keys are needed (decrypt-with-public is RSA verify-style; data was sign-encrypted by FromSoft).
  - Use `BCRYPT_PAD_PKCS1` or `BCRYPT_PAD_OAEP` — upstream community uses `BCRYPT_PAD_NONE` (raw modular exp); the BHD wrapping is custom.
  - Forget to strip leading zero bytes after raw RSA decrypt (output may have leading 0x00 padding).
  - Cache the BCrypt key handle across calls (different games need different keys; cache per-game-enum key handle if optimizing).
  - Hardcode keys outside `bhd5_keys.c` — the keys are archive-specific data, must live there.
  - Distribute Oodle DLL or any game asset under any pretext.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Crypto + multi-file integration + sensitive (PEM keys, MUST be public-only). Higher effort for correctness verification.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T3 (bhd5_keys.c uses these constants), T10 (BHD5 read auto-detects RSA wrap)
  - **Blocked By**: None

  **References**:
  - Pattern: `src/crypto/aes_cng.c` — Phase 2 BCrypt provider lifetime + error mapping
  - Pattern: `src/crypto/regulation.c` — Phase 2 example of encrypted file format with embedded key
  - API: Upstream `BHD5.cs:107` comment — the only line acknowledging RSA layer exists outside upstream's scope
  - External: Microsoft CNG docs https://learn.microsoft.com/en-us/windows/win32/seccng/cng-cryptographic-primitive-functions — `BCryptOpenAlgorithmProvider`, `BCryptDecrypt`, `BCRYPT_PAD_NONE`
  - External: Community-known PEM keys — source these from public repos like soulsmods/UXM-Selective-Unpack or TKGP's pinned commits; the implementing agent must cite a public source URL in the file header comment of `bhd5_keys.c`
  - WHY each: AES wrappers are the closest existing crypto integration template; regulation shows embedded-key precedent; BHD5.cs:107 confirms scope demarcation; CNG docs are authoritative API ref; community keys must be cited to maintain GPL provenance.

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_bhd5.h` SKELETON exists with opaque type + `sf_bhd5_game_t` enum + drift _Static_assert; included from umbrella `souls_formats.h`
  - [ ] `src/crypto/rsa_cng.{h,c}` exist; header is internal-only (not under `include/`)
  - [ ] 4 game PEM keys embedded in `bhd5_keys.c` with citation comment per key
  - [ ] `src/archive/bhd5_keys.h` exposes `sfi_bhd5_get_pem_key(sf_bhd5_game_t)` accessor; `bhd5_keys.c` implements it
  - [ ] **`test_rsa.c` Sub-test 1 (throwaway-keypair functional test)** PASSES — verifies `sfi_rsa_decrypt_pkcs1` C wrapper is correct using a generated test keypair we hold both halves of (per QA Scenario 1)
  - [ ] **`test_rsa.c` Sub-test 2 (real-BHD5 magic-byte test)** PASSES OR SKIPS-gracefully — when ER copy present, asserts decrypted bytes start with "BHD5" using the embedded `SF_BHD5_PEM_KEY_ELDENRING` (per QA Scenario 2). When ER copy absent → `TEST_IGNORE_MESSAGE`
  - [ ] **NOT** asserting public-key-only encrypt+decrypt round-trip on real game keys (mathematically impossible without the private key — see QA Scenarios for explanation)
  - [ ] Decrypt path tolerates leading zero padding (verified via Sub-test 1)
  - [ ] BCrypt provider cached across calls for the same game
  - [ ] DLL export count after T0d INCREASES by 0 from public types (sf_bhd5_t opaque + sf_bhd5_game_t enum are types, not symbols); rsa_cng symbols all internal; PEM accessor internal
  - [ ] `extensions.md` records the deviation
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_rsa$' --output-on-failure` PASS or SKIP

  **QA Scenarios**:

  > **CORRECTED RSA validation strategy** (per Momus review):
  > Public-key-only encrypt+decrypt cannot mathematically round-trip a synthetic plaintext (decrypt requires private key). We use TWO complementary scenarios:
  > 1. **Throwaway-keypair functional test** — verifies the C wrapper / CNG plumbing is correct using a generated test keypair (we hold both halves).
  > 2. **Real-BHD5 magic-byte validation** — verifies the embedded GAME public PEM correctly decrypts From's encrypted blob, observable as the "BHD5" magic appearing after raw-RSA modular exponentiation strips off the From private key's encryption.

  ```
  Scenario 1: Throwaway-keypair RSA round-trip (functional verification of the C wrapper)
    Tool: Bash + openssl (test fixture generation pre-build)
    Preconditions: build-mingw configured; openssl in PATH; T0d merged
    Steps:
      1. At test fixture generation time (CMake `add_custom_command` or first ctest invocation), generate a throwaway 2048-bit RSA keypair:
         openssl genrsa -out /tmp/sf-test-rsa.pem 2048
         openssl rsa -in /tmp/sf-test-rsa.pem -pubout -out /tmp/sf-test-rsa-pub.pem
      2. Generate a 100-byte plaintext "fixture-plaintext-from-test"
      3. Encrypt with the PRIVATE key (raw modular exp, no padding):
         openssl rsautl -sign -inkey /tmp/sf-test-rsa.pem -raw -in /tmp/plaintext.bin -out /tmp/encrypted.bin
         (or use openssl rsa -engine -- whatever produces a raw RSA-encrypted block matching the From wrap pattern)
      4. The test (`test_rsa.c`) loads /tmp/sf-test-rsa-pub.pem as the public key, /tmp/encrypted.bin as input
      5. Calls sfi_rsa_decrypt_pkcs1(test_pub_pem, encrypted, encrypted_size, &out, &out_size, NULL)
      6. Asserts SF_OK; asserts decrypted bytes (after stripping leading zero padding) byte-equal to original 100-byte plaintext
      7. cmake --build build-mingw --target souls_formats_test_rsa 2>&1 | tee .sisyphus/evidence/task-T0d-build.log
      8. ctest --test-dir build-mingw -R '^souls_formats_test_rsa$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T0d-ctest.log
      9. grep -E 'test_rsa_throwaway_keypair' .sisyphus/evidence/task-T0d-ctest.log
    Expected Result: Sub-test PASS; the C wrapper correctly performs raw RSA decrypt with a public key for a known sign-encrypted blob
    Failure Indicators: FAIL; PEM parse error; CNG STATUS_INVALID_PARAMETER; decrypted bytes != original plaintext
    SKIP if: openssl not in PATH (CI may lack it; record TEST_IGNORE_MESSAGE("openssl missing — generate fixture manually"))
    Evidence: .sisyphus/evidence/task-T0d-build.log + .sisyphus/evidence/task-T0d-ctest.log

  Scenario 2: Real Data0.bhd RSA-decrypts to "BHD5" magic (validates the GAME PEM key)
    Tool: Bash (`ctest`)
    Preconditions: /mnt/c/Games/ELDEN RING/Game/Data0.bhd present; first 4 bytes are e1 0e 36 ab; T0d merged
    Steps:
      1. New sub-test reads first 256 bytes of /mnt/c/Games/ELDEN RING/Game/Data0.bhd via sf_istream_t
      2. Calls sfi_rsa_decrypt_pkcs1(SF_BHD5_PEM_KEY_ELDENRING, in, 256, &out, &out_size, NULL)
      3. Asserts SF_OK; asserts decrypted output starts with "BHD5" (bytes 42 48 44 35) after leading-zero stripping
    Expected Result: Sub-test PASS; first 4 decrypted bytes == "BHD5" (this proves the embedded ER public PEM successfully decrypts From's encrypted production BHD)
    Failure Indicators: Decrypt fails (wrong key embedded), or produces non-BHD5 magic (key drift between game versions)
    SKIP if: ER copy not at /mnt/c/Games/ELDEN RING/Game/Data0.bhd → TEST_IGNORE_MESSAGE("ER copy not present")
    Evidence: .sisyphus/evidence/task-T0d-ctest.log (same file)

  Scenario 3: rsa_cng symbols NOT exported in DLL
    Tool: Bash (`objdump`)
    Preconditions: build-mingw produces libsouls_formats.dll
    Steps:
      1. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -E 'rsa|RSA' | tee .sisyphus/evidence/task-T0d-objdump.log
    Expected Result: Empty output (no RSA symbols leaked via DLL exports)
    Failure Indicators: Any line containing sfi_rsa_decrypt
    Evidence: .sisyphus/evidence/task-T0d-objdump.log
  ```

  **Evidence to Capture**: build log, ctest log, objdump log

  **Commit**: YES
  - Message: `archive(bhd5): add sf_bhd5.h skeleton (opaque + game enum) + RSA decrypt + 4 game PEM keys`
  - Files: `include/souls_formats/sf_bhd5.h` (NEW skeleton), `include/souls_formats/souls_formats.h` (umbrella include), `src/crypto/rsa_cng.h`, `src/crypto/rsa_cng.c`, `src/archive/bhd5_keys.h` (NEW: declares PEM accessor), `src/archive/bhd5_keys.c` (PEM constants + accessor impl — AES extension lands in T3), `tests/crypto/test_rsa.c`, `tests/CMakeLists.txt` (register new test), `CMakeLists.txt` (add `bhd5_keys.c` to `SF_SOURCES`), `docs/api-mapping/extensions.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T0e. **Add minimal `sfi_dds_parse_header` for TPF metadata derivation**

  **What to do**:
  - Create internal-only `src/internal/dds_header.h` and `src/internal/dds_header.c`:
    ```c
    /* Minimal DDS header parser. Reads 124-byte DDS_HEADER + optional 20-byte
     * DDS_HEADER_DXT10. Does NOT decode pixel data. Only extracts:
     *   - cubemap (bool, from dwCaps2)
     *   - mipmap_count (u32, from dwMipMapCount)
     *   - depth (u32, from dwDepth)
     *   - dxgi_format (u32, from DDS_HEADER_DXT10.dxgiFormat, or 0 if non-DX10)
     */
    typedef struct {
        bool     cubemap;
        uint32_t mipmap_count;
        uint32_t depth;
        uint32_t dxgi_format;  /* 0 if header has no DX10 extension */
    } sfi_dds_metadata_t;
    sf_result_t sfi_dds_parse_header(const uint8_t *bytes, size_t size,
                                     sfi_dds_metadata_t *out);
    ```
  - Implement: verify magic `'DDS '` (0x20534444 LE), assert `dwSize == 124`, read fields at fixed offsets per Microsoft DDS spec. If `dwFourCC == 'DX10'`, read additional 20 bytes for DDS_HEADER_DXT10.
  - Add `tests/core/test_dds_header.c` with a synthetic minimal DDS (8×8 BC1, 1 mip, no cubemap, no DX10 extension) and a synthetic DX10 (any DXGI format).
  - Add to `docs/api-mapping/extensions.md`: row "`sfi_dds_parse_header` — extension. Upstream `DDS.cs` is `_skipped_` (full pixel decoder out of scope); we add minimal header-only reader to derive Texture metadata in TPF without depending on full DDS class."

  **Must NOT do**:
  - Implement pixel decoding (DDS class is `_skipped_`).
  - Implement DDS write (only read).
  - Treat `dwSize != 124` as warning — must be `SF_ERR_BAD_MAGIC` (truly malformed).
  - Forget to handle DX10 extension (some ER textures use DXGI formats).
  - Allocate output buffer; out is a stack-friendly POD struct.
  - Reverse the magic byte order (DDS magic is `'DDS '` in little-endian as `44 44 53 20`).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: ~50 LOC of well-specified header parsing; pure function.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T11 (TPF Texture metadata derivation calls this)
  - **Blocked By**: None

  **References**:
  - Pattern: `src/core/binary_reader.c` — for fixed-offset binary reads from bytes (or just hand-code with byte-pointer reads since it's only 144 bytes max)
  - API: Microsoft DDS Programming Guide https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header — definitive spec for DDS_HEADER (124 bytes) + DDS_HEADER_DXT10 (20 bytes)
  - API: Upstream `DDS.cs` (skipped but readable for offset confirmation) — see how upstream extracts the same fields
  - WHY each: Microsoft is canonical; upstream DDS.cs reads same offsets, useful sanity check.

  **Acceptance Criteria**:
  - [ ] `src/internal/dds_header.{h,c}` exist
  - [ ] Function correctly parses minimum DDS_HEADER for non-DX10
  - [ ] Function correctly parses DX10 extension when `dwFourCC == 'DX10'`
  - [ ] Synthetic test 8×8 BC1 round-trip metadata extraction PASS
  - [ ] DX10 synthetic test PASS
  - [ ] Returns `SF_ERR_BAD_MAGIC` on malformed input
  - [ ] DLL export count UNCHANGED (internal)

  **QA Scenarios**:

  ```
  Scenario: Synthetic 8×8 BC1 DDS metadata extraction
    Tool: Bash (`ctest`)
    Preconditions: build-mingw configured; T0e merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_dds_header 2>&1 | tee .sisyphus/evidence/task-T0e-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_dds_header$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T0e-ctest.log
      3. grep -E 'test_dds_parse_8x8_bc1' .sisyphus/evidence/task-T0e-ctest.log
    Expected Result: Sub-test PASS; metadata.cubemap=false, mipmap_count=1, depth=0, dxgi_format=0
    Failure Indicators: any FAIL line; segfault; field mismatch
    Evidence: .sisyphus/evidence/task-T0e-build.log + .sisyphus/evidence/task-T0e-ctest.log

  Scenario: DX10 extension parses dxgiFormat
    Tool: Bash (in-test assertion)
    Preconditions: T0e merged
    Steps:
      1. New sub-test crafts DDS with dwFourCC='DX10' + 20-byte DDS_HEADER_DXT10 having dxgiFormat=98 (DXGI_FORMAT_BC7_UNORM)
      2. Asserts metadata.dxgi_format == 98 after parse
      3. Captured in same ctest log
    Expected Result: Sub-test PASS
    Failure Indicators: dxgi_format != 98
    Evidence: .sisyphus/evidence/task-T0e-ctest.log (same file)

  Scenario: Malformed input rejected
    Tool: Bash (in-test assertion)
    Preconditions: T0e merged
    Steps:
      1. Test calls sfi_dds_parse_header on bytes with wrong magic ('BMP ' or zero)
      2. Asserts return == SF_ERR_BAD_MAGIC
    Expected Result: PASS
    Failure Indicators: SF_OK returned; segfault
    Evidence: .sisyphus/evidence/task-T0e-ctest.log (same file)
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `core(dds): add minimal sfi_dds_parse_header for TPF metadata derivation`
  - Files: `src/internal/dds_header.h`, `src/internal/dds_header.c`, `tests/core/test_dds_header.c`, `tests/CMakeLists.txt`, `docs/api-mapping/extensions.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T1. **Add `sf_binder.h` shared types (Format/FileFlags as `uint8_t` typedefs + sf_binder_file_t)**

  **What to do**:
  - Create `include/souls_formats/sf_binder.h` exposing the public surface shared by all 4 BND/BXF formats.
  - **`sf_binder_format_t`** as `uint8_t` typedef + named bit constants mirroring upstream `Binder.Format` (which is `[Flags] enum : byte`). Read upstream `Binder.cs:17-65` for exact bit values + names:
    ```c
    typedef uint8_t sf_binder_format_t;
    #define SF_BINDER_FORMAT_NONE         ((sf_binder_format_t)0x00)
    #define SF_BINDER_FORMAT_BIG_ENDIAN   ((sf_binder_format_t)0x01)
    #define SF_BINDER_FORMAT_IDS          ((sf_binder_format_t)0x02)
    #define SF_BINDER_FORMAT_NAMES1       ((sf_binder_format_t)0x04)
    #define SF_BINDER_FORMAT_NAMES2       ((sf_binder_format_t)0x08)
    #define SF_BINDER_FORMAT_LONG_OFFSETS ((sf_binder_format_t)0x10)
    #define SF_BINDER_FORMAT_COMPRESSION  ((sf_binder_format_t)0x20)
    #define SF_BINDER_FORMAT_FLAG6        ((sf_binder_format_t)0x40)
    #define SF_BINDER_FORMAT_FLAG7        ((sf_binder_format_t)0x80)
    /* Drift-guard: assert each bit lives at expected position */
    _Static_assert(SF_BINDER_FORMAT_BIG_ENDIAN   == 0x01, "binder format bit drift");
    _Static_assert(SF_BINDER_FORMAT_FLAG7        == 0x80, "binder format bit drift");
    ```
    (Verify exact bit assignments against upstream — the names above are illustrative; pull canonical values from `Binder.cs`.)
  - **`sf_binder_file_flags_t`** as `uint8_t` typedef + named bit constants mirroring upstream `Binder.FileFlags` (also `[Flags] enum : byte` from `Binder.cs:137-185`):
    ```c
    typedef uint8_t sf_binder_file_flags_t;
    #define SF_BINDER_FILE_FLAG_NONE       ((sf_binder_file_flags_t)0x00)
    #define SF_BINDER_FILE_FLAG_COMPRESSED ((sf_binder_file_flags_t)0x01)
    /* ... rest per Binder.cs:137-185 ... */
    _Static_assert(SF_BINDER_FILE_FLAG_COMPRESSED == 0x01, "binder file flag drift");
    ```
  - **WHY uint8_t typedef instead of `enum`** (per Momus review): C11's `enum` defaults to `int`-sized; you cannot reliably make an enum 1 byte across MSVC + clang-cl + MinGW-w64. Project precedent: `sf_dcx_type_t` in `sf_dcx.h` uses `enum` (4 bytes — sentinel `SF_DCX_TYPE_COUNT_` + `_Static_assert(SF_DCX_TYPE_COUNT_ == 9, ...)`); but here we need an exact 1-byte serialized representation, so `uint8_t` is mandatory. Bit-level `_Static_assert` per constant catches drift instead of size assertion.
  - At every byte read/write site (in T2 internal helpers and T6-T11 source), explicitly cast: `bw.write_u8((uint8_t)format)` and `format = (sf_binder_format_t)br.read_u8()`. This is unambiguous in C and matches upstream's `Read(BinaryReaderEx).ReadByte()` cast pattern.
  - Public POD type:
    ```c
    typedef struct sf_binder_file {
        int32_t                     id;            /* upstream Int32; sentinel -1 */
        const char                 *name_utf8;     /* heap-owned by parent binder; do not free */
        const uint8_t              *data;          /* heap-owned by parent binder */
        size_t                      size;          /* uncompressed byte count */
        sf_binder_file_flags_t      flags;         /* per-entry flags */
        sf_dcx_compression_info_t   compression_info; /* preset preserved for round-trip */
    } sf_binder_file_t;
    ```
  - Forward-declare opaque types: `typedef struct sf_bnd3 sf_bnd3_t;`, `sf_bnd4_t`, `sf_bxf3_t`, `sf_bxf4_t`, `sf_bnd3_reader_t`, `sf_bnd4_reader_t`, `sf_bxf3_reader_t`, `sf_bxf4_reader_t`.
  - Public format-introspection helpers (matching upstream `Binder.HasIDs/HasNames1/HasNames2/HasLongOffsets/HasCompression` which are static methods):
    ```c
    SF_API bool sf_binder_format_has_ids          (sf_binder_format_t f);
    SF_API bool sf_binder_format_has_names1       (sf_binder_format_t f);  /* low-bit short names */
    SF_API bool sf_binder_format_has_names2       (sf_binder_format_t f);
    SF_API bool sf_binder_format_has_long_offsets (sf_binder_format_t f);
    SF_API bool sf_binder_format_has_compression  (sf_binder_format_t f);
    SF_API bool sf_binder_format_has_flag6        (sf_binder_format_t f);
    SF_API bool sf_binder_format_has_flag7        (sf_binder_format_t f);
    SF_API bool sf_binder_format_force_big_endian (sf_binder_format_t f);
    ```
  - Public timestamp helpers (mirroring `Binder.BinderTimestampToDate` / `DateToBinderTimestamp` from `Binder.cs:215-245`).
    **CORRECTED upstream semantics** (per Momus review — verified against `Binder.cs:210-244`): the timestamp regex is `(\d\d)(\w)(\d+)(\w)(\d+)` matching 5 fields; year offset by +2000, month from `[A-L]` letter (raw `value - 'A'`), day from digits, hour from `[A-X]` letter (raw `value - 'A'`), minute from digits. Format pads to 8 chars with `\0`.
    ```c
    /* Mirrors upstream DateTime fields stored in BND/BXF timestamp.
     * Note: month uses upstream's raw 'A'=0..'L'=11 encoding (NOT C's 1-12).
     * hour uses 'A'=0..'X'=23. day and minute are decimal. */
    typedef struct sf_binder_datetime {
        int year;    /* 4-digit, e.g. 2007 */
        int month;   /* 0-11 (raw upstream encoding: 'A'-'A' .. 'L'-'A') */
        int day;     /* 1-31 */
        int hour;    /* 0-23 (raw upstream encoding: 'A'-'A' .. 'X'-'A') */
        int minute;  /* 0-59 */
    } sf_binder_datetime_t;

    /* Parse 8-char ASCII BND timestamp like "07D7R6\0\0" into a sf_binder_datetime_t.
     * Returns SF_ERR_INVALID_ARG on malformed input (regex no-match or out-of-range). */
    SF_API sf_result_t sf_binder_timestamp_parse(const char *timestamp,
                                                 sf_binder_datetime_t *out);
    /* Format a sf_binder_datetime_t back to 8-char timestamp (padded with \0).
     * Returns SF_ERR_INVALID_ARG if year is outside 2000-2099. */
    SF_API sf_result_t sf_binder_timestamp_format(const sf_binder_datetime_t *dt,
                                                  char out_timestamp[9]);
    ```
    Tests must verify: parse "07D7R6\0\0" → year=2007, month=3 (D-A), day=7, hour=17 (R-A), minute=6; then format back yields exactly "07D7R6\0\0" (8 bytes).

  **Must NOT do**:
  - Use `int64_t id` — upstream is Int32 (4 bytes). Wrong type = round-trip corruption on real files.
  - Use bare `sf_dcx_type_t compression` — must be `sf_dcx_compression_info_t compression_info` to preserve presets.
  - Expose `BinderFileHeader` as public — it's an internal serialization concept; only `sf_binder_file_t` is public.
  - Use `enum sf_binder_format_t { ... }` — C11's enum is not reliably 1 byte across MSVC/clang-cl/MinGW. Use `typedef uint8_t` + `#define`s instead. (Per Momus review.)
  - Add `_Static_assert(sizeof(sf_binder_format_t) == 1)` — would fail on enum (sizeof int); also unnecessary because we explicitly type as `uint8_t` so size is guaranteed by the typedef itself.
  - Inline `name_utf8` as `char[256]` — names can be arbitrary length; pointer-owned is correct.
  - Declare hash table or BinderFileHeader struct in this header — those are internal (see T2).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Header decls + simple public-helper IMPLEMENTATIONS (8 has-* + 2 timestamp); ≤250 LOC total. T1 produces a self-contained, linkable, exportable module — no T2 dependency for compile/link/export.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (after T0a)
  - **Parallel Group**: Wave 1 (with T2, T3, T4, T5)
  - **Blocks**: T2, T6, T7, T8, T9, T11, T12 (every consumer of Format/FileFlags or sf_binder_file_t)
  - **Blocked By**: T0a (`sf_reverse_bits_u8` — referenced via macro/inline in `binder_common.c` from T2, NOT directly used by T1's helpers)

  > **CORRECTED scope** (per Momus review): T1 includes BOTH header declarations AND implementations of the 10 public helpers (8 `sf_binder_format_has_*` + 2 timestamp). T1 is fully linkable on its own commit (DLL export count rises by 10 immediately). T2 then adds the **internal-only** binder common helpers in `binder_common.h/c` that T6-T9 source files consume.

  **References**:
  - Pattern: `include/souls_formats/sf_dcx.h` — example of public umbrella header with opaque types, enums, _Static_assert
  - Pattern: `include/souls_formats/sf_common.h` — `SF_API` decoration style
  - API: Upstream `Binder.cs:17-65` (`Format` enum) + `Binder.cs:137-185` (`FileFlags` enum) — exact byte values
  - API: Upstream `Binder.cs:215-243` (`BinderTimestampToDate` / `DateToBinderTimestamp`) — algorithm
  - API: Upstream `BinderFile.cs:8-75` — public Class field shapes that map to `sf_binder_file_t`
  - WHY each: sf_dcx.h shows similar class-of-formats umbrella pattern; Binder.cs is canonical for enum byte values that must round-trip; BinderFile.cs determines the field set we expose.

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_binder.h` exists with all 8 forward decls + uint8_t typedefs + named bit constants + sf_binder_file_t + sf_binder_datetime_t + 8 has-* helper decls + 2 timestamp helper decls
  - [ ] `src/archive/sf_binder.c` (NEW source file in `src/archive/`) implements all 10 public helpers (8 `sf_binder_format_has_*` + 2 timestamp). T1 is self-contained and linkable.
  - [ ] `sf_binder_format_t` is `typedef uint8_t` (NOT `enum`)
  - [ ] `sf_binder_file_flags_t` is `typedef uint8_t` (NOT `enum`)
  - [ ] At least 2 `_Static_assert` per type checking specific bit values match upstream (drift catches per-constant, not on size)
  - [ ] `id` field is `int32_t`, NOT `int64_t`
  - [ ] `compression_info` field is `sf_dcx_compression_info_t`, NOT `sf_dcx_type_t`
  - [ ] `cmake --build build-mingw` 0 warnings — T1 fully linkable on its own commit
  - [ ] DLL export count INCREASES by 10 (8 has-* + 2 timestamp) immediately after T1 lands
  - [ ] `souls_formats.h` umbrella includes `sf_binder.h`
  - [ ] `CMakeLists.txt` adds `src/archive/sf_binder.c` to `SF_SOURCES`

  **QA Scenarios**:

  ```
  Scenario: T1 commit links and exports all 10 helpers cleanly (self-contained)
    Tool: Bash (`cmake --build` + `objdump`)
    Preconditions: T1 merged (T1 contains BOTH header decls AND implementations; no T2 dependency)
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T1-build.log
      2. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -E 'sf_binder_(format_has_|timestamp_)' | tee .sisyphus/evidence/task-T1-exports.log
      3. wc -l .sisyphus/evidence/task-T1-exports.log
    Expected Result: Build succeeds with 0 warnings. Export count for sf_binder_format_has_* and sf_binder_timestamp_* is exactly 10 (8 has-* + 2 timestamp).
    Failure Indicators: Build error; <10 exports; T1 commit breaks link
    Evidence: .sisyphus/evidence/task-T1-build.log + .sisyphus/evidence/task-T1-exports.log

  Scenario: Bit-level drift assertions hold across all 3 toolchains
    Tool: Bash (`cmake --build`)
    Preconditions: T1 merged
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T1-build.log
      2. grep -E 'static_assertion failed|_Static_assert' .sisyphus/evidence/task-T1-build.log
    Expected Result: Empty grep output (no _Static_assert failures); build green. Each `SF_BINDER_FORMAT_*` and `SF_BINDER_FILE_FLAG_*` bit value is asserted against expected hex literal.
    Failure Indicators: Any "_Static_assert failed" line; bit drift between `Binder.cs` and our header
    Evidence: .sisyphus/evidence/task-T1-build.log

  Scenario: Types are uint8_t typedefs (not enums)
    Tool: Bash (`grep`)
    Preconditions: T1 merged
    Steps:
      1. grep -E 'typedef\s+uint8_t\s+sf_binder_(format|file_flags)_t' include/souls_formats/sf_binder.h | tee .sisyphus/evidence/task-T1-typedef.log
      2. grep -E 'enum\s+sf_binder_(format|file_flags)' include/souls_formats/sf_binder.h | tee -a .sisyphus/evidence/task-T1-typedef.log
    Expected Result: Two typedef lines present; zero enum lines for these two types
    Failure Indicators: 0 typedef matches; OR 1+ enum match for these names → indicates someone reverted to enum
    Evidence: .sisyphus/evidence/task-T1-typedef.log
  ```

  **Evidence to Capture**: compile log, build log

  **Commit**: YES
  - Message: `archive: add sf_binder.h shared types + has-*/timestamp helpers`
  - Files: `include/souls_formats/sf_binder.h`, `src/archive/sf_binder.c`, `include/souls_formats/souls_formats.h` (add `#include`), `CMakeLists.txt` (add new source to `SF_SOURCES`)
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T2. **Add `binder_common.c` shared helpers (timestamps, hash table, file header r/w, format/flags helpers)**

  **What to do**:
  - Create `src/archive/binder_common.c` with the implementation of T1's public surface PLUS internal helpers used by all 4 BND/BXF format implementations.
  - Create `src/archive/binder_common.h` (internal header, NOT under `include/souls_formats/`) declaring internal helpers used by bnd3/bnd4/bxf3/bxf4 source files:
    ```c
    /* Internal: Binder file header — not public; serialization-only struct.
     * Different physical sizes for BND3 vs BND4 — read functions parameterize. */
    typedef struct sfi_binder_file_header {
        sf_binder_file_flags_t flags;
        int32_t                id;
        char                  *name_utf8;          /* heap-owned, freed via sfi_binder_file_header_destroy */
        uint64_t               compressed_size;    /* upstream: long for BND4, int for BND3 */
        uint64_t               uncompressed_size;
        uint64_t               data_offset;        /* upstream: long for BND4 LongOffsets, int else */
        sf_dcx_compression_info_t compression_info;
    } sfi_binder_file_header_t;

    /* Read/write Binder.Format byte using BitBigEndian inversion via sf_reverse_bits_u8 */
    sf_binder_format_t  sfi_binder_read_format       (sf_binder_reader_t *br, bool bit_big_endian);
    void                sfi_binder_write_format      (sf_binder_writer_t *bw, bool bit_big_endian, sf_binder_format_t f);
    sf_binder_file_flags_t sfi_binder_read_file_flags(sf_binder_reader_t *br, bool bit_big_endian);
    void                sfi_binder_write_file_flags  (sf_binder_writer_t *bw, bool bit_big_endian, sf_binder_file_flags_t f);

    /* BND3-shape file header read/write */
    sf_result_t sfi_binder3_read_file_header  (sf_binary_reader_t *br, sf_binder_format_t f, sfi_binder_file_header_t *out, const sf_allocator_t *a);
    sf_result_t sfi_binder3_write_file_header (sf_binary_writer_t *bw, sf_binder_format_t f, const sfi_binder_file_header_t *h, size_t entry_index);
    sf_result_t sfi_binder3_write_file_data   (sf_binary_writer_t *bw, sf_binder_format_t f, const sfi_binder_file_header_t *h, const uint8_t *raw, size_t entry_index);
    sf_result_t sfi_binder3_write_file_name   (sf_binary_writer_t *bw, sf_binder_format_t f, const sfi_binder_file_header_t *h, size_t entry_index);
    /* Same trio for BND4 — different field widths and Names1 corner case */
    sf_result_t sfi_binder4_read_file_header  (sf_binary_reader_t *br, sf_binder_format_t f, bool unicode, byte extended, sfi_binder_file_header_t *out, const sf_allocator_t *a);
    sf_result_t sfi_binder4_write_file_header (sf_binary_writer_t *bw, sf_binder_format_t f, byte extended, const sfi_binder_file_header_t *h, size_t entry_index);
    sf_result_t sfi_binder4_write_file_data   (sf_binary_writer_t *bw, sf_binder_format_t f, const sfi_binder_file_header_t *h, const uint8_t *raw, size_t entry_index);
    sf_result_t sfi_binder4_write_file_name   (sf_binary_writer_t *bw, sf_binder_format_t f, bool unicode, const sfi_binder_file_header_t *h, size_t entry_index);

    /* BND4/BXF4 hash table — re-derived at write time from current Files list. */
    sf_result_t sfi_binder_hash_table_assert(sf_binary_reader_t *br, size_t file_count);
    sf_result_t sfi_binder_hash_table_write (sf_binary_writer_t *bw, const sf_binder_file_t *files, size_t file_count);

    /* Compute BND4 entry header size given Format flags. */
    size_t      sfi_binder_get_bnd4_file_header_size(sf_binder_format_t f);

    /* Free heap-owned name on a sfi_binder_file_header_t */
    void        sfi_binder_file_header_destroy(sfi_binder_file_header_t *h, const sf_allocator_t *a);
    ```
  - **Internal-only scope** (per Momus review): T1 owns and exports the 10 public `sf_binder_format_has_*` + `sf_binder_timestamp_*` helpers. T2 is **strictly internal**, adding only `sfi_*` helpers in `binder_common.h/c`. T2 does NOT add to the DLL export surface.
  - Implement all internal helpers above per upstream `Binder.cs` + `BinderFileHeader.cs` + `BinderHashTable.cs`. Read those files line-by-line — DO NOT guess.
  - Hash table re-derivation algorithm (mirror upstream `BinderHashTable.cs:Write`):
    1. Choose prime: `for p = ceil(file_count / 7); p ≤ 100000; if IsPrime(p) break`
    2. Compute hash for each file via `sf_path_hash` (32-bit) MOD prime → bucket index
    3. Sort files within each bucket by hash ascending
    4. Write the `Hashes` block (count entries: hash + index) then `Buckets` block (count entries: count + offset_to_hashes)
  - **Format=Names1 corner case** (BND4 PC saves only): `BinderFileHeader.cs:149-153` — extra ID read + 0 padding when `Format == Names1`. Implement in `sfi_binder4_read_file_header`.
  - Use `sf_reverse_bits_u8` (T0a) for the BitBigEndian flip in `sfi_binder_read_format` / `sfi_binder_write_format` / `sfi_binder_read_file_flags` / `sfi_binder_write_file_flags`.
  - Add `tests/archive/test_binder_common.c` with sub-tests: (a) all 8 has-* helpers return correct booleans for canonical format values (e.g. `sf_binder_format_has_ids(SF_BINDER_FORMAT_IDs|SF_BINDER_FORMAT_Names1) == true`); (b) timestamp parse "07D7R6 " → year=2007, day=215; (c) timestamp format → string match; (d) hash table prime selection for 7/49/100/1000 file counts matches upstream output; (e) ReadFormat-WriteFormat round-trip for 4 known bytes.

  **Must NOT do**:
  - Guess any bit value or wire format — read the .cs files line-by-line.
  - Pre-compute hash tables on add (must re-derive at write time).
  - Sort the in-memory `Files` list (only sort within hash table during write).
  - Recompute SHA hashes anywhere — irrelevant; this is binder common, not BHD5.
  - Forget the `bw.Pad(0x10)` before file data when `bytes_size > 0` (BinderFileHeader.cs:236).
  - Cache the prime across writes — re-search per write (file_count may change).
  - Skip the BinderHashTable assertions during read (`0x10`, `8`, `8`, `0` — they validate version compatibility).
  - Forget that BND3 file header uses 32-bit sizes/offsets, BND4 uses 64-bit (when LongOffsets) — DIFFERENT layouts, separate read/write functions per format family.
  - Inline-copy each binder file's name from temp buffer multiple times — own once, free once (consume careful allocator discipline).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~700 LOC of foundation; gravity well that determines whether 4 downstream format tasks (T6-T9) silently diverge. Subtle bugs in Format/FileFlags handling cascade to every binder. Higher effort warranted.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO — serial after T1
  - **Parallel Group**: Wave 1 (gates Wave 2)
  - **Blocks**: T6, T7, T8, T9 (all 4 BND/BXF format tasks consume binder_common helpers)
  - **Blocked By**: T0a (uses `sf_reverse_bits_u8`), T1 (uses `sf_binder_format_t`/etc public types)

  **References**:
  - Pattern: `src/core/binary_reader.c` — internal-only function naming via `sfi_*`, error-path discipline
  - Pattern: `src/compression/dcx.c` — internal helper organization (multiple internal helpers in one file)
  - API: Upstream `Binder.cs` (lines 17-243) — Format enum, FileFlags enum, has-* helpers, timestamps
  - API: Upstream `BinderFile.cs` (lines 8-75) — public class shape
  - API: Upstream `BinderFileHeader.cs` (lines 8-295) — both BND3 and BND4 file header serialization, INCLUDING Names1 PC-save corner case at lines 149-153 + Pad(0x10) at line 236
  - API: Upstream `BinderHashTable.cs` (lines 7-128) — hash table read/assert/write, prime selection algorithm
  - API: Upstream `BinderReader.cs` (lines 9-86) — base class abstract methods (used by T6-T9 streaming readers, but signatures locked here)
  - WHY each: Each .cs file maps to a specific subset of T2's surface; ignoring any of these = silent corruption in real files.

  **Acceptance Criteria**:
  - [ ] `src/archive/binder_common.h` (internal) exists with all internal helper declarations
  - [ ] `src/archive/binder_common.c` implements ~15 internal `sfi_*` helpers (NOT the public has-*/timestamp surface — those live in T1)
  - [ ] Hash table prime selection matches upstream output for {7, 49, 100, 1000} file counts (sub-test)
  - [ ] Timestamp round-trip via T1's `sf_binder_timestamp_*` helpers covered in T1's own test (T2 has no timestamp ownership)
  - [ ] `sfi_binder_read_format` ↔ `sfi_binder_write_format` round-trips 256 byte values (sub-test)
  - [ ] `sfi_binder_read_file_flags` ↔ `sfi_binder_write_file_flags` round-trips 256 byte values (sub-test)
  - [ ] `_Static_assert` on relevant internal constants where applicable (drift guard for hash table primes etc.)
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_binder_common$' --output-on-failure` PASS
  - [ ] **DLL export count UNCHANGED** by T2 (only internal `sfi_*`; T1 already supplied the public surface)

  **QA Scenarios**:

  ```
  Scenario: Format/FileFlags byte-level round-trip (256 values each)
    Tool: Bash (`ctest`)
    Preconditions: T0a + T1 + T2 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_binder_common 2>&1 | tee .sisyphus/evidence/task-T2-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_binder_common$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T2-ctest.log
      3. grep -E 'test_(binder_format|binder_file_flags)_roundtrip_256' .sisyphus/evidence/task-T2-ctest.log
    Expected Result: Both round-trip sub-tests PASS — every 0..255 byte input survives Read→Write→Read with bit_big_endian=true and bit_big_endian=false
    Failure Indicators: any FAIL; mismatch on any byte
    Evidence: .sisyphus/evidence/task-T2-build.log + .sisyphus/evidence/task-T2-ctest.log

  Scenario: Hash table prime selection for known file counts
    Tool: Bash (in-test assertion)
    Preconditions: T2 merged
    Steps:
      1. New sub-test asserts: prime(7) ≥ 2, prime(49) ≥ 7, prime(100) ≥ 14, prime(1000) ≥ 142 (lower bounds; upstream picks first prime ≥ ceil(n/7))
      2. Cross-verify against ground-truth values pulled from upstream `BinderHashTable.cs` debug runs
    Expected Result: All 4 sub-asserts PASS
    Failure Indicators: prime returned is composite or below ceil(n/7)
    Evidence: .sisyphus/evidence/task-T2-ctest.log (same file)

  Scenario: T2 adds zero new exports (internal-only contract)
    Tool: Bash (`objdump`)
    Preconditions: T1 + T2 merged
    Steps:
      1. Compare DLL export count BEFORE T2 (after T1) and AFTER T2: count should be identical
      2. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -c 'sf_' | tee .sisyphus/evidence/task-T2-exports.log
      3. Compare against T1-only export count from task-T1-exports.log: same number
    Expected Result: T2 adds zero new public symbols
    Failure Indicators: any new public sf_* symbol appears (T2 should be internal-only)
    Evidence: .sisyphus/evidence/task-T2-exports.log
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `archive: add binder_common.c shared helpers (timestamps, hash table, file header r/w)`
  - Files: `src/archive/binder_common.h`, `src/archive/binder_common.c`, `tests/archive/test_binder_common.c`, `tests/CMakeLists.txt` (register `archive` label + new test), `CMakeLists.txt` (add `src/archive/binder_common.c` to `SF_SOURCES`)
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T3. **~~Embed AES-128-ECB keys for 4 v1 games~~ CANCELLED: BHD5 AES keys are stored inline per-file in the .bhd data (BHD5.cs:668 `Key = br.ReadBytes(16)`), not as per-game constants. T10 reads inline keys directly.**

  **What to do**:
  - **CORRECTED scope** (per Momus review): T0d already created `bhd5_keys.h` + `bhd5_keys.c` with PEM keys + `sfi_bhd5_get_pem_key` accessor + `sf_bhd5_game_t` enum (in `sf_bhd5.h` skeleton). T3 ONLY adds AES key constants + AES accessor:
    ```c
    /* T3 ADDS to existing bhd5_keys.c:
     * AES-128-ECB keys per game. Keys are publicly known in soulsmods community.
     * Source citation: <URL or commit reference> per key. */
    static const uint8_t SF_BHD5_AES_KEY_ELDENRING[16] = { 0x?? ... };
    static const uint8_t SF_BHD5_AES_KEY_SEKIRO   [16] = { 0x?? ... };
    static const uint8_t SF_BHD5_AES_KEY_NIGHTREIGN[16] = { 0x?? ... };
    static const uint8_t SF_BHD5_AES_KEY_ARMOREDCORE6[16] = { 0x?? ... };
    ```
  - Add AES lookup function (internal) to `bhd5_keys.h`: `const uint8_t *sfi_bhd5_get_aes_key(sf_bhd5_game_t game);` returning NULL on unknown. Implement in `bhd5_keys.c` as a switch.
  - Cite the source repo + commit hash for each AES key in the header comment block at the top of `bhd5_keys.c` (T0d already established the citation block; T3 extends).

  **Must NOT do**:
  - Make up key values — use community-known sources.
  - Embed Oodle DLL.
  - Embed any binary blob from a game install.
  - Forget to cite the source per key.
  - Expose AES keys via `SF_API` — they're internal data; only the `sfi_bhd5_get_aes_key` accessor is internal.
  - Hardcode the keys in `bhd5.c` directly — keep keys in `bhd5_keys.c` for ease of update.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Pure data file; ≤80 LOC of constants + lookup.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (after T0d for PEM keys; AES keys can be added independently)
  - **Parallel Group**: Wave 1
  - **Blocks**: T10 (BHD5 read/write requires keys)
  - **Blocked By**: T0d (PEM keys live in same file)

  **References**:
  - Pattern: `src/crypto/regulation_keys.c` (or similar in `src/crypto/`) — Phase 2 already embeds AES keys for regulation.bin; mirror that file's citation+const structure
  - API: Community-known source — soulsmods/UXM-Selective-Unpack repo, TKGP commits, or similar public references. Implementing agent must cite specific repo + commit hash per key.
  - API: Upstream `BHD5.cs` does NOT contain the keys (it punts to caller); keys come from external community sources.
  - WHY each: regulation_keys is the closest existing file pattern; community sources are the only legal source for the keys (cannot be derived from game binary by us).

  **Acceptance Criteria**:
  - [ ] `src/archive/bhd5_keys.h` (internal) declares `sfi_bhd5_get_aes_key`
  - [ ] 4 AES-128-ECB keys + 4 RSA PEM keys in `bhd5_keys.c` with citation per key
  - [ ] Lookup returns NULL for unknown game
  - [ ] DLL export count UNCHANGED (all internal)
  - [ ] No raw byte arrays from game install committed (only known-public crypto constants)

  **QA Scenarios**:

  ```
  Scenario: Lookup returns 16-byte key for each game
    Tool: Bash (small helper test or via T10 dependency)
    Preconditions: T3 merged
    Steps:
      1. Create test stub `tests/archive/test_bhd5_keys.c` (or fold into T10's test): assert sfi_bhd5_get_aes_key returns non-NULL pointer for each of 4 game enum values, NULL for an out-of-range value
      2. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T3-build.log
      3. ctest --test-dir build-mingw -R 'test_bhd5_keys' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T3-ctest.log
    Expected Result: 4 PASS for valid games, NULL for invalid; build clean
    Failure Indicators: NULL for valid game; non-NULL for invalid
    Evidence: .sisyphus/evidence/task-T3-build.log + .sisyphus/evidence/task-T3-ctest.log

  Scenario: No key/PEM symbol is exported
    Tool: Bash (`objdump`)
    Preconditions: build-mingw produces libsouls_formats.dll
    Steps:
      1. x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -E 'AES_KEY|PEM_KEY' | tee .sisyphus/evidence/task-T3-objdump.log
    Expected Result: Empty (internal data must NOT leak)
    Failure Indicators: Any matching line
    Evidence: .sisyphus/evidence/task-T3-objdump.log
  ```

  **Evidence to Capture**: build log, ctest log, objdump log

  **Commit**: YES
  - Message: `archive(bhd5): embed AES-128-ECB keys for Sekiro/ER/Nightreign/AC6`
  - Files: `src/archive/bhd5_keys.h`, `src/archive/bhd5_keys.c` (extends T0d), `tests/archive/test_bhd5_keys.c`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T4. **Add `er_test_helper.h` skeleton (declarations only)**

  **What to do**:
  - Create `tests/e2e/er_test_helper.h` with the public-to-tests-only declarations (no SF_API; this is test-internal). T0d created `sf_bhd5.h` with the opaque `sf_bhd5_t` forward decl, so this header can `#include "souls_formats/sf_bhd5.h"`:
    ```c
    #include "souls_formats/sf_common.h"
    #include "souls_formats/sf_bhd5.h"  /* for sf_bhd5_t forward decl */

    /* Process-wide singleton holding an open Data0.bhd/bdt pair for ER e2e tests.
     * Lazy init on first er_extract_from_data0; cleaned up via atexit in main. */

    /* Initialize the singleton (called automatically; idempotent). */
    sf_result_t er_helper_init(void);

    /* Extract entry by BHD5 path (UTF-8). Decompresses outer DCX wrapper.
     * Returns heap-owned bytes via *out (caller frees via sf_free).
     * Skips test gracefully (returns SF_ERR_OODLE_NOT_FOUND or similar) if
     * Data0.bhd missing, RSA-encrypted state cannot be unwrapped, or Oodle DLL
     * not found. */
    sf_result_t er_extract_from_data0(const char *bhd5_path_utf8,
                                      void **out, size_t *out_size);

    /* Tear down. atexit-registered. */
    void er_helper_shutdown(void);

    /* Probe: is the helper functional in the current environment? Returns:
     *   true  → er_extract_from_data0 will work
     *   false → caller should TEST_IGNORE_MESSAGE and skip the e2e test. */
    bool er_helper_is_available(void);

    /* Test-only accessor: returns the underlying sf_bhd5_t* for low-level
     * inspection by tests like test_bhd5_e2e_er. Returns NULL if not yet
     * initialized. Test-internal only — never exposed via SF_API. */
    sf_bhd5_t *er_helper_get_bhd5_for_testing(void);
    ```
  - DECLARATIONS ONLY — implementation lives in T14.
  - Add `tests/e2e/CMakeLists.txt` (if not exists) wiring with `target_compile_definitions` injecting hardcoded paths per PLAN.md §8.4:
    ```cmake
    target_compile_definitions(souls_formats_e2e PRIVATE
        SF_E2E_ELDEN_RING_DIR=L"C:/Games/ELDEN RING"
        SF_E2E_OODLE_DIR=L"\\\\wsl.localhost\\\\Ubuntu\\\\home\\\\soar\\\\dev\\\\oodle"
    )
    ```

  **Must NOT do**:
  - Implement any function — T4 is skeleton only.
  - Add `SF_API` decoration — this is test-only, never exposed via DLL.
  - Hardcode paths into the .h — paths come via compile_definitions (PLAN.md §8.4 binds the rule).
  - Forget that the e2e tests may run in environments without ER copy → must SKIP, not fail.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Skeleton header file only; ~30 LOC.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T14 (implementation lands later), T15-T18 (e2e tests #include this header)
  - **Blocked By**: None — can be merged before T7/T10 are done

  **References**:
  - Pattern: PLAN.md §8.4 — hardcoded path scheme via target_compile_definitions
  - Pattern: existing `tests/CMakeLists.txt` — Phase 0/1/2 test wiring style
  - WHY each: PLAN.md §8.4 is the canonical rule for test-data path wiring; existing CMakeLists shows project conventions.

  **Acceptance Criteria**:
  - [ ] `tests/e2e/er_test_helper.h` exists with 4 declarations
  - [ ] `tests/e2e/CMakeLists.txt` exists (or is updated) with the SF_E2E_*_DIR macros
  - [ ] Header compiles when consumed by an empty stub `tests/e2e/er_test_helper.c` (placeholder only — full impl in T14)
  - [ ] No SF_API decoration
  - [ ] DLL export count UNCHANGED

  **QA Scenarios**:

  ```
  Scenario: Header compiles standalone
    Tool: Bash (compile a synth-test)
    Preconditions: T4 merged
    Steps:
      1. echo '#include "er_test_helper.h"\nint main() { return 0; }' > /tmp/synth-T4.c
      2. x86_64-w64-mingw32-gcc-posix -I tests/e2e -I include -c /tmp/synth-T4.c -o /tmp/synth-T4.o 2>&1 | tee .sisyphus/evidence/task-T4-compile.log
    Expected Result: Compile succeeds with 0 warnings
    Failure Indicators: any warning or error
    Evidence: .sisyphus/evidence/task-T4-compile.log

  Scenario: Stub binds via cmake
    Tool: Bash (`cmake --build`)
    Preconditions: T4 merged + placeholder er_test_helper.c stub present
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T4-build.log
    Expected Result: Build succeeds; no link errors (since no e2e test consumes it yet)
    Failure Indicators: link error; missing symbol
    Evidence: .sisyphus/evidence/task-T4-build.log
  ```

  **Evidence to Capture**: compile log, build log

  **Commit**: YES
  - Message: `tests(e2e): add er_test_helper.h skeleton (declarations only)`
  - Files: `tests/e2e/er_test_helper.h`, `tests/e2e/CMakeLists.txt`, `tests/e2e/er_test_helper.c` (stub `// implementation lands in T14`)
  - Pre-commit: `cmake --build build-mingw`

- [x] T5. **Add `examples/sf_bnd_extract.c` skeleton + CMake wiring**

  **What to do**:
  - Create `examples/sf_bnd_extract.c` skeleton:
    ```c
    /* sf_bnd_extract — CLI: extract a BND archive to a directory.
     * Usage: sf_bnd_extract <input.bnd[.dcx]> <output_dir>
     * Implementation lands in T19; this skeleton is structure only. */
    int main(int argc, char **argv) {
        (void)argc; (void)argv;
        /* TODO(T19): actual extraction logic */
        return 0;
    }
    ```
  - Create or extend `examples/CMakeLists.txt` to add the executable target:
    ```cmake
    add_executable(sf_bnd_extract sf_bnd_extract.c)
    target_link_libraries(sf_bnd_extract PRIVATE souls_formats_static)
    sf_apply_compiler_warnings(sf_bnd_extract)
    set_target_properties(sf_bnd_extract PROPERTIES OUTPUT_NAME sf_bnd_extract)
    ```
  - Verify build green; the skeleton main returns 0 silently.

  **Must NOT do**:
  - Implement extraction logic in T5 — that's T19.
  - Print Hello World or other placeholder noise.
  - Use `printf("TODO: ...")` — keep it silent.
  - Commit any extracted files into the repo.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: 30 LOC skeleton + CMake glue.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T19 (implementation)
  - **Blocked By**: None

  **References**:
  - Pattern: existing `examples/CMakeLists.txt` (if exists) — mirror the target setup
  - Pattern: existing examples in similar pure-C library projects use `sf_apply_compiler_warnings`-like helper
  - WHY each: To match project conventions and avoid CMake drift.

  **Acceptance Criteria**:
  - [ ] `examples/sf_bnd_extract.c` exists with skeleton main
  - [ ] `examples/CMakeLists.txt` adds the target
  - [ ] `cmake --build build-mingw` produces `examples/sf_bnd_extract.exe` (or `.exe` under build-mingw/examples/)
  - [ ] Running the .exe with no args returns 0 silently
  - [ ] No new warnings under `-Werror`
  - [ ] Top-level `CMakeLists.txt` adds `add_subdirectory(examples)` if not already

  **QA Scenarios**:

  ```
  Scenario: Skeleton builds and runs
    Tool: Bash
    Preconditions: T5 merged
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T5-build.log
      2. ls build-mingw/examples/sf_bnd_extract.exe 2>&1 | tee -a .sisyphus/evidence/task-T5-build.log
      3. build-mingw/examples/sf_bnd_extract.exe; echo "exit=$?" | tee .sisyphus/evidence/task-T5-run.log
    Expected Result: Executable exists; exit code 0; no stdout
    Failure Indicators: missing exe; non-zero exit; warning during build
    Evidence: .sisyphus/evidence/task-T5-build.log + .sisyphus/evidence/task-T5-run.log

  Scenario: Linkage to souls_formats_static is clean
    Tool: Bash (`objdump`)
    Preconditions: T5 merged
    Steps:
      1. x86_64-w64-mingw32-objdump -p build-mingw/examples/sf_bnd_extract.exe | head -50 | tee .sisyphus/evidence/task-T5-link.log
    Expected Result: Output shows .exe is a PE32+ binary with no missing imports
    Failure Indicators: "import not found" or similar
    Evidence: .sisyphus/evidence/task-T5-link.log
  ```

  **Evidence to Capture**: build log, run log, link log

  **Commit**: YES
  - Message: `examples: add sf_bnd_extract.c skeleton`
  - Files: `examples/sf_bnd_extract.c`, `examples/CMakeLists.txt`, top-level `CMakeLists.txt` (if needed for `add_subdirectory(examples)`)
  - Pre-commit: `cmake --build build-mingw`

- [x] T6. **Port BND3 read/write + BND3Reader (eager + streaming) + synthetic round-trip test**

  **What to do**:
  - Create `include/souls_formats/sf_bnd3.h` with public API mirroring upstream BND3 + BND3Reader:
    - Opaque types: `sf_bnd3_t`, `sf_bnd3_reader_t`
    - Properties via getters/setters: `version` (string), `format` (`sf_binder_format_t`), `big_endian` (bool), `bit_big_endian` (bool), `unk18` (int32_t), `write_file_headers_end` (bool)
    - Eager API (mirrors upstream `BND3` class): `sf_bnd3_create`, `sf_bnd3_destroy`, `sf_bnd3_read_from_path/_from_memory`, `sf_bnd3_write_to_path/_to_memory`, `sf_bnd3_file_count`, `sf_bnd3_get_file`, `sf_bnd3_add_file`, `sf_bnd3_remove_file`
    - Reader API (mirrors `BND3Reader`): `sf_bnd3_reader_t` opens file lazily, exposes `sf_bnd3_reader_open`, `sf_bnd3_reader_close`, `sf_bnd3_reader_file_count`, `sf_bnd3_reader_read_file_by_index` (returns heap-owned bytes; caller frees), `sf_bnd3_reader_read_file_by_id`
    - Internally uses `sfi_binder3_*` helpers from T2.
    - DCX outer wrapper: `sf_bnd3_read_from_path("foo.bnd.dcx")` auto-unwraps via `sf_get_decompressed_reader` (Phase 1 helper); on write, replays original DCX type from `sf_dcx_compression_info_t` cached at parse.
  - Create `src/archive/bnd3.c`. Mirror `BND3.cs` (149 LOC) + `BND3Reader.cs` (137 LOC) line by line. Critical fields: `Unk18` (int, asserted 0 or 0x80000000) at byte 0x18, `WriteFileHeadersEnd` (bool, controls whether file headers are written before vs interleaved with data) — DS3 sometimes uses `false`.
  - Create `tests/archive/test_bnd3_synthetic.c` with sub-tests:
    1. **Round-trip 3 entries**: create BND3 v1.0 with id 100/200/300 and names `a.txt`/`b.bin`/`c.dat`, set `Unk18=0`, `WriteFileHeadersEnd=true`, `BigEndian=false`, write to memory, read back, write to memory again, assert byte-equal.
    2. **`WriteFileHeadersEnd=false`** variant: same content, different flag, byte-equal round-trip.
    3. **`Unk18=0x80000000`** variant: byte-equal round-trip.
    4. **`BigEndian=true` + `BitBigEndian=true`** variant (DeS-style, even if v2 game): exercises reverse-bits path; byte-equal round-trip.
    5. **Reader pattern**: open from file, read each entry by index, assert content matches eager-mode reader output for same fixture.
  - Update `docs/api-mapping/format-bnd3.md` flipping all 17 rows from `未实现` to `✓ aligned`.

  **Must NOT do**:
  - Skip `Unk18` or `WriteFileHeadersEnd` — both are required for round-trip on real DS-era files.
  - Re-implement Format byte read/write — use `sfi_binder_read_format` / `_write_format` from T2.
  - Sort the in-memory Files list.
  - Eager-compute hash table (BND3 doesn't have one anyway, but mention as guardrail for parity with BND4 task).
  - Forget `bw.Pad(0x10)` before writing file data when `bytes_size > 0`.
  - Mix DCX wrapping detection with binder parsing — outer DCX is unwrapped FIRST, then BND3 is parsed on the decompressed bytes.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~600 LOC C output (header + impl + test); real round-trip semantics; multiple corner cases.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T7/T8/T9/T10/T11/T12
  - **Parallel Group**: Wave 2
  - **Blocks**: T13 (mapping doc update)
  - **Blocked By**: T1, T2

  **References**:
  - Pattern: `src/compression/dcx.c` — internal organization with multiple sub-routines, error path discipline
  - Pattern: T2 `binder_common.c` — sfi_binder3_* helpers used here
  - API: Upstream `BND3.cs` (149 lines) + `BND3Reader.cs` (137 lines) — line-by-line port targets
  - API: Upstream `IBND3.cs` — interface contract for both eager and reader
  - API: `docs/api-mapping/format-bnd3.md` — 17 rows enumerating exact upstream symbols
  - Test: `tests/compression/test_dcx_dflt.c` — Phase 2 round-trip pattern
  - WHY each: BND3.cs/BND3Reader.cs are canonical wire-format spec; mapping doc is the contract for what must land; existing tests show round-trip framework.

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_bnd3.h` declares all eager + reader public API
  - [ ] `src/archive/bnd3.c` implements per upstream
  - [ ] All 5 synthetic sub-tests PASS byte-equal
  - [ ] `Unk18` and `WriteFileHeadersEnd` both round-trip
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bnd3_synthetic$' --output-on-failure` PASS
  - [ ] DLL export count INCREASES by ~12 (eager 8 + reader 5 - some overlap)
  - [ ] `format-bnd3.md` 0 `未实现` rows remain

  **QA Scenarios**:

  ```
  Scenario: 5 round-trip variants byte-equal
    Tool: Bash (`ctest`)
    Preconditions: T1 + T2 + T6 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bnd3_synthetic 2>&1 | tee .sisyphus/evidence/task-T6-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bnd3_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T6-ctest.log
      3. grep -E 'test_bnd3_(roundtrip|unk18|write_file_headers_end|big_endian|reader)' .sisyphus/evidence/task-T6-ctest.log
    Expected Result: All 5 sub-tests PASS
    Failure Indicators: any FAIL; byte-mismatch in any variant
    Evidence: .sisyphus/evidence/task-T6-build.log + .sisyphus/evidence/task-T6-ctest.log

  Scenario: Mapping doc has zero 未实现 for BND3
    Tool: Bash (`grep`)
    Preconditions: T6 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-bnd3.md | tee .sisyphus/evidence/task-T6-mapping.log
    Expected Result: "0"
    Failure Indicators: any non-zero count
    Evidence: .sisyphus/evidence/task-T6-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(bnd3): port BND3 read/write + reader + synthetic round-trip test`
  - Files: `include/souls_formats/sf_bnd3.h`, `src/archive/bnd3.c`, `tests/archive/test_bnd3_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt` (`SF_PUBLIC_HEADERS` + `SF_SOURCES`), `docs/api-mapping/format-bnd3.md`, `include/souls_formats/souls_formats.h` (umbrella include)
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T7. **Port BND4 read/write + BND4Reader (eager + streaming) + synthetic round-trip test (ER critical)**

  **What to do**:
  - Create `include/souls_formats/sf_bnd4.h` with public API mirroring upstream BND4 + BND4Reader:
    - Opaque: `sf_bnd4_t`, `sf_bnd4_reader_t`
    - Properties: `version`, `format`, `big_endian`, `bit_big_endian`, `unicode` (bool), `extended` (byte), `unk04` (bool), `unk05` (bool)
    - Eager API: `sf_bnd4_create/destroy/read_from_path/read_from_memory/write_to_path/write_to_memory`, `sf_bnd4_file_count/get_file/add_file/remove_file`, `sf_bnd4_find_by_path_hash` (uses BinderHashTable lookup)
    - Reader API: `sf_bnd4_reader_open/close/file_count/read_file_by_index/read_file_by_id/read_file_by_path_hash`, also expose `sf_bnd4_reader_get_outer_compression` returning `sf_dcx_compression_info_t` for write-back fidelity
    - DCX outer auto-unwrap on `_read_from_path`
  - Create `src/archive/bnd4.c`. Mirror `BND4.cs` (206 LOC) + `BND4Reader.cs` (146 LOC) line by line. Critical fields: `Unk04` (bool, asserted), `Unk05` (bool, asserted), `Unicode` (controls UTF-16 vs Shift-JIS for entry names), `Extended` (byte: 0/1/4/0x80, controls hash table presence and entry header size).
  - **Format=Names1 PC-save corner case** (BinderFileHeader.cs:149-153): when `format == Names1` only (no other name flags), reads ID twice + 0 padding. Test fixture must cover.
  - **!BitBigEndian inversion at offset 0x0A**: BND4 stores `!BitBigEndian` (inverted) at this offset. T2 helpers should already handle, but synthetic test must explicitly cover both `BitBigEndian=true` and `=false`.
  - **Hash table** (when `extended >= 4`): re-derive at write time via `sfi_binder_hash_table_write` from T2.
  - **Per-entry DCX**: cache `sf_dcx_compression_info_t` at parse for replay on write.
  - Create `tests/archive/test_bnd4_synthetic.c` with sub-tests:
    1. **Names1 PC-save fixture**: 3 entries, `format=Names1`, no IDs, byte-equal round-trip.
    2. **Names2 fixture** (Sekiro/ER style): 3 entries with names + IDs, byte-equal.
    3. **DS3 default** (`Format = IDs|Names1|Names2|Compression`): 3 entries, hash table, byte-equal.
    4. **`Unicode=false` Shift-JIS names** (older binders): byte-equal.
    5. **`Unicode=true` UTF-16 LE names** with Japanese `日本.bin` (matches phase-3 doc fixture spec): byte-equal.
    6. **`Extended=0/1/4/0x80`** four variants: assert hash table presence flips correctly.
    7. **`Unk04=true`/`Unk05=true`** variant (asserted but exists): byte-equal.
    8. **Reader pattern**: lazy read each entry by index, by path_hash; assert outer compression info accessible via `sf_bnd4_reader_get_outer_compression`.
  - Update `docs/api-mapping/format-bnd4.md` flipping all 17 rows to `✓ aligned`.

  **Must NOT do**:
  - Skip `Unk04`, `Unk05`, `Unicode`, `Extended` — required for ER round-trip.
  - Skip Names1 PC-save corner case — easy to miss; must be in synthetic test fixture.
  - Forget `!BitBigEndian` inversion at 0x0A — silent corruption.
  - Eager-compute hash table on `add_file` — re-derive at write time.
  - Sort the in-memory Files list (write-time-only sort within hash table).
  - Re-implement BinderHashTable.Write — use `sfi_binder_hash_table_write` from T2.
  - Forget `bw.Pad(0x10)` before file data.
  - Compute the prime at any time other than write — re-search per write.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: ER-critical foundation for downstream e2e; complex hash table semantics; many edge cases. ~750 LOC C output. Bugs here cascade to every Phase 4-7 e2e test.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T6/T8/T9/T10/T11/T12
  - **Parallel Group**: Wave 2
  - **Blocks**: T13 (mapping), T14 (er_helper uses BND4 to walk inside chrbnd), T16 (e2e), T18 (TPF e2e walks via BND4), T19 (CLI extracts BND4)
  - **Blocked By**: T1, T2

  **References**:
  - Pattern: T6 BND3 (sister format; structurally similar but with extra unicode/hash table)
  - API: Upstream `BND4.cs` (206 lines) + `BND4Reader.cs` (146 lines) + `IBND4.cs`
  - API: Upstream `BinderFileHeader.cs:149-153` — Names1 PC-save corner case
  - API: Upstream `BinderHashTable.cs` (T2 ports the helpers; T7 just consumes)
  - API: `docs/api-mapping/format-bnd4.md` — 17 rows
  - WHY each: BND4 is the format ER uses for chrbnd/anibnd/etc. Wire format precision is non-negotiable.

  **Acceptance Criteria**:
  - [ ] `sf_bnd4.h` declares eager + reader API including `Unk04/Unk05/Unicode/Extended`
  - [ ] `bnd4.c` implements per upstream
  - [ ] All 8 synthetic sub-tests PASS byte-equal
  - [ ] Names1 PC-save corner case covered
  - [ ] Hash table re-derived at write time, not pre-computed
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bnd4_synthetic$' --output-on-failure` PASS
  - [ ] DLL export count INCREASES by ~16 (eager + reader + find-by-hash + outer compression getter)
  - [ ] `format-bnd4.md` 0 `未实现` rows remain

  **QA Scenarios**:

  ```
  Scenario: 8 round-trip variants byte-equal
    Tool: Bash (`ctest`)
    Preconditions: T1 + T2 + T7 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bnd4_synthetic 2>&1 | tee .sisyphus/evidence/task-T7-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bnd4_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T7-ctest.log
      3. grep -E 'test_bnd4_(names1_pcsave|names2|ds3_default|shiftjis|unicode_jp|extended|unk04_unk05|reader)' .sisyphus/evidence/task-T7-ctest.log
    Expected Result: All 8 sub-tests PASS
    Failure Indicators: any FAIL; byte-mismatch
    Evidence: .sisyphus/evidence/task-T7-build.log + .sisyphus/evidence/task-T7-ctest.log

  Scenario: Names1 PC-save corner case explicitly tested
    Tool: Bash (in-test)
    Preconditions: T7 merged
    Steps:
      1. The synthetic test creates a BND4 with Format=Names1 only (no IDs|Names2)
      2. Writes; reads back; asserts entry IDs preserved, names readable, byte-equal
      3. Captured in same ctest log
    Expected Result: PASS
    Failure Indicators: id corruption; name corruption
    Evidence: .sisyphus/evidence/task-T7-ctest.log (same file)

  Scenario: Mapping doc has zero 未实现 for BND4
    Tool: Bash (`grep`)
    Preconditions: T7 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-bnd4.md | tee .sisyphus/evidence/task-T7-mapping.log
    Expected Result: "0"
    Failure Indicators: any non-zero count
    Evidence: .sisyphus/evidence/task-T7-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(bnd4): port BND4 read/write + reader + synthetic round-trip test`
  - Files: `include/souls_formats/sf_bnd4.h`, `src/archive/bnd4.c`, `tests/archive/test_bnd4_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-bnd4.md`, `include/souls_formats/souls_formats.h`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T8. **Port BXF3 read/write + BXF3Reader (split header+data) + synthetic round-trip test**

  **What to do**:
  - Create `include/souls_formats/sf_bxf3.h` with public API mirroring upstream BXF3 + BXF3Reader:
    - Opaque: `sf_bxf3_t`, `sf_bxf3_reader_t`
    - Properties: `version`, `format`, `big_endian`, `bit_big_endian`
    - Eager API: `sf_bxf3_create/destroy`, **two-path** `sf_bxf3_read_from_paths(bhd_path, bdt_path)` and `sf_bxf3_read_from_memory(bhd_bytes, bhd_size, bdt_bytes, bdt_size)`, **two-path** `sf_bxf3_write_to_paths`, `sf_bxf3_write_to_memory(out_bhd, out_bdt)`, plus `sf_bxf3_file_count/get_file/add_file/remove_file`
    - Reader API: `sf_bxf3_reader_open(bhd_path, bdt_path)`, etc.
  - Create `src/archive/bxf3.c`. Mirror `BXF3.cs` (483 LOC) + `BXF3Reader.cs` (381 LOC). Write path produces TWO byte arrays (BHD + BDT) — internal helpers `sfi_binder3_write_file_header` (T2) emit BHD bytes, `sfi_binder3_write_file_data` emits BDT bytes.
  - Read path: BHD has the file headers + format + version; BDT contains raw file data at offsets referenced from BHD. **BDF Header** (in BHD): magic "BHF3", version, file_count, then file headers. BDT magic: "BDF3" + version + reserved.
  - Create `tests/archive/test_bxf3_synthetic.c`:
    1. **Round-trip 3 entries** with `format=IDs|Names1|Names2` (no Compression — BXF3 sometimes lacks compression flag). BHD round-trips byte-equal AND BDT round-trips byte-equal.
    2. **`BigEndian=true` + `BitBigEndian=true`** variant.
    3. **Reader pattern**: open BHD+BDT, read each entry by index, content matches eager mode.
  - Update `docs/api-mapping/format-bxf3.md` flipping all 15 rows.

  **Must NOT do**:
  - Combine BHD+BDT into a single buffer — the format is intentionally split; keep separate.
  - Forget that BHD3 magic is `"BHF3"` (note: header file uses BHF, not BHD prefix).
  - Forget BDT3 magic is `"BDF3"`.
  - Use BND3 helpers without parameterization — BXF uses different file_data layout (offset is into BDT, not into self).
  - Forget that `sfi_binder3_write_file_data` for BXF must take a SEPARATE BDT writer, not the BHD writer.
  - Mix BXF3 and BXF4 wire formats — they share API shape but have different bytes (T9 is separate).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~1k LOC C output; split-archive complexity (two byte streams to manage); reader pattern.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T6/T7/T9/T10/T11/T12
  - **Parallel Group**: Wave 2
  - **Blocks**: T13
  - **Blocked By**: T1, T2

  **References**:
  - Pattern: T6 BND3 (uses same `sfi_binder3_*` helpers from T2 with different layout parameters)
  - API: Upstream `BXF3.cs` (483 lines) + `BXF3Reader.cs` (381 lines) + `IBXF3.cs`
  - API: `docs/api-mapping/format-bxf3.md` — 15 rows
  - WHY each: BXF3.cs spec for split-archive layout; T6 already proves the BND3 file_header path so BXF3 mostly differs in stream handling.

  **Acceptance Criteria**:
  - [ ] `sf_bxf3.h` declares eager + reader, two-path variants
  - [ ] `bxf3.c` implements both BHD and BDT serialization
  - [ ] All 3 synthetic sub-tests PASS byte-equal on BOTH BHD and BDT
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bxf3_synthetic$' --output-on-failure` PASS
  - [ ] `format-bxf3.md` 0 `未实现` rows remain

  **QA Scenarios**:

  ```
  Scenario: BHD + BDT both byte-equal round-trip
    Tool: Bash (`ctest`)
    Preconditions: T1 + T2 + T8 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bxf3_synthetic 2>&1 | tee .sisyphus/evidence/task-T8-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bxf3_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T8-ctest.log
      3. grep -E 'test_bxf3_(roundtrip|big_endian|reader)' .sisyphus/evidence/task-T8-ctest.log
    Expected Result: All 3 sub-tests PASS, asserting both BHD and BDT byte-equal in each
    Failure Indicators: any FAIL; byte-mismatch on either stream
    Evidence: .sisyphus/evidence/task-T8-build.log + .sisyphus/evidence/task-T8-ctest.log

  Scenario: BHD/BDT magics correct
    Tool: Bash (in-test)
    Preconditions: T8 merged
    Steps:
      1. Test inspects first 4 bytes of written BHD = "BHF3"
      2. Test inspects first 4 bytes of written BDT = "BDF3"
    Expected Result: Both PASS
    Failure Indicators: wrong magic
    Evidence: .sisyphus/evidence/task-T8-ctest.log (same file)

  Scenario: Mapping doc clean
    Tool: Bash (`grep`)
    Preconditions: T8 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-bxf3.md | tee .sisyphus/evidence/task-T8-mapping.log
    Expected Result: "0"
    Failure Indicators: non-zero
    Evidence: .sisyphus/evidence/task-T8-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(bxf3): port BXF3 read/write + reader + synthetic round-trip test`
  - Files: `include/souls_formats/sf_bxf3.h`, `src/archive/bxf3.c`, `tests/archive/test_bxf3_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-bxf3.md`, `include/souls_formats/souls_formats.h`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T9. **Port BXF4 read/write + BXF4Reader (split header+data, modern) + synthetic round-trip test**

  **What to do**:
  - Create `include/souls_formats/sf_bxf4.h` with public API mirroring upstream BXF4 + BXF4Reader:
    - Opaque: `sf_bxf4_t`, `sf_bxf4_reader_t`
    - Properties: `version`, `format`, `big_endian`, `bit_big_endian`, `unicode`, `extended`, `unk04`, `unk05`
    - Eager: two-path read/write API (mirrors BXF3 shape but with BND4-style fields).
    - Reader: lazy two-path open, expose `outer_compression` getter on each side.
  - Create `src/archive/bxf4.c`. Mirror `BXF4.cs` (578 LOC) + `BXF4Reader.cs` (401 LOC). BHD4 magic "BHF4", BDF4 magic "BDF4".
  - **BDF4 fields read-and-discard** (line 237-238 in BXF4.cs): read past the BDF header but discard most fields except for the magic + signature; on write, emit a canonical default BDF header (mirror upstream).
  - **Hash table** on BHD4 side (when `extended >= 4`): re-derive at write via T2 helper.
  - **Per-entry DCX**: cache `sf_dcx_compression_info_t` from BDT side at parse for replay on write.
  - Create `tests/archive/test_bxf4_synthetic.c`:
    1. **Names1+Names2 default DS3 style** with hash table, 3 entries, byte-equal both BHD and BDT.
    2. **`Unicode=true` UTF-16 LE names** with `日本.bin` like BND4: byte-equal.
    3. **`Extended=4` hash table present** vs **`Extended=0` no hash table** variants.
    4. **Reader pattern**: lazy open both sides, read by index/path_hash; outer compression getter exposed.
  - Update `docs/api-mapping/format-bxf4.md` flipping all 17 rows.

  **Must NOT do**:
  - Skip Unk04/Unk05/Unicode/Extended.
  - Forget BHD4="BHF4" / BDF4="BDF4" magics.
  - Re-implement BinderHashTable — reuse T2 helper.
  - Combine BHD+BDT into one buffer.
  - Sort the in-memory Files list.
  - Mix BXF3 and BXF4 helpers — separate code paths.
  - Forget `bw.Pad(0x10)` before file data on BDT side.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~1.5k LOC C output; combination of BND4's complexity (hash table, unicode) AND BXF3's split-archive shape.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T6/T7/T8/T10/T11/T12
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T17 (BXF4 e2e for ER tpfbhd/tpfbdt)
  - **Blocked By**: T1, T2

  **References**:
  - Pattern: T7 BND4 (sister modern format) + T8 BXF3 (sister split format)
  - API: Upstream `BXF4.cs` (578 lines) + `BXF4Reader.cs` (401 lines) + `IBXF4.cs`
  - API: `docs/api-mapping/format-bxf4.md` — 17 rows
  - WHY each: BXF4 = BND4 features × split-archive structure; both prior tasks provide partial templates.

  **Acceptance Criteria**:
  - [ ] `sf_bxf4.h` declares eager + reader, two-path variants, all unk fields exposed
  - [ ] `bxf4.c` implements per upstream
  - [ ] 4 synthetic sub-tests PASS byte-equal both BHD and BDT
  - [ ] Hash table re-derived at write
  - [ ] BDF4 fields read-and-discard works
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bxf4_synthetic$' --output-on-failure` PASS
  - [ ] `format-bxf4.md` 0 `未实现` rows remain

  **QA Scenarios**:

  ```
  Scenario: 4 round-trip variants both BHD and BDT byte-equal
    Tool: Bash (`ctest`)
    Preconditions: T1 + T2 + T9 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bxf4_synthetic 2>&1 | tee .sisyphus/evidence/task-T9-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bxf4_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T9-ctest.log
      3. grep -E 'test_bxf4_(default|unicode|hashtable|reader)' .sisyphus/evidence/task-T9-ctest.log
    Expected Result: All 4 sub-tests PASS
    Failure Indicators: any FAIL
    Evidence: .sisyphus/evidence/task-T9-build.log + .sisyphus/evidence/task-T9-ctest.log

  Scenario: Mapping doc clean
    Tool: Bash (`grep`)
    Preconditions: T9 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-bxf4.md | tee .sisyphus/evidence/task-T9-mapping.log
    Expected Result: "0"
    Failure Indicators: non-zero
    Evidence: .sisyphus/evidence/task-T9-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(bxf4): port BXF4 read/write + reader + synthetic round-trip test`
  - Files: `include/souls_formats/sf_bxf4.h`, `src/archive/bxf4.c`, `tests/archive/test_bxf4_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-bxf4.md`, `include/souls_formats/souls_formats.h`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T10. **Port BHD5 streaming-only reader (RSA wrap detect + AES range decrypt + per-game branch) + synthetic round-trip test**

  **What to do**:
  - **CORRECTED scope** (per Momus review): T0d already created `include/souls_formats/sf_bhd5.h` with the opaque type forward decl + `sf_bhd5_game_t` enum + `_Static_assert`. T10 ADDS function declarations to the existing file (does NOT create from scratch):
    - Append the streaming-only API declarations to `sf_bhd5.h`:
      ```c
      SF_API sf_result_t sf_bhd5_open(sf_bhd5_t **out,
                                      const wchar_t *bhd_path,
                                      const wchar_t *bdt_path,
                                      sf_bhd5_game_t game,
                                      const sf_allocator_t *a);
      SF_API void        sf_bhd5_close(sf_bhd5_t *b);
      SF_API size_t      sf_bhd5_bucket_count (const sf_bhd5_t *b);
      SF_API size_t      sf_bhd5_total_file_count(const sf_bhd5_t *b);
      SF_API const char *sf_bhd5_get_salt   (const sf_bhd5_t *b);  /* heap-owned by b */
      SF_API bool        sf_bhd5_get_big_endian(const sf_bhd5_t *b);
      SF_API sf_result_t sf_bhd5_extract_by_hash_64(const sf_bhd5_t *b,
                                                    uint64_t path_hash,
                                                    void **out, size_t *out_size,
                                                    const sf_allocator_t *a);
      SF_API sf_result_t sf_bhd5_extract_by_hash_32(const sf_bhd5_t *b,
                                                    uint32_t path_hash,
                                                    void **out, size_t *out_size,
                                                    const sf_allocator_t *a);
      SF_API sf_result_t sf_bhd5_extract_by_path  (const sf_bhd5_t *b,
                                                    const char *utf8_path,
                                                    void **out, size_t *out_size,
                                                    const sf_allocator_t *a);
      SF_API sf_result_t sf_bhd5_write             (const sf_bhd5_t *b,
                                                    const wchar_t *bhd_path);
      ```
    - Internal-only struct `sfi_bhd5_file_header_t` matching upstream: `file_name_hash` (u64 for ER+, u32 for older), `padded_size`, `file_size`, `file_offset`, optional `sha_hash` (32 bytes verbatim), optional `aes_key` + `aes_ranges`.
  - Create `src/archive/bhd5.c`. Mirror `BHD5.cs` (746 LOC) line-by-line. CRITICAL flows:
    - **BHD file open**: read first 4 bytes → if `42 48 44 35` ("BHD5") then plaintext path; else assume RSA-wrapped → read entire BHD into RAM (≤ ~5 MB), call `sfi_rsa_decrypt_pkcs1` with the matching game's PEM key, then verify decrypted output starts with "BHD5".
    - **`is64Bit` auto-detection** at offset 0x14 (`BHD5.cs:151-163`): three i32 reads; if `test0 == 0 && test1 == 0` → 64-bit fields; else 32-bit. Branches all bucket reads.
    - **Per-game branching in FileHeader read/write** (`BHD5.cs:466-516`): switch on `game` enum:
      - DS1: 32-bit hash + 32-bit padded_size + 32-bit file_size + 32-bit file_offset
      - DS2: above + i32 sha_hash_offset + i32 aes_key_offset
      - DS3: 64-bit hash + 64-bit fields + sha + aes
      - ER+: 64-bit hash + 32-bit padded_size + 32-bit file_size + 64-bit file_offset + sha + aes
    - **`!BitBigEndian` inversion at byte 0x0A** (mirrors BND4): same trick.
    - **Salted SHA hash blob** (32 bytes per entry): READ verbatim into `sha_hash` field; on WRITE, emit verbatim from same field. NEVER compute fresh.
    - **`EncryptionSupported >= DarkSouls2` gate**: only DS2+ have SHA/AES; DS1 skips.
    - **AES range decryption** via `sfi_aes_decrypt_ecb_buffer` (T0b): each `sfi_bhd5_aes_range_t` has `start_offset`, `end_offset`. Skip if `start == end` (legal). Multi-of-16 enforced. Decrypt in-place after reading the BDT range.
    - **Range bounds check**: every range must have `end_offset <= bdt_file_length`. Reject as `SF_ERR_TRUNCATED` if not.
    - **Streaming**: `sf_bhd5_t` owns the BDT `sf_istream_t` (file-backed). `sf_bhd5_extract_by_*` seeks to `file_offset`, reads `padded_size` bytes, applies AES range decrypts in-place, returns the unencrypted bytes. NEVER load the full BDT.
    - **Path lookup**: `sf_bhd5_extract_by_path` calls `sf_path_hash` (32-bit) AND `sf_path_hash_64` (64-bit, T0c) per `is64Bit`, then linear scans bucket entries. (Buckets are pre-grouped by hash mod count in upstream — keep that structure for parity.)
  - Create `tests/archive/test_bhd5_synthetic.c`:
    1. **`game=SF_BHD5_GAME_ELDENRING`, is64Bit=true, 1 bucket × 2 files, 1 file with 256-byte AES-128-ECB encrypted range**: round-trip byte-equal (BHD only — synthetic BDT is in-memory).
    2. **`game=SF_BHD5_GAME_SEKIRO`, is64Bit=true** (DS3-equivalent): same shape, byte-equal.
    3. **Range with `StartOffset == EndOffset`** (legal skip): no decryption attempted.
    4. **Empty bucket** (count=0 entries): parses + writes correctly.
    5. **Range bounds check**: malformed BHD with `end_offset > bdt_file_length` returns `SF_ERR_TRUNCATED`.
  - Update `docs/api-mapping/format-bhd5.md` flipping all 13 rows.

  **Must NOT do**:
  - Compute SHA on write.
  - Load Data0.bdt into RAM — `sf_bhd5_t` MUST hold a file-backed `sf_istream_t` and seek+read.
  - Forget the `is64Bit` auto-detect — older games will mis-parse.
  - Forget `EncryptionSupported >= DarkSouls2` gate — DS1 has no SHA/AES.
  - Range-decrypt without bounds check against actual BDT length.
  - Implement DSR/DS2/DS3 enum values — extension is 4 v1 games only (extensions.md per GAP-2).
  - Trust the salted SHA — `sfi_bhd5_*` does NOT compare hash on read; it's stored opaque.
  - Forget that `Range.StartOffset == Range.EndOffset` is **legal** (means "skip this range").
  - Hash-table-style sort buckets — buckets are preserved as input.
  - Compute keys from PEM — keys are pre-embedded in `bhd5_keys.c` (T3). T10 just looks them up via `sfi_bhd5_get_aes_key`.

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: ~1.5k LOC C output; encryption layers (RSA + AES range); per-game branching; streaming lifetime; range bounds checking. Highest single-task complexity in Phase 3.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T6-T9, T11, T12
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T14, T15
  - **Blocked By**: T0b (AES range), T0c (sf_path_hash_64), T0d (RSA + PEM), T3 (AES keys + lookup)

  **References**:
  - Pattern: T8/T9 BXF (split-archive lifetime as analog for BHD/BDT)
  - Pattern: `src/compression/dcx.c` — large internal file with multiple sub-routines
  - API: Upstream `BHD5.cs` (746 lines) — line-by-line port, every branch must be mirrored
  - API: `docs/api-mapping/format-bhd5.md` — 13 rows
  - API: T0b sfi_aes_decrypt_ecb_buffer — consumer of this helper
  - API: T0d sfi_rsa_decrypt_pkcs1 — consumer of this helper
  - API: T3 sfi_bhd5_get_aes_key — consumer of this lookup
  - WHY each: BHD5.cs is the canonical source; the 3 prereq tasks provide infrastructure that must integrate cleanly.

  **Acceptance Criteria**:
  - [ ] `sf_bhd5.h` (extended from T0d) now declares the full streaming API; `sf_bhd5_game_t` enum (4 values) was already added by T0d
  - [ ] `bhd5.c` implements per upstream with all branches
  - [ ] RSA-wrapped BHD auto-detected and unwrapped
  - [ ] AES range decrypt uses T0b helper, bounds-checked against BDT length
  - [ ] Per-game branching in FileHeader read/write switch present
  - [ ] All 5 synthetic sub-tests PASS
  - [ ] Streaming verified: opening a 10 MB synthetic BDT does NOT load into RAM (verify via heap allocator instrumentation OR strict bounded `sf_alloc` count assertion)
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bhd5_synthetic$' --output-on-failure` PASS
  - [ ] DLL export count INCREASES by ~10 (open/close + 5 getters/extractors + write)
  - [ ] `format-bhd5.md` 0 `未实现` rows remain
  - [ ] `extensions.md` `sf_bhd5_game_t` extension recorded

  **QA Scenarios**:

  ```
  Scenario: 5 synthetic BHD5 variants
    Tool: Bash (`ctest`)
    Preconditions: T0b + T0c + T0d + T3 + T10 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bhd5_synthetic 2>&1 | tee .sisyphus/evidence/task-T10-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bhd5_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T10-ctest.log
      3. grep -E 'test_bhd5_(eldenring|sekiro|range_skip|empty_bucket|bounds_check)' .sisyphus/evidence/task-T10-ctest.log
    Expected Result: All 5 sub-tests PASS
    Failure Indicators: any FAIL
    Evidence: .sisyphus/evidence/task-T10-build.log + .sisyphus/evidence/task-T10-ctest.log

  Scenario: Streaming — large synthetic BDT does not blow heap
    Tool: Bash (custom alloc-counting allocator)
    Preconditions: T10 merged
    Steps:
      1. Sub-test creates a synthetic 10 MB BDT file, opens via sf_bhd5_open with a custom counting allocator
      2. Asserts peak heap < 1 MB during sf_bhd5_open + sf_bhd5_extract_by_hash_64 of a 4 KB entry (verifies no full BDT load)
    Expected Result: Heap usage < 1 MB
    Failure Indicators: Heap usage > 5 MB → indicates BDT was loaded
    Evidence: .sisyphus/evidence/task-T10-ctest.log (same file)

  Scenario: Mapping doc clean
    Tool: Bash (`grep`)
    Preconditions: T10 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-bhd5.md | tee .sisyphus/evidence/task-T10-mapping.log
    Expected Result: "0"
    Failure Indicators: non-zero
    Evidence: .sisyphus/evidence/task-T10-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(bhd5): port BHD5 streaming reader + synthetic round-trip test`
  - Files: `include/souls_formats/sf_bhd5.h` (EXTEND — T0d created the file; T10 adds function decls), `src/archive/bhd5.c`, `tests/archive/test_bhd5_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-bhd5.md`, `docs/api-mapping/extensions.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T11. **Port TPF + PC-only Headerizer + minimal DDS metadata + synthetic round-trip test**

  **What to do**:
  - Create `include/souls_formats/sf_tpf.h`:
    - Public enum `sf_tpf_platform_t` mirroring upstream `TPFPlatform`: PC=0, Xbox360=1, PS3=2, PS4=4, Xbox1=5, PS5=6, ... (verify exact values from `TPF.cs:476`)
    - Opaque `sf_tpf_t`, `sf_tpf_texture_t`
    - Eager API: `sf_tpf_create/destroy/read_from_path/_from_memory/write_to_path/_to_memory`, `sf_tpf_texture_count/get_texture/add_texture/remove_texture`
    - Properties: `platform`, `encoding`, `flag2` (byte)
    - Texture properties: `name` (UTF-8), `format` (byte), `flags1`/`flags2`, `mipmap_count`, `cubemap` (bool), `dxgi_format` (u32, derived via T0e), `bytes` + `bytes_size`
  - Create `src/archive/tpf.c`. Mirror `TPF.cs` (630 LOC). CRITICAL flows:
    - Magic "TPF\0" + endian byte + bit-endian byte + version byte.
    - Texture list: count, then per-texture header → name pool → byte data pool.
    - **Per-texture DCX**: when `Flags1 == 2 || Flags1 == 3`, the bytes are themselves DCP_EDGE-wrapped — auto-decompress via `sf_dcx_decompress_from_buffer` with assert on type=DCP_EDGE.
    - **DX10 cubemap fix at byte 0x8C** (PC platform only, TPF.cs:357): read DDS header from texture bytes via `sfi_dds_parse_header` (T0e) → if `dxgi_format` indicates cubemap (BC1_TYPELESS etc) and `cubemap` flag in TPF metadata is true, patch the DDS dwCaps2 byte at offset 0x8C in-place.
    - **Switch (Virtuos) detection** at offset 0x24 > 0xD when Platform==PS4 + length≥0x28: branches per upstream `TPF.cs` ~line 215. Implement detection but mark unsupported via `SF_ERR_UNSUPPORTED_VERSION` for v1 (PS4-Switch is rare in target games).
    - **PS3 special-case in Write**: writes `textureDataSize` (sum of all texture byte arrays) instead of `bw.Position - dataStart` to DataSize field. Per upstream `TPF.cs` ~line 470. Implement only as `SF_ERR_UNSUPPORTED_VERSION` for v1 PC-only scope; document deviation in extensions.md.
  - Create `src/archive/tpf_headerizer.c` with PC-only Headerizer (capped per D3 user decision):
    - Static `textureFormatMap` table (small, ~80 entries; mirror upstream `Headerizer.cs` early static dictionary)
    - `sfi_tpf_headerize(const sf_tpf_texture_t *tex, sf_tpf_platform_t platform, void **out, size_t *out_size, const sf_allocator_t *a)`:
      - if platform == PC → just return `tex->bytes` (verbatim)
      - else → return `SF_ERR_UNSUPPORTED_VERSION` (PS3/PS4/Xbox/PS5 paths deferred to v1.x)
  - Create `tests/archive/test_tpf_synthetic.c`:
    1. **PC + 2 textures** (8×8 BC1 each) round-trip byte-equal.
    2. **PS4 + Header w/ DXGI_FORMAT** read but `Headerize` returns `SF_ERR_UNSUPPORTED_VERSION`.
    3. **Texture w/ Flags1=2 (DCP_EDGE wrapped)** auto-decompresses on read; on write, re-wraps with same compression info.
    4. **Texture w/ FloatStruct** present (some PS3 textures have it; we still parse-and-store for round-trip).
  - Update `docs/api-mapping/format-tpf.md` flipping all 9 rows; PS3/PS4/Xbox/PS5 rows marked `~ partial` with extension note in extensions.md.

  **Must NOT do**:
  - Implement DDS pixel decoding.
  - Implement Xbox360/Xbone/PS5 paths — return `SF_ERR_UNSUPPORTED_VERSION` (mirror upstream NotImplementedException).
  - Implement PS3/PS4 Headerize write path beyond stub.
  - Apply DX10 cubemap fix on non-PC platforms (it's PC-only at 0x8C).
  - Forget that PS3 has different padding (0x80) and different data-size accounting.
  - Forget per-texture DCP_EDGE for `Flags1 == 2 || == 3`.
  - Inline DDS header parser — reuse T0e `sfi_dds_parse_header`.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~1k LOC C output for tpf.c + ~200 LOC for headerizer + textureFormatMap data; per-platform branching; DCX inline.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: T13, T18 (ER tpf e2e)
  - **Blocked By**: T0e (DDS parser), T1 (sf_binder.h), T2 implicit via DCX usage

  **References**:
  - Pattern: T6/T7/T8/T9 BND/BXF — eager + reader pattern, but TPF doesn't have a streaming reader (texture data is small)
  - API: Upstream `TPF.cs` (630 lines) + `Headerizer.cs` (919 lines, port only PC subset) + `DDS.cs` (skipped for pixels)
  - API: `docs/api-mapping/format-tpf.md` — 9 rows
  - API: T0e `sfi_dds_parse_header` — consumer of this for cubemap derivation
  - WHY each: TPF.cs is canonical; Headerizer is large but most of it is PS3/PS4/Xbox paths we're scoping out.

  **Acceptance Criteria**:
  - [ ] `sf_tpf.h` exposes platform enum + texture POD + read/write API
  - [ ] `tpf.c` + `tpf_headerizer.c` implement PC-only path; non-PC returns `SF_ERR_UNSUPPORTED_VERSION` cleanly
  - [ ] textureFormatMap is a static const lookup table
  - [ ] All 4 synthetic sub-tests PASS
  - [ ] DX10 cubemap fix at 0x8C applied on PC platform only
  - [ ] Per-texture DCP_EDGE round-trips byte-equal
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_tpf_synthetic$' --output-on-failure` PASS
  - [ ] `format-tpf.md` 0 `未实现` rows for in-scope; PS3/PS4/Xbox/PS5 rows updated as `~ partial` with link to extensions.md
  - [ ] `extensions.md` records "PC-only Headerizer" and "PS3/PS4/Xbox/PS5 stubs return `SF_ERR_UNSUPPORTED_VERSION`"

  **QA Scenarios**:

  ```
  Scenario: 4 round-trip variants
    Tool: Bash (`ctest`)
    Preconditions: T0e + T1 + T11 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_tpf_synthetic 2>&1 | tee .sisyphus/evidence/task-T11-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_tpf_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T11-ctest.log
      3. grep -E 'test_tpf_(pc_2tex|ps4_dxgi|dcp_edge|float_struct)' .sisyphus/evidence/task-T11-ctest.log
    Expected Result: All 4 sub-tests PASS (`ps4_dxgi` PASSes by asserting expected `SF_ERR_UNSUPPORTED_VERSION`)
    Failure Indicators: any FAIL
    Evidence: .sisyphus/evidence/task-T11-build.log + .sisyphus/evidence/task-T11-ctest.log

  Scenario: Mapping doc clean
    Tool: Bash (`grep`)
    Preconditions: T11 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-tpf.md | tee .sisyphus/evidence/task-T11-mapping.log
    Expected Result: "0"
    Failure Indicators: non-zero
    Evidence: .sisyphus/evidence/task-T11-mapping.log

  Scenario: extensions.md records the deviations
    Tool: Bash (`grep`)
    Preconditions: T11 merged
    Steps:
      1. grep -E 'sfi_dds_parse_header|PC-only Headerizer|PS3|PS4|Xbox|PS5' docs/api-mapping/extensions.md | tee .sisyphus/evidence/task-T11-extensions.log
    Expected Result: Multiple matches confirming each extension/deviation is noted
    Failure Indicators: 0 matches for "PC-only Headerizer"
    Evidence: .sisyphus/evidence/task-T11-extensions.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log, extensions log

  **Commit**: YES
  - Message: `archive(tpf): port TPF + PC-only Headerizer + synthetic round-trip test`
  - Files: `include/souls_formats/sf_tpf.h`, `src/archive/tpf.c`, `src/archive/tpf_headerizer.c`, `tests/archive/test_tpf_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-tpf.md`, `docs/api-mapping/extensions.md`, `include/souls_formats/souls_formats.h`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T12. **Port ENFL (load preload list with zlib payload) + synthetic round-trip test**

  **What to do**:
  - Create `include/souls_formats/sf_enfl.h`:
    - Opaque `sf_enfl_t`
    - Public POD types `sf_enfl_struct1_t`, `sf_enfl_struct2_t` (mirror upstream Struct1/Struct2 fields)
    - Eager API: `sf_enfl_create/destroy/read_from_path/_from_memory/write_to_path/_to_memory`
    - Properties: `struct1_count/get_struct1`, `struct2_count/get_struct2`, `string_count/get_string`
  - Create `src/archive/enfl.c`. Mirror `ENFL.cs` (177 LOC):
    - Magic "ENFL" + assert constant `0x10415` after magic.
    - **Internal zlib** (NOT external DCX) — use existing zlib-ng helper from Phase 2 (`sfi_zlib_decompress` / `sfi_zlib_compress`).
    - Read/Write three sections: Struct1s, Struct2s, Strings, each padded to 0x10 boundary (`bw.Pad(0x10)`).
  - Create `tests/archive/test_enfl_synthetic.c`:
    1. **5 entries with zlib-compressed payload** round-trip byte-equal.
    2. **Multiple strings** including UTF-16 names (some load lists use UTF-16) round-trip.
  - Update `docs/api-mapping/format-enfl.md` flipping all 6 rows to `✓ aligned`.

  **Must NOT do**:
  - Use external DCX wrapping (ENFL's payload is internal zlib).
  - Forget the `0x10415` magic constant.
  - Forget `bw.Pad(0x10)` between sections.
  - Skip Strings section if empty (must still emit 0-length section, not omit).

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: ~300 LOC C output; small straightforward format.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of all Wave 2 tasks
  - **Parallel Group**: Wave 2
  - **Blocks**: T13
  - **Blocked By**: T1 (uses sf_binder common allocator/result conventions)

  **References**:
  - Pattern: T6 BND3 (smaller eager-only port template)
  - API: Upstream `ENFL.cs` (177 lines)
  - API: `docs/api-mapping/format-enfl.md` — 6 rows
  - API: Phase 2 zlib helpers in `src/compression/`
  - WHY each: ENFL.cs is canonical; existing zlib helpers are infrastructure.

  **Acceptance Criteria**:
  - [ ] `sf_enfl.h` declares opaque + struct1/struct2 POD types + eager API
  - [ ] `enfl.c` implements per upstream
  - [ ] 2 synthetic sub-tests PASS byte-equal
  - [ ] Internal zlib used, not external DCX
  - [ ] Magic constant `0x10415` present after "ENFL"
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_enfl_synthetic$' --output-on-failure` PASS
  - [ ] `format-enfl.md` 0 `未实现` rows remain

  **QA Scenarios**:

  ```
  Scenario: 5-entry ENFL byte-equal round-trip
    Tool: Bash (`ctest`)
    Preconditions: T12 merged
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_enfl_synthetic 2>&1 | tee .sisyphus/evidence/task-T12-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_enfl_synthetic$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T12-ctest.log
      3. grep -E 'test_enfl_(zlib_payload|utf16_strings)' .sisyphus/evidence/task-T12-ctest.log
    Expected Result: Both sub-tests PASS
    Failure Indicators: any FAIL
    Evidence: .sisyphus/evidence/task-T12-build.log + .sisyphus/evidence/task-T12-ctest.log

  Scenario: Mapping doc clean
    Tool: Bash (`grep`)
    Preconditions: T12 merged
    Steps:
      1. grep -c '未实现' docs/api-mapping/format-enfl.md | tee .sisyphus/evidence/task-T12-mapping.log
    Expected Result: "0"
    Failure Indicators: non-zero
    Evidence: .sisyphus/evidence/task-T12-mapping.log
  ```

  **Evidence to Capture**: build log, ctest log, mapping log

  **Commit**: YES
  - Message: `archive(enfl): port ENFL + synthetic round-trip test`
  - Files: `include/souls_formats/sf_enfl.h`, `src/archive/enfl.c`, `tests/archive/test_enfl_synthetic.c`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `docs/api-mapping/format-enfl.md`, `include/souls_formats/souls_formats.h`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T13. **api-mapping doc updates: flip all Phase 3 rows to `✓ aligned` + record extensions**

  **What to do**:
  - **Phase 3 row flip** (verify each prior task already flipped its own; T13 is the consolidating sweep):
    - `docs/api-mapping/format-binder-common.md` — 13 rows
    - `docs/api-mapping/format-bnd3.md` — 16 rows (verify T6 flipped)
    - `docs/api-mapping/format-bnd4.md` — 16 rows (verify T7 flipped)
    - `docs/api-mapping/format-bxf3.md` — 14 rows (verify T8 flipped)
    - `docs/api-mapping/format-bxf4.md` — 16 rows (verify T9 flipped)
    - `docs/api-mapping/format-bhd5.md` — 12 rows (verify T10 flipped)
    - `docs/api-mapping/format-tpf.md` — 10 rows (verify T11 flipped + PS3/PS4 marked partial)
    - `docs/api-mapping/format-enfl.md` — 6 rows (verify T12 flipped)
    - **Total: ~103 rows (some may have been collapsed during impl)**, all should be `✓ aligned` or `~ partial` with link to extensions
  - **`extensions.md` consolidation**: ensure all 5+ extensions added across the prior tasks are coherent and cite the upstream method they extend or replace. The required rows:
    - `sf_reverse_bits_u8` (T0a)
    - `sf_path_hash_64` (T0c)
    - `sf_bhd5_game_t` 4 v1-target enum values (T10)
    - `sf_bhd5_open(bhd, bdt)` two-path API (vs upstream's combined-stream `Read(string path, Game game)`) (T10)
    - `sfi_dds_parse_header` (T0e) + minimal DDS scope cap (vs upstream `DDS.cs` `_skipped_`)
    - `sfi_rsa_decrypt_pkcs1` + 4 game PEM keys (T0d) — extension because upstream punts on RSA layer
    - `Headerizer` PC-only scope cap; PS3/PS4/Xbox/PS5 stubs return `SF_ERR_UNSUPPORTED_VERSION` (T11)
  - **`POLICY.md` updates**: add 3 policy notes:
    - "RSA-bhd-decryption is integrated in our crypto layer; upstream BHD5 punts to caller. We support 4 v1 games via embedded PEM public keys."
    - "TPF Headerizer scope cap: PC platform only in v1; non-PC platforms return `SF_ERR_UNSUPPORTED_VERSION` (mirrors upstream NotImplementedException semantics)."
    - "Round-trip semantic: synthetic fixtures byte-equal; real ER e2e content-equal (FromSoft hash table layouts are non-deterministic vs our writer)."
  - **`drift-checklist.md` update**: add "Phase 1 retro-fit: `sf_reverse_bits_u8`" and "Phase 2 retro-fit: `sfi_aes_decrypt_ecb_buffer`" entries with timestamp.

  **Must NOT do**:
  - Mark a row `✓ aligned` if any field, edge case, or unk* property is unimplemented — verify by reading the impl side.
  - Forget the `~ partial` rows (TPF PS3/PS4/Xbox/PS5).
  - Touch any non-doc file.
  - Add new policy beyond the 3 listed.
  - Mix this task with code changes — T13 is doc-only.

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Pure documentation editing; structured table updates.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO — must run AFTER T6-T12 are merged so we can audit each row's actual state
  - **Parallel Group**: Late Wave 2 (gates Wave 3 only loosely; T20 also touches docs but different files)
  - **Blocks**: T20 (PLAN.md retrospective references this completion state)
  - **Blocked By**: T6, T7, T8, T9, T10, T11, T12

  **References**:
  - Pattern: existing `docs/api-mapping/util-io-binary-reader-ex.md` — Phase 1 mapping doc with `✓ aligned` rows for reference style
  - Pattern: existing `docs/api-mapping/extensions.md` — extension row format
  - Pattern: existing `docs/api-mapping/POLICY.md` — policy note style
  - WHY each: Match existing project style; consistency across docs.

  **Acceptance Criteria**:
  - [ ] All 8 archive mapping docs have 0 `未实现` rows for in-scope upstream symbols (TPF non-PC rows are `~ partial` not `未实现`)
  - [ ] `extensions.md` lists 7+ extension rows for Phase 3
  - [ ] `POLICY.md` has 3 new policy notes
  - [ ] `drift-checklist.md` has 2 new retro-fit entries
  - [ ] No code files modified

  **QA Scenarios**:

  ```
  Scenario: Phase 3 mapping docs are clean
    Tool: Bash (`grep`)
    Preconditions: T13 merged
    Steps:
      1. for f in docs/api-mapping/format-{binder-common,bnd3,bnd4,bxf3,bxf4,bhd5,tpf,enfl}.md; do
           echo "$f: $(grep -c '未实现' $f)"
         done | tee .sisyphus/evidence/task-T13-mapping.log
    Expected Result: Every line shows ":0"
    Failure Indicators: Any line with non-zero count
    Evidence: .sisyphus/evidence/task-T13-mapping.log

  Scenario: Extensions doc has all 7+ Phase 3 rows
    Tool: Bash (`grep`)
    Preconditions: T13 merged
    Steps:
      1. grep -c -E 'sf_reverse_bits_u8|sf_path_hash_64|sf_bhd5_game_t|sf_bhd5_open|sfi_dds_parse_header|sfi_rsa_decrypt_pkcs1|Headerizer.*PC-only' docs/api-mapping/extensions.md | tee .sisyphus/evidence/task-T13-extensions.log
    Expected Result: ≥7 (each extension at least once)
    Failure Indicators: <7
    Evidence: .sisyphus/evidence/task-T13-extensions.log

  Scenario: POLICY.md has 3 new notes
    Tool: Bash (`grep`)
    Preconditions: T13 merged
    Steps:
      1. grep -c -E 'RSA-bhd-decryption|TPF Headerizer scope cap|Round-trip semantic' docs/api-mapping/POLICY.md | tee .sisyphus/evidence/task-T13-policy.log
    Expected Result: ≥3
    Failure Indicators: <3
    Evidence: .sisyphus/evidence/task-T13-policy.log
  ```

  **Evidence to Capture**: mapping log, extensions log, policy log

  **Commit**: YES
  - Message: `docs(api-mapping): flip Phase 3 rows to ✓ aligned + record extensions`
  - Files: `docs/api-mapping/format-binder-common.md`, `format-bnd3.md`, `format-bnd4.md`, `format-bxf3.md`, `format-bxf4.md`, `format-bhd5.md`, `format-tpf.md`, `format-enfl.md`, `extensions.md`, `POLICY.md`, `drift-checklist.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure` (no code change but verify nothing broke)

- [x] T14. **Implement `er_test_helper.c` singleton (Data0.bhd/bdt opener with atexit shutdown)**

  **What to do**:
  - Replace the T4 stub `tests/e2e/er_test_helper.c` with full implementation:
    - `static sf_bhd5_t *g_data0 = NULL;` process-wide singleton.
    - `static bool g_init_attempted = false; static sf_result_t g_init_result = SF_ERR_INTERNAL;`
    - `er_helper_init(void)`: if `g_init_attempted` → return `g_init_result`. Otherwise:
      1. Probe Data0.bhd path `SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd"` via `GetFileAttributesW` — if missing → set result to `SF_ERR_IO`, set `g_init_attempted=true`, return.
      2. Probe Data0.bdt similarly.
      3. Probe Oodle DLL via `sf_oodle_set_search_path(SF_E2E_OODLE_DIR)`; check at least one of `oo2core_{6,8,9}_win64.dll` present — if missing → set result to `SF_ERR_OODLE_NOT_FOUND`, return.
      4. Call `sf_bhd5_open(&g_data0, bhd_w, bdt_w, SF_BHD5_GAME_ELDENRING, NULL)` — if fails → propagate result.
      5. `atexit(er_helper_shutdown);`
      6. Set `g_init_result = SF_OK; g_init_attempted = true;` and return SF_OK.
    - `er_extract_from_data0(const char *path, void **out, size_t *out_size)`:
      1. Call `er_helper_init()`; if not SF_OK → return propagated error.
      2. Compute `uint64_t hash64 = sf_path_hash_64(path);`
      3. Call `sf_bhd5_extract_by_hash_64(g_data0, hash64, out, out_size, NULL)` — returns the BDT-decrypted DCX bytes (allocated by sf_bhd5).
      4. Sniff DCX outer wrap: `sf_dcx_type_t type; sf_dcx_sniff(*out, *out_size, &type);`
      5. If `type != SF_DCX_TYPE_NONE`:
         - Call `sf_dcx_decompress(*out, *out_size, &decompressed, &decomp_size, NULL /*out_type*/, NULL /*alloc*/)` — note the **6-arg signature** matches `include/souls_formats/sf_dcx.h:175-177`: `(in, in_size, **out, *out_size, *out_type, *alloc)`. Pass `NULL` for `out_type` since we already have it; pass `NULL` for `alloc` to use default malloc.
         - Free the original `*out` via `sf_free(NULL, *out)`.
         - Reassign `*out = decompressed; *out_size = decomp_size;`.
      6. Else: return as-is.
    - `er_helper_shutdown(void)`: if `g_data0` non-NULL → `sf_bhd5_close(g_data0); g_data0 = NULL;`. Idempotent.
    - `er_helper_is_available(void)`: probe without side effects; return true iff init can succeed.
    - **Test-only accessor for T15 keystone test** (added to `er_test_helper.h` declarations + impl): `sf_bhd5_t *er_helper_get_bhd5_for_testing(void);` — returns `g_data0` after init. Documented as "test-internal only; never SF_API". Allows T15 to perform low-level BHD5 inspection without re-opening the file. Returns NULL if helper not initialized. **Add this to T4's skeleton declaration list update if T4 is re-touched**, otherwise add it in T14 directly to the .h file.
  - Add `tests/e2e/test_er_helper_smoke.c`: a minimal test that calls `er_helper_init` and asserts either SF_OK (env present) or SKIP (env absent). NOT a full e2e — just verifies the helper itself.
  - Update `tests/CMakeLists.txt` to register the e2e label (if not already from T4).

  **Must NOT do**:
  - Allow re-entry: `er_helper_init` is idempotent; first call wins.
  - Open per-test instead of process-wide singleton.
  - Hardcode paths inline — use `SF_E2E_*_DIR` macros from `target_compile_definitions`.
  - Forget atexit registration — leaking the BHD5 handle on test process exit is a leak.
  - Use mutex/critical section — single-threaded test runner; no need.
  - Forget that ER's outer DCX is KRAK and requires Oodle; SKIP gracefully if not found.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Lifetime + error-path + integrating 4 prior tasks. ~150 LOC C output.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO — sequential after T7 + T10
  - **Parallel Group**: Wave 3 (gates T15-T18)
  - **Blocks**: T15, T16, T17, T18
  - **Blocked By**: T7 (BND4 reader, used downstream), T10 (BHD5 streaming), T4 (skeleton header)

  **References**:
  - Pattern: T4 skeleton header (declares the surface T14 implements)
  - API: T10 sf_bhd5_open / sf_bhd5_extract_by_hash_64 / sf_bhd5_close
  - API: Existing `sf_oodle_set_search_path` from Phase 2
  - API: Existing `sf_dcx_sniff` / `sf_dcx_decompress` from Phase 2
  - Phase 2 retrospective: how `test_dcx_krak.c` handles Oodle search path
  - WHY each: T14 is pure integration — every helper call is to an existing function.

  **Acceptance Criteria**:
  - [ ] T4 stub replaced with full impl
  - [ ] `er_helper_init` is idempotent
  - [ ] `er_extract_from_data0` returns DCX-unwrapped bytes for ER paths
  - [ ] `er_helper_shutdown` registered via atexit
  - [ ] `test_er_helper_smoke.exe` PASSes or SKIPs gracefully
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_er_helper_smoke$' --output-on-failure -V` PASS or SKIP

  **QA Scenarios**:

  ```
  Scenario: Init succeeds when env complete
    Tool: Bash (`ctest`)
    Preconditions: T14 merged + ER copy present + Oodle DLL present
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_er_helper_smoke 2>&1 | tee .sisyphus/evidence/task-T14-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_er_helper_smoke$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T14-ctest.log
    Expected Result: Test PASS or SKIP-with-message; never FAIL
    Failure Indicators: any FAIL
    Evidence: .sisyphus/evidence/task-T14-build.log + .sisyphus/evidence/task-T14-ctest.log

  Scenario: Init is idempotent
    Tool: Bash (in-test sub-assertion)
    Preconditions: T14 merged
    Steps:
      1. Test calls er_helper_init() three times in sequence; asserts all three return same result
      2. Asserts er_extract_from_data0 succeeds at least once when init succeeded
    Expected Result: PASS
    Failure Indicators: different return values across calls
    Evidence: .sisyphus/evidence/task-T14-ctest.log (same file)

  Scenario: Shutdown is idempotent
    Tool: Bash (in-test sub-assertion)
    Preconditions: T14 merged
    Steps:
      1. Test explicitly calls er_helper_shutdown() twice; second call no-op (no segfault)
    Expected Result: PASS
    Failure Indicators: segfault on second call
    Evidence: .sisyphus/evidence/task-T14-ctest.log (same file)
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `tests(e2e): implement er_test_helper.c singleton (Data0.bhd/bdt opener)`
  - Files: `tests/e2e/er_test_helper.c` (replaces stub), `tests/e2e/test_er_helper_smoke.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T15. **Add `test_bhd5_e2e_er.c` — KEYSTONE e2e (RSA → BHD5 → AES → DCX_KRAK → BND4 magic)**

  **What to do**:
  - Create `tests/e2e/test_bhd5_e2e_er.c` with the keystone scenario.
  - **CORRECTED API USAGE** (per Momus review): T15 must NOT touch the static `g_data0` directly — it's confined to `er_test_helper.c`'s translation unit. Use `er_helper_get_bhd5_for_testing()` accessor declared by T14 in `er_test_helper.h`. All `sf_dcx_decompress` calls use the 6-arg signature `(in, in_size, **out, *out_size, *out_type, *alloc)` per `sf_dcx.h:175-177`.
  - Sub-test 1 — **Open & summarize**:
    1. Call `er_helper_init()`. SKIP test if not SF_OK with descriptive `TEST_IGNORE_MESSAGE`.
    2. `sf_bhd5_t *bhd = er_helper_get_bhd5_for_testing();` — assert non-NULL.
    3. Assert `sf_bhd5_bucket_count(bhd) > 0`.
    4. Assert `sf_bhd5_total_file_count(bhd) > 1000`.
  - Sub-test 2 — **Verify BHD5 magic after RSA unwrap**:
    1. Read `/mnt/c/Games/ELDEN RING/Game/Data0.bhd` first 4 bytes via raw `sf_istream_t` (separate from helper's open).
    2. If they are "BHD5" → confirm `sf_bhd5_open` accepted plaintext path.
    3. If they are `e1 0e 36 ab` → confirm `sf_bhd5_open` invoked RSA unwrap and produced "BHD5" internally (verified indirectly by Sub-test 1's `bucket_count > 0` — only possible if BHD5 successfully parsed).
  - Sub-test 3 — **Path hash lookup**:
    1. Compute `uint64_t h = sf_path_hash_64("/chr/c0000.chrbnd.dcx");` — value should be deterministic across runs.
    2. `sf_bhd5_t *bhd = er_helper_get_bhd5_for_testing();`
    3. Call `sf_bhd5_extract_by_hash_64(bhd, h, &out, &out_size, NULL)` — assert SF_OK, `out_size > 1000`.
    4. The extracted bytes are the raw DCX-wrapped chrbnd from BDT.
  - Sub-test 4 — **DCX type detection**:
    1. `sf_dcx_type_t type;`
    2. Call `sf_dcx_sniff(out, out_size, &type)`; assert `type == SF_DCX_TYPE_DCX_KRAK`.
  - Sub-test 5 — **Oodle decompress + verify BND4 magic**:
    1. Call `sf_oodle_set_search_path(SF_E2E_OODLE_DIR)` (idempotent).
    2. `void *decompressed = NULL; size_t decomp_size = 0; sf_dcx_type_t out_type = 0;`
    3. Call `sf_dcx_decompress(out, out_size, &decompressed, &decomp_size, &out_type, NULL)` — **6-arg form** matching `sf_dcx.h:175-177`. Assert SF_OK.
    4. Assert `out_type == SF_DCX_TYPE_DCX_KRAK` (confirms what was decompressed).
    5. Assert first 4 bytes of `decompressed` == "BND4" (`42 4E 44 34`).
    6. Free both buffers via `sf_free(NULL, out); sf_free(NULL, decompressed);`.
  - Use `er_extract_from_data0` (T14) for sub-tests 6-7 to validate the higher-level helper:
    - Sub-test 6: `void *result = NULL; size_t result_size = 0;` Call `er_extract_from_data0("/chr/c0000.chrbnd.dcx", &result, &result_size);` — assert SF_OK; `result_size > 1000`.
    - Sub-test 7: Assert `((uint8_t*)result)[0..3]` == "BND4" (helper auto-unwraps outer DCX). Free `sf_free(NULL, result);`.

  **Must NOT do**:
  - Hard-fail when ER copy or Oodle DLL is missing — must SKIP gracefully.
  - Reference `g_data0` directly (it's a `static` in `er_test_helper.c`'s translation unit) — use `er_helper_get_bhd5_for_testing()` accessor instead.
  - Use the wrong `sf_dcx_decompress` arity. Per `include/souls_formats/sf_dcx.h:175-177` it is **6-arg**: `(in, in_size, **out, *out_size, *out_type, *alloc)`. Anything fewer is a compile error.
  - Read more than first 4 bytes of Data0.bhd by raw stream (full file is for `sf_bhd5_open` to handle).
  - Hardcode the path hash value without computing it via `sf_path_hash_64` (the function is the test target).
  - Leak buffers — every successful extract pairs with `sf_free`.
  - Use byte-equal assertion on real ER files (only content checks).
  - Run with synthetic data — this is the REAL keystone, not a synthetic.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: ~200 LOC test scaffolding; integrates entire Phase 2/3 stack; failure modes diverse.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO — must come after T10 + T14
  - **Parallel Group**: Wave 3
  - **Blocks**: T20 (retrospective references this completion)
  - **Blocked By**: T10, T14

  **References**:
  - Pattern: Phase 2 `test_dcx_krak.c` — Oodle path setup pattern, SKIP-on-missing-DLL idiom
  - API: T14 `er_extract_from_data0` — high-level helper
  - API: T10 `sf_bhd5_*` — low-level streaming
  - API: Phase 2 `sf_dcx_*` — outer DCX handling
  - WHY each: This test orchestrates the full Phase 2/3 stack; reference each layer's existing test for SKIP-graceful patterns.

  **Acceptance Criteria**:
  - [ ] `tests/e2e/test_bhd5_e2e_er.c` has 7 sub-tests
  - [ ] All 7 sub-tests PASS or test SKIPs gracefully (test never FAILs in a clean env)
  - [ ] On a fully-equipped dev machine (ER copy + Oodle), all 7 PASS
  - [ ] `cmake --build build-mingw --target souls_formats_test_bhd5_e2e_er` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bhd5_e2e_er$' --output-on-failure -V` PASS or SKIP
  - [ ] No memory leaks (valgrind-clean if available; otherwise visually verify free/sf_free pairs)

  **QA Scenarios**:

  ```
  Scenario: Full keystone chain on dev machine
    Tool: Bash (`ctest`)
    Preconditions: T15 merged + ER copy at /mnt/c/Games/ELDEN RING/Game/ + Oodle DLL at ~/dev/oodle/
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bhd5_e2e_er 2>&1 | tee .sisyphus/evidence/task-T15-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bhd5_e2e_er$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T15-ctest.log
      3. grep -E '(PASS|FAIL|IGNORE)' .sisyphus/evidence/task-T15-ctest.log
    Expected Result: 7 sub-tests show PASS (in dev env); summary "1 Tests, 0 Failures"
    Failure Indicators: any FAIL; or SKIP without descriptive message; or test hangs > 60s
    Evidence: .sisyphus/evidence/task-T15-build.log + .sisyphus/evidence/task-T15-ctest.log

  Scenario: Graceful SKIP when ER copy missing
    Tool: Bash (manual env manipulation)
    Preconditions: T15 merged
    Steps:
      1. Temporarily move /mnt/c/Games/ELDEN\ RING somewhere; rerun the test
      2. Verify test reports SKIP with TEST_IGNORE_MESSAGE("ER copy not found") not FAIL
      3. Restore the dir
    Expected Result: SKIP with message; CI ctest reports PASS overall
    Failure Indicators: FAIL on missing dir
    Evidence: .sisyphus/evidence/task-T15-skip-test.log

  Scenario: BND4 magic byte sequence verified
    Tool: Bash (in-test)
    Preconditions: T15 merged + dev env
    Steps:
      1. Test asserts first 4 bytes of decompressed == 0x42, 0x4E, 0x44, 0x34
    Expected Result: PASS
    Failure Indicators: byte mismatch (would indicate Oodle decompress failure or DCX type mis-id)
    Evidence: .sisyphus/evidence/task-T15-ctest.log (same file)
  ```

  **Evidence to Capture**: build log, ctest log, skip-test log

  **Commit**: YES
  - Message: `tests(e2e): add test_bhd5_e2e_er (RSA → BHD5 → AES → DCX_KRAK → BND4)`
  - Files: `tests/e2e/test_bhd5_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T16. **Add `test_bnd4_e2e_er.c` (downstream e2e via `er_extract_from_data0`)**

  **What to do**:
  - Create `tests/e2e/test_bnd4_e2e_er.c`. Sub-tests:
    1. SKIP if `er_helper_is_available()` returns false.
    2. Call `er_extract_from_data0("/chr/c0000.chrbnd.dcx", &bytes, &size)` — assert SF_OK; bytes start with "BND4".
    3. Parse via `sf_bnd4_read_from_memory(&bnd, bytes, size, NULL)` — assert SF_OK.
    4. Assert `sf_bnd4_file_count(bnd) >= 5`.
    5. Linear scan entries; find one named `c0000.flver`; assert size > 100 KB.
    6. Verify the file's `compression_info` is `SF_DCX_TYPE_NONE` (chrbnd entries are NOT individually DCX-wrapped after outer unwrap).
    7. Verify `Unicode` flag matches expected ER value (true).
    8. `sf_bnd4_destroy(bnd); sf_free(NULL, bytes);`
  - Use the BND4Reader streaming variant in a separate sub-test:
    1. After step 2 above, also test: `sf_bnd4_reader_t *r;` open via in-memory bytes, iterate by index, assert same count + content as eager read.

  **Must NOT do**:
  - Hard-fail on missing env.
  - Forget to free bytes / destroy bnd / close reader.
  - Assert byte-equal — content equal only.
  - Hardcode `c0000.flver` size — use lower bound (> 100 KB).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: ~80 LOC test scaffolding; consumes existing helpers.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T17, T18
  - **Parallel Group**: Wave 3
  - **Blocks**: T20
  - **Blocked By**: T7, T14

  **References**:
  - Pattern: T15 (sister keystone test for SKIP-graceful + helper usage)
  - API: T7 sf_bnd4_* eager + reader
  - API: T14 er_extract_from_data0
  - WHY each: T15 sets the SKIP pattern; T7+T14 are the consumed surfaces.

  **Acceptance Criteria**:
  - [ ] All sub-tests PASS or test SKIPs gracefully
  - [ ] On dev env, asserts file count + flver presence + compression_info preserved
  - [ ] `cmake --build build-mingw --target souls_formats_test_bnd4_e2e_er` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bnd4_e2e_er$' --output-on-failure -V` PASS or SKIP
  - [ ] No memory leaks

  **QA Scenarios**:

  ```
  Scenario: BND4 e2e on real ER chrbnd
    Tool: Bash (`ctest`)
    Preconditions: T16 merged + dev env
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bnd4_e2e_er 2>&1 | tee .sisyphus/evidence/task-T16-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bnd4_e2e_er$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T16-ctest.log
      3. grep -E '(PASS|FAIL|IGNORE|c0000.flver)' .sisyphus/evidence/task-T16-ctest.log
    Expected Result: PASS in dev env; SKIP otherwise; assertion lines confirm c0000.flver found and >100KB
    Failure Indicators: FAIL; missing c0000.flver
    Evidence: .sisyphus/evidence/task-T16-build.log + .sisyphus/evidence/task-T16-ctest.log

  Scenario: Eager + Reader read same content
    Tool: Bash (in-test)
    Preconditions: T16 merged + dev env
    Steps:
      1. Sub-test reads c0000.chrbnd via eager mode; gets entry list & sizes
      2. Re-reads via Reader mode; iterates by index
      3. Asserts each entry name, size, content-byte-equal across modes
    Expected Result: PASS
    Failure Indicators: any divergence between eager and reader
    Evidence: .sisyphus/evidence/task-T16-ctest.log (same file)
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `tests(e2e): add test_bnd4_e2e_er via er_extract_from_data0`
  - Files: `tests/e2e/test_bnd4_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T17. **Add `test_bxf4_e2e_er.c` (BXF4 e2e for ER `.tpfbhd`/`.tpfbdt` if present)**

  **What to do**:
  - Create `tests/e2e/test_bxf4_e2e_er.c`. Sub-tests:
    1. SKIP if `er_helper_is_available()` returns false.
    2. Try a list of candidate `.tpfbhd` paths from ER's BHD5 path inventory: `/parts/wp_a_0010.tpfbhd`, `/asset/aeg/aeg007/aeg007_xxx.tpfbhd`, etc. (Implementing agent finds 2-3 candidates by enumerating ER BHD5 buckets in a separate exploratory script.)
    3. For each candidate path: try `er_extract_from_data0(path, ...)` — if returns SF_OK with bytes starting with "BHF4" → proceed; else continue.
    4. If ALL candidates fail or are not present → SKIP with message "no .tpfbhd/.tpfbdt pair found in this ER version".
    5. If found: extract the matching `.tpfbdt` from the same bucket (same path with `.tpfbdt` extension); parse as BXF4 via `sf_bxf4_read_from_memory(&bxf, bhd_bytes, bhd_size, bdt_bytes, bdt_size, NULL)`.
    6. Assert `sf_bxf4_file_count(bxf) > 0`; assert at least one entry has size > 0.

  **Must NOT do**:
  - FAIL if specific .tpfbhd path missing — SKIP instead (some ER versions inline tpfs into BND4 directly).
  - Hardcode a single .tpfbhd path; the BHD5 entry inventory may shift between ER patches.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: ~100 LOC test; some discovery logic for candidate paths.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: T20
  - **Blocked By**: T9, T14

  **References**:
  - Pattern: T15 / T16 — SKIP-graceful + er_extract_from_data0 usage
  - API: T9 sf_bxf4_* (note: `read_from_memory` two-byte-array variant per T9 design)
  - API: T14 er_extract_from_data0
  - WHY each: BXF4 e2e differs from BND4 e2e mainly in needing TWO byte streams from BHD5.

  **Acceptance Criteria**:
  - [ ] All sub-tests PASS or test SKIPs gracefully
  - [ ] At least 2-3 candidate paths attempted before SKIP
  - [ ] On dev env where .tpfbhd exists, `sf_bxf4_read_from_memory` succeeds with file count > 0
  - [ ] `cmake --build build-mingw --target souls_formats_test_bxf4_e2e_er` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_bxf4_e2e_er$' --output-on-failure -V` PASS or SKIP

  **QA Scenarios**:

  ```
  Scenario: BXF4 e2e attempts multiple candidate paths
    Tool: Bash (`ctest`)
    Preconditions: T17 merged + dev env
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bxf4_e2e_er 2>&1 | tee .sisyphus/evidence/task-T17-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_bxf4_e2e_er$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T17-ctest.log
    Expected Result: PASS in dev env (attempts multiple candidates, finds at least one); or SKIP with descriptive message naming the candidates tried
    Failure Indicators: FAIL; or SKIP without trying ≥ 2 candidates
    Evidence: .sisyphus/evidence/task-T17-build.log + .sisyphus/evidence/task-T17-ctest.log
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `tests(e2e): add test_bxf4_e2e_er via er_extract_from_data0`
  - Files: `tests/e2e/test_bxf4_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T18. **Add `test_tpf_e2e_er.c` (TPF e2e via BND4 inside chrbnd)**

  **What to do**:
  - Create `tests/e2e/test_tpf_e2e_er.c`. Sub-tests:
    1. SKIP if `er_helper_is_available()` returns false.
    2. Extract `chrbnd` (re-using T16's path `/chr/c0000.chrbnd.dcx`).
    3. Open as BND4; iterate entries; find one named with `.tpf` suffix (e.g. `c0000_0010.tpf`). If none found, try other characters. If still none → SKIP.
    4. Get the .tpf entry bytes; parse via `sf_tpf_read_from_memory(&tpf, tpf_bytes, tpf_size, NULL)`.
    5. Assert `sf_tpf_texture_count(tpf) > 0`.
    6. Get first texture; assert `bytes` non-NULL, `bytes_size > 0`.
    7. Assert first 4 bytes of texture bytes == 'D','D','S',' ' (0x44 0x44 0x53 0x20) — DDS magic.
    8. (Optional) Verify `sfi_tpf_headerize` for PC platform returns the same bytes verbatim (no transformation needed).
    9. Cleanup: `sf_tpf_destroy(tpf); sf_bnd4_destroy(bnd); sf_free(...)`.

  **Must NOT do**:
  - Decode DDS pixel data.
  - Hardcode a specific .tpf entry name — scan dynamically.
  - Fail if .tpf is per-texture DCX-wrapped — auto-decompress per T11 logic.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: ~100 LOC test; reuses BND4 + TPF surfaces.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 3
  - **Blocks**: T20
  - **Blocked By**: T7, T11, T14

  **References**:
  - Pattern: T16 BND4 e2e
  - API: T11 sf_tpf_*
  - API: T7 sf_bnd4_*
  - API: T14 er_extract_from_data0
  - WHY each: This is the deepest layered e2e — 3 formats nested.

  **Acceptance Criteria**:
  - [ ] All sub-tests PASS or test SKIPs gracefully
  - [ ] On dev env, finds at least one .tpf inside c0000.chrbnd
  - [ ] DDS magic verified
  - [ ] `cmake --build build-mingw --target souls_formats_test_tpf_e2e_er` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_tpf_e2e_er$' --output-on-failure -V` PASS or SKIP
  - [ ] No memory leaks

  **QA Scenarios**:

  ```
  Scenario: TPF e2e finds DDS-prefixed texture
    Tool: Bash (`ctest`)
    Preconditions: T18 merged + dev env
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_tpf_e2e_er 2>&1 | tee .sisyphus/evidence/task-T18-build.log
      2. ctest --test-dir build-mingw -R '^souls_formats_test_tpf_e2e_er$' --output-on-failure -V 2>&1 | tee .sisyphus/evidence/task-T18-ctest.log
      3. grep -E '(PASS|FAIL|IGNORE|DDS)' .sisyphus/evidence/task-T18-ctest.log
    Expected Result: PASS in dev env; magic DDS verified
    Failure Indicators: FAIL; missing DDS magic; texture count == 0
    Evidence: .sisyphus/evidence/task-T18-build.log + .sisyphus/evidence/task-T18-ctest.log
  ```

  **Evidence to Capture**: build log, ctest log

  **Commit**: YES
  - Message: `tests(e2e): add test_tpf_e2e_er via er_extract_from_data0`
  - Files: `tests/e2e/test_tpf_e2e_er.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T19. **Implement `sf_bnd_extract.c` (CLI BND extraction)**

  **What to do**:
  - Replace T5 skeleton with full implementation:
    ```c
    /* Usage: sf_bnd_extract <input.bnd[.dcx]> <output_dir>
     * Reads the input as BND4, creates output_dir if missing,
     * writes each entry as <output_dir>/<entry_name> (substituting
     * '/' with '_' if needed for filesystem safety, mirroring upstream BinderTool).
     */
    int main(int argc, char **argv) {
        if (argc != 3) { fprintf(stderr, "Usage: ...\n"); return 1; }
        sf_bnd4_t *bnd = NULL;
        sf_result_t r = sf_bnd4_read_from_path(&bnd, argv[1]_to_wide, NULL);
        if (r != SF_OK) { fprintf(stderr, "Read failed: %s\n", sf_result_str(r)); return 2; }
        size_t n = sf_bnd4_file_count(bnd);
        for (size_t i = 0; i < n; i++) {
            const sf_binder_file_t *f = sf_bnd4_get_file(bnd, i);
            char out_path[1024];
            // sanitize name (replace '/' with '_')
            snprintf(out_path, sizeof out_path, "%s/%s", argv[2], sanitized_name(f->name_utf8));
            FILE *out = fopen(out_path, "wb");  // Wait — guardrail says NO fopen!
            // ... actually use sf_ostream_t!
            sf_ostream_t *out_stream = NULL;
            sf_ostream_open_file(&out_stream, out_path_w, NULL);
            sf_ostream_write(out_stream, f->data, f->size);
            sf_ostream_close(out_stream);
        }
        sf_bnd4_destroy(bnd);
        return 0;
    }
    ```
  - **CRITICAL CORRECTION**: per AGENTS.md "do not read or write paths via stdio (`fopen`)" — use `sf_ostream_t` exclusively.
  - Convert UTF-8 argv to UTF-16 for path APIs via Phase 1's encoding helpers.
  - Handle DCX outer wrap automatically (`sf_bnd4_read_from_path` already does this when input is `.dcx`).
  - Print to stderr count of files extracted on success.
  - Return non-zero on any error with descriptive message.

  **Must NOT do**:
  - Use `fopen/fwrite/fread` — must use `sf_ostream_t` per AGENTS.md.
  - Pollute stdout with success messages — keep stdout clean (machine consumers).
  - Forget to handle paths with `/` in entry names (some BNDs have nested-looking names).
  - Forget UTF-16 path conversion for Win32.

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: ~100 LOC CLI; mostly orchestration of existing APIs.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T14-T18
  - **Parallel Group**: Wave 3
  - **Blocks**: T20
  - **Blocked By**: T7 (BND4 eager API)

  **References**:
  - Pattern: T5 skeleton (the structure to fill in)
  - API: T7 `sf_bnd4_*`
  - API: Phase 1 `sf_ostream_*`
  - API: Phase 1 encoding helpers for UTF-8 → wchar_t conversion
  - WHY each: T19 just glues prior layers; the only novelty is path sanitization for filesystem.

  **Acceptance Criteria**:
  - [ ] T5 skeleton replaced; `examples/sf_bnd_extract.c` is fully functional
  - [ ] CLI emits non-zero on bad args / read failure / write failure
  - [ ] No use of stdio file APIs (fopen/fread/fwrite) — only `sf_ostream_*`
  - [ ] **Synthetic deterministic test** (`tests/examples/test_sf_bnd_extract_smoke.c`) generates a 3-entry synthetic BND4 + DCX_DFLT wrap, runs CLI on it, verifies all 3 expected files extracted with exact byte content
  - [ ] **Optional ER smoke test** SKIPs gracefully when env missing
  - [ ] `cmake --build build-mingw` 0 warnings
  - [ ] `ctest --test-dir build-mingw -R '^souls_formats_test_sf_bnd_extract_smoke$' --output-on-failure -V` PASS
  - [ ] CLI runs via WSL interop directly (`./examples/sf_bnd_extract.exe <args>`)

  **QA Scenarios**:

  > **CORRECTED test fixture strategy** (per Momus review): vanilla ER ships ZERO loose `.bnd.dcx` files (everything is inside `Data0.bhd`). The CLI test must NOT depend on a non-existent path. We use TWO deterministic approaches in sequence:
  > - **Primary**: a small fixture-generation test creates a synthetic BND4 with 3 named entries via `sf_bnd4_create + sf_bnd4_add_file`, writes via `sf_bnd4_write_to_path` with DCX_DFLT outer wrap to a temp file. The CLI is then invoked on this temp file. Fully self-contained, deterministic, no external dependencies, no Oodle needed (DFLT not KRAK).
  > - **Optional secondary**: if `er_helper_is_available()` returns true, the test also extracts `/chr/c0000.chrbnd.dcx` via `er_extract_from_data0` to a temp file, then runs the CLI against it as a real-data sanity check.

  ```
  Scenario: CLI extracts a deterministic synthetic BND4 (primary, always runs)
    Tool: Bash (`./sf_bnd_extract.exe` + verification)
    Preconditions: T19 merged + T7 BND4 already passed CI (so the synthetic generation path is verified)
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T19-build.log
      2. The test program (a small helper test like `tests/examples/test_sf_bnd_extract_smoke.c`) builds a synthetic BND4 in-memory:
         - sf_bnd4_create(&b, NULL); sf_bnd4_set_format(b, SF_BINDER_FORMAT_IDS|SF_BINDER_FORMAT_NAMES2|SF_BINDER_FORMAT_COMPRESSION);
         - sf_bnd4_add_file(b, 100, "alpha.txt", "ALPHA-CONTENT", 13, 0);
         - sf_bnd4_add_file(b, 200, "beta.bin", "\xDE\xAD\xBE\xEF", 4, 0);
         - sf_bnd4_add_file(b, 300, "gamma.dat", "G", 1, 0);
         - sf_bnd4_set_compression(b, SF_DCX_TYPE_DCX_DFLT);
         - sf_bnd4_write_to_path(b, L"/tmp/synth.bnd.dcx");
      3. The test program then invokes (via WSL POSIX `system()`):
         system("build-mingw/examples/sf_bnd_extract.exe /tmp/synth.bnd.dcx /tmp/synth-extracted/")
      4. Asserts exit code == 0
      5. Asserts /tmp/synth-extracted/alpha.txt exists with size 13 bytes and content "ALPHA-CONTENT"
      6. Asserts /tmp/synth-extracted/beta.bin exists with size 4 bytes and content 0xDE 0xAD 0xBE 0xEF
      7. Asserts /tmp/synth-extracted/gamma.dat exists with size 1 byte and content "G"
      8. Output: ctest log to .sisyphus/evidence/task-T19-synth.log
    Expected Result: All assertions PASS
    Failure Indicators: exit code != 0; any missing file; any content mismatch
    Evidence: .sisyphus/evidence/task-T19-build.log + .sisyphus/evidence/task-T19-synth.log

  Scenario: CLI extracts real ER chrbnd (optional secondary; SKIP if env missing)
    Tool: Bash
    Preconditions: T19 merged + dev env (ER copy + Oodle DLL); er_helper_is_available() returns true
    Steps:
      1. The test program calls er_extract_from_data0("/chr/c0000.chrbnd.dcx", &bytes, &size); if not SF_OK → SKIP this scenario with TEST_IGNORE_MESSAGE
      2. Write `bytes` to /tmp/real-c0000.chrbnd.dcx via sf_ostream_open_file + sf_ostream_write
         (Note: `bytes` is the DCX-unwrapped BND4. To CLI-test the outer DCX-unwrap path, instead extract the RAW DCX-wrapped bytes via sf_bhd5_extract_by_hash_64 directly and write those.)
      3. system("build-mingw/examples/sf_bnd_extract.exe /tmp/real-c0000.chrbnd.dcx /tmp/c0000-extracted/")
      4. Asserts exit code == 0
      5. Asserts /tmp/c0000-extracted/ contains a file matching glob "*.flver" with size > 100 KB
    Expected Result: PASS in dev env; SKIP otherwise
    Failure Indicators: FAIL with env present
    Evidence: .sisyphus/evidence/task-T19-real.log

  Scenario: CLI fails gracefully on bad input
    Tool: Bash
    Preconditions: T19 merged
    Steps:
      1. build-mingw/examples/sf_bnd_extract.exe; echo "exit=$?" 2>&1 | tee .sisyphus/evidence/task-T19-bad-args.log
      2. build-mingw/examples/sf_bnd_extract.exe /nonexistent.bnd /tmp/out 2>&1; echo "exit=$?" 2>&1 | tee -a .sisyphus/evidence/task-T19-bad-args.log
    Expected Result: exit code != 0; stderr has descriptive message
    Failure Indicators: exit code 0 on failure; segfault
    Evidence: .sisyphus/evidence/task-T19-bad-args.log

  Scenario: No stdio file APIs used (architectural compliance)
    Tool: Bash (`grep`)
    Preconditions: T19 merged
    Steps:
      1. grep -nE '\b(fopen|fread|fwrite|fclose)\b' examples/sf_bnd_extract.c | tee .sisyphus/evidence/task-T19-stdio.log
    Expected Result: Empty (no matches — must use sf_ostream_t exclusively per AGENTS.md §7)
    Failure Indicators: any line referencing stdio file API
    Evidence: .sisyphus/evidence/task-T19-stdio.log
  ```

  **Evidence to Capture**: build log, run log, bad-args log, stdio log

  **Commit**: YES
  - Message: `examples: implement sf_bnd_extract.c (CLI BND extraction)`
  - Files: `examples/sf_bnd_extract.c` (replaces T5 stub), `tests/examples/test_sf_bnd_extract_smoke.c` (NEW synthetic deterministic test), `tests/CMakeLists.txt` (register the new test)
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] T20. **PLAN.md / AGENTS.md / roadmap README retrospective + Phase 3 status update**

  **What to do**:
  - **PLAN.md** Phase 3 section update:
    - Tick all checkboxes under `### Phase 3 — 档案容器（预估 2 周）`
    - Add `**Completion Retrospective**: Completed (2026-XX-XX)` line at top of section, mirroring Phase 1/2 style.
    - Add concrete numbers: total tests, sub-test counts, ER e2e PASS/SKIP breakdown.
    - Update §13 "下一步" pointer from Phase 3 → Phase 4.
  - **AGENTS.md**:
    - Update §2 status table row 3: `Phase 3 ✅ done | N/N PASS across N binaries (verified YYYY-MM-DD)`.
    - Update top-of-file LOC counts if the actual delta differs significantly from estimate.
  - **`docs/roadmap/README.md`**:
    - Update Phase 3 row in the index table to `✅ done` + completion timestamp + test count.
  - **`docs/roadmap/phase-3-archive-containers.md`** (the original phase doc):
    - Tick all checkboxes in "Exit criteria" section
    - Add `**Status**: ✅ Complete (2026-XX-XX)` at top
  - **`README.md`**:
    - Update top status banner: `Pre-alpha (v0.3.0) — Phase 0/1/2/3 complete`.
  - **`CHANGELOG.md`**:
    - Add v0.3.0 entry with: archive container layer, 7 formats (BND3/BND4/BXF3/BXF4/BHD5/TPF/ENFL), Phase 1/2 retro-fits (sf_reverse_bits_u8, sf_path_hash_64, sfi_aes_decrypt_ecb_buffer), RSA decrypt for BHD wrap, minimal DDS header parser, ER e2e helper.
  - **Bump version**: `CMakeLists.txt` project version 0.2.0 → 0.3.0 (minor, since v0.x permits ABI breaks).

  **Must NOT do**:
  - Touch any code file.
  - Add new sections to PLAN.md beyond the retrospective and tick marks.
  - Forget the version bump (per AGENTS.md "v0.x ABI breaks ARE permitted" rule — minor bump required).

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Pure documentation; coordinated updates across 6 files.
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: NO — must come last
  - **Parallel Group**: Wave 3 final
  - **Blocks**: F1-F4 verification
  - **Blocked By**: T13, T15, T16, T17, T18, T19 (everything must be merged first)

  **References**:
  - Pattern: PLAN.md Phase 1 retrospective (lines 449-487) — exact style for completion retrospective
  - Pattern: PLAN.md Phase 2 retrospective (lines 489-530) — same
  - Pattern: AGENTS.md §2 table row 1/2/3 — Phase 0/1/2 completion display style
  - Pattern: README.md current banner — version + phase callout style
  - WHY each: Maintain consistency with prior phase retrospectives.

  **Acceptance Criteria**:
  - [ ] PLAN.md Phase 3 section all checkboxes ticked + retrospective added
  - [ ] AGENTS.md §2 table row 3 = `✅ done`
  - [ ] `docs/roadmap/README.md` Phase 3 row = `✅ done`
  - [ ] `docs/roadmap/phase-3-archive-containers.md` Exit criteria all ticked
  - [ ] README.md banner updated
  - [ ] CHANGELOG.md has v0.3.0 entry
  - [ ] CMakeLists.txt project version 0.3.0
  - [ ] No code files modified
  - [ ] `cmake --build build-mingw` still 0 warnings (just verifying nothing broke)

  **QA Scenarios**:

  ```
  Scenario: All Phase 3 status indicators flipped
    Tool: Bash (`grep`)
    Preconditions: T20 merged
    Steps:
      1. grep -E 'Phase 3.*done' AGENTS.md docs/roadmap/README.md README.md | tee .sisyphus/evidence/task-T20-status.log
    Expected Result: ≥ 3 lines confirming Phase 3 status flipped to done in each file
    Failure Indicators: < 3 matches
    Evidence: .sisyphus/evidence/task-T20-status.log

  Scenario: Version bump correctly
    Tool: Bash (`grep`)
    Preconditions: T20 merged
    Steps:
      1. grep -E 'project.*VERSION 0.3.0' CMakeLists.txt | tee .sisyphus/evidence/task-T20-version.log
    Expected Result: 1 match
    Failure Indicators: 0 matches; or version still 0.2.0
    Evidence: .sisyphus/evidence/task-T20-version.log

  Scenario: Build still green
    Tool: Bash
    Preconditions: T20 merged
    Steps:
      1. cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/task-T20-build.log
      2. grep -cE '(error|warning):' .sisyphus/evidence/task-T20-build.log
    Expected Result: "0"
    Failure Indicators: any non-zero count
    Evidence: .sisyphus/evidence/task-T20-build.log
  ```

  **Evidence to Capture**: status log, version log, build log

  **Commit**: YES
  - Message: `docs: mark Phase 3 done in PLAN.md/AGENTS.md/roadmap README + retrospective`
  - Files: `.sisyphus/plans/PLAN.md`, `AGENTS.md`, `docs/roadmap/README.md`, `docs/roadmap/phase-3-archive-containers.md`, `README.md`, `CHANGELOG.md`, `CMakeLists.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing Phase 3.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking Phase 3 done.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user feedback → fix → re-run → present again → wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`

  Read this plan end-to-end. For each "Must Have": verify implementation exists (read file, run command, check evidence file). For each "Must NOT Have": search codebase for forbidden patterns — REJECT with `file:line` if found. Specifically grep for: `int64_t id` in archive headers, `compute_sha`-like names in BHD5 write path, `fopen/fread/fwrite` anywhere, `SF_ERR_UNSUPPORTED_VERSION` lacking on Xbox/PS3-write/PS5 paths, missing `Unk04/Unk05/Unk18`. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan. Verify all 8 api-mapping docs have ZERO `未实现` rows for Phase 3 formats; verify `extensions.md` has the 5 listed deviations.

  Output: `Must Have [N/N] | Must NOT Have [N/N clean] | Tasks [N/N] | Mapping ✓ aligned [107/107] | Extensions [5/5] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`

  Run `cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/final-build.log` (must be 0 errors, 0 warnings due to `-Werror`); run `ctest --test-dir build-mingw --output-on-failure 2>&1 | tee .sisyphus/evidence/final-ctest.log`. Review all changed files (`git diff --stat HEAD~ -- include/ src/ tests/ examples/` + per-file diff): verify no `as any`-equivalent (`(void *)` casts circumventing types), no `// TODO` lacking owner, no `#pragma warning disable`, no commented-out code blocks, no unused variables, no dead branches, no generic names like `data`/`result`/`temp` without context, no `_v2`/`_internal`/`_helper2` (AI slop). Check Reserve/Fill discipline: every `sf_binary_writer_reserve_*` matched with exactly one `sf_binary_writer_fill_*` of same kind. Check every `_create` API takes `const sf_allocator_t *alloc`. Check every public symbol has `SF_API` decoration.

  Output: `Build [PASS/FAIL] | Tests [N pass/N fail/N skip] | Files reviewed [N] | AI slop instances [N] | Reserve/Fill discipline [CLEAN/N issues] | Allocator discipline [CLEAN/N issues] | SF_API discipline [CLEAN/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`

  Start from clean state (`git stash` any uncommitted; `cmake --build build-mingw --clean-first`). Execute EVERY QA scenario from EVERY task — follow exact steps, capture evidence to `.sisyphus/evidence/final-qa/`. Specifically: (a) full ER chain via `er_extract_from_data0("/chr/c0000.chrbnd.dcx")` returns BND4-magic bytes ≥ 100 KB; (b) `sf_bnd_extract.exe` CLI on the same chrbnd outputs ≥ 5 files including `c0000.flver`; (c) all 7 synthetic round-trips byte-equal; (d) all 4 e2e tests PASS or SKIP-gracefully; (e) Phase 0/1/2 regression (5 + 5 + 13 + new T0b sub-test) all PASS. Test cross-task integration: sf_bnd_extract uses BND4 reader uses binder_common, all functioning together.

  Output: `Synthetic [7/7 byte-equal] | E2E [N/4 pass + N skip] | CLI [PASS/FAIL] | Regression [22+/22+ PASS] | Integration [VERIFIED/N issues] | Evidence files [N saved] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`

  For each task T0a-T20: read "What to do" + "Must NOT do", read actual `git log --oneline T0a..HEAD -- <task files>` + `git diff` for each task's declared files. Verify 1:1 correspondence — everything in spec was built (no missing fields/methods/edge cases), nothing beyond spec was built (no scope creep, no premature abstraction, no extra "while-I'm-here" changes). Check "Must NOT do" compliance per task. Detect cross-task contamination: T6 (BND3) touching BND4 files, T10 (BHD5) touching TPF, etc. Flag unaccounted changes outside any task's declared scope. Verify T0d RSA keys are PEM strings (not embedded binary blobs). Verify Headerizer in T11 is capped to PC platform (no PS3/PS4/Xbox/PS5 implementation).

  Output: `Tasks [N/N compliant] | Must-NOT-do violations [N] | Contamination [CLEAN/N issues] | Unaccounted changes [CLEAN/N files] | RSA key format [PEM-only] | Headerizer cap [PC-only verified] | VERDICT`

---

## Commit Strategy

One commit per task (T0a-T20), each must keep the build green and all relevant tests passing. Commit message format:

| Task | Commit message format |
|---|---|
| T0a | `core(io): add sf_reverse_bits_u8 (Phase 1 retro-fit for binder)` |
| T0b | `crypto(aes): expose sfi_aes_decrypt_ecb_buffer for BHD5 range decrypt` |
| T0c | `core(hash): add sf_path_hash_64 for ER+ BHD5 (64-bit fold)` |
| T0d | `crypto(rsa): add sfi_rsa_decrypt + 4 game PEM keys (Sekiro/ER/Nightreign/AC6)` |
| T0e | `core(dds): add minimal sfi_dds_parse_header for TPF metadata derivation` |
| T1 | `archive: add sf_binder.h shared types (Format/FileFlags enums + sf_binder_file_t)` |
| T2 | `archive: add binder_common.c shared helpers (timestamps, hash table, file header r/w)` |
| T3 | `archive(bhd5): embed AES-128-ECB keys for Sekiro/ER/Nightreign/AC6` |
| T4 | `tests(e2e): add er_test_helper.h skeleton (declarations only)` |
| T5 | `examples: add sf_bnd_extract.c skeleton` |
| T6 | `archive(bnd3): port BND3 read/write + reader + synthetic round-trip test` |
| T7 | `archive(bnd4): port BND4 read/write + reader + synthetic round-trip test` |
| T8 | `archive(bxf3): port BXF3 read/write + reader + synthetic round-trip test` |
| T9 | `archive(bxf4): port BXF4 read/write + reader + synthetic round-trip test` |
| T10 | `archive(bhd5): port BHD5 streaming reader + synthetic round-trip test` |
| T11 | `archive(tpf): port TPF + PC-only Headerizer + synthetic round-trip test` |
| T12 | `archive(enfl): port ENFL + synthetic round-trip test` |
| T13 | `docs(api-mapping): flip Phase 3 rows to ✓ aligned + record extensions` |
| T14 | `tests(e2e): implement er_test_helper.c singleton (Data0.bhd/bdt opener)` |
| T15 | `tests(e2e): add test_bhd5_e2e_er (RSA → BHD5 → AES → DCX_KRAK → BND4)` |
| T16 | `tests(e2e): add test_bnd4_e2e_er via er_extract_from_data0` |
| T17 | `tests(e2e): add test_bxf4_e2e_er via er_extract_from_data0` |
| T18 | `tests(e2e): add test_tpf_e2e_er via er_extract_from_data0` |
| T19 | `examples: implement sf_bnd_extract.c (CLI BND extraction)` |
| T20 | `docs: mark Phase 3 done in PLAN.md/AGENTS.md/roadmap README + retrospective` |

**Pre-commit hook** (mandatory before each commit):
```bash
cmake --build build-mingw 2>&1 | tee /tmp/build.log
# Must show 0 errors, 0 warnings (project is -Werror)
ctest --test-dir build-mingw --output-on-failure
# Must show 0 failures (skip is OK)
```

---

## Success Criteria

### Verification Commands

```bash
# Build green
cmake --build build-mingw 2>&1 | grep -E '(error|warning):' | wc -l
# Expected: 0

# All tests green or skipped
ctest --test-dir build-mingw --output-on-failure 2>&1 | tail -20
# Expected: "100% tests passed, 0 tests failed out of N"

# Phase 3 archive label specifically
ctest --test-dir build-mingw -L archive --output-on-failure 2>&1 | grep -E 'Total|Passed|Failed'
# Expected: 11+ tests (7 synthetic + 4 e2e), 0 failed

# DLL exports include all new sf_* archive symbols
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -c 'sf_'
# Expected: ≥ 200 (Phase 1 baseline 137 + Phase 2 + Phase 3 archive APIs)

# Keystone e2e
ctest --test-dir build-mingw -R '^souls_formats_test_bhd5_e2e_er$' -V 2>&1 | tail -10
# Expected: PASS or SKIP-gracefully

# Mapping docs ✓ aligned (Phase 3 formats)
grep -l '未实现' docs/api-mapping/format-{binder-common,bnd3,bnd4,bxf3,bxf4,bhd5,tpf,enfl}.md | wc -l
# Expected: 0 files contain 未实现 rows

# CLI example works on the deterministic synthetic fixture (always runs; no ER required)
ctest --test-dir build-mingw -R '^souls_formats_test_sf_bnd_extract_smoke$' --output-on-failure -V 2>&1 | tail -10
# Expected: PASS — the embedded synthetic-BND4 + DCX_DFLT fixture is extracted by the CLI to a temp dir;
# the test asserts 3 expected files (alpha.txt/beta.bin/gamma.dat) appear with exact byte content.

# Optional ER smoke (only when dev env present): extract a real chrbnd via er_extract_from_data0
# to a temp file, then invoke the CLI on it. SKIPS gracefully when ER copy/Oodle missing.
ctest --test-dir build-mingw -R '^souls_formats_test_sf_bnd_extract_smoke$' --output-on-failure -V 2>&1 | grep -E 'real-er|IGNORE'
# Expected: either "real-er" sub-scenario PASS line, OR an IGNORE/SKIP line citing missing env
```

### Final Checklist
- [ ] All 25 implementation tasks (T0a-T20) checked off in this plan
- [ ] All 4 verification reviews (F1-F4) APPROVE
- [ ] User explicit "okay" received
- [ ] All 7 archive synthetic tests PASS byte-equal
- [ ] All 4 ER e2e tests PASS or SKIP-gracefully
- [ ] Phase 0/1/2 regression all green
- [ ] All 8 api-mapping docs at `✓ aligned` (0 `未实现` rows for Phase 3 formats)
- [ ] `extensions.md` has 5 deviations recorded
- [ ] PLAN.md Phase 3 boxes ticked with timestamp + concrete pass count
- [ ] AGENTS.md status table shows Phase 3 ✅ done
- [ ] `docs/roadmap/README.md` Phase 3 row updated to "done"
