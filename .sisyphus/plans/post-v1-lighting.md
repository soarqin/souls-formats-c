# souls-formats-c — Post-v1 Lighting Batch (v0.5.0)

## TL;DR

> **Quick Summary**: Implement the only remaining post-v1 batch — 5 lighting formats
> (BTAB, BTL, BTPB, GPARAM, PMDCL) — completing the upstream API mapping coverage
> for souls-formats-c. All 9 other post-v1 batches landed; this batch is the gap.
>
> **Deliverables**:
> - 5 public headers (`sf_btab.h`, `sf_btl.h`, `sf_btpb.h`, `sf_gparam.h`, `sf_pmdcl.h`)
> - 5 source modules under new `src/lighting/` dir
> - 1 probe binary (Wave-0 scope-lock evidence)
> - ≥5 synthetic round-trip tests + ≥5 ER-multi-shard e2e tests
> - 5 tier-A `docs/api-mapping/format-<name>.md` files + `legacy.md` rows removed
> - `docs/api-mapping/README.md` tier-A index updated
> - `docs/api-mapping/extensions.md` new `## Post-v1: Lighting` section
> - `CHANGELOG.md` new `## [0.5.0]` block; `CMakeLists.txt` version bumped 0.4.1 → 0.5.0
> - `AGENTS.md` status table row added; `README.md` version mention bumped
> - `post-v1.md` and `PLAN.md` reflect lighting milestone closure
>
> **Estimated Effort**: Medium (1.5-2 wall-weeks; ~6-8 working days with parallelism)
> **Parallel Execution**: YES — 7 waves total (Wave 0 probe + Waves 1-5 work + Wave Final review)
> **Critical Path**: W0 probe → W1 foundation → W4 GPARAM (longest) → W5 e2e+docs → W Final reviewers → user okay

---

## Context

### Original Request

> 已经完成了所有的分步post-v1 plan，请再次比对一下api实现表格，检查剩余的gap，
> 确认是否已经完成全部实现，没有的话编写一个新的plan补齐

**Translation**: All staged post-v1 plans are done. Verify gaps against the API mapping
table and confirm whether full implementation is complete; if not, write a new plan to
close the remaining gap.

### Interview Summary

**Gap analysis result**: 9 of 10 post-v1 batches completed; **LIGHTING batch never started**.

| Already-implemented batches | Verification |
|---|---|
| TAE Templates (2026-05-13, 84/84 PASS) | `next-batch-tae-templates.md` Completion section |
| Effects Misc (ANI/FFXDLSE/FXR1/MQB/NMB/NSA) | `src/effects/*.c`, `src/misc/{ani,mqb}.c` |
| AC Specific main (AcParts4/FSDATA/FSLIBLZS/MLB_AC4/AC5/PARAMDBP) | `src/ac/*.c` |
| Legacy Binder (BND/BND2) | `src/archive/{bnd,bnd2}.c` |
| Legacy FLVER (FLVER0/MDL/MDL0/MDL4) | `src/geom/{flver0,mdl,mdl0,mdl4}.c` |
| Legacy MSB (11 variants) | `src/map/{msb1..msbvd}/*.c` |
| Navmesh (EDGE/MCG/MCP/NGP/NVA/NVM) | `src/navmesh/*.c` |
| Text/Script Misc (EMELD/FMB/LUAGNL/LUAINFO) | `src/script/{emeld,luagnl,luainfo}.c`, `src/misc/fmb.c` |
| Uncategorized Deferred main (DRB/ACB/AIP/CCM/CLM2/EDD/F2TR/GRASS/RMB/SMD4 + KF4/Kuon/MWC) | `src/misc/*.c` |

| Remaining gap | State |
|---|---|
| LIGHTING — BTAB, BTL, BTPB, GPARAM, PMDCL | **0 source files, 0 headers, 0 CMake registration** |

Verification: `find src include -iname "*btab*" -o -iname "*btl*" -o -iname "*btpb*" -o -iname "*gparam*" -o -iname "*pmdcl*"` → 0 matches. CHANGELOG.md 0.4.1 contains no lighting mentions.

**Key user decisions (Question tool, 2026-05-13)**:
- Scope = ONLY 5 lighting formats. AC4 sub-formats + PARAMDBP serialization excluded.
- Tests = synthetic fixture round-trip + ER e2e (Phase 6/7 cadence).
- GPARAM = full v1 target game coverage (Sekiro V5 + ER V5 + Nightreign V5 + AC6 V6).
- Version = v0.5.0 (SemVer minor bump — new SF_API symbols).
- Quality gate = Momus high-accuracy review loop AFTER plan generation.

### Research Findings (2026-05-13)

**BTAB** (164 LOC upstream) — simple. No Game enum; `BigEndian + LongFormat` bools.
21 LOC read, 35 LOC write. 5 public fields per Entry.

**BTL** (533 LOC) — medium. Versions {1,2,5,6,16,18}; `LightType` enum; **47+ public
fields in nested `Light`**; light-size 0xC0/0xC8/0xE8; Sekiro+ tail block on
`version >= 16`. File comment says BB/DS3/Sekiro; ER/NR/AC6 likely use V18 (probe needed).

**BTPB** (414 LOC) — medium with critical risk. Upstream `BTPBVersion` enum **terminates
at DarkSouls3**. Sekiro/ER/NR/AC6 applicability is UNCERTAIN — Wave-0 probe MUST verify
file existence in v1 games or this format **drops from batch** and stays in legacy.md.

**GPARAM** (1279 LOC) — most complex. enum `GparamVersion {V2, V3, V5, V6}` =
DS2/BB+later/Sekiro+later/AC6+later. **16 typed FieldType** (Sbyte..String) with
polymorphic `Field<T>` + `FieldValue<T>` (mapped to single tagged-union POD
`sf_gparam_value_t` in C). Signature UTF-16LE `"filt"` (not ASCII). V5+ adds
Unk50/Unk04/Unk0c; V6+ changes field-header layout. No ApplyTemplate (simpler than feared).

**PMDCL** (170 LOC) — simplest. No version branching; flat decal list with numeric
sentinels only (no magic bytes). 20 LOC read, 18 LOC write.

**ER file locations**: likely under `/map/mapstudio/<map_id>/<file>.<ext>.dcx` but
`probe_nightreign_msb.c` notes ER map assets may be in **Data2 not Data0**. Existing
`er_extract_from_data0()` helper handles BHD5 + DCX transparently across shards if
initialized to scan all 4 Data archives.

### Metis Review

**Identified gaps (now addressed in this plan)**:
- **CRITICAL**: BTPB upstream stops at DS3 — Wave-0 probe MUST confirm v1 applicability before commit. If zero v1 files found → drop BTPB, batch reduces to 4 formats.
- **CRITICAL**: BTL version 18 hypothesis (ER/NR/AC6) is unverified — Wave-0 probe MUST read version field of real `.btl` files.
- **CRITICAL**: Probe MUST be Wave 0 (was originally Wave 6). Implementation scope depends on probe results.
- **CRITICAL**: GPARAM polymorphism uses **single tagged-union POD `sf_gparam_value_t`** (mirror FXR3 precedent), NOT 16 per-type structs.
- **CRITICAL**: NO `lighting_common.c`. One file per format under `src/lighting/`.
- **CRITICAL**: One e2e test per format with **multi-game probing inside** (not 5×4=20 tests).
- **CRITICAL**: Per-format synthetic test ships in **same wave** as its implementation (PR-atomic principle), not deferred.
- **HIGH**: GPARAM string encoding (UTF-16 vs Shift-JIS) MUST be verified against upstream verbatim — both encodings exist in FromSoft formats.
- **HIGH**: V6 GPARAM layout switch MUST be in a separate code path from V5 (no shared silent fall-through).
- **HIGH**: `docs/api-mapping/README.md` tier-A list is stale (stops at FXR3). Lighting batch MUST update it. **Not in scope to backfill prior 9 batches.**
- **HIGH**: `legacy.md` currently mis-classifies GPARAM ("Legacy params, v2") and lighting ("Lighting, v2"). Removal during this batch is a CORRECTION, must be documented in CHANGELOG.
- **HIGH**: ASan-clean build (`SF_ENABLE_SANITIZERS=ON`) MUST be a Final-Wave acceptance gate.

---

## Work Objectives

### Core Objective

Close the last remaining post-v1 gap by implementing the 5 lighting formats
(BTAB / BTL / BTPB / GPARAM / PMDCL) with full read+write support for v1 target
games (Sekiro / ER / Nightreign / AC6), shipping under v0.5.0.

### Concrete Deliverables

- 5 public headers in `include/souls_formats/`: `sf_btab.h`, `sf_btl.h`, `sf_btpb.h` (if Wave-0 probe confirms), `sf_gparam.h`, `sf_pmdcl.h`
- 5 source modules in new `src/lighting/`: `btab.c`, `btl.c`, `btpb.c` (conditional), `gparam.c`, `pmdcl.c`, plus `src/lighting/gparam_internal.h` for polymorphism scaffolding
- 1 probe binary `tests/probes/probe_lighting_files.c` (gated by `SF_BUILD_PROBES`), producing `.sisyphus/evidence/lighting-probe.md`
- 5 synthetic round-trip tests `tests/lighting/test_<format>_synthetic.c`
- 5 e2e tests `tests/lighting/test_<format>_e2e.c` (multi-game probing inside each)
- 5 tier-A mapping docs `docs/api-mapping/format-<format>.md`
- Updated `docs/api-mapping/README.md` (lighting links in tier-A index)
- Updated `docs/api-mapping/legacy.md` (5 rows removed, with CHANGELOG note)
- Updated `docs/api-mapping/extensions.md` (`## Post-v1: Lighting` section for divergences)
- `CHANGELOG.md` new `## [0.5.0]` block
- `CMakeLists.txt` version bumped `0.4.1` → `0.5.0` + new SF_PUBLIC_HEADERS lighting list + new SF_SOURCES lighting list + new test label `lighting`
- `AGENTS.md` Phase status table row "lighting" added
- `README.md` version mention bumped
- `docs/roadmap/post-v1.md` reflects v0.5.0 closure

### Definition of Done

- [ ] Build: `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw 2>&1 | grep -cE "warning:|error:"` outputs `0`
- [ ] Build (ASan): `cmake -B build-asan -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_SANITIZERS=ON && cmake --build build-asan 2>&1 | grep -cE "warning:|error:"` outputs `0`
- [ ] Tests: `ctest --test-dir build-mingw -L lighting --output-on-failure` → 100% pass; binary count ≥ 8 (4-5 synthetic + 4-5 e2e + 1 probe). e2e tests gracefully skip when files unavailable but never silently no-op.
- [ ] ASan tests: `ctest --test-dir build-asan -L lighting --output-on-failure` → 100% pass (zero leaks/UB on synthetic + e2e paths).
- [ ] Symbol export: `x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -cE 'sf_(btab|btl|btpb|gparam|pmdcl)_'` outputs ≥ 30
- [ ] Static asserts: `grep -cE '_Static_assert' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h` ≥ 5 (≥1 per public enum across files)
- [ ] Stdio absence: `grep -rE 'fopen\(|fread\(|fwrite\(|fclose\(' src/lighting/` outputs empty
- [ ] Mapping docs: `for f in btab btl btpb gparam pmdcl; do test -f docs/api-mapping/format-$f.md || echo MISSING $f; done` outputs nothing (BTPB allowed missing if Wave-0 dropped it)
- [ ] README tier-A: `grep -cE '\[format-(btab|btl|btpb|gparam|pmdcl)\]' docs/api-mapping/README.md` ≥ 4
- [ ] Legacy.md removal: `grep -cE '^\| (BTAB|BTL|BTPB|GPARAM|PMDCL) ' docs/api-mapping/legacy.md` == 0 (or only BTPB remains if Wave-0 dropped it)
- [ ] CHANGELOG entry: `grep -c '^## \[0.5.0\]' CHANGELOG.md` == 1
- [ ] Version bumped: `grep -c 'VERSION 0.5.0' CMakeLists.txt` ≥ 1
- [ ] AGENTS.md status row: `grep -c 'lighting' AGENTS.md` ≥ 2 (status table mention + reference)
- [ ] No regressions: `ctest --test-dir build-mingw --output-on-failure` (all labels) → previously passing tests still pass
- [ ] Round-trip integrity: every implemented format's synthetic test demonstrates `write(read(write(x))) == write(x)` byte-equal
- [ ] Real-file content equality: every e2e test that finds a real file shows field-by-field equality after re-serialize
- [ ] Final Verification Wave: 4 named reviewer agents all return `VERDICT: APPROVE`
- [ ] User explicitly says "okay" to proceed; no F1-F4 task checked before that

### Must Have

- **STRICT UPSTREAM REFERENCE** at every public function: header-style comment naming the upstream `.cs` method (e.g., `// Upstream: GPARAM.cs:Read()`)
- **API MIRRORS UPSTREAM**: every public symbol traces to an upstream member via `docs/api-mapping/format-*.md` row. Functional divergences documented in `extensions.md`.
- **One file per format** under `src/lighting/`
- **One e2e test per format** with multi-game probing inside (not per-game test explosion)
- **Wave-0 probe-first** scope-lock — BTPB inclusion + BTL versions confirmed by real-file evidence before code is written
- **`sf_gparam_value_t` single tagged-union POD** (FXR3 precedent), not 16 per-type structs
- **`_Static_assert` after every public enum** (matches existing convention)
- **`SF_API` decoration on every public symbol** in lighting headers
- **`goto cleanup` single-label** convention per function (POLICY.md error cleanup)
- **All `Reserve_*` paired with exactly one `Fill_*`** before `sf_binary_writer_finish`
- **Allocator-correct** — every `sf_xalloc` paired with `sf_xfree` in cleanup or destroy path
- **README.md tier-A list updated** with 4-5 new rows (depending on BTPB)
- **`docs/api-mapping/legacy.md` rows removed** for newly shipped formats + CHANGELOG note documenting the re-classification
- **Probe binary** at `tests/probes/probe_lighting_files.c` gated behind `SF_BUILD_PROBES`
- **`extensions.md` lighting section** documenting `sf_gparam_value_t` tagged-union POD as a C-style adaptation

### Must NOT Have (Guardrails — non-negotiable)

- **NO `src/lighting/lighting_common.c`** — each format is independent; no shared scaffolding
- **NO `include/souls_formats/sf_lighting.h` umbrella** — `souls_formats.h` is the global umbrella
- **NO 16 per-type GPARAM field structs** (e.g., `sf_gparam_sbyte_field_t`) — single tagged union only
- **NO new sf_math types** — Vec2/3/4 and Color already exist in `sf_math.h`
- **NO XML pipeline / ApplyTemplate for GPARAM** — confirmed not in upstream
- **NO new arena-style allocator infrastructure** for v0.5.0 (no FXR3-`xml_arena`-style sophisticated arenas with custom lifecycle). **Single bulk `sf_xalloc` "name pool" patterns mirroring the existing BND3/BND4 name pool** are explicitly ALLOWED for GPARAM/BTAB/BTL/BTPB string storage (this is the established codebase precedent — see extensions.md BND3/BND4 row). The distinction: name pool = one bulk allocation + one bulk free; arena = re-allocating bump-pointer allocator with custom API.
- **NO BE byte order support** — v1 is LE-only (same policy as FLVER2 in extensions.md)
- **NO new `SF_<FORMAT>_GAME_*` enum extensions** unless probe confirms wire-distinct game variants — mirror upstream enum values verbatim
- **NO refactor of `er_extract_from_data0()` / any `<game>_test_helper.c`** — adapt tests to the helper, not vice versa. Multi-shard helper expansion is explicitly **deferred to v0.5.1** (separate batch).
- **NO new `<game>_test_helper.c` shard helpers** in this batch — Wave-0 probe inlines per-shard keys probe-locally; Wave-5 e2e tests are Data0-only.
- **NO backfilling tier-A docs for the prior 9 post-v1 batches** — out of scope, flag in handoff
- **NO new probe binary outside `SF_BUILD_PROBES` gate**
- **NO `fopen`/`fread`/`fwrite`/`fclose`/stdio** anywhere in `src/lighting/` (AGENTS.md §7)
- **NO Oodle DLL commits** / **NO FromSoft game-byte commits** (AGENTS.md §7)
- **NO `-Wno-*` suppression** — `-Werror` honored at all warnings
- **NO version bump past v0.5.0** — exact target is 0.5.0
- **NO probe-failure ignore** — if Wave 0 probe shows BTPB absent from all v1 games, the batch drops BTPB and updates legacy.md to keep it in Tier B
- **NO touching existing format implementations** outside `src/lighting/` (lighting batch is additive only)
- **NO bypassing `sf_xalloc`/`sf_xfree`** for any heap allocation

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — All verification is agent-executed. No "user manually
> tests/confirms" steps. Acceptance criteria are runnable shell commands with exact
> expected outputs.

### Test Decision

- **Infrastructure exists**: YES — Unity already integrated via `tests/CMakeLists.txt` + `sf_add_test()` helper
- **Automated tests**: YES (tests-after, per existing project rhythm) — every implementation task ships its synthetic test in the same wave
- **Framework**: Unity (ThrowTheSwitch) per existing convention
- **e2e test infrastructure**: `tests/e2e/er_test_helper.{h,c}` + `souls_formats_e2e_helpers` static lib + `SF_E2E_*_DIR` macros — reuse without modification

### QA Policy

Every task MUST include agent-executed QA scenarios. Evidence saved to
`.sisyphus/evidence/lighting-task-{N}-{slug}.{ext}`.

- **Build/lint**: Use Bash (cmake/ctest/objdump/grep) — output captured to evidence file
- **Synthetic round-trip**: Use Bash (ctest -L lighting + assertion line counts) — pass/fail line in ctest output is evidence
- **e2e**: Use Bash (ctest with SF_E2E_*_DIR set or probe-skip when missing) — evidence is per-test stdout
- **ASan/UBSan**: Use Bash (cmake -DSF_ENABLE_SANITIZERS=ON build-asan + ctest) — evidence is ctest output showing zero leaks

---

## Execution Strategy

### Parallel Execution Waves

> 7 waves total. Wave 0 is probe-first scope-lock. Waves 1-5 ship code+docs. Final
> wave is 4 parallel reviewers + user okay.

```
Wave 0 (Probe & Scope-Lock — runs FIRST, gates everything else):
└── Task 0: probe_lighting_files.c + evidence capture [unspecified-low]

Wave 1 (Foundation — parallel after Wave 0):
├── Task 1: 5 public headers skeleton (decls only) [quick]
├── Task 2: CMake src/lighting/ + SF_PUBLIC_HEADERS lighting block + label [quick]
├── Task 3: gparam_internal.h + sf_gparam_value_t POD design [unspecified-high]
├── Task 4: extensions.md "## Post-v1: Lighting" stub + AGENTS.md status row [writing]
└── Task 5: CMakeLists.txt VERSION bump 0.4.1→0.5.0 + README.md version mention [quick]

Wave 2 (Simple formats + tests — parallel after Wave 1):
├── Task 6: PMDCL impl (read+write) + synth test [unspecified-low]
├── Task 7: BTAB impl (read+write) + synth test [unspecified-low]
└── Task 8: BTPB impl + synth test [unspecified-high] (CONDITIONAL: only if Wave-0 confirmed presence)

Wave 3 (BTL — sequential after Wave 2):
└── Task 9: BTL impl (47+ Light fields, V16 + Wave-0-confirmed versions) + synth test [unspecified-high]

Wave 4 (GPARAM — parallel sub-tasks after Task 10 lands):
├── Task 10: GPARAM data model (Param/IField/Value mirror + sf_gparam_value_t POD impl) [unspecified-high]
├── Task 11: GPARAM reader (V5/V6 mandatory + V3 optional + V2 unsupported; signature UTF-16LE "filt"; string encoding verified) [unspecified-high]
├── Task 12: GPARAM writer (matching V5/V6 branches; reserve/fill paired) [unspecified-high]
└── Task 13: GPARAM synth test (V5 + V6 fixtures; one V3 safety fixture) [unspecified-low]

Wave 5 (e2e + docs + finalize — parallel after Wave 4):
├── Task 14: 5 e2e tests (one per format, multi-game probe inside, graceful skip) [unspecified-high]
├── Task 15: format-pmdcl.md + format-btab.md tier-A mapping docs [writing]
├── Task 16: format-btl.md + format-btpb.md (conditional) tier-A mapping docs [writing]
├── Task 17: format-gparam.md tier-A mapping doc (most rows) [writing]
├── Task 18: docs/api-mapping/README.md tier-A list update + legacy.md row removal [quick]
├── Task 19: CHANGELOG.md ## [0.5.0] block + extensions.md lighting section finalize [writing]
└── Task 20: docs/roadmap/post-v1.md update + PLAN.md §13 reflect closure [writing]

Wave FINAL (After ALL tasks — 4 parallel reviewers, then user okay):
├── F1: Plan compliance audit (oracle) — every Must Have/Not present, evidence intact
├── F2: Code quality review (unspecified-high) — tsc/lint/test green, no slop
├── F3: Real manual QA (unspecified-high) — exec every QA scenario, capture evidence
└── F4: Scope fidelity check (deep) — diff each task vs plan, no contamination
→ Present results → Get explicit user okay → mark batch complete

Critical Path: T0 → T2 → T10 → T11 → T12 → T14 → F1-F4 → user okay
Parallel Speedup: ~55-65% faster than sequential (W2 has 3 parallel, W4 has 3 after data model, W5 has 7 parallel)
Max Concurrent: 7 (Wave 5)
```

### Dependency Matrix

| Task | Depends on | Blocks |
|---|---|---|
| 0 | — | **ALL Wave 2-4 tasks** (T6-T13) — Wave-0 probe is a hard scope-lock gate; nothing in Wave 2-4 starts until probe evidence is captured |
| 1 | 0 | 6, 7, 8, 9, 10 (headers needed for impl) |
| 2 | 0 | 6, 7, 8, 9, 10 (CMake list needed to link) |
| 3 | 0 | 10 (gparam_internal.h precondition) |
| 4 | 0 | 19 (extensions.md stub before finalize) |
| 5 | 0 | 19 (version bump precondition for CHANGELOG) |
| 6 | **0**, 1, 2 | 14, 15 |
| 7 | **0**, 1, 2 | 14, 15 |
| 8 | 0, 1, 2 (CONDITIONAL: skip if T0 says BTPB absent) | 14, 16 |
| 9 | **0**, 1, 2, 6, 7 (after Wave 2 simple tests to free reviewer cycles; T0 also confirms BTL versions) | 14, 16 |
| 10 | **0**, 1, 2, 3 | 11, 12, 13 |
| 11 | **0**, 10 | 14, 17 |
| 12 | **0**, 10 | 14, 17 |
| 13 | **0**, 11, 12 | 14, 17 |
| 14 | 6, 7, 8, 9, 11, 12, 13 | F3 |
| 15 | 6, 7 | F1 |
| 16 | 8, 9 | F1 |
| 17 | 11, 12, 13 | F1 |
| 18 | 15, 16, 17 | F1 |
| 19 | 4, 5, 18 | F1 |
| 20 | 18, 19 | F1 |
| F1 | All T20 ← upstream | user okay |
| F2 | All T20 ← upstream | user okay |
| F3 | T14 | user okay |
| F4 | All T20 ← upstream | user okay |

### Agent Dispatch Summary

| Wave | Task count | Agent profile |
|---|---|---|
| 0 | 1 | T0 → `unspecified-low` (probe is straightforward) |
| 1 | 5 | T1/T2/T5 → `quick`, T3 → `unspecified-high` (polymorphism design), T4 → `writing` |
| 2 | 2-3 | T6/T7 → `unspecified-low`, T8 → `unspecified-high` (conditional) |
| 3 | 1 | T9 → `unspecified-high` (BTL has 47+ fields) |
| 4 | 4 | T10/T11/T12 → `unspecified-high`, T13 → `unspecified-low` |
| 5 | 7 | T14 → `unspecified-high`, T15/T16/T17/T19/T20 → `writing`, T18 → `quick` |
| FINAL | 4 | F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep` |

---

## TODOs

- [x] 0. **Wave 0 — Probe lighting file presence in v1 game archives** [`unspecified-low`]

  **What to do**:
  - Create `tests/probes/probe_lighting_files.c` gated by `SF_BUILD_PROBES=ON`.
  - **Adopt the established multi-shard probe pattern from `tests/probes/probe_nightreign_msb.c:23-39, 116-251`**: existing `tests/e2e/{er,nightreign,ac6,sekiro}_test_helper.c` files only open the Data0 shard with the single key from `src/archive/bhd5_keys.c`. To scan ER Data1/2/3 and Nightreign extra shards, this probe inlines additional PEM keys per shard (mirroring `probe_nightreign_msb.c:k_er_data2_pem` / `k_nr_data2_pem`) and performs **probe-local BHD5 extraction** (manual file read → `sfi_rsa_decrypt_pkcs1` → magic-byte hash lookup via `find_entry()` → DCX unwrap). **DO NOT** modify shared helpers (`er_test_helper.c`, etc.) — Wave 5 scope explicitly excludes helper refactor.
  - For Data0 shards: use existing `er_extract_from_data0` / equivalent helpers (works without changes).
  - For non-Data0 shards: inline the per-shard PEM key + manual extraction code in this probe binary only.
  - Build a **candidate-path list** from known ER/Sekiro/Nightreign/AC6 map-ID patterns (e.g., `/map/mapstudio/m60_42_36_00/m60_42_36_00_lighting.gparam.dcx`, etc. — pattern: `/map/mapstudio/<mapid>/<mapid>_<purpose>.<ext>[.dcx]`). For Data0: use `sf_bhd5_extract_by_path(b, candidate)`; for non-Data0: use manual hash-lookup pattern from `probe_nightreign_msb.c`. On success, sniff DCX-decoded payload bytes 0..16 for magic detection:
    - BTAB: 4-byte sentinel `0x01 0x00 0x00 0x00` then `0x00 0x00 0x00 0x00` (Read.cs:33-34)
    - BTL: 4-byte `0x02 0x00 0x00 0x00` then version int in {1,2,5,6,16,18}
    - BTPB: `Int32(2 or 3)` + `Int32(0 or 1)` + BigEndian byte (no ASCII magic)
    - GPARAM: UTF-16LE `"filt"` = `0x66 0x00 0x69 0x00 0x6C 0x00 0x74 0x00`
    - PMDCL: `Int64(count) Int64(0x20) Int64(0) Int64(0)` (numeric-only)
  - Write evidence to `.sisyphus/evidence/lighting-probe.md` with one row per (format × game): `| Format | Game | Archive | EntryHash | DcxType | Version | Size |`
  - For each format-game combination found, record at least 1 sample BHD5 path hash + size + version field value
  - If BTPB count is 0 across ALL v1 games → emit explicit log line `LIGHTING-PROBE: BTPB NOT PRESENT IN V1 GAMES — drop from batch`
  - If BTL has any V18 hits → emit `LIGHTING-PROBE: BTL V18 confirmed in <game>` and record max version observed
  - If GPARAM has both `.gparam` and `.fltparam` extension hits → record dual-extension presence

  **Must NOT do**:
  - Probe binary in default build (`SF_BUILD_PROBES=ON` is opt-in)
  - Write/modify any game files (read-only access)
  - Skip games with missing archives — print "SKIP <game>: archive missing" and continue
  - Commit any game-data bytes to repo (capture metadata only — counts, sizes, hashes, version field values; NO file contents)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low` — straightforward probe code, mostly iterating BHD5 + magic-byte sniffing
    - Reason: Bounded scope. No new format parsers. Reuse existing helpers.
  - **Skills**: `[]`
    - No specialized skills needed — this is plain BHD5 iteration

  **Parallelization**:
  - **Can Run In Parallel**: NO (single task)
  - **Parallel Group**: Wave 0 alone
  - **Blocks**: Tasks 6, 7, 8, 9 (scope-lock for BTPB inclusion + BTL versions)
  - **Blocked By**: None (can start immediately)

  **References**:

  **Pattern References**:
  - `tests/probes/probe_nightreign_msb.c:23-39, 116-251` — **THE canonical multi-shard probe pattern**. Inline PEM keys per non-Data0 shard (e.g., `k_er_data2_pem`, `k_nr_data2_pem`), then `read_file` → `rsa_unwrap` → `find_entry` (hash lookup) → BDT extract → `decrypt_ranges` (AES) → DCX sniff. Copy this template verbatim, then swap the candidate-path list for lighting extensions.
  - `tests/probes/probe_matbin_paramtypes.c` — second probe example showing magic-byte sniff pattern (Data0-only)
  - `tests/e2e/er_test_helper.c:202-247` — `er_extract_from_data0` impl. **NOTE**: this helper opens ONLY Data0 (see `tests/e2e/er_test_helper.c:38-40`). For Data1/2/3 scans, the probe MUST inline per-shard keys per the `probe_nightreign_msb.c` pattern.
  - `tests/e2e/sekiro_test_helper.c` / `tests/e2e/ac6_test_helper.c` / `tests/e2e/nightreign_test_helper.c` — per-game helper templates. Same limitation: each helper opens its game's primary shard only.
  - `src/archive/bhd5_keys.c:10-18, 73-79` — the shipped keys (one per game, Data0 only). Additional shard keys must be embedded inline in the probe binary only.

  **API References** (verified against current `sf_bhd5.h`):
  - `include/souls_formats/sf_bhd5.h` public API: `sf_bhd5_open`, `sf_bhd5_close`, `sf_bhd5_bucket_count`, `sf_bhd5_total_file_count`, `sf_bhd5_extract_by_hash_64`, `sf_bhd5_extract_by_hash_32`, `sf_bhd5_extract_by_path` (use `_by_path` for the probe — pass each candidate path string and let the helper hash internally)
  - `include/souls_formats/sf_dcx.h` — `sf_dcx_sniff`, `sf_dcx_decompress_from_buffer`
  - `tests/e2e/er_test_helper.h` — public probe-helper API surface
  - **Probe strategy without `iterate_buckets`**: BHD5 stores only path hashes, so the probe cannot reverse-enumerate file names. Instead, build a **candidate-path list** from known ER map-ID patterns (e.g., `m60_42_36_00`, `m10_00_00_00`, `m11_00_00_00` per PLAN.md §8.4) cross-product with lighting extensions (`.btab`, `.btl`, `.btpb`, `.gparam`, `.fltparam`, `.pmdcl`) cross-product with typical container suffixes (`.dcx`, raw) and call `sf_bhd5_extract_by_path` for each. The probe sniffs DCX magic for hits.

  **Upstream References (for magic recognition)**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTAB.cs:29-49` — header layout
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTL.cs:41-59` — version field at offset 4
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTPB.cs:45-80` — version auto-detection from combo
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/GPARAM.cs:70-91` — UTF-16LE "filt" signature
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PMDCL.cs:28-48` — Int64 sentinel pattern

  **WHY Each Reference Matters**:
  - Reverse-lookup not feasible (BHD5 only stores path hashes). Use forward iteration + magic-byte sniff after DCX unwrap. The probe template (`probe_nightreign_msb.c`) shows the exact pattern.
  - Magic-byte signatures from upstream are the ground truth — DO NOT guess.

  **Acceptance Criteria**:

  **Build acceptance**:
  - [ ] `cmake -B build-probe -DSF_BUILD_PROBES=ON ...` succeeds
  - [ ] `cmake --build build-probe --target probe_lighting_files` succeeds with zero warnings
  - [ ] `./build-probe/tests/probes/probe_lighting_files.exe` exits 0 across all available games

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Probe runs against Elden Ring Data0+1+2+3 and produces evidence
    Tool: Bash
    Preconditions: /mnt/c/Games/ELDEN RING/Game/Data{0,1,2,3}.bhd present; Oodle DLL present at ~/dev/oodle/
    Steps:
      1. cmake -B build-probe -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_PROBES=ON
      2. cmake --build build-probe --target probe_lighting_files
      3. ./build-probe/tests/probes/probe_lighting_files.exe > .sisyphus/evidence/lighting-probe.md
      4. grep -cE '^\| (BTAB|BTL|BTPB|GPARAM|PMDCL) \|' .sisyphus/evidence/lighting-probe.md → ≥ 5
    Expected Result: lighting-probe.md exists, contains at least 1 row per format (or SKIP marker)
    Failure Indicators: file missing, zero rows for all formats, exit code non-zero
    Evidence: .sisyphus/evidence/lighting-probe.md (the evidence file IS the test output)

  Scenario: BTPB absence triggers drop-from-batch marker
    Tool: Bash
    Preconditions: probe has completed
    Steps:
      1. grep -c "LIGHTING-PROBE: BTPB NOT PRESENT" .sisyphus/evidence/lighting-probe.md
      2. If line exists → BTPB scope decision logged; Wave 2 Task 8 is CANCELLED
      3. If line absent → BTPB present in at least one v1 game; Task 8 proceeds
    Expected Result: explicit scope-decision marker present
    Evidence: .sisyphus/evidence/lighting-probe-scope.md (human-readable summary)

  Scenario: BTL version-18 confirmation
    Tool: Bash
    Preconditions: probe has completed
    Steps:
      1. grep -cE "BTL V18 confirmed" .sisyphus/evidence/lighting-probe.md → 0 or ≥ 1
      2. Record decision: if 0, Task 9 fixture set = {V16}; if ≥ 1, Task 9 fixture set = {V16, V18}
    Expected Result: decision recorded for Task 9 scope
    Evidence: same as above
  ```

  **Evidence to Capture**:
  - [ ] `.sisyphus/evidence/lighting-probe.md` — full probe output table
  - [ ] `.sisyphus/evidence/lighting-probe-scope.md` — scope decisions for Tasks 8, 9

  **Commit**: YES (atomic per task)
  - Message: `chore(tests): probe lighting file existence in v1 games`
  - Files: `tests/probes/probe_lighting_files.c`, `tests/CMakeLists.txt`
  - Pre-commit: `cmake --build build-probe --target probe_lighting_files`

- [x] 1. **Wave 1 — 5 public headers (declarations only)** [`quick`]

  **What to do**:
  - Create `include/souls_formats/sf_btab.h`:
    - `typedef struct sf_btab sf_btab_t;` (opaque)
    - `typedef struct sf_btab_entry sf_btab_entry_t;` (opaque)
    - Public API: `sf_btab_read_from_memory`, `sf_btab_write_to_buffer`, `sf_btab_destroy`, `sf_btab_is_big_endian`, `sf_btab_is_long_format`, `sf_btab_entry_count`, `sf_btab_get_entry`, plus entry accessors for PartName/MaterialName/AtlasID/UVOffset/UVScale
    - All public functions decorated with `SF_API`
  - Same shape for `sf_btl.h` (include `sf_btl_light_type_t` enum + `_Static_assert` for the enum values mirroring upstream; opaque `sf_btl_t` + `sf_btl_light_t`)
  - `sf_btpb.h` (CONDITIONAL — only create if Wave-0 confirmed presence). Include `sf_btpb_version_t` enum **with exactly the upstream enum members (DarkSouls2LE/BE/Bloodborne/DarkSouls3)**. Do NOT invent Sekiro/ER/AC6 values.
  - `sf_pmdcl.h` — simplest. Opaque `sf_pmdcl_t` + `sf_pmdcl_decal_t`; no enum.
  - `sf_gparam.h`:
    - `typedef enum sf_gparam_version` enum with `V2/V3/V5/V6` mirror of upstream
    - `typedef enum sf_gparam_field_type` enum with all **16 upstream FieldType values** at upstream-EXACT numeric values: `SBYTE=1, SHORT=2, INT=3, LONG=4, BYTE=5, USHORT=6, UINT=7, ULONG=8, FLOAT=9, DOUBLE=10, BOOL=11, VEC2=12, VEC3=13, VEC4=14, COLOR=15, STRING=16` (per GPARAM.cs:220-238 — **upstream uses 1-16, NOT 0-15**)
    - `_Static_assert(SF_GPARAM_FIELD_TYPE_SBYTE == 1, "FieldType.Sbyte must match upstream numeric value 1")` and `_Static_assert(SF_GPARAM_FIELD_TYPE_STRING == 16, "FieldType.String must match upstream numeric value 16")` (drift guards on first + last)
    - `typedef struct sf_gparam_value sf_gparam_value_t;` — public POD tagged-union (see Task 3 for definition)
    - Opaque `sf_gparam_t`, `sf_gparam_param_t`, `sf_gparam_field_t`, `sf_gparam_unk_param_extra_t` (mirror upstream — **NO Group layer**; upstream `GPARAM.Params` is top-level per `GPARAM.cs:36`)
    - Public API: `_read_from_memory`, `_write_to_buffer`, `_destroy`, `_get_version`, `_get_unk0d`, `_get_count14`, `_get_unk40`, `_get_unk50`, `_get_data30(*ptr, *size)`, `_param_count`, `_get_param`, `_param_get_key`, `_param_get_name`, `_param_field_count`, `_param_get_field`, `_param_comment_count`, `_param_get_comment`, `_field_get_key`, `_field_get_name`, `_field_get_type`, `_field_value_count`, `_field_get_value` (returns POD by value), `_unk_param_extra_count`, `_get_unk_param_extra`
  - Add header-style block comment at top of each: `// Upstream: <path>.cs` per AGENTS.md §5.x STRICT UPSTREAM REFERENCE
  - Update `include/souls_formats/souls_formats.h` umbrella to include the 5 new headers

  **Must NOT do**:
  - Define any struct internals in public headers (opaque only)
  - Include any `lighting_common.h` (does not exist)
  - Add Sekiro/ER/AC6/NR enum values to `sf_btpb_version_t` (upstream stops at DS3; mirror exactly)
  - Implement any read/write functions yet (this task is declarations only)
  - Add `sf_lighting.h` umbrella header

  **Recommended Agent Profile**:
  - **Category**: `quick` — header declarations are small, repetitive, mechanical
    - Reason: Single file changes, clear template (mirror existing `sf_fxr3.h`)
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (5 files can be drafted in parallel sub-tasks, but small enough for one agent to handle)
  - **Parallel Group**: Wave 1 (with Tasks 2, 3, 4, 5)
  - **Blocks**: Tasks 6, 7, 8, 9, 10 (impl tasks need declarations)
  - **Blocked By**: Task 0 (Wave-0 probe must confirm BTPB inclusion)

  **References**:

  **Pattern References**:
  - `include/souls_formats/sf_fxr3.h` — tagged-union POD `sf_fxr3_field_t` precedent (THE model for `sf_gparam_value_t`)
  - `include/souls_formats/sf_param.h` — opaque type + `_count` + `_get_*` accessor pattern
  - `include/souls_formats/sf_tae.h` — enum with `_Static_assert` precedent

  **API References**:
  - `include/souls_formats/sf_common.h` — `SF_API`, `sf_result_t`, `sf_allocator_t`
  - `include/souls_formats/sf_math.h` — `sf_vec2_t`, `sf_vec3_t`, `sf_vec4_t`, `sf_color_t` (already exist for GPARAM Vec/Color values)

  **Upstream References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTAB.cs:9-24`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTL.cs:11-26, 93-112`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTPB.cs:11-31, 143-167`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/GPARAM.cs:1-217`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PMDCL.cs:10-15, 76-101`

  **WHY Each Reference Matters**:
  - FXR3 is the only existing C-side polymorphic format precedent — copy its tagged-union POD shape verbatim
  - sf_param.h/sf_tae.h establish the opaque-type + `_count` + `_get_*` accessor naming
  - Upstream class signatures dictate the C structures' field set; mirror exactly

  **Acceptance Criteria**:

  - [ ] All 4-5 headers exist and compile in `cmake --build build-mingw`
  - [ ] `grep -cE '^SF_API' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h` ≥ 30 (avg 6 per header)
  - [ ] `grep -cE '_Static_assert' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h` ≥ 5
  - [ ] `grep -E '// Upstream:' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h` returns at least 1 per file (STRICT UPSTREAM REFERENCE comment present)
  - [ ] `include/souls_formats/souls_formats.h` includes all 4-5 new headers
  - [ ] No struct internals exposed: `grep -E 'struct sf_(btab|btl|btpb|gparam|pmdcl)_[a-z]* \{' include/souls_formats/` returns empty (opaque preserved)

  **QA Scenarios**:

  ```
  Scenario: Headers compile via test-include
    Tool: Bash
    Preconditions: Wave-0 probe complete; CMake project configured (Task 2 not yet needed for compile-test)
    Steps:
      1. Create a temp C file: cat > /tmp/sf_headers_test.c <<'EOF'\n#include <souls_formats/souls_formats.h>\nint main() { return 0; }\nEOF
      2. x86_64-w64-mingw32-gcc -Iinclude -c /tmp/sf_headers_test.c -o /tmp/sf_headers_test.o
      3. Exit code 0
    Expected Result: zero compile errors, zero warnings
    Failure Indicators: any error, any warning (Werror)
    Evidence: .sisyphus/evidence/lighting-task-1-compile.log

  Scenario: Opaque-type guarantee
    Tool: Bash
    Steps:
      1. grep -rE 'struct sf_(btab|btl|btpb|gparam|pmdcl)_[a-z_]+ \{' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h
    Expected Result: empty (no struct body in public headers; tagged-union POD `sf_gparam_value_t` is an exception and its definition lives in sf_gparam.h)
    Evidence: .sisyphus/evidence/lighting-task-1-opaque.log
  ```

  **Commit**: YES
  - Message: `feat(lighting): add public headers for BTAB/BTL/BTPB/GPARAM/PMDCL`
  - Files: 4-5 new `.h` files + `souls_formats.h`

- [x] 2. **Wave 1 — CMake registration: src/lighting/ + lighting label** [`quick`]

  **What to do**:
  - Edit `CMakeLists.txt`:
    - Add new `list(APPEND SF_PUBLIC_HEADERS ...)` block for `sf_btab.h sf_btl.h sf_btpb.h sf_gparam.h sf_pmdcl.h` (skip BTPB if Wave-0 dropped it)
    - Add new `list(APPEND SF_SOURCES ...)` block for `src/lighting/btab.c src/lighting/btl.c src/lighting/btpb.c src/lighting/gparam.c src/lighting/pmdcl.c` (skip BTPB if dropped)
  - Edit `tests/CMakeLists.txt`:
    - Register `lighting` as a recognized label (verify existing `sf_add_test()` helper supports new labels — should be automatic)
  - Create empty stub `src/lighting/btab.c`, `btl.c`, `btpb.c`, `gparam.c`, `pmdcl.c` containing only `#include` lines so build doesn't fail at registration time. Actual implementations come in Waves 2-4.

  **Must NOT do**:
  - Hardcode anything game-specific
  - Add `lighting_common.c` to SF_SOURCES
  - Touch any other `list(APPEND SF_*)` block

  **Recommended Agent Profile**:
  - **Category**: `quick` — CMake list updates are mechanical
    - Reason: clear edit pattern, no new logic

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with Tasks 1, 3, 4, 5)
  - **Blocks**: Tasks 6, 7, 8, 9, 10 (need source dir registered to compile)
  - **Blocked By**: Task 0

  **References**:
  - Pattern: `CMakeLists.txt:SF_PUBLIC_HEADERS` and `SF_SOURCES` existing lists
  - Pattern: previous batch CMake additions (search git log for "list(APPEND SF_PUBLIC_HEADERS")

  **Acceptance Criteria**:
  - [ ] `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake` succeeds
  - [ ] `cmake --build build-mingw` builds the (empty-stub) lighting sources without error
  - [ ] `grep -c 'src/lighting/' CMakeLists.txt` ≥ 4
  - [ ] `tests/CMakeLists.txt` accepts label `lighting` (verified by next-wave tests)

  **QA Scenarios**:
  ```
  Scenario: Build configures + compiles with new sources
    Tool: Bash
    Steps:
      1. cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake
      2. cmake --build build-mingw 2>&1 | grep -cE 'warning:|error:' → 0
    Evidence: .sisyphus/evidence/lighting-task-2-build.log
  ```

  **Commit**: YES
  - Message: `build(lighting): register src/lighting/ + lighting test label`
  - Files: `CMakeLists.txt`, `tests/CMakeLists.txt`, 4-5 empty `src/lighting/*.c` stubs

- [x] 3. **Wave 1 — gparam_internal.h + sf_gparam_value_t POD design** [`unspecified-high`]

  **What to do**:
  - Create `src/lighting/gparam_internal.h` — **mirror upstream `GPARAM.cs` directly; NO Group layer**:
    - `sf_gparam_t` holds: `version`, `unk0d`, `count14`, `unk40`, `unk50` (V≥V5), `params` ptr+count, `data30` ptr+size, `unk_param_extras` ptr+count, `name_pool`, `alloc` (mirrors `GPARAM.cs:18-56` field set exactly)
    - `sf_gparam_param_t` holds: `key`, `name`, `fields` ptr+count, `comments` ptr+count (mirrors `GPARAM.cs:Param` class — top-level item under GPARAM.Params per `GPARAM.cs:36`)
    - `sf_gparam_field_t` holds: `key`, `name`, `type` (sf_gparam_field_type_t), `capacity`, `unk` (V<V6) or `unk_byte` (V≥V6), `values` ptr+count typed by `type`
    - `sf_gparam_unk_param_extra_t` holds: upstream `UnkParamExtra` mirror — unk00, ids ptr+count, unk0c (V≥V5)
  - Define public POD `sf_gparam_value_t` IN `include/souls_formats/sf_gparam.h` (NOT in internal):
    ```c
    typedef struct sf_gparam_value {
        sf_gparam_field_type_t type;
        uint32_t id;       /* upstream IFieldValue.Id */
        uint32_t unk04;    /* V5+ extra; 0 when version < V5 */
        union {
            int8_t   as_sbyte;
            int16_t  as_short;
            int32_t  as_int;
            int64_t  as_long;
            uint8_t  as_byte;
            uint16_t as_ushort;
            uint32_t as_uint;
            uint64_t as_ulong;
            float    as_float;
            double   as_double;
            int8_t   as_bool;       /* upstream stores as int8 */
            sf_vec2_t as_vec2;
            sf_vec3_t as_vec3;
            sf_vec4_t as_vec4;
            sf_color_t as_color;
            const char *as_string;  /* borrowed pointer into gparam-owned name pool (single bulk sf_xalloc; freed in destroy) */
        } v;
    } sf_gparam_value_t;
    ```
  - Add `_Static_assert(sizeof(sf_gparam_value_t) <= 32, "value POD too large")` as drift guard
  - Provide internal helpers in `gparam_internal.h`:
    - `static inline size_t sfi_gparam_field_payload_size(sf_gparam_field_type_t t);` (table-driven, covers all 16)
    - `static inline bool sfi_gparam_field_type_valid(uint8_t raw);` (0..15)
  - Mirror upstream `FieldType` enum order exactly (`GPARAM.cs:6-30`)
  - Document in `extensions.md` task (T4) the choice of single tagged-union over 16-per-type structs

  **Must NOT do**:
  - Expose internal structs in `include/souls_formats/sf_gparam.h`
  - Add per-type structs `sf_gparam_sbyte_field_t` (forbidden)
  - Allocate `sf_gparam_value_t` on heap individually — POD returned by value
  - Reserve any union member > 16 bytes that would bloat sizeof — Vec4 (16B) is the largest

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — polymorphism design has non-trivial trade-offs
    - Reason: Tagged-union design decisions cascade to all of GPARAM impl

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 10 (data model needs internal layout decided), 11, 12, 13
  - **Blocked By**: Task 0

  **References**:
  - Precedent: `include/souls_formats/sf_fxr3.h` — `sf_fxr3_field_t` tagged-union with `kind` discriminator + `as_*` union members
  - Precedent: `src/effects/fxr3_internal.h` (if exists) or similar internal header
  - Upstream: `GPARAM.cs:6-30` (FieldType), `GPARAM.cs:220-300` (IField/FieldValue<T> shapes)
  - sf_math.h Static asserts: `_Static_assert(sizeof(sf_vec3_t) == 12, ...)` already present

  **WHY Each Reference Matters**:
  - Tagged-union POD has only ONE precedent in this codebase (FXR3) — that's THE pattern to follow.
  - Bloated value POD = wasted memory across thousands of values in a single GPARAM. Cap sizeof.

  **Acceptance Criteria**:
  - [ ] `src/lighting/gparam_internal.h` exists; no struct internals leak to `include/souls_formats/`
  - [ ] `sf_gparam_value_t` defined in public header, `sizeof(sf_gparam_value_t) <= 32` enforced by `_Static_assert`
  - [ ] Table `sfi_gparam_field_payload_size` covers all 16 FieldType values 1..16 (compile-time `_Static_assert` on table length; index by `type - 1` or sparse 17-entry array — implementer choice)
  - [ ] Mirror of upstream FieldType enum verified by `_Static_assert(SF_GPARAM_FIELD_TYPE_SBYTE == 1)` and `_Static_assert(SF_GPARAM_FIELD_TYPE_STRING == 16)`
  - [ ] `sfi_gparam_field_type_valid` accepts `[1..16]`, rejects 0 and ≥17

  **QA Scenarios**:
  ```
  Scenario: gparam_internal.h compiles standalone
    Tool: Bash
    Steps:
      1. Create temp C: `#include "src/lighting/gparam_internal.h"` + main
      2. Compile with -Iinclude -Isrc → exit 0
    Evidence: .sisyphus/evidence/lighting-task-3-compile.log

  Scenario: sf_gparam_value_t size constraint
    Tool: Bash
    Steps:
      1. cmake --build build-mingw  → no _Static_assert failure
      2. cat .../build-mingw/CMakeCache.txt → confirm Debug build
    Evidence: .sisyphus/evidence/lighting-task-3-static-assert.log
  ```

  **Commit**: YES
  - Message: `feat(lighting): gparam_internal.h + sf_gparam_value_t POD design`
  - Files: `src/lighting/gparam_internal.h`, `include/souls_formats/sf_gparam.h` (POD definition)

- [x] 4. **Wave 1 — extensions.md "Post-v1: Lighting" stub + AGENTS.md status row** [`writing`]

  **What to do**:
  - Edit `docs/api-mapping/extensions.md`:
    - Add new heading `## Post-v1: Lighting`
    - Add stub rows (to be finalized in T19):
      - `sf_gparam_value_t` tagged-union POD — rationale: C lacks C# generics; mirrors FXR3 precedent; sizeof ≤ 32 bytes; covers all 16 upstream FieldType variants
      - `sf_btab_t/sf_btl_t/sf_btpb_t/sf_pmdcl_t/sf_gparam_t` opaque types — rationale: standard C-API convention, matches all other format modules
      - Any divergences discovered during implementation get appended here (placeholder section: "_Additional divergences appended during Wave 2-4 implementation_")
  - Edit `AGENTS.md` "Current status" table (§2):
    - Add row: `| Lighting (BTAB/BTL/BTPB/GPARAM/PMDCL) | (post-v1) | 🔄 in progress | TBD |`
    - After completion, this row updates to "✅ done" with test counts (left to T20 for finalization)

  **Must NOT do**:
  - Document specific allocator/perf optimizations (none for 0.5.0)
  - Mention BTPB as confirmed v1-applicable until probe evidence supports it (use phrasing like "BTPB conditionally included pending Wave-0 probe")
  - Touch any other section of AGENTS.md

  **Recommended Agent Profile**:
  - **Category**: `writing` — small documentation edits with conventions
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: Task 19 (CHANGELOG/extensions finalize)
  - **Blocked By**: Task 0

  **References**:
  - `docs/api-mapping/extensions.md` — existing `## Phase 6: Geometry + Material` and `## Phase 7: Animation + Effects` sections as template
  - `AGENTS.md:21-30` current status table

  **Acceptance Criteria**:
  - [ ] `grep -c '## Post-v1: Lighting' docs/api-mapping/extensions.md` == 1
  - [ ] `grep -c 'sf_gparam_value_t' docs/api-mapping/extensions.md` ≥ 1
  - [ ] `grep -c 'Lighting' AGENTS.md` ≥ 2 (heading + table row)

  **QA Scenarios**:
  ```
  Scenario: Extensions section structure
    Tool: Bash
    Steps:
      1. grep -A 5 '## Post-v1: Lighting' docs/api-mapping/extensions.md
      2. Verify at least 1 sub-section row exists
    Evidence: .sisyphus/evidence/lighting-task-4-extensions.log
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): extensions.md lighting stub + AGENTS.md status row`
  - Files: `docs/api-mapping/extensions.md`, `AGENTS.md`

- [x] 5. **Wave 1 — Version bump 0.4.1 → 0.5.0 + README mention** [`quick`]

  **What to do**:
  - Edit `CMakeLists.txt`:
    - Change `project(souls_formats_c VERSION 0.4.1 ...)` to `VERSION 0.5.0`
    - Same in any other VERSION literal
  - Edit `README.md`:
    - Update version mention "(v0.3.0)" or "(v0.4.x)" → "(v0.5.0)"
  - Note: CHANGELOG.md `## [0.5.0]` block is added in T19 with full content; here we just establish the version literal.

  **Must NOT do**:
  - Touch CHANGELOG.md (T19's job)
  - Change SemVer scheme

  **Recommended Agent Profile**:
  - **Category**: `quick` — single literal updates
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: T19 (CHANGELOG entry must match this version)
  - **Blocked By**: Task 0

  **References**:
  - Existing pattern: `git log --oneline CMakeLists.txt` shows previous version bumps (0.1.0 → 0.2.0 → 0.3.0 → 0.4.0 → 0.4.1)

  **Acceptance Criteria**:
  - [ ] `grep -c 'VERSION 0.5.0' CMakeLists.txt` ≥ 1
  - [ ] `grep -c 'v0.5.0' README.md` ≥ 1
  - [ ] No leftover `VERSION 0.4.1` strings in CMakeLists.txt
  - [ ] `cmake -B build-mingw && grep VERSION build-mingw/CMakeCache.txt` shows 0.5.0 line

  **QA Scenarios**:
  ```
  Scenario: Version bump propagates to CMake cache
    Tool: Bash
    Steps:
      1. cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake
      2. grep -E 'PROJECT_VERSION:STATIC=0\.5\.0' build-mingw/CMakeCache.txt
    Expected Result: line present
    Evidence: .sisyphus/evidence/lighting-task-5-version.log
  ```

  **Commit**: YES
  - Message: `build: bump VERSION 0.4.1 → 0.5.0 + README mention`
  - Files: `CMakeLists.txt`, `README.md`

- [x] 6. **Wave 2 — PMDCL implementation + synthetic round-trip test** [`unspecified-low`]

  **What to do**:
  - Implement `src/lighting/pmdcl.c` (mirror upstream `PMDCL.cs`, 170 LOC):
    - Internal struct `sf_pmdcl_t { sf_pmdcl_decal_t *decals; size_t decal_count; const sf_allocator_t *alloc; }`
    - Internal struct `sf_pmdcl_decal_t { sf_vec3_t x_angles, y_angles, z_angles; sf_vec3_t position; float unk3c; int32_t decal_param_id; int16_t size1, size2; }`
    - Implement `sf_pmdcl_read_from_memory` — force LE, read Int64(count) + AssertInt64(0x20) + 2× AssertInt64(0) + per-decal step-in
    - Implement `sf_pmdcl_write_to_buffer` — ReserveInt64 for offsets, Pad(0x20), FillInt64 mirroring upstream Write()
    - Implement `sf_pmdcl_destroy`, `_decal_count`, `_get_decal`, decal accessors
    - Single `goto cleanup` per fallible function
    - Every `Reserve_*` paired with `Fill_*` before `sf_binary_writer_finish`
    - `// Upstream: PMDCL.cs:Read()` and `:Write()` attribution comments
  - Implement `tests/lighting/test_pmdcl_synthetic.c`:
    - 1 fixture: synthetic PMDCL with 3 decals (varied angles/positions/IDs)
    - Round-trip: build → write → read → write → byte-compare both writes (`TEST_ASSERT_EQUAL_MEMORY`)
    - Negative test: missing `Fill_*` (intentional bug injection in a separate test path) returns SF_ERR_INTERNAL — verify writer correctness
    - Edge case: zero-decal PMDCL must round-trip
  - Register test in `tests/CMakeLists.txt` via `sf_add_test(test_pmdcl_synthetic LABELS lighting)`
  - Build + ASan verify

  **Must NOT do**:
  - Add any version branching (PMDCL has none)
  - Allocate decals one-at-a-time (use single bulk array allocation)
  - Use stdio
  - Expose internal struct in `sf_pmdcl.h`

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low` — simple format, mostly boilerplate
    - Reason: 170 LOC upstream, no version logic, smallest of the 5
  - **Skills**: `[]`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 7, 8 if BTPB included)
  - **Parallel Group**: Wave 2
  - **Blocks**: Tasks 14, 15 (e2e + mapping doc)
  - **Blocked By**: Tasks **0** (Wave-0 scope-lock gate), 1, 2

  **References**:
  - Upstream: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PMDCL.cs:10-170` — full file is the source spec
  - Pattern: `src/effects/nsa.c` — small format C implementation (NSA is similar size, mostly POD lists)
  - Pattern: `src/misc/grass.c` — even simpler format
  - Tests: `tests/misc/test_nsa_synthetic.c` — synth round-trip pattern

  **WHY Each Reference Matters**:
  - PMDCL is the simplest of the 5 — use it to validate the entire pipeline (header→impl→test→commit) before tackling complex formats
  - Mirror NSA pattern verbatim — same shape: opaque type, count + accessor API, single read/write pair

  **Acceptance Criteria**:
  - [ ] Build: `cmake --build build-mingw 2>&1 | grep -cE 'warning:|error:' (touching pmdcl)` == 0
  - [ ] Test: `ctest --test-dir build-mingw -L lighting -R pmdcl_synthetic --output-on-failure` → PASS
  - [ ] ASan: `ctest --test-dir build-asan -L lighting -R pmdcl_synthetic` → PASS, zero leaks
  - [ ] Symbol export: `objdump -p build-mingw/libsouls_formats.dll | grep -cE 'sf_pmdcl_'` ≥ 6 (read, write, destroy, count, get_decal, accessors)
  - [ ] Reserve/fill pair: `grep -cE 'sf_binary_writer_reserve_' src/lighting/pmdcl.c` == `grep -cE 'sf_binary_writer_fill_' src/lighting/pmdcl.c`

  **QA Scenarios**:
  ```
  Scenario: PMDCL synthetic 3-decal round-trip
    Tool: Bash
    Preconditions: Wave 1 complete; build-mingw configured
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_pmdcl_synthetic
      2. ctest --test-dir build-mingw -R pmdcl_synthetic --output-on-failure
    Expected Result: exit 0; "1 Tests 0 Failures" in stdout; "test_pmdcl_synthetic ........  Passed"
    Failure Indicators: any FAIL line, non-zero exit, ASan errors
    Evidence: .sisyphus/evidence/lighting-task-6-pmdcl-synth.log

  Scenario: PMDCL empty (zero decals) edge case
    Tool: Bash
    Steps:
      1. Test code contains an explicit zero-decal fixture
      2. ctest pass for that sub-case
    Evidence: same log file, sub-case marker
  ```

  **Commit**: YES
  - Message: `feat(lighting): implement PMDCL read/write + synthetic test`
  - Files: `src/lighting/pmdcl.c`, `tests/lighting/test_pmdcl_synthetic.c`, `tests/CMakeLists.txt` (one-line label registration)

- [x] 7. **Wave 2 — BTAB implementation + synthetic round-trip test** [`unspecified-low`]

  **What to do**:
  - Implement `src/lighting/btab.c` (mirror upstream `BTAB.cs`, 164 LOC):
    - Internal `sf_btab_t { bool big_endian; bool long_format; sf_btab_entry_t *entries; size_t entry_count; const sf_allocator_t *alloc; char *name_pool; }` (use single name pool like BND3/BND4 pattern for PartName+MaterialName)
    - Internal `sf_btab_entry_t { const char *part_name; const char *material_name; int32_t atlas_id; sf_vec2_t uv_offset, uv_scale; }` (name pointers borrow into pool)
    - Implement `sf_btab_read_from_memory`:
      - Read header: AssertInt32(1), AssertInt32(0), entryCount, stringsLength, BigEndian byte, AssertByte(0)x3, entrySize ∈ {0x1C, 0x28} (determines LongFormat), AssertPattern(0x24, 0)
      - Skip strings; read entryCount entries via varint string offsets + AtlasID + UVOffset + UVScale + optional Int32(0) for VarintLong
      - Build name pool from strings; entry pointers borrow into pool
    - Implement `sf_btab_write_to_buffer`:
      - Mirror Write(): write header, ReserveInt32("StringsLength"), write UTF-16 strings with 8-byte relative padding, FillInt32, write entries with captured string offsets
    - Implement destroy, count, get_entry, BigEndian/LongFormat accessors, per-entry accessors
    - Strings encoded UTF-16 per upstream (verify via `Encoding.Unicode.GetString` in BTAB.cs:64)
    - `// Upstream: BTAB.cs:Read()` / `:Write()` per public function
  - Implement `tests/lighting/test_btab_synthetic.c`:
    - 3 fixtures:
      1. DS2-style (BigEndian=false, LongFormat=false, entrySize=0x1C)
      2. DS3-style (BigEndian=false, LongFormat=true, entrySize=0x28)
      3. UTF-16 names with Japanese characters (round-trip Shift-JIS-safe? No — BTAB strings are UTF-16 per upstream)
    - Round-trip: build → write → read → write → byte-compare
    - 1 negative: malformed header (wrong AssertInt32(1) value) returns SF_ERR_BAD_MAGIC

  **Must NOT do**:
  - Implement BigEndian path (return SF_ERR_UNSUPPORTED_VERSION if big_endian byte is non-zero — same policy as FLVER2)
  - Allocate per-entry name strings (use bulk pool)
  - Use stdio

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: 164 LOC upstream, only two layout switches, name-pool pattern is well-established

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Task 6, 8)
  - **Parallel Group**: Wave 2
  - **Blocks**: Tasks 14, 15
  - **Blocked By**: Tasks **0** (Wave-0 scope-lock gate), 1, 2

  **References**:
  - Upstream: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTAB.cs:9-162`
  - Pattern: `src/archive/bnd4.c` — name pool implementation (single bulk allocation for entry names)
  - Pattern: `src/archive/bnd3.c` — similar name pool pattern
  - Extensions.md: BND3/BND4 name pool documented as `+ extension`

  **Acceptance Criteria**:
  - [ ] Build clean
  - [ ] `ctest -L lighting -R btab_synthetic` → PASS
  - [ ] ASan PASS
  - [ ] `objdump | grep -cE 'sf_btab_'` ≥ 8
  - [ ] BigEndian refusal documented in `extensions.md` (T4 stub or T19 finalize)
  - [ ] Reserve/fill count matched

  **QA Scenarios**:
  ```
  Scenario: BTAB DS2 + DS3 + Japanese-name round-trip
    Tool: Bash
    Steps:
      1. ctest -L lighting -R btab_synthetic --output-on-failure
      2. Verify 3 sub-cases all pass
    Evidence: .sisyphus/evidence/lighting-task-7-btab.log

  Scenario: BigEndian refusal
    Tool: Bash
    Steps:
      1. Craft synthetic BTAB with BigEndian byte = 1
      2. sf_btab_read_from_memory returns SF_ERR_UNSUPPORTED_VERSION
    Evidence: same log file
  ```

  **Commit**: YES
  - Message: `feat(lighting): implement BTAB read/write + synthetic test`
  - Files: `src/lighting/btab.c`, `tests/lighting/test_btab_synthetic.c`

- [x] 8. **Wave 2 — BTPB implementation + synthetic test (CONDITIONAL on Wave-0 probe)** [`unspecified-high`] — CANCELLED: Wave-0 probe found 0 BTPB files in any v1 game; BTPB dropped from batch per plan §97-99

  **CONDITIONAL**: This task ONLY proceeds if Wave-0 probe found at least 1 `.btpb` file in any v1 game. If probe evidence shows BTPB absent, this task is **CANCELLED** and:
  - `sf_btpb.h` is NOT created (Task 1 skips it)
  - `src/lighting/btpb.c` is NOT created (Task 2 stub is removed)
  - `legacy.md` keeps the BTPB row (it stays Tier B)
  - `format-btpb.md` is NOT created (Task 16 drops it)

  **If proceeding, What to do**:
  - Implement `src/lighting/btpb.c` (mirror upstream `BTPB.cs`, 414 LOC):
    - Internal `sf_btpb_t { sf_btpb_version_t version; sf_vec3_t unk1c, unk28; sf_btpb_group_t *groups; size_t group_count; const sf_allocator_t *alloc; }`
    - Internal `sf_btpb_group_t { char *name; int32_t flags08, unk10, unk14, unk18; float unk1c, unk20, unk24; sf_vec3_t unk28, unk34; sf_btpb_probe_t *probes; size_t probe_count; float unk48, unk4c, unk50; uint8_t unk94, unk95, unk96; }`
    - Internal `sf_btpb_probe_t { int16_t coefficients[12]; int16_t light_mask, unk1a; sf_vec3_t position; }`
    - Implement reader following BTPB.cs:45-80, including 4-way version auto-detect from (bigEndian, unk00, unk04, groupSize, probeSize)
    - **Refuse BigEndian** (DS2BE branch): if bigEndian byte != 0 → SF_ERR_UNSUPPORTED_VERSION (PS3/X360 deferred to v2)
    - Implement writer mirroring BTPB.cs:85-141 with matching reserve/fill
    - Bloodborne+ Probe Position + 0x20 zero pad (gated by `version >= SF_BTPB_VERSION_BLOODBORNE`)
    - DS3 Group tail (Unk48/4C/50/94/95/96 + 0x40 zero pad) gated by `version >= SF_BTPB_VERSION_DARK_SOULS_3`
  - Implement `tests/lighting/test_btpb_synthetic.c`:
    - Fixtures: Bloodborne LE (0x48/0x48 sizes), DS3 LE (0x98/0x48 sizes)
    - Round-trip both
    - DS2 BE refused (negative test)
    - Empty groups + empty probes edge cases

  **Must NOT do**:
  - Invent Sekiro/ER/AC6 enum values (upstream stops at DS3; if v1 games use BTPB, they use DS3 wire format)
  - Implement BE path
  - Allocate probes one-at-a-time per Group (per-group bulk array)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 414 LOC upstream, 4-way version auto-detect, nested Group + Probe with version-gated fields

  **Parallelization**:
  - **Can Run In Parallel**: YES (with Tasks 6, 7)
  - **Parallel Group**: Wave 2
  - **Blocks**: Tasks 14, 16 (if proceeding)
  - **Blocked By**: Tasks 0 (scope-lock), 1, 2

  **References**:
  - Upstream: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTPB.cs:1-414`
  - Pattern: `src/geom/flver2.c` — version-gated section reads + writes
  - Pattern: `src/archive/bnd4.c` — multi-section reserve/fill
  - Decision evidence: `.sisyphus/evidence/lighting-probe.md` row for BTPB

  **Acceptance Criteria**:
  - [ ] If proceeding: build clean, `ctest -R btpb_synthetic` PASS, ASan PASS
  - [ ] If skipped: zero BTPB references in `src/lighting/`, `sf_btpb.h` absent, `legacy.md` retains BTPB row
  - [ ] `objdump | grep -cE 'sf_btpb_'` ≥ 8 (only if proceeding)

  **QA Scenarios**:
  ```
  Scenario (CONDITIONAL): BTPB Bloodborne + DS3 round-trip
    Tool: Bash
    Preconditions: Wave-0 probe positive for BTPB
    Steps:
      1. Verify .sisyphus/evidence/lighting-probe.md has BTPB row with count ≥ 1
      2. ctest -L lighting -R btpb_synthetic --output-on-failure → PASS
    Evidence: .sisyphus/evidence/lighting-task-8-btpb.log

  Scenario (CONDITIONAL): BTPB scope-skip path
    Tool: Bash
    Preconditions: Wave-0 probe negative for BTPB
    Steps:
      1. grep "LIGHTING-PROBE: BTPB NOT PRESENT" .sisyphus/evidence/lighting-probe.md → match
      2. test -e include/souls_formats/sf_btpb.h → absent
      3. test -e src/lighting/btpb.c → absent
      4. grep -c '^| BTPB ' docs/api-mapping/legacy.md → 1 (row retained)
    Evidence: .sisyphus/evidence/lighting-task-8-btpb-skipped.log
  ```

  **Commit**: YES (if proceeding) or no-commit task no-op note
  - Message: `feat(lighting): implement BTPB read/write + synthetic test` OR `chore(plan): BTPB dropped — not present in v1 games per Wave-0 probe`
  - Files: `src/lighting/btpb.c`, `tests/lighting/test_btpb_synthetic.c` OR none

- [x] 9. **Wave 3 — BTL implementation + synthetic test (V16 + Wave-0-confirmed versions)** [`unspecified-high`]

  **What to do**:
  - Implement `src/lighting/btl.c` (mirror upstream `BTL.cs`, 533 LOC):
    - Internal `sf_btl_t { int32_t version; bool long_offsets; sf_btl_light_t *lights; size_t light_count; const sf_allocator_t *alloc; char *name_pool; }`
    - Internal `sf_btl_light_t` with **all 47+ upstream Light fields** (Unk00 byte[16], Name (UTF-16), Type, DiffuseColor/Power/CastShadows/SpecularColor/SpecularPower, ConeAngle, Unk30/34, Position/Rotation, Unk50/54, Radius, Unk5C, Unk64 byte[4], Unk68, ShadowColor, Unk70, Flicker*, Unk80, Unk84 byte[16], Unk88/90/98, NearClip, UnkA0 byte[4], Sharpness, UnkAC, Width, UnkBC, UnkC0 byte[4], UnkC4; **Sekiro+ tail (version >= 16)**: UnkC8/CC/D0/D4/D8 floats, UnkDC int, UnkE0 float, UnkE4 int)
    - Implement reader:
      - AssertInt32(2), Version ∈ {1,2,5,6,16,18}, lightCount, ReserveInt32 namesLength, AssertInt32(0), lightSize ∈ {0xC0, 0xC8, 0xE8}, AssertPattern(0x24, 0)
      - Derive LongOffsets from lightSize
      - Skip names; read lights; per-light step-in with version-gated tail
    - Implement writer mirroring upstream Write() at BTL.cs:64-91 + Light.Write at 452-508
    - Helpers: `ReadRGB() / WriteRGB()` mirror at BTL.cs:519-530
    - Single name pool for UTF-16 light names
  - Implement `tests/lighting/test_btl_synthetic.c`:
    - **Mandatory fixtures**: V16 (Sekiro) Light at size 0xE8
    - **Conditional fixture**: V18 (if Wave-0 probe confirmed) at size 0xE8 (likely same layout as V16)
    - **Optional fixtures**: V6 (DS3) at size 0xC8, V2 (BB) at size 0xC0 — for completeness, may be skipped if time-pressed
    - All round-trip byte-equal
    - Per-LightType (Point/Spot/Directional) sub-case for each version
    - Zero-lights edge case
    - Light with all RGB endpoints saturated (255,255,255) and zero (0,0,0)
    - Light with Sekiro+ tail fields (UnkC8..UnkE4) non-zero

  **Must NOT do**:
  - Hard-code v1 game-version values inside BTL struct — version is data-driven from `Version` field
  - Allocate Light array per-element
  - Add new enum values to LightType beyond upstream (Point/Spot/Directional)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 533 LOC upstream, 47+ Light fields requiring careful per-field mirroring, version-gated tail

  **Parallelization**:
  - **Can Run In Parallel**: NO (large standalone task, occupies one agent fully)
  - **Parallel Group**: Wave 3 (alone)
  - **Blocks**: Tasks 14, 16
  - **Blocked By**: Tasks 0 (V18 confirmation), 1, 2; depends ordering on Wave 2 only for reviewer-cycle availability

  **References**:
  - Upstream: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BTL.cs:1-533`
  - Pattern: `src/effects/fxr3.c` — large POD struct with many fields (FXR3 has similar density)
  - Pattern: `src/effects/tae.c` — version-switch reader
  - Evidence: `.sisyphus/evidence/lighting-probe.md` BTL version histogram
  - sf_math: `sf_vec3_t` already 12-byte aligned; `sf_color_t` byte-packed for RGB

  **WHY Each Reference Matters**:
  - 47 fields is the largest single struct in lighting batch — FXR3/TAE patterns show how to organize per-field reads without spaghetti
  - sf_color_t format MUST match upstream RGB packing (3 bytes + 1 reserved = 4-byte alignment)

  **Acceptance Criteria**:
  - [ ] Build clean
  - [ ] `ctest -L lighting -R btl_synthetic` → all sub-cases PASS
  - [ ] ASan PASS
  - [ ] `objdump | grep -cE 'sf_btl_'` ≥ 10 (read/write/destroy/count/get + LightType enum accessors + per-field accessors)
  - [ ] Reserve/fill paired
  - [ ] Round-trip byte-equal for at least V16 fixture
  - [ ] Per-version sub-case all pass

  **QA Scenarios**:
  ```
  Scenario: BTL V16 (Sekiro) full round-trip with mixed light types
    Tool: Bash
    Steps:
      1. ctest -L lighting -R btl_synthetic --output-on-failure
      2. Verify "test_btl_synthetic .... Passed" + sub-case count matches version-fixture count
    Evidence: .sisyphus/evidence/lighting-task-9-btl-v16.log

  Scenario: BTL V18 (if Wave-0 confirmed)
    Tool: Bash
    Preconditions: lighting-probe-scope.md says "BTL V18 confirmed"
    Steps:
      1. Verify test_btl_synthetic.c has explicit V18 fixture (grep)
      2. ctest passes V18 sub-case
    Evidence: .sisyphus/evidence/lighting-task-9-btl-v18.log

  Scenario: All 3 LightTypes (Point/Spot/Directional)
    Tool: Bash
    Steps:
      1. Test asserts each LightType.Type enum value can be set on a Light and round-trips
    Evidence: same log file, sub-case markers
  ```

  **Commit**: YES
  - Message: `feat(lighting): implement BTL read/write + synthetic test (V16 + V18)`
  - Files: `src/lighting/btl.c`, `tests/lighting/test_btl_synthetic.c`

- [x] 10. **Wave 4 — GPARAM data model + scaffolding** [`unspecified-high`]

  **What to do**:
  - Implement `src/lighting/gparam.c` SHELL (data model + lifecycle only — no read/write yet):
    - Concrete internal structs from `gparam_internal.h` (T3) — mirroring upstream `GPARAM.cs` 1-to-1 (NO Group layer; top-level is `params` array per `GPARAM.cs:36`)
    - `sf_gparam_destroy` walks params → fields → values, then unk_param_extras → ids, then frees data30, then frees the per-gparam name pool (single `sf_xfree`)
    - Accessors per T1 API list: `_get_version`, `_get_unk0d`, `_get_count14`, `_get_unk40`, `_get_unk50`, `_get_data30`, `_param_count`, `_get_param`, `_param_get_key`, `_param_get_name`, `_param_field_count`, `_param_get_field`, `_param_comment_count`, `_param_get_comment`, `_field_get_key/name/type`, `_field_value_count`, `_field_get_value` (returns `sf_gparam_value_t` POD by value), `_unk_param_extra_count`, `_get_unk_param_extra`
    - 16-arm switch for `_field_get_value` that produces correct POD based on `field->type` (values 1..16) — central function with `_Static_assert` table coverage; `default:` returns `SF_ERR_INVALID_ARG`
    - String borrowing: field/value/param names are owned by a per-gparam **name pool** (single bulk `sf_xalloc` allocation, same pattern as existing BND3/BND4 entry-name pools — NOT a re-allocating arena allocator); values' `as_string` pointers borrow into the same pool; pool freed in `sf_gparam_destroy` via a single `sf_xfree`
    - Test stub `tests/lighting/test_gparam_synthetic.c` for verifying lifecycle: alloc fresh `sf_gparam_t` via internal API (not from read) → query → destroy → ASan clean
  - Add upstream-attribution comments per public accessor
  - Verify `_field_get_value` covers all 16 FieldType variants 1..16 (use `_Static_assert(SF_GPARAM_FIELD_TYPE_STRING == 16)` as drift guard; switch must have all 16 cases — `default:` returns `SF_ERR_INVALID_ARG`)

  **Must NOT do**:
  - Implement read/write paths in this task (T11/T12 jobs)
  - Allocate value strings individually — use the per-gparam **name pool** (single bulk `sf_xalloc` allocation following the BND3/BND4 name-pool pattern; NOT an arena-style allocator)
  - Expose internal struct layout in `sf_gparam.h`
  - Introduce any FXR3-`xml_arena`-style allocator infrastructure

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Data model decisions cascade; allocator+lifecycle correctness is foundational

  **Parallelization**:
  - **Can Run In Parallel**: NO (foundation for T11/T12)
  - **Parallel Group**: Wave 4 (alone, before T11/T12/T13 split)
  - **Blocks**: T11, T12, T13
  - **Blocked By**: T**0** (Wave-0 scope-lock gate), T1, T2, T3

  **References**:
  - Upstream: `GPARAM.cs:6-30` (FieldType), `:220-300` (IField/Field<T>/FieldValue<T> hierarchy), `:980-1100` (per-typed-field Read/Write)
  - Pattern: `src/effects/fxr3.c:sf_fxr3_destroy` — recursive walk + lifecycle teardown (NOTE: FXR3's `xml_arena` field is a v0.4.1 optimization explicitly excluded for v0.5.0; consult FXR3 for tree-walk pattern only, NOT its arena allocator)
  - Pattern: `src/archive/bnd3.c` + `src/archive/bnd4.c` — the canonical **name pool** pattern (single bulk `sf_xalloc` for entry-name strings, freed wholesale in destroy). This is the precedent GPARAM follows for its internal strings.
  - Pattern: `src/param/param.c` — row-data bulk allocation pattern (for hinting at bulk allocation strategy for `Param[]` arrays); not used for strings

  **Acceptance Criteria**:
  - [ ] Build clean
  - [ ] `ctest -L lighting -R gparam_synthetic` (lifecycle test only at this stage) → PASS
  - [ ] ASan PASS on lifecycle test
  - [ ] `_Static_assert` enforcing 16 FieldType coverage compiles
  - [ ] `objdump | grep -cE 'sf_gparam_'` ≥ 12 (the accessor set)

  **QA Scenarios**:
  ```
  Scenario: GPARAM data-model lifecycle (alloc → walk → destroy)
    Tool: Bash
    Steps:
      1. ctest -L lighting -R gparam_synthetic_lifecycle --output-on-failure
      2. ASan clean
    Evidence: .sisyphus/evidence/lighting-task-10-gparam-lifecycle.log
  ```

  **Commit**: YES
  - Message: `feat(lighting): GPARAM data model + Param/Field scaffolding (no Group layer per upstream)`
  - Files: `src/lighting/gparam.c` (initial shell), `src/lighting/gparam_internal.h` (struct finalization)

- [x] 11. **Wave 4 — GPARAM reader (V5/V6 mandatory + V3 optional + V2 unsupported; UTF-16LE "filt" signature)** [`unspecified-high`]

  **What to do**:
  - Implement `sf_gparam_read_from_memory` in `src/lighting/gparam.c`:
    - Force LE
    - Read signature: 8 bytes UTF-16LE = `f\0i\0l\0t\0` (use `sf_binary_reader_assert_pattern` or 4× AssertByte)
    - Read version (uint32 → cast to `sf_gparam_version_t`).
      - **MANDATORY accept**: `V5` (Sekiro / ER / Nightreign) and `V6` (AC6) — these are the v1 target game versions per user decision
      - **OPTIONAL accept**: `V3` (Bloodborne+ legacy safety net — implement if it falls out cleanly from V5 code path; do NOT add separate scaffolding)
      - **REJECT**: `V2` (DS2-only) → return `SF_ERR_UNSUPPORTED_VERSION`. V2 is deferred to v2 legacy work.
      - The `sf_gparam_version_t` enum still mirrors upstream verbatim (V2/V3/V5/V6 all present) for API drift-guard purposes, but read/write only accepts V3 (optional) / V5 / V6.
    - Read header: byte 0, Unk0D bool, AssertInt16(0), `paramCount`, `Count14`, all section bases, optional Unk50 if V≥V5
    - Parse params section: per-param read of `key`, `name`, field-offsets
    - Parse fields section: per-field switch on FieldType to read typed values
      - **V<V6 field header**: `[type byte][capacity sbyte][unk int16=0]`
      - **V≥V6 field header**: `[capacity int16][type byte][unk byte]`
    - Parse values section: typed value reads following field-type-table sizes (`sfi_gparam_field_payload_size`)
    - Parse value IDs section (per-value `Id` + V≥V5 `Unk04`)
    - Parse `Data30` opaque blob (preserved as byte array on the gparam_t for round-trip)
    - Parse param extras: count + IDs + V≥V5 Unk0c
    - Parse comment offset tables + comment strings (UTF-16LE indirected)
    - All strings concatenated into a single per-gparam **name pool** (one bulk `sf_xalloc`, NOT a re-allocating arena allocator; mirrors existing BND3/BND4 entry-name pool pattern); values borrow pointers; pool ownership transferred to `sf_gparam_t` for `_destroy` cleanup
    - Add fuzz-safe bounds checking (no read past buffer)
  - Add `// Upstream: GPARAM.cs:Read()` at function head, plus per-section attribution comments citing line ranges

  **Must NOT do**:
  - Fall-through V5 → V6 silently — explicit separated switch arms
  - Use Shift-JIS for any GPARAM string (everything is UTF-16LE per upstream `Encoding.Unicode.GetString`)
  - Allocate value POD on heap

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Most complex reader in the batch; per-version branching + 16-arm field-type switch + multi-section offset table

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T12 writer once both have the data model)
  - **Parallel Group**: Wave 4 (with T12)
  - **Blocks**: T14, T17
  - **Blocked By**: T**0** (Wave-0 scope-lock gate), T10

  **References**:
  - Upstream: `GPARAM.cs:70-121` (Read main), `:600-1100` (per-typed-field readers)
  - Pattern: `src/effects/fxr3.c:sf_fxr3_read_from_memory` — multi-section reader with offset tables
  - Pattern: `src/param/param.c:sf_param_read_*` — version-switch reader
  - encoding: `src/core/encoding_win32.c` — `sf_encoding_utf16le_to_utf8` for string conversion

  **Acceptance Criteria**:
  - [ ] Build clean
  - [ ] V5 + V6 branches present with explicit `if/else if` (no fall-through)
  - [ ] V3 branch optional (acceptable if implemented; acceptable if returns SF_ERR_UNSUPPORTED_VERSION)
  - [ ] V2 path explicitly returns `SF_ERR_UNSUPPORTED_VERSION` (verified via test)
  - [ ] `_Static_assert` on field-type switch coverage
  - [ ] No `printf`/`fopen` in reader
  - [ ] ASan clean on lifecycle test
  - [ ] `grep -c 'GPARAM.cs:' src/lighting/gparam.c` ≥ 10 (attribution comments per section)

  **QA Scenarios**:
  ```
  Scenario: GPARAM reader compiles + ASan-clean stub-read
    Tool: Bash
    Preconditions: T10 lifecycle test passes
    Steps:
      1. cmake --build build-mingw → 0 warnings
      2. ASan build clean
    Evidence: .sisyphus/evidence/lighting-task-11-gparam-reader.log
  ```

  **Commit**: YES
  - Message: `feat(lighting): GPARAM reader V5/V6 mandatory, V3 optional, V2 unsupported`
  - Files: `src/lighting/gparam.c` (reader portion)

- [x] 12. **Wave 4 — GPARAM writer (V5/V6 mandatory + V3 optional + V2 unsupported; matching reserve/fill pairs)** [`unspecified-high`]

  **What to do**:
  - Implement `sf_gparam_write_to_buffer`:
    - Mirror `GPARAM.cs:Write()` (lines 123-218)
    - Force LE; write UTF-16LE "filt" signature
    - **Version policy** (matching reader, T11): accept V5 + V6 unconditionally; V3 optional (graceful write if reader handles it); V2 returns `SF_ERR_UNSUPPORTED_VERSION` (caller should not be able to construct a V2 file via the public API in v0.5.0)
    - Write header (version, byte 0, Unk0D, Int16(0), counts)
    - Reserve all section base pointers up front: ParamOffsets, Params, FieldOffsets, Fields, Values, ValueIds, Unk30, ParamExtras, ParamExtraIds, Unk40, ParamCommentsOffsets, CommentOffsets, Comments
    - Write Unk50 if V≥V5
    - Section-by-section: param-offsets (reserve per-param `ParamOffset[i]`), params (fill `ParamOffset[i]`, reserve `Param[i]FieldOffsetsOffset` + `Param[i]CommentOffsetsOffset`), field offsets, fields (V<V6 vs V≥V6 layout), values (per typed), value IDs, Data30 opaque blob, param extras + IDs, Unk40, comment offset tables, comment strings (UTF-16LE)
    - All reserved labels filled before `sf_binary_writer_finish`
    - Pad(4) at end of params, fields, values, Data30, and each comment string per upstream
  - Add upstream-attribution comment + line range
  - **Critical mirror check**: `diff <(grep -n 'version >=' gparam.c | grep 'reader_') <(grep -n 'version >=' gparam.c | grep 'writer_')` should show matching branch count and approximate matching version cutoffs

  **Must NOT do**:
  - Skip any Reserve_*/Fill_* pair
  - Fall-through V5 → V6 silently
  - Write strings as Shift-JIS

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 96 LOC upstream + per-typed-value writers; multi-section reserve/fill discipline; symmetry with reader

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T11 reader once data model lands)
  - **Parallel Group**: Wave 4 (with T11)
  - **Blocks**: T13 (test needs both reader+writer), T14, T17
  - **Blocked By**: T**0** (Wave-0 scope-lock gate), T10

  **References**:
  - Upstream: `GPARAM.cs:123-218` (Write main), `:600-1100` (per-typed-field writers)
  - Pattern: `src/effects/fxr3.c:sf_fxr3_write_to_buffer` — multi-section writer with reserve/fill
  - Convention: AGENTS.md §7 reserve/fill must be paired

  **Acceptance Criteria**:
  - [ ] Build clean
  - [ ] V5 + V6 branches mirror reader explicitly; V2 path returns `SF_ERR_UNSUPPORTED_VERSION` symmetric to reader
  - [ ] `grep -cE 'sf_binary_writer_reserve_' src/lighting/gparam.c` matches `grep -cE 'sf_binary_writer_fill_' src/lighting/gparam.c` modulo helper indirection
  - [ ] ASan clean
  - [ ] T13 round-trip passes once writer lands

  **QA Scenarios**:
  ```
  Scenario: GPARAM writer compile + reserve/fill audit
    Tool: Bash
    Steps:
      1. cmake --build build-mingw → 0 warnings
      2. Reserve/fill count matched (or any mismatch documented)
    Evidence: .sisyphus/evidence/lighting-task-12-gparam-writer.log
  ```

  **Commit**: YES
  - Message: `feat(lighting): GPARAM writer V5/V6 mandatory + V3 optional + reserve/fill paired`
  - Files: `src/lighting/gparam.c` (writer portion)

- [x] 13. **Wave 4 — GPARAM synthetic test (V5 + V6 mandatory, V3 optional)** [`unspecified-low`]

  **What to do**:
  - Implement `tests/lighting/test_gparam_synthetic.c`:
    - **MANDATORY V5 fixture (Sekiro/ER/NR)**: top-level GPARAM with 3 Params (e.g., named "DofParam", "FogParam", "SkyParam"), each Param having 4 Fields across typed variants (Int, Float, Vec3, Color), each Field having 2-3 typed values; comments present; UnkParamExtra present (NO Group layer — upstream `GPARAM.Params` is top-level per `GPARAM.cs:36`)
    - **MANDATORY V6 fixture (AC6)**: same structure with new V6 field-header layout (`[capacity16][type][unk8]`); verify byte-equal round-trip
    - **Optional V3 fixture (BB+)**: minimal sanity check that V<V5 path doesn't break (V<V5 lacks Unk50/Unk04/Unk0c)
    - **Each fixture round-trips**: build → write → read → write → byte-compare both writes (`TEST_ASSERT_EQUAL_MEMORY`)
    - **Cross-version negative**: read a V2 file → must return `SF_ERR_UNSUPPORTED_VERSION`
    - **All 16 FieldType variants 1..16**: at least one fixture must contain each FieldType (Sbyte..String) to verify the central switch handles all
    - Empty params list edge case
    - Zero values in a field edge case
    - Empty `Data30` blob edge case

  **Must NOT do**:
  - Test V2 (DS2-only, v2-deferred)
  - Test BE byte order
  - Use real game files (this is synthetic only; e2e in T14)

  **Recommended Agent Profile**:
  - **Category**: `unspecified-low`
    - Reason: Test boilerplate following established Unity patterns

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on T11+T12 both landing)
  - **Parallel Group**: Wave 4 final
  - **Blocks**: T14, T17
  - **Blocked By**: T**0** (Wave-0 scope-lock gate), T11, T12

  **References**:
  - Pattern: `tests/anim/test_fxr3_synthetic.c` — synth fixture pattern
  - Pattern: `tests/param/test_synthetic_roundtrip.c` — multi-version round-trip
  - Upstream: `GPARAM.cs:6-30` for the 16 FieldType variants the test must exercise

  **Acceptance Criteria**:
  - [ ] `ctest -L lighting -R gparam_synthetic --output-on-failure` PASS
  - [ ] ASan PASS
  - [ ] Test covers V5 + V6 + (optional V3); all 16 FieldType variants exercised
  - [ ] `grep -c 'TEST_ASSERT_EQUAL_MEMORY' tests/lighting/test_gparam_synthetic.c` ≥ 2 (V5 + V6 round-trips)

  **QA Scenarios**:
  ```
  Scenario: GPARAM V5 + V6 round-trip
    Tool: Bash
    Steps:
      1. ctest -L lighting -R gparam_synthetic --output-on-failure
      2. Verify both V5 and V6 sub-cases PASS
    Evidence: .sisyphus/evidence/lighting-task-13-gparam-synth.log

  Scenario: All 16 FieldType variants exercised
    Tool: Bash
    Steps:
      1. grep -c 'SF_GPARAM_FIELD_TYPE_' tests/lighting/test_gparam_synthetic.c → ≥ 16
    Evidence: same log
  ```

  **Commit**: YES
  - Message: `test(lighting): GPARAM synthetic V5 + V6 fixtures`
  - Files: `tests/lighting/test_gparam_synthetic.c`

- [x] 14. **Wave 5 — e2e tests for 5 formats with multi-game probe-skip** [`unspecified-high`]

  **What to do**:
  - Implement ONE e2e test per implemented format (4-5 files):
    - `tests/lighting/test_pmdcl_e2e.c`
    - `tests/lighting/test_btab_e2e.c`
    - `tests/lighting/test_btl_e2e.c`
    - `tests/lighting/test_btpb_e2e.c` (CONDITIONAL on Wave-0 BTPB presence)
    - `tests/lighting/test_gparam_e2e.c`
  - **Shard policy** (explicit per Momus): existing `<game>_test_helper.c` files (`er_test_helper.c:38-40` etc.) open ONLY the Data0 shard. For v0.5.0 these e2e tests use Data0-only via the existing helpers. **No new shard-aware helper APIs are added** in this batch (out of scope; would be a separate v0.5.1 task). The Wave-0 probe evidence file (`.sisyphus/evidence/lighting-probe.md`) records both Data0 AND non-Data0 hits; e2e tests consume Data0 paths only.
  - Each e2e test:
    - Uses existing `er_extract_from_data0` / `<game>_extract_from_data0` helpers — Data0 only
    - Iterates known/probed BHD5 paths from `.sisyphus/evidence/lighting-probe.md` filtered to Data0-only entries
    - For first found Data0 file per format: parse via `sf_<format>_read_from_memory`, re-serialize via `sf_<format>_write_to_buffer`, field-by-field equality check (NOT byte-equal — POLICY.md round-trip note: real game files may have FromSoft-side non-determinism)
    - If NO Data0 file found for a format (because probe found it only in Data1/2/3): print `SKIP: <format> only in non-Data0 shard; multi-shard e2e deferred to v0.5.1`, exit 0
    - If NO file found in any shard for a format: print `SKIP: no .<ext> files in any v1 game archive`, exit 0
    - **Anti-pattern**: do NOT silently pass with zero work — must either complete one Data0 round-trip OR emit explicit SKIP line with reason
    - Verify Oodle DLL availability before relying on DCX_KRAK; if absent, SKIP with informative message
  - **Acknowledged limitation**: if Wave-0 probe shows a format exists ONLY in Data1/Data2/Data3 (not in Data0), that format's e2e gracefully SKIPs in v0.5.0. This is acceptable per Metis directive "graceful skip with explicit reason is OK; silent no-op is forbidden".
  - Register all e2e tests in `tests/CMakeLists.txt` with both `lighting` and `e2e` labels
  - Verify each test compiles with `souls_formats_e2e_helpers` static lib linkage (no per-file `target_sources` duplication)

  **Must NOT do**:
  - Hardcode a single game archive path
  - 5×4=20 separate tests (per-format-per-game explosion)
  - Refactor `er_extract_from_data0()` or any helper signature
  - Add new SF_E2E_*_DIR macros (use existing)
  - Skip silently with no message
  - Modify game files

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: 5 e2e tests with multi-shard probe logic; cross-format integration sensibility needed

  **Parallelization**:
  - **Can Run In Parallel**: YES (5 sub-tasks for 5 e2e files; can be split if needed)
  - **Parallel Group**: Wave 5
  - **Blocks**: F3
  - **Blocked By**: T6, T7, T8(cond), T9, T11, T12, T13

  **References**:
  - Pattern: `tests/anim/test_fxr3_e2e_er.c` — ER e2e with `er_extract_from_data0`
  - Pattern: `tests/map/msbe/test_msbe_e2e_er.c` — multi-shard MSB extraction
  - Helper: `tests/e2e/er_test_helper.h` — public API
  - Evidence reference: `.sisyphus/evidence/lighting-probe.md` rows

  **Acceptance Criteria**:
  - [ ] `ctest -L lighting --output-on-failure` includes all 4-5 e2e tests
  - [ ] When game files available: at least 1 real-file round-trip per format completes with field-by-field equality
  - [ ] When game files absent: each e2e prints SKIP and exits 0
  - [ ] ASan clean across all e2e tests
  - [ ] `grep -E 'SF_E2E_.*_DIR' tests/lighting/test_*_e2e.c` shows reuse of existing macros only

  **QA Scenarios**:
  ```
  Scenario: PMDCL e2e against ER if file present
    Tool: Bash
    Preconditions: probe found PMDCL in ER + Oodle DLL present
    Steps:
      1. ctest -L lighting -R pmdcl_e2e --output-on-failure
      2. Test stdout contains "ROUND-TRIP OK for <bhd5-path>" or "SKIP: no .pmdcl files"
    Evidence: .sisyphus/evidence/lighting-task-14-pmdcl-e2e.log

  Scenario: All 4-5 e2e tests graceful skip when ER missing
    Tool: Bash
    Steps:
      1. Temporarily move /mnt/c/Games/ELDEN RING aside (or test by setting SF_E2E_ELDEN_RING_DIR=L"C:/nonexistent")
      2. Rebuild and ctest -L lighting -R '_e2e' --output-on-failure
      3. Every test exits 0 with SKIP line
    Evidence: .sisyphus/evidence/lighting-task-14-skip-behavior.log
  ```

  **Commit**: YES
  - Message: `test(lighting): e2e tests for 5 formats with multi-game probe-skip`
  - Files: 4-5 `tests/lighting/test_*_e2e.c`, `tests/CMakeLists.txt` (registrations)

- [x] 15. **Wave 5 — tier-A mapping docs: format-pmdcl.md + format-btab.md** [`writing`]

  **What to do**:
  - Create `docs/api-mapping/format-pmdcl.md`:
    - Header block: upstream file path + LOC, target version, applicable v1 games
    - "Upstream Class | Upstream File" intro
    - **Full row-level mapping table** with columns: Upstream Symbol | C Symbol | Type | Status | Phase | Notes
    - Row per: `PMDCL` class, `PMDCL.Decals`, each nested Decal field (XAngles/YAngles/ZAngles/Position/Unk3C/DecalParamID/Size1/Size2), `Read()`, `Write()`, ctor
    - Status: `✓ aligned` for all (this is a simple format)
    - Notes: `Phase: 0.5.0` for every row
  - Create `docs/api-mapping/format-btab.md`:
    - Same structure
    - Rows for BTAB class + Entry fields + Read/Write
    - Status: `✓ aligned` for read/write/entry accessors; `✗ deviation` for BE refusal (extension noted in extensions.md)
    - Document `+ extension`: name pool (bulk allocation)
  - Reference upstream line numbers with each row (POLICY format)

  **Must NOT do**:
  - Document anything that isn't actually implemented in src/lighting/
  - Backfill mapping docs for prior batches
  - Skip the Notes column

  **Recommended Agent Profile**:
  - **Category**: `writing`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T16, T17)
  - **Parallel Group**: Wave 5
  - **Blocks**: T18, F1
  - **Blocked By**: T6, T7

  **References**:
  - Pattern: `docs/api-mapping/format-fxr3.md` — recent tier-A mapping precedent
  - Pattern: `docs/api-mapping/format-tae.md` — tabular row format

  **Acceptance Criteria**:
  - [ ] Both files exist
  - [ ] Each file has ≥ 10 rows in the mapping table
  - [ ] All public C symbols (`grep SF_API include/souls_formats/sf_{pmdcl,btab}.h`) have a corresponding row
  - [ ] Status legend used correctly (`✓ aligned`, `✗ deviation`, `+ extension`)

  **QA Scenarios**:
  ```
  Scenario: Mapping docs structural validity
    Tool: Bash
    Steps:
      1. test -f docs/api-mapping/format-pmdcl.md && test -f docs/api-mapping/format-btab.md
      2. grep -c '^| \\|' docs/api-mapping/format-pmdcl.md (table-row count) ≥ 10
    Evidence: .sisyphus/evidence/lighting-task-15-mapping.log
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): tier-A docs for PMDCL + BTAB`
  - Files: 2 `.md` files

- [x] 16. **Wave 5 — tier-A mapping docs: format-btl.md + format-btpb.md (conditional)** [`writing`]

  **What to do**:
  - Create `docs/api-mapping/format-btl.md`:
    - All 47+ Light fields mapped
    - LightType enum mapped
    - Version field mapping with notes about V16=Sekiro, V18=ER/NR/AC6 (citing Wave-0 probe evidence)
    - Read/Write rows
    - `✗ deviation`: BE refusal
    - `+ extension`: name pool
  - Create `docs/api-mapping/format-btpb.md` (CONDITIONAL on T8 inclusion):
    - All BTPBVersion enum values (DarkSouls2LE/BE/Bloodborne/DarkSouls3) — note BE refusal as deviation
    - Group + Probe nested classes
    - V≥Bloodborne Probe Position tail
    - V≥DarkSouls3 Group tail
    - Note: "Sekiro/ER/AC6/NR use DarkSouls3 wire format per Wave-0 probe evidence" if applicable
  - If BTPB dropped, skip this part entirely and log it in T18 (legacy.md kept BTPB)

  **Must NOT do**:
  - Invent BTPB enum values beyond upstream
  - Document fields not actually implemented

  **Recommended Agent Profile**:
  - **Category**: `writing`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T15, T17)
  - **Parallel Group**: Wave 5
  - **Blocks**: T18, F1
  - **Blocked By**: T8 (conditional), T9

  **References**:
  - Pattern: `docs/api-mapping/format-flver2.md` — large field-set mapping precedent (133 lines, similar density to BTL)
  - Pattern: `docs/api-mapping/format-tae.md`

  **Acceptance Criteria**:
  - [ ] `format-btl.md` exists with ≥ 50 rows (covers 47+ Light fields + accessors + ctor + read/write)
  - [ ] `format-btpb.md` exists (if BTPB included) with ≥ 25 rows
  - [ ] Both reference upstream line ranges

  **QA Scenarios**:
  ```
  Scenario: BTL mapping doc coverage
    Tool: Bash
    Steps:
      1. wc -l docs/api-mapping/format-btl.md → ≥ 80
      2. grep -c '^| `' docs/api-mapping/format-btl.md → ≥ 50
    Evidence: .sisyphus/evidence/lighting-task-16-btl-mapping.log
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): tier-A docs for BTL + BTPB`
  - Files: 1-2 `.md` files

- [x] 17. **Wave 5 — tier-A mapping doc: format-gparam.md** [`writing`]

  **What to do**:
  - Create `docs/api-mapping/format-gparam.md` (LARGEST mapping doc in this batch):
    - GparamVersion enum: V2/V3/V5/V6 with game annotations
    - FieldType enum: all 16 variants
    - `IField` interface + 16 `Field<T>` concrete types — note these are NOT exposed as separate C types (only `sf_gparam_field_t` opaque + `sf_gparam_value_t` POD)
    - `IFieldValue` + `FieldValue<T>` — same treatment
    - Param (top-level under GPARAM.Params, per `GPARAM.cs:36` — **NO Group layer**) / UnkParamExtra / BaseOffsets / Comments / Data30 blob
    - Read/Write methods
    - `+ extension`: `sf_gparam_value_t` tagged-union POD (cite extensions.md)
    - `✗ deviation`: `_Skipped_` for any internal-only types not exposed (e.g., `BaseOffsets`)
    - Note: "GPARAM was previously misclassified as v2 in legacy.md; corrected in 0.5.0"
    - File extensions: `.fltparam` (Sekiro pre-V6) and `.gparam` (V5+ ER/NR/AC6) — both handled transparently

  **Must NOT do**:
  - Document 16 per-type field structs (they don't exist)
  - Pretend ApplyTemplate is implemented (it's not in upstream either)

  **Recommended Agent Profile**:
  - **Category**: `writing`

  **Parallelization**:
  - **Can Run In Parallel**: YES (with T15, T16)
  - **Parallel Group**: Wave 5
  - **Blocks**: T18, F1
  - **Blocked By**: T11, T12, T13

  **References**:
  - Pattern: `docs/api-mapping/format-fxr3.md` — polymorphic format mapping precedent
  - Upstream: full `GPARAM.cs` for line-cite-rich rows

  **Acceptance Criteria**:
  - [ ] `format-gparam.md` exists
  - [ ] All 16 FieldType variants documented
  - [ ] All 4 version values documented
  - [ ] `sf_gparam_value_t` POD design noted with link to extensions.md
  - [ ] Re-classification note present

  **QA Scenarios**:
  ```
  Scenario: GPARAM mapping doc structural completeness
    Tool: Bash
    Steps:
      1. wc -l docs/api-mapping/format-gparam.md → ≥ 100
      2. grep -cE 'SF_GPARAM_FIELD_TYPE_' docs/api-mapping/format-gparam.md → ≥ 16
      3. grep -cE 'SF_GPARAM_VERSION_' docs/api-mapping/format-gparam.md → ≥ 4
    Evidence: .sisyphus/evidence/lighting-task-17-gparam-mapping.log
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): tier-A doc for GPARAM`
  - Files: `docs/api-mapping/format-gparam.md`

- [x] 18. **Wave 5 — README tier-A list update + legacy.md row removal** [`quick`]

  **What to do**:
  - Edit `docs/api-mapping/README.md`:
    - Add new tier-A list entries under existing tier-A section:
      - `- [BTAB](format-btab.md)`
      - `- [BTL](format-btl.md)`
      - `- [BTPB](format-btpb.md)` (only if shipped)
      - `- [GPARAM](format-gparam.md)`
      - `- [PMDCL](format-pmdcl.md)`
  - Edit `docs/api-mapping/legacy.md`:
    - Remove rows for BTAB / BTL / BTPB (only if shipped) / GPARAM / PMDCL
    - Leave a brief CHANGELOG-style line at top: `> 2026-05-13 (v0.5.0): BTAB/BTL/PMDCL promoted to Tier A (Lighting section now empty); GPARAM moved from Legacy params section to Tier A; BTPB <kept|removed> per Wave-0 probe evidence`

  **Must NOT do**:
  - Touch rows for non-lighting formats
  - Backfill any missing prior tier-A links (FXR3 etc. are out of scope)

  **Recommended Agent Profile**:
  - **Category**: `quick` — small text edits

  **Parallelization**:
  - **Can Run In Parallel**: YES (after T15, T16, T17 land)
  - **Parallel Group**: Wave 5
  - **Blocks**: F1, T19, T20
  - **Blocked By**: T15, T16, T17

  **References**:
  - `docs/api-mapping/README.md:7-48` — existing tier-A list
  - `docs/api-mapping/legacy.md` — current row layout

  **Acceptance Criteria**:
  - [ ] `grep -cE '\[format-(btab|btl|btpb|gparam|pmdcl)\]' docs/api-mapping/README.md` ≥ 4 (4 if BTPB dropped, 5 otherwise)
  - [ ] `grep -cE '^\| (BTAB|BTL|GPARAM|PMDCL) ' docs/api-mapping/legacy.md` == 0
  - [ ] If BTPB included: `grep -cE '^\| BTPB ' docs/api-mapping/legacy.md` == 0; if dropped: == 1 (preserved)

  **QA Scenarios**:
  ```
  Scenario: README tier-A and legacy.md consistency
    Tool: Bash
    Steps:
      1. grep -cE '\[format-(btab|btl|btpb|gparam|pmdcl)\]' docs/api-mapping/README.md → 4 or 5
      2. grep -cE '^\| (BTAB|BTL|GPARAM|PMDCL) ' docs/api-mapping/legacy.md → 0
    Evidence: .sisyphus/evidence/lighting-task-18-readme-legacy.log
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): README tier-A list update + legacy.md row removal`
  - Files: `docs/api-mapping/README.md`, `docs/api-mapping/legacy.md`

- [x] 19. **Wave 5 — CHANGELOG ## [0.5.0] block + extensions.md lighting finalize** [`writing`]

  **What to do**:
  - Edit `CHANGELOG.md`:
    - Add new top block:
      ```
      ## [0.5.0] - 2026-05-13

      ### Added
      - Lighting format support: BTAB, BTL, BTPB (if shipped), GPARAM, PMDCL — full read+write
      - 4-5 new public headers in `include/souls_formats/`
      - New `src/lighting/` source directory
      - `tests/probes/probe_lighting_files.c` (gated by `SF_BUILD_PROBES`)
      - 5 tier-A mapping docs under `docs/api-mapping/`

      ### Notes / corrections
      - GPARAM was previously misclassified as "Legacy params, v2" in legacy.md; corrected here. GPARAM is used by all v1 target games (Sekiro V5, ER V5, Nightreign V5, AC6 V6).
      - BTAB/BTL/PMDCL were classified as "Lighting, v2" — corrected to v1 since v1 target games use them.
      - BTPB: <included as V≥DarkSouls3 wire-equivalent based on Wave-0 probe evidence | dropped from batch as Wave-0 probe found zero v1 files; remains Tier B in legacy.md for v2 work>
      - Public API: `sf_gparam_value_t` tagged-union POD documented as a C-style adaptation in `extensions.md` (mirrors FXR3 precedent)
      - BigEndian byte order is refused (SF_ERR_UNSUPPORTED_VERSION) on BTAB and BTPB read paths — same policy as FLVER2

      ### v1 closure
      - This is the LAST post-v1 batch. All 10 next-batch plans now complete:
        9 batches landed previously (TAE Templates / Effects Misc / AC Specific / Legacy Binder / Legacy FLVER / Legacy MSB / Navmesh / Text-Script Misc / Uncategorized Deferred); Lighting closes the set in v0.5.0.
      ```
  - Edit `docs/api-mapping/extensions.md`:
    - Replace the T4 stub `## Post-v1: Lighting` section with finalized rows:
      - `sf_gparam_value_t` tagged-union POD — full description with rationale (mirror FXR3) + impact + lifecycle: stable
      - BTAB/BTL/BTPB BigEndian refusal (functional divergence, mirror FLVER2 BE-refusal pattern)
      - BTPB: Sekiro/ER/AC6/NR mapped to upstream `DarkSouls3` enum value (no new enum extensions, per Wave-0 evidence) — OR — BTPB dropped from v1 entirely (deferred to v2)
      - GPARAM dual extension transparency: `.gparam` and `.fltparam` handled by same read/write API
      - GPARAM name pool (per-gparam single bulk `sf_xalloc` allocation for all internal strings, mirrors existing BND3/BND4 name pool pattern; **NOT** a re-allocating arena allocator) — internal helper, not exposed; rationale: thousands of strings per file
      - Per-format name pool (BTAB/BTL) — `+ extension`, mirrors BND3/BND4 pattern

  **Must NOT do**:
  - Add unrelated entries
  - Forget the corrections paragraph (the re-classification is non-obvious to future maintainers)

  **Recommended Agent Profile**:
  - **Category**: `writing`

  **Parallelization**:
  - **Can Run In Parallel**: YES (after T18 lands so legacy.md state is known)
  - **Parallel Group**: Wave 5
  - **Blocks**: F1, T20
  - **Blocked By**: T4, T5, T18

  **References**:
  - `CHANGELOG.md` existing 0.2.0 / 0.3.0 / 0.4.0 / 0.4.1 blocks
  - `docs/api-mapping/extensions.md` existing post-Phase-X sections

  **Acceptance Criteria**:
  - [ ] `grep -c '^## \[0.5.0\]' CHANGELOG.md` == 1
  - [ ] CHANGELOG includes the re-classification paragraph (corrections)
  - [ ] `grep -c 'sf_gparam_value_t' docs/api-mapping/extensions.md` ≥ 2 (stub + finalized row)
  - [ ] Extensions.md lighting section has ≥ 4 rows finalized

  **QA Scenarios**:
  ```
  Scenario: CHANGELOG entry presence + extensions finalization
    Tool: Bash
    Steps:
      1. grep -c '^## \[0.5.0\]' CHANGELOG.md → 1
      2. grep -A 20 'Post-v1: Lighting' docs/api-mapping/extensions.md → ≥ 4 sub-rows
    Evidence: .sisyphus/evidence/lighting-task-19-changelog.log
  ```

  **Commit**: YES
  - Message: `docs: CHANGELOG ## [0.5.0] block + extensions.md lighting finalize`
  - Files: `CHANGELOG.md`, `docs/api-mapping/extensions.md`

- [x] 20. **Wave 5 — post-v1.md update + PLAN.md §13 reflect v0.5.0 closure** [`writing`]

  **What to do**:
  - Edit `docs/roadmap/post-v1.md`:
    - Add new section near top: `## v0.5.0 — Lighting closure (LATEST)` with bullet list:
      - 5 (or 4) formats shipped: BTAB / BTL / BTPB (if) / GPARAM / PMDCL
      - All 10 post-v1 batches now complete
      - Reference to `next-batch-lighting.md` (mark as historical: "implemented in v0.5.0 — see CHANGELOG")
    - Update v1.1 / v2.0 / v3.0 section dates if relevant
  - Edit `.sisyphus/plans/PLAN.md` §13:
    - Add note: "v0.5.0 (Lighting) completed YYYY-MM-DD; v1.0 GA can now be scheduled as all post-v1 batches are done."
  - Update `AGENTS.md` "Current status" table row for lighting batch: change "🔄 in progress" → "✅ done — N/N PASS (date)"
  - Edit `.sisyphus/plans/next-batch-lighting.md`:
    - Add Completion section at end (mirror tae-templates plan's pattern):
      ```
      ## Completion

      **Completed: <date>**

      All acceptance criteria met. Lighting batch landed as v0.5.0:
      - Public headers shipped: <list>
      - Tests pass: <ctest count>
      - See CHANGELOG.md ## [0.5.0] and docs/api-mapping/extensions.md "Post-v1: Lighting" for details.
      ```

  **Must NOT do**:
  - Modify any other PLAN.md section
  - Touch v1 phase docs (Phase 0-7 status rows)
  - Add new v2/v3 plans (out of scope)

  **Recommended Agent Profile**:
  - **Category**: `writing`

  **Parallelization**:
  - **Can Run In Parallel**: YES (after T19 lands)
  - **Parallel Group**: Wave 5 final
  - **Blocks**: F1, F2, F3, F4
  - **Blocked By**: T18, T19

  **References**:
  - `docs/roadmap/post-v1.md` current state
  - `.sisyphus/plans/PLAN.md` §13
  - `.sisyphus/plans/next-batch-tae-templates.md:72-82` Completion section template

  **Acceptance Criteria**:
  - [ ] `grep -c 'v0.5.0' docs/roadmap/post-v1.md` ≥ 1
  - [ ] `grep -c '## Completion' .sisyphus/plans/next-batch-lighting.md` == 1
  - [ ] `grep -c 'lighting' AGENTS.md` ≥ 2 with "✅ done" marker

  **QA Scenarios**:
  ```
  Scenario: Roadmap + PLAN.md + AGENTS.md reflect closure
    Tool: Bash
    Steps:
      1. grep -c 'v0.5.0' docs/roadmap/post-v1.md → ≥ 1
      2. grep -c '✅ done' AGENTS.md (lighting row context) → 1
      3. grep -c '## Completion' .sisyphus/plans/next-batch-lighting.md → 1
    Evidence: .sisyphus/evidence/lighting-task-20-roadmap.log
  ```

  **Commit**: YES
  - Message: `docs(roadmap): post-v1.md reflect v0.5.0 closure + PLAN.md note`
  - Files: `docs/roadmap/post-v1.md`, `.sisyphus/plans/PLAN.md`, `AGENTS.md`, `.sisyphus/plans/next-batch-lighting.md`

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to
> user and get explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before
> marking work complete.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user
> feedback → fix → re-run → present again → wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read this plan end-to-end. For each "Must Have": verify implementation exists
  (read file, run command, check evidence). For each "Must NOT Have": search
  codebase for forbidden patterns — reject with file:line if found. Check evidence
  files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Specific checks:
  - `find src/lighting/ -name lighting_common.c` outputs nothing
  - `find include/souls_formats/ -name sf_lighting.h` outputs nothing
  - `grep -rE 'sf_gparam_sbyte_field_t|sf_gparam_short_field_t' include/` outputs nothing (no per-type field structs)
  - `find src/lighting/ -name '*.c' | xargs grep -lE 'fopen\(|fread\(|fwrite\(|fclose\('` outputs nothing
  - `grep -rE 'arena' src/lighting/` outputs nothing (no new arena-style allocator infrastructure for 0.5.0; single bulk `sf_xalloc` name pools using the BND3/BND4 terminology — `name_pool` — are permitted and expected for GPARAM/BTAB/BTL string storage)
  - `git diff --stat src/archive/ src/geom/ src/map/ src/effects/` shows no unrelated changes
  - probe evidence file `.sisyphus/evidence/lighting-probe.md` exists with format×game rows
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build-mingw` + `cmake --build build-asan` (zero warnings).
  Run `ctest --test-dir build-mingw -L lighting --output-on-failure`. Run
  `ctest --test-dir build-asan -L lighting --output-on-failure` (zero leaks/UB).
  Review every `src/lighting/*.c` file for: `as any`-style C casts, empty error
  paths, `printf` in prod code, commented-out blocks, unused `static` functions,
  AI-slop patterns (over-comment, redundant abstraction, generic names like
  `data`/`result`/`item`/`temp` in hot paths).
  Specific checks:
  - Every `sf_binary_writer_reserve_*` site has a paired `sf_binary_writer_fill_*` in same function
  - Every `sf_xalloc` site has a corresponding `sf_xfree` in cleanup or destroy path
  - Every public enum has trailing `_Static_assert` ensuring no value drift
  - Every public function has upstream-attribution comment
  Output: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | ASan [PASS/FAIL] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean state (`rm -rf build-mingw build-asan; cmake -B build-mingw ...
  ; cmake --build build-mingw`). Execute every QA scenario from every task —
  follow exact steps, capture evidence under `.sisyphus/evidence/final-qa/`.
  Test cross-task integration: synth + e2e together, ASan + Werror together.
  Test graceful skip: temporarily unset `SF_E2E_ELDEN_RING_DIR` and verify e2e
  tests SKIP cleanly with informative message. Test probe-skip: build with
  `-DSF_BUILD_PROBES=OFF` and confirm probe binary not in build tree.
  Output: `Scenarios [N/N pass] | Integration [N/N] | Skip-behavior [N tested] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (`git log --stat` /
  `git diff HEAD~N..HEAD`). Verify 1:1 — everything in spec was built (no
  missing), nothing beyond spec was built (no creep). Check "Must NOT do"
  compliance. Detect cross-task contamination: Task N touching Task M's files.
  Flag unaccounted changes. Verify no untouched-files-claim drift.
  Specific checks:
  - Files modified outside `{include/souls_formats/, src/lighting/, tests/lighting/, tests/probes/, docs/api-mapping/, CMakeLists.txt, CHANGELOG.md, AGENTS.md, README.md, docs/roadmap/post-v1.md, .sisyphus/plans/PLAN.md}` are listed and justified
  - No backfilled tier-A docs for non-lighting batches
  - No new files in `src/archive/`, `src/geom/`, `src/map/`, `src/effects/`, etc.
  - `legacy.md` removals match exactly the formats shipped (BTPB conditional)
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

> Atomic commits per task. Pre-commit hook (CMake `-Werror`) gates every commit.
> Group commits per wave at user okay time.

| Task | Commit type | Files | Pre-commit |
|---|---|---|---|
| 0 | `chore(tests): probe lighting file existence in v1 games` | `tests/probes/probe_lighting_files.c`, `tests/CMakeLists.txt` | `cmake --build build-mingw --target probe_lighting_files` |
| 1 | `feat(lighting): add public headers for BTAB/BTL/BTPB/GPARAM/PMDCL` | 5 `.h` files | `cmake --build build-mingw` |
| 2 | `build(lighting): register src/lighting/ + lighting test label` | `CMakeLists.txt`, `tests/CMakeLists.txt` | `cmake -B build-mingw && cmake --build build-mingw` |
| 3 | `feat(lighting): gparam_internal.h + sf_gparam_value_t POD` | `src/lighting/gparam_internal.h` | `cmake --build build-mingw` |
| 4 | `docs(api-mapping): extensions.md lighting stub + AGENTS.md status row` | `docs/api-mapping/extensions.md`, `AGENTS.md` | `grep -c 'Post-v1: Lighting' docs/api-mapping/extensions.md` |
| 5 | `build: bump VERSION 0.4.1 → 0.5.0 + README mention` | `CMakeLists.txt`, `README.md` | `grep -c 'VERSION 0.5.0' CMakeLists.txt` |
| 6 | `feat(lighting): implement PMDCL read/write + synthetic test` | `src/lighting/pmdcl.c`, `tests/lighting/test_pmdcl_synthetic.c` | `ctest -L lighting -R pmdcl_synthetic` |
| 7 | `feat(lighting): implement BTAB read/write + synthetic test` | `src/lighting/btab.c`, `tests/lighting/test_btab_synthetic.c` | `ctest -L lighting -R btab_synthetic` |
| 8 | `feat(lighting): implement BTPB read/write + synthetic test` (CONDITIONAL) | `src/lighting/btpb.c`, `tests/lighting/test_btpb_synthetic.c` | `ctest -L lighting -R btpb_synthetic` |
| 9 | `feat(lighting): implement BTL read/write + synthetic test (V16+V18)` | `src/lighting/btl.c`, `tests/lighting/test_btl_synthetic.c` | `ctest -L lighting -R btl_synthetic` |
| 10 | `feat(lighting): GPARAM data model + Group/Param/Field scaffolding` | `src/lighting/gparam.c` (model only), `src/lighting/gparam_internal.h` update | `cmake --build build-mingw` |
| 11 | `feat(lighting): GPARAM reader V5/V6 mandatory, V3 optional, V2 unsupported` | `src/lighting/gparam.c` (reader portion) | `cmake --build build-mingw` |
| 12 | `feat(lighting): GPARAM writer V5/V6 mandatory + reserve/fill paired` | `src/lighting/gparam.c` (writer portion) | `cmake --build build-mingw` |
| 13 | `test(lighting): GPARAM synthetic V5 + V6 fixtures` | `tests/lighting/test_gparam_synthetic.c` | `ctest -L lighting -R gparam_synthetic` |
| 14 | `test(lighting): e2e tests for 5 formats with multi-game probe-skip` | `tests/lighting/test_*_e2e.c` (5 files) | `ctest -L lighting -R e2e` |
| 15-17 | `docs(api-mapping): tier-A docs for BTAB/PMDCL/BTL/BTPB/GPARAM` | 5 mapping `.md` files | `for f in btab btl btpb gparam pmdcl; do test -f docs/api-mapping/format-$f.md; done` |
| 18 | `docs(api-mapping): README tier-A list update + legacy.md row removal` | `docs/api-mapping/README.md`, `docs/api-mapping/legacy.md` | grep counts above |
| 19 | `docs: CHANGELOG ## [0.5.0] block + extensions.md lighting finalize` | `CHANGELOG.md`, `docs/api-mapping/extensions.md` | `grep -c '## \[0.5.0\]' CHANGELOG.md` == 1 |
| 20 | `docs(roadmap): post-v1.md reflect v0.5.0 closure + PLAN.md note` | `docs/roadmap/post-v1.md`, `.sisyphus/plans/PLAN.md` | manual review by F1 |

---

## Success Criteria

### Verification Commands

```bash
# Clean build (Mingw + ASan)
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw 2>&1 | grep -cE "warning:|error:"  # → 0
cmake -B build-asan -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_SANITIZERS=ON
cmake --build build-asan 2>&1 | grep -cE "warning:|error:"  # → 0

# Tests
ctest --test-dir build-mingw -L lighting --output-on-failure  # → 100% pass, ≥ 8 binaries
ctest --test-dir build-asan -L lighting --output-on-failure   # → 100% pass

# Symbol exports
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | \
  grep -cE 'sf_(btab|btl|btpb|gparam|pmdcl)_'  # → ≥ 30

# Static asserts
grep -cE '_Static_assert' include/souls_formats/sf_{btab,btl,btpb,gparam,pmdcl}.h  # → ≥ 5

# No stdio in src/lighting/
grep -rE 'fopen\(|fread\(|fwrite\(|fclose\(' src/lighting/  # → empty

# Mapping docs exist
for f in btab btl btpb gparam pmdcl; do test -f docs/api-mapping/format-$f.md || echo MISSING $f; done  # → empty

# README tier-A updated
grep -cE '\[format-(btab|btl|btpb|gparam|pmdcl)\]' docs/api-mapping/README.md  # → ≥ 4

# legacy.md rows removed
grep -cE '^\| (BTAB|BTL|BTPB|GPARAM|PMDCL) ' docs/api-mapping/legacy.md  # → 0 (or 1 if BTPB dropped)

# Version bumps
grep -c '^## \[0.5.0\]' CHANGELOG.md  # → 1
grep -c 'VERSION 0.5.0' CMakeLists.txt  # → ≥ 1

# Probe evidence captured
test -f .sisyphus/evidence/lighting-probe.md  # → 0 (exists)
```

### Final Checklist

- [ ] All "Must Have" present
- [ ] All "Must NOT Have" absent (verified via grep/find commands above)
- [ ] All tests pass (synth + e2e + ASan)
- [ ] Probe evidence captured and reviewed
- [ ] Tier-A README updated
- [ ] legacy.md cleaned up
- [ ] CHANGELOG 0.5.0 entry present
- [ ] Version bumped 0.4.1 → 0.5.0
- [x] F1-F4 reviewers all return APPROVE
- [ ] User explicitly says "okay"
