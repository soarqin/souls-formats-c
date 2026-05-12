# souls-formats-c — Internal Refactor + CMake Test/Example Isolation + Upstream Gap Analysis

## TL;DR

> **Quick Summary**: Aggressive internal refactor of souls-formats-c (107 files, 34,921 LOC,
> 997 allocator calls) **preserving the public API byte-for-byte**. Three deliverable streams
> in one plan: (1) internal restructuring + perf optimizations, (2) MANDATORY CMake test/example
> isolation (`BUILD_TESTING=${PROJECT_IS_TOP_LEVEL}` + `SF_BUILD_PROBES=OFF`), (3) upstream gap
> analysis producing **10 per-cluster planning documents** (`next-batch-*.md`) for the next
> batch of dev work.
>
> **Deliverables**:
> - Public API (33 headers, ~600+ `SF_API` symbols) byte-identical exports verified via `objdump`
> - Tests reorganized into `tests/` subdirectory tree (unit / e2e / probe / golden) behind `BUILD_TESTING`
> - Examples reorganized into `examples/` behind `SF_BUILD_EXAMPLES` AND `PROJECT_IS_TOP_LEVEL`
> - Probes (debug aids) gated behind new `SF_BUILD_PROBES=OFF` default
> - e2e test helper sources consolidated into a single static lib target (eliminate ~30+ duplicated `target_sources(... er_test_helper.c)` lines)
> - CTest labels unified: `e2e` umbrella + `e2e_<game>` per-game
> - `sf_sl2.h` added to `SF_PUBLIC_HEADERS` (pre-existing bug fix)
> - `examples/CMakeLists.txt` target alias unified
> - `src/script/emevd_internal.h` audited (relocate only if cross-module use found)
> - `SF_ENABLE_PHASE7` dead reference cleaned from CHANGELOG
> - DCX-wrap helper (`sf_get_decompressed_reader`) adoption across the 7 caller sites where signatures match
> - Per-entry allocation reduction on BND3/BND4/BXF3/BXF4/BHD5 read paths (target ≥30% reduction in steady-state alloc count)
> - klib khash adoption in BHD5 entry lookup (only if Wave 0 audits clear it)
> - PARAMDEF apply-path field-layout precompute (only if reuse pattern justifies)
> - binary_reader endian inline fast-path (only if profiling shows ≥5% win)
> - MSB shared scaffolding extraction (skeleton only — offset table + count + alloc + index backfill — strictly NOT per-subtype field reads)
> - 10 `next-batch-*.md` cluster planning documents covering all ~50 unimplemented upstream formats
> - CHANGELOG entries per wave; version bump 0.4.0 → 0.4.1 (PATCH; pure-internal, zero ABI break)
>
> **Estimated Effort**: Large (5-7 wall-days for a single agent; ~3 days with maximum parallelism per wave)
> **Parallel Execution**: YES — 8 waves total (Wave 0 pre-flight + Waves 1-6 work + Wave 7 final review)
> **Critical Path**: W0 audits → W1 baselines/CMake → W2-W5 parallel-where-possible → W6 gap analysis → W7 final verification → user okay

---

## Context

### Original Request

> 第一批 Phase 0-7 已经全部完成，请审查整个仓库的代码并进行合理的重构，如果可能的话优化内部结构和运行效率，
> 但保留 library 外部接口不变。
>
> 必须优化的部分：test 要放进单独的 CMake testing 里，如果有 example 也要一样的方式处理。
>
> 完成之后，和 C# 的接口比对，分析还没实现的部分，规划下一批开发任务。

**Translation**: Phases 0-7 of souls-formats-c are all complete. Review the entire repo and perform a
reasonable refactoring; optimize internal structure and runtime efficiency where possible, but the
library's external interface must remain unchanged. **MANDATORY: tests must be moved into a separate
CMake testing setup; same for examples if any exist.** After the refactoring is complete, compare
against the C# upstream interface, analyze unimplemented parts, and plan the next batch of dev work.

### Interview Summary

**Key user decisions**:
- **Refactor depth = AGGRESSIVE**: includes klib khash for BHD5, binary_reader endian-specialization, PARAMDEF precompute, MSB shared engine extraction.
- **Test cadence = per-wave green**: `ctest -L core -L compression -L crypto -L archive -L param -L script -L map -L geom -L anim` 100% pass after every wave, plus symbol export baseline check via `objdump`.
- **CMake default = library-only**: `BUILD_TESTING=${PROJECT_IS_TOP_LEVEL}` (preserves standalone dev loop, isolates consumers); `SF_BUILD_EXAMPLES=OFF`; legacy `SF_BUILD_TESTS` aliased.
- **Gap analysis output = per-cluster plans**: 9 user-named clusters + 1 catch-all `uncategorized-deferred`.

### Research Findings (direct repo survey — 2026-05-12)

**Architecture**:
- 107 source files, 34,921 LOC under `src/` across 11 module dirs
- 35 public headers, 5,204 LOC under `include/souls_formats/`
- `src/internal/sf_internal.h` (97 LOC) is the de-facto central private header — included in ~68 of 107 source files (`sf_xalloc/xfree/xrealloc`, `sf_bswap16/32/64`, `SF_CHECK_ARG/SF_RETURN_IF`, `sf_strdup`)
- 13 internal headers total; 9 are intentionally format-local (kept that way per Metis); only `src/script/emevd_internal.h` flagged for potential relocation **pending cross-module-use audit**
- Largest modules: `src/archive/` (7086 LOC, 14 files), `src/geom/` (5234 LOC), `src/map/` (5225 LOC), `src/param/` (4340 LOC), `src/effects/` (3839 LOC)
- MSB sub-modules: msbs (2726 LOC) > msbe (1150) > msbvi (985); **most divergence in `parts_param.c` (627/132/99) is per-subtype field count, NOT scaffolding duplication** — confirmed by Metis sub-module audit

**Allocation density** (997 total `sf_x{alloc,free,realloc,strdup}` calls):
- Top files: `fxr3_xml_read.c`(76), `fxr3.c`(46), `bnd3.c`(44), `bxf3.c`(43), `bxf4.c`(42), `bnd4.c`(41), `tae.c`(39), `bhd5.c`(37), `encoding_win32.c`(36), `esd.c`(31)
- Per-entry hot site confirmed: `src/archive/bnd4.c:366` and `:771` — `sf_strdup(b->alloc, headers[i].name_utf8)` inside loop

**Duplication signal**:
- 123 `SF_ERR_BAD_MAGIC` sites
- 249 `reserve_*` vs 239 `fill_*` writer pairs → **10-entry mismatch** requires Wave 0 audit (potential bugs)
- 812 `goto cleanup/fail/err/done/out` sites
- DCX decompression invoked from 7 files; `sf_util.c::sf_get_decompressed_reader()` already exists as the helper

**Test infrastructure**:
- `tests/CMakeLists.txt` is 484 lines; ~30 e2e tests duplicate the same 5-line `target_compile_definitions + target_sources + target_include_directories` boilerplate
- CTest labels are inconsistent — some e2e tests only have per-game labels, missing the `e2e` umbrella
- `SF_BUILD_TESTS=ON` is the current default (consumer-unfriendly per user's mandate)
- Two probe binaries (`probe_nightreign_msb`, `probe_matbin_paramtypes`) are `add_executable` orphans not registered as ctests, hardcoding game paths
- `tests/golden/capture_golden.c` already exists with `golden_hashes.h` — extending, not re-inventing

**Examples**:
- 2 binaries: `sf_bnd_extract.c`, `sf_param_dump.c`
- Alias inconsistency: one links explicit target, the other links the alias (cosmetic, both resolve to `souls_formats_static`)
- `SF_BUILD_EXAMPLES=OFF` default — good, but no `PROJECT_IS_TOP_LEVEL` gating

**Pre-existing bugs surfaced**:
- `include/souls_formats/sf_sl2.h` exists but is NOT in `SF_PUBLIC_HEADERS` list in CMakeLists.txt (install/packaging bug)
- `SF_ENABLE_PHASE7` referenced in CHANGELOG but not in CMakeLists.txt (documentation drift)
- 10-entry `reserve_*` vs `fill_*` count mismatch (possible writer bugs)
- CI explicitly passes `-DSF_BUILD_TESTS=ON` (will break when renamed without alias)

**Upstream gap (sizing for Wave 6)**:
- 413 .cs files; ~152K LOC upstream
- Our v1 coverage: DCX, BHD5, BND3/4, BXF3/4, ENFL, TPF, PARAM, PARAMDEF, PARAMTDF, FMG, EMEVD, ESD, MSBS/E/VI, FLVER2, MTD, MATBIN, TAE (SDT only), FXR3
- Unimplemented: MSB1/2/3/AC4/B/D/DR/FA/N/V/VD (10 of 13 MSB variants); BND/BND2; FLVER0; ~33 standalone formats (ACB, AIP, ANI, AcParts, BTAB, BTL, BTPB, CCM, CLM2, DRB, EDD, EDGE, EMELD, F2TR, FFXDLSE, FMB, FSDATA, FSLIBLZS, FXR1, GPARAM, GRASS, LUAGNL, LUAINFO, MCG, MCP, MLB, MQB, Morpheme, NGP, NVA, NVM, PMDCL, RMB, SMD4)

### Metis Review

**Identified gaps (now addressed in this plan)**:
- **CRITICAL**: User's "1M+ entries" claim for BHD5 is overstated (real ~25K). klib justification revised to "O(1) worst-case lookup regardless of dataset size" (still valid, but evidence-driven).
- **CRITICAL**: 79 upstream sites toggle `BigEndian` mid-stream — binary_reader **type-specialization** would break upstream alignment. Limit endian work to **inlined fast-path with branch hint only**.
- **CRITICAL**: MSB `parts_param.c` LOC divergence (627/132/99) is **mostly per-subtype fields**, NOT scaffolding duplication. Cap MSB shared engine to ~50-150 LOC skeleton per file.
- **CRITICAL**: Reserve/fill 10-entry mismatch could be **bugs** — must audit BEFORE extracting scaffold macro.
- **CRITICAL**: Wave 0 pre-flight audits added to gate Waves 2-5 (klib compile spike, reserve/fill audit, PARAMDEF reuse audit, endian-toggle audit, MSB scaffolding-vs-subtype LOC breakdown).
- **CRITICAL**: Wave 6 must include a **10th cluster** (`uncategorized-deferred`) covering GPARAM, MQB, DRB, ACB, CCM, RMB, FMB, FSDATA, FSLIBLZS, EMELD, F2TR, EDD, GRASS, Morpheme, AIP — formats that don't fit the user's original 9 clusters.
- Pre-existing bugs to fix: `sf_sl2.h` in `SF_PUBLIC_HEADERS`, `SF_ENABLE_PHASE7` CHANGELOG drift, examples target alias inconsistency, CI `.github/workflows/ci.yml` `SF_BUILD_TESTS` flag rename.
- Existing `tests/golden/capture_golden.c` infrastructure means Wave 1 captures fresh golden, not re-invents.
- Probe binaries must be gated behind new `SF_BUILD_PROBES=OFF` default in Wave 1.

---

## Work Objectives

### Core Objective

Refactor `souls-formats-c` (34,921 LOC) for cleaner internal structure and measurably better runtime
efficiency on hot paths, while preserving every public-API symbol byte-identically. Reorganize tests
and examples so they only build when the project is consumed as the top-level (or explicitly opted-in).
End with a per-cluster planning document set covering all unimplemented upstream C# formats so the
next batch of work can begin immediately.

### Concrete Deliverables

- **Top-level standalone** `cmake -B build && cmake --build build` (no flags) builds the library (static + shared) AND tests by default — preserving the AGENTS.md dev loop documented at `cmake -B build-mingw -G Ninja --toolchain ...`. Examples and probes remain OFF by default and require explicit opt-in.
- **Consumer via `add_subdirectory(souls-formats-c)`** NEVER builds tests, examples, or probes (because `PROJECT_IS_TOP_LEVEL` is FALSE in that scope), regardless of what the consumer's own `BUILD_TESTING` is set to.
- `cmake -B build -DBUILD_TESTING=OFF` (standalone) builds **ONLY the library** (static + shared).
- `cmake -B build -DSF_BUILD_TESTS=OFF` (legacy alias) emits a DEPRECATION message AND turns tests off; works during the transition window.
- `cmake -B build -DSF_BUILD_EXAMPLES=ON` (standalone only) ALSO builds the examples; in consumer scope the flag is honored only if PROJECT_IS_TOP_LEVEL is TRUE.
- `objdump -p build-mingw/libsouls_formats.dll | grep sf_` count and symbol set is **byte-identical**
  to a Wave 0 baseline (no removals; additions may occur per CHANGELOG).
- All wave-acceptance bash commands pass agent-executed; evidence files saved under `.sisyphus/evidence/`.
- Final 10 `.sisyphus/plans/next-batch-*.md` cluster planning files exist, each with `Upstream formats covered`, `Must Have`, `Must NOT Have`, `Dependencies on prior clusters`, `Acceptance criteria` (executable), and a STRICT-UPSTREAM-REFERENCE table.
- CHANGELOG.md has new `## [0.4.1]` block summarizing internal changes; `CMakeLists.txt` `VERSION` line is `0.4.1`.

### Definition of Done

- [ ] Build: `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw` succeeds, **zero warnings** (`-Werror` honored).
- [ ] Symbol stability: `diff <(sort .sisyphus/evidence/symbols-baseline.txt) <(x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | awk '/^\s*\[\s*[0-9]+\]\s+sf_/{print $NF}' | sort -u)` shows **zero removed** symbols.
- [ ] Tests: `ctest --test-dir build-mingw -L 'core|compression|crypto|archive|param|script|map|geom|anim|hygiene|golden' --output-on-failure` → 100% pass; ER e2e tests pass when Oodle+game-files present, gracefully skip otherwise (skip count unchanged vs baseline).
- [ ] Sanitizer: `cmake -B build-asan -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_ENABLE_SANITIZERS=ON && cmake --build build-asan && ctest --test-dir build-asan -L core --output-on-failure` → 100% pass.
- [ ] Library-only consumer build verified via a `/tmp/sf-consumer/CMakeLists.txt` `add_subdirectory` proof (no tests/examples/probes built).
- [ ] CHANGELOG.md has `## [0.4.1] - <date>` entry summarizing all internal changes per wave.
- [ ] `CMakeLists.txt` VERSION line is `0.4.1`.
- [ ] `ls .sisyphus/plans/next-batch-*.md | wc -l` returns **10**.
- [ ] Each `next-batch-*.md` passes the grep-based structural validator (Wave 6 acceptance).
- [ ] `docs/api-mapping/extensions.md` updated with rows for every new internal helper that has no upstream counterpart (klib khash adoption, MSB engine, PARAMDEF precompute if landed, etc.).
- [ ] Final Verification Wave: 4 named reviewer agents all return `Verdict: PASS`.
- [ ] User explicitly says "okay" to proceed; no F1-F4 task checked before that.

### Must Have

- Public API of include/souls_formats/*.h unchanged byte-for-byte at the declaration level.
- All current 95+ test binaries continue to build and pass under `BUILD_TESTING=ON`.
- `BUILD_TESTING` default = `${PROJECT_IS_TOP_LEVEL}` (i.e., ON for standalone dev builds, OFF when consumed as subdir).
- `SF_BUILD_EXAMPLES` and `SF_BUILD_PROBES` default OFF; only effective when project is top-level.
- `SF_BUILD_TESTS` aliased to `BUILD_TESTING` with deprecation warning to preserve CI invocations.
- `.github/workflows/ci.yml` updated to use new flag names (in same commit as the rename).
- `sf_sl2.h` listed in `SF_PUBLIC_HEADERS`.
- Examples link target name harmonized (both use `souls_formats_static`).
- CHANGELOG.md gets an `## [Unreleased]` line per wave; finalized as `## [0.4.1]` at the very end.
- Wave 0 audits gate Waves 4-5 risky optimizations (no go-ahead without evidence).
- Probe binaries gated behind `SF_BUILD_PROBES=OFF`.
- New internal-only helpers documented in `docs/api-mapping/extensions.md`.
- 10 `next-batch-*.md` cluster plans, each citing the upstream `.cs` files it covers.

### Must NOT Have (Guardrails — non-negotiable)

- **DO NOT** change any signature in `include/souls_formats/*.h`. Adding new `SF_API` symbols is allowed but rare in this plan; removals are forbidden.
- **DO NOT** remove or rename any existing `SF_API`-decorated exported symbol (Wave acceptance verifies via `objdump`).
- **DO NOT** type-specialize the binary_reader (no `sf_binary_reader_le_t` / `sf_binary_reader_be_t`). 79 upstream sites toggle endian mid-stream; the public `sf_binary_reader_set_big_endian()` must continue to work.
- **DO NOT** intern caller-returned strings (the public contract guarantees `sf_free(...)` works on every returned pointer).
- **DO NOT** adopt klib khash anywhere with <10K typical entries (BND, BXF, TPF, FMG, PARAM, MSB all excluded; **BHD5 only**, gated by Wave 0 audit).
- **DO NOT** touch per-subtype field reads in `src/map/msbs/parts_param.c` etc. Wave 5 limit = scaffolding only (≤150 LOC extracted per file).
- **DO NOT** extract the reserve/fill scaffold macro until the 10-entry mismatch is explained (and bugs fixed if any).
- **DO NOT** relocate `*_internal.h` files that are deliberately format-local (msbs_internal.h, tae_internal.h, etc.). Only `src/script/emevd_internal.h` is a candidate, and only if Wave 0 confirms cross-module use.
- **DO NOT** auto-delete `build-on/build-off/build-mingw` stale dirs (user-local state; only verify `.gitignore` covers `build*/`).
- **DO NOT** introduce new third-party dependencies (klib is already in CPM).
- **DO NOT** silence warnings; fix at source. `-Werror` stays.
- **DO NOT** use GNU-only extensions (C11 only).
- **DO NOT** run Wave 6 (gap analysis) before Waves 1-5 land (cluster plans must reference post-refactor code).
- **DO NOT** mark F1-F4 final-verification tasks complete before the user gives explicit "okay" — rejection → fix → re-run → re-present → wait.
- **DO NOT** vendor Oodle DLLs or copy any FromSoftware game bytes into the repo.
- **DO NOT** modify `docs/api-mapping/format-*.md` rows for internal-only refactors; only update `extensions.md` for new C-extension helpers.
- **DO NOT** change `SF_API` macro semantics; do not change `SF_BUILD_DLL` semantics.
- **DO NOT** bypass `sf_xalloc`/`sf_xfree` for any new heap allocation in the refactor (allocator hook respect is a hard rule).

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No "user manually checks" steps.

### Test Decision
- **Infrastructure exists**: YES (Unity via CPM; 95+ test binaries; `tests/golden/capture_golden.c` baseline; ASan build via `SF_ENABLE_SANITIZERS=ON`).
- **Automated tests**: TDD for net-new helpers (RED-GREEN-REFACTOR); tests-after for refactor of existing code (no behavior change → existing tests preserve correctness).
- **Framework**: Unity (ThrowTheSwitch).

### Agent-Executed QA Policy

Every task includes agent-executed QA scenarios. Evidence saved to `.sisyphus/evidence/task-{wave}-{slug}.{ext}`.

- **Library API/Binary tools**: `Bash` running `cmake --build`, `ctest`, `objdump`, `comm/diff`, `grep`.
- **No frontend/UI/CLI/TUI** in this plan (refactor-only).

### Wave-Acceptance Common Gates

After **every** wave (W1–W5), the agent runs these in order:
```bash
# 1. Build clean
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw 2>&1 | tee .sisyphus/evidence/wave-{N}-build.log
test ${PIPESTATUS[0]} -eq 0   # zero warnings (Werror)

# 2. Full test matrix
ctest --test-dir build-mingw -L 'core|compression|crypto|archive|param|script|map|geom|anim|hygiene|golden' --output-on-failure | tee .sisyphus/evidence/wave-{N}-ctest.log
test ${PIPESTATUS[0]} -eq 0

# 3. e2e skip-count unchanged
ctest --test-dir build-mingw -L 'e2e|e2e_er|e2e_ac6|e2e_sekiro|e2e_nightreign' -V --output-on-failure 2>&1 \
  | grep -cE 'SKIP:|gracefully skipping|TEST_IGNORE_MESSAGE' > /tmp/skip-now.txt
diff .sisyphus/evidence/skip-count-baseline.txt /tmp/skip-now.txt   # must match

# 4. Symbol export — zero removals
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll \
  | awk '/^\s*\[\s*[0-9]+\]\s+sf_/{print $NF}' | sort -u > /tmp/symbols-now.txt
comm -23 .sisyphus/evidence/symbols-baseline.txt /tmp/symbols-now.txt > /tmp/removed.txt
test ! -s /tmp/removed.txt

# 5. Sanitizer clean
cmake --build build-asan && ctest --test-dir build-asan -L core --output-on-failure

# 6. CHANGELOG entry exists
grep -q "Wave {N}" CHANGELOG.md
```

If any of (1)–(6) fails, the wave is REJECTED and must be fixed before next wave starts.

---

## Execution Strategy

### Parallel Execution Waves

> 8 waves total. Wave 0 is pre-flight audits (no refactoring), gates Waves 4-5. Waves 1-5 do the
> actual work. Wave 6 produces the gap-analysis cluster plans. Wave 7 is the final review wave.
> Each wave ends with the common acceptance gate above before the next begins.

```
Wave 0 — PRE-FLIGHT AUDITS (no source changes; produce evidence files):
├── T0.1 klib 3-toolchain compile spike (gates W4 BHD5 conversion)            [quick]
├── T0.2 Reserve/fill 10-entry mismatch audit (gates W2 scaffold macro)        [deep]
├── T0.3 PARAMDEF apply caller-pattern audit (gates W4 precompute)             [deep]
├── T0.4 binary_reader endian-toggle audit across our format readers (gates W4) [deep]
├── T0.5 MSB scaffolding-vs-subtype LOC breakdown per *_param.c (gates W5)     [deep]
├── T0.6 Per-entry vs per-archive alloc-site audit (gates W3)                  [unspecified-high]
└── T0.7 emevd_internal.h cross-module use audit (gates W1 relocation)         [quick]

Wave 1 — BASELINES + CMAKE REORG + CHEAP FIXES (8 parallel tasks):
├── T1.1 Capture symbol/test/skip/sanitizer baselines to .sisyphus/evidence/    [quick]
├── T1.2 BUILD_TESTING + PROJECT_IS_TOP_LEVEL + SF_BUILD_TESTS alias gating     [unspecified-high]
├── T1.3 Update .github/workflows/ci.yml to new flag names                       [quick]
├── T1.4 Add sf_sl2.h to SF_PUBLIC_HEADERS; harmonize examples target alias     [quick]
├── T1.5 Probe gating behind SF_BUILD_PROBES=OFF; CTest label unification        [unspecified-high]
├── T1.6 Fix SF_ENABLE_PHASE7 CHANGELOG drift; doc trail for stale build dirs    [quick]
├── T1.7 Apply emevd_internal.h relocation decision from T0.7                    [quick]
└── T1.8 Fix any reserve/fill bugs surfaced by T0.2 (if isolated to W1 scope)    [unspecified-high]

Wave 2 — DEDUP HELPERS (audited-safe only — gated by T0.2):
├── T2.1 Adopt sf_get_decompressed_reader at the 7 caller sites (BND3/4/BXF3/4/TPF) [unspecified-high]
├── T2.2 Magic-check helper macro (only if ≥80% sites are uniform after audit)      [deep]
├── T2.3 Reserve/fill scaffold macro (only if T0.2 cleared)                          [deep]
├── T2.4 e2e test helper consolidation into static lib target                        [unspecified-high]
└── T2.5 goto-cleanup pattern review (no global macro — only document the pattern)  [quick]

Wave 3 — PER-ENTRY ALLOC REDUCTION (audit-confirmed paths only — gated by T0.6):
├── T3.1 BND3/BND4 name-pool: bulk-alloc names array in single block                [deep]
├── T3.2 BXF3/BXF4 mirror BND optimization                                           [deep]
├── T3.3 BHD5 entry-list bulk alloc + name pool                                     [deep]
├── T3.4 FXR3 XML read path: scratch buffer pattern for transient strings           [unspecified-high]
└── T3.5 PARAM row data: contiguous arena per param (audited subset only)            [deep]

Wave 4 — ALGORITHMIC (each task gated by a Wave 0 audit):
├── T4.1 klib khash adoption in src/archive/bhd5.c entry lookup                     [deep]
├── T4.2 PARAMDEF apply-path field-layout precompute                                [deep]
└── T4.3 binary_reader endian inline fast-path (skip if profiling <5% win)          [ultrabrain]

Wave 5 — MSB SHARED SCAFFOLDING EXTRACTION (limited scope — gated by T0.5):
├── T5.1 Extract msb_entry_list_read / msb_entry_list_write into msb_common.c       [deep]
├── T5.2 Apply to msbs/msbe/msbvi (one PR each module, fully reversible)            [unspecified-high]
└── T5.3 Document chosen extraction technique in docs/api-mapping/POLICY.md or extensions.md [writing]

Wave 6 — UPSTREAM GAP ANALYSIS → 10 CLUSTER PLANS (parallel; after W1-W5 land):
├── T6.0 Enumerate upstream Formats/*.cs + their public surface inventory          [librarian]
├── T6.1 Write next-batch-legacy-binder.md (BND, BND2)                             [writing]
├── T6.2 Write next-batch-legacy-msb.md (MSB1/2/3/AC4/B/D/DR/FA/N/V/VD)            [writing]
├── T6.3 Write next-batch-legacy-flver.md (FLVER0 + console vertex formats)        [writing]
├── T6.4 Write next-batch-tae-templates.md (TAE Template; non-SDT TAE variants)    [writing]
├── T6.5 Write next-batch-lighting.md (BTAB, BTL, BTPB, GPARAM, PMDCL)             [writing]
├── T6.6 Write next-batch-navmesh.md (NVA, NVM, NGP, MCG, MCP, EDGE)               [writing]
├── T6.7 Write next-batch-text-script-misc.md (LUAGNL, LUAINFO, EMELD, FMB)        [writing]
├── T6.8 Write next-batch-effects-misc.md (FXR1, FFXDLSE, ANI, MQB, Morpheme)      [writing]
├── T6.9 Write next-batch-ac-specific.md (AcParts, MLB variants, FSDATA, FSLIBLZS) [writing]
└── T6.10 Write next-batch-uncategorized-deferred.md (DRB, ACB, CCM, RMB, GRASS, F2TR, EDD, AIP, SMD4, CLM2) [writing]

Wave FINAL — VERIFICATION (4 parallel reviewers; ALL must approve, then user okays):
├── F1 Plan compliance audit                            [oracle]
├── F2 Code quality + symbol stability + sanitizer       [unspecified-high]
├── F3 Real manual QA (run every wave's acceptance again from clean checkout) [unspecified-high]
└── F4 Scope fidelity check (no creep beyond plan)       [deep]
-> Present results -> Wait for explicit user "okay" -> finalize CHANGELOG ## [0.4.1]

Critical Path:
  W0 (gates) → W1.1+W1.2 (baselines + CMake) → W2/W3/W4/W5 (parallel where possible) → W6 → F1-F4 → user okay → finalize

Parallel-execution metrics:
  - Wave 0: 7 audits parallel (max 7 concurrent)
  - Wave 1: 8 tasks parallel (max 8 concurrent)
  - Wave 2: 5 tasks parallel (after W0.2 and W1.2 land)
  - Wave 3: 5 tasks parallel (after W0.6 lands)
  - Wave 4: 3 tasks parallel (after W0.1, W0.3, W0.4 land respectively)
  - Wave 5: 3 tasks sequential-with-batching (T5.1 → T5.2 fan-out → T5.3)
  - Wave 6: 11 tasks parallel (T6.0 leads, T6.1–T6.10 fan out)
  - Wave 7: 4 reviewers parallel
```

### Dependency Matrix (abbreviated; full per-task deps in TODO bodies)

| Wave-task | Depends on | Blocks |
|---|---|---|
| W0.1 (klib spike) | — | T4.1 |
| W0.2 (reserve/fill audit) | — | T2.3, T1.8 |
| W0.3 (paramdef caller audit) | — | T4.2 |
| W0.4 (endian-toggle audit) | — | T4.3 |
| W0.5 (MSB scaffold-vs-subtype LOC) | — | T5.1 |
| W0.6 (alloc-site audit) | — | T3.1–T3.5 |
| W0.7 (emevd internal use) | — | T1.7 |
| W1.1 (baselines) | — | all later W1/W2/W3/W4/W5 acceptance |
| W1.2 (BUILD_TESTING) | W1.1 | W1.3, W1.5 |
| W1.3 (ci.yml) | W1.2 | — |
| W1.4 (sf_sl2.h + alias) | — | — |
| W1.5 (probes/labels) | W1.2 | W6 (label-aware gap doc consistency) |
| W1.6 (CHANGELOG drift) | — | — |
| W1.7 (emevd reloc) | W0.7 | — |
| W1.8 (reserve/fill bugs) | W0.2 | T2.3 |
| W2.1 (DCX-wrap adoption) | W1.1 | — |
| W2.2 (magic macro) | W1.1 | — |
| W2.3 (reserve/fill scaffold) | W1.8 | — |
| W2.4 (e2e helper lib) | W1.5 | — |
| W2.5 (goto-cleanup) | W1.1 | — |
| W3.1–W3.5 (alloc reduction) | W0.6 | — |
| W4.1 (khash BHD5) | W0.1 | — |
| W4.2 (paramdef precompute) | W0.3 | — |
| W4.3 (endian fast-path) | W0.4 | — |
| W5.1 (msb_common engine) | W0.5 | T5.2 |
| W5.2 (msbs/e/vi apply) | T5.1 | T5.3 |
| W5.3 (POLICY doc) | T5.2 | — |
| W6.0 (upstream enumeration) | W1–W5 all complete | T6.1–T6.10 |
| W6.1–W6.10 (cluster plans) | T6.0 | — |
| F1–F4 (review) | W1–W6 all complete | user-okay |

### Agent Dispatch Summary

| Wave | Concurrent | Recommended agent profile per task |
|---|---:|---|
| W0 | 7 | T0.1 → `quick`; T0.2/T0.3/T0.4/T0.5 → `deep`; T0.6 → `unspecified-high`; T0.7 → `quick` |
| W1 | 8 | T1.1/T1.3/T1.4/T1.6/T1.7 → `quick`; T1.2/T1.5/T1.8 → `unspecified-high` |
| W2 | 5 | T2.1/T2.4 → `unspecified-high`; T2.2/T2.3 → `deep`; T2.5 → `quick` |
| W3 | 5 | T3.1/T3.2/T3.3/T3.5 → `deep`; T3.4 → `unspecified-high` |
| W4 | 3 | T4.1/T4.2 → `deep`; T4.3 → `ultrabrain` (perf microbench + decision) |
| W5 | 3 | T5.1/T5.2 → `deep`/`unspecified-high`; T5.3 → `writing` |
| W6 | 11 | T6.0 → `librarian`; T6.1–T6.10 → `writing` |
| W7 | 4 | F1 → `oracle`; F2 → `unspecified-high`; F3 → `unspecified-high`; F4 → `deep` |

---

## TODOs

### Wave 0 — PRE-FLIGHT AUDITS

> No source code changes in Wave 0. Each task produces an `.sisyphus/evidence/*.md` audit file that
> either GREEN-LIGHTS or RED-LIGHTS its downstream tasks. Run all 7 tasks in parallel.

- [ ] 0.1 **klib 3-toolchain compile spike**

  **What to do**:
  1. Create a throwaway `tests/spike/test_klib_compile.c` that instantiates `KHASH_MAP_INIT_INT64(bhd5_map, void*)` with a tiny put/get sequence.
  2. Add a one-off `add_executable(souls_formats_test_klib_spike ...)` in `tests/CMakeLists.txt` (will be removed in Wave 4 when proper klib usage lands).
  3. Build under all three toolchains: MinGW-w64 (`build-mingw`), MSVC (if `cl` is reachable), clang-cl (if reachable). For non-Windows host, skip MSVC/clang-cl and document SKIP with reason.
  4. Capture all compiler output (warnings, errors) verbatim to `.sisyphus/evidence/klib-toolchain-spike.md`.
  5. Verdict line at top of evidence file: `GO` (clean compile on all reachable toolchains) or `NO-GO` (with reason).

  **Must NOT do**:
  - Do NOT integrate klib into `src/archive/bhd5.c` yet — that's Wave 4.
  - Do NOT add `-Wno-...` to silence klib warnings; if it doesn't pass `-Werror /W4`, that's a NO-GO signal.

  **Recommended Agent Profile**:
  - **Category**: `quick` — single throwaway file, 3 builds, write evidence. Low cognitive load.
  - **Skills**: none required.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0 (with T0.2 through T0.7)
  - **Blocks**: T4.1 (klib khash adoption in BHD5)
  - **Blocked By**: None — can start immediately.

  **References**:
  - **Pattern**: `cmake/deps/klib.cmake` (existing CPM recipe, `DOWNLOAD_ONLY YES`); `cmake/deps/unity.cmake` (analogous third-party CPM recipe for the test runner).
  - **External**: klib README at https://github.com/attractivechaos/klib — specifically the `KHASH_MAP_INIT_*` macros and the basic `kh_put / kh_get / kh_value` cycle.
  - **WHY**: klib uses heavy macro expansion; MSVC `/W4 /WX` historically emits `C4127 conditional expression is constant` from klib internals. We need to know before Wave 4 whether klib survives our warning level intact.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/klib-toolchain-spike.md` exists with verdict line on line 1.
  - [ ] `souls_formats_test_klib_spike.exe` runs (under MinGW at minimum) and prints `KLIB-SPIKE OK`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: klib compiles under MinGW with -Werror
    Tool: Bash (cmake + ctest)
    Preconditions: cmake/deps/klib.cmake includes klib in include path; spike file added.
    Steps:
      1. cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build-mingw --target souls_formats_test_klib_spike 2>&1 | tee /tmp/spike-build.log
      3. grep -cE 'warning:|error:' /tmp/spike-build.log  # expect 0
      4. ./build-mingw/tests/spike/souls_formats_test_klib_spike.exe  (via WSL interop)
    Expected Result: Build emits 0 warnings, runs and prints "KLIB-SPIKE OK", exit 0.
    Failure Indicators: Any compiler warning, link error, or runtime crash.
    Evidence: .sisyphus/evidence/task-0.1-mingw-build.log

  Scenario: klib emits warnings under -Werror (NO-GO)
    Tool: Bash
    Preconditions: same
    Steps:
      1. Build with cmake --build build-mingw --target souls_formats_test_klib_spike
      2. If grep -cE 'warning:' /tmp/spike-build.log returns >0, document the warnings.
      3. Write verdict NO-GO in evidence file with concrete warning text.
    Expected Result: T4.1 is now marked BLOCKED in the plan; Wave 4 task either drops T4.1 or adds a klib-warnings carve-out CMake task.
    Evidence: .sisyphus/evidence/task-0.1-nogo-warnings.log
  ```

  **Commit**: YES (groups standalone)
  - Message: `audit(klib): 3-toolchain compile spike`
  - Files: `tests/spike/test_klib_compile.c tests/CMakeLists.txt .sisyphus/evidence/klib-toolchain-spike.md`
  - Pre-commit: `cmake --build build-mingw --target souls_formats_test_klib_spike`

- [ ] 0.2 **Reserve/fill 10-entry mismatch audit**

  **What to do**:
  1. Grep the codebase for every `sf_binary_writer_reserve_*(...)` call site and the corresponding `sf_binary_writer_fill_*(...)` call site, keyed by the literal name string passed as the placeholder identifier (`"entry_count"`, `"data_offset"`, etc.).
  2. Match reserves to fills by name+type tuple. The known imbalance is **249 reserves vs 239 fills = 10 unmatched reserves**.
  3. For each unmatched reserve, classify:
     - **Bug**: reserve issued on a control-flow path where fill is missed (e.g., error path forgets the fill). Must be fixed in Wave 1.8.
     - **Legit**: reserve issued, control flow goes to early return (cleanup-by-`_destroy` is fine because the writer is destroyed without `_finish`). Document as expected.
     - **Conditional**: reserve issued only if predicate; fill only if same predicate. Should not appear in raw grep imbalance — verify counter is symmetric within both branches.
  4. Cross-reference each finding with upstream `BinaryWriterEx.cs` to confirm the upstream pattern matches our intent.
  5. Write `.sisyphus/evidence/reserve-fill-audit.md` with a table: `Site | Name | Type | Reserve file:line | Fill file:line (or NONE) | Classification | Action`.

  **Must NOT do**:
  - Do NOT extract any reserve/fill scaffold macro in Wave 2 until this audit completes.
  - Do NOT fix bugs in Wave 0 — record them; Wave 1.8 will do the fixing in scope.

  **Recommended Agent Profile**:
  - **Category**: `deep` — requires understanding the writer state machine + careful classification.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T1.8 (fix bugs found) and T2.3 (scaffold macro)
  - **Blocked By**: None.

  **References**:
  - **Pattern**: `src/core/binary_writer.c` (lines around the `reserve` / `fill` implementations — name lookup, type tagging, finish-pending-check).
  - **API/Type**: `sf_binary_writer_finish()` returns `SF_ERR_INTERNAL` if any reservation remains — this is the safety net that catches bugs at runtime. The audit must explain why each unmatched reserve does NOT hit this safety net.
  - **Test**: `tests/core/test_binary_writer.c` — confirm the existing tests cover the "missed fill → finish fails" case (negative scenario).
  - **External**: Upstream `Utilities/IO/BinaryWriterEx.cs` — `ReserveInt32`, `FillInt32`, the `Reservation` inner struct, the `Finish()` method's missed-reservation check.
  - **WHY**: Extracting a macro across 249 sites without knowing which 10 are bugs would risk freezing the bugs into the abstraction layer. Worse, the macro might shadow the diagnostic that currently reports the bug at `finish()`.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/reserve-fill-audit.md` exists with a complete site-by-site table.
  - [ ] Total rows = 249 (every reserve accounted for).
  - [ ] Each unmatched reserve has a Classification of {Bug, Legit, Conditional} and an Action of {fix-in-W1.8, document-as-expected, false-positive-of-grep}.
  - [ ] If any Bugs found: bug count and concise description listed in the verdict line.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Audit completes with full coverage
    Tool: Bash (grep + awk + manual review)
    Preconditions: src/ tree intact; binary_writer.c functions known.
    Steps:
      1. grep -rEn 'sf_binary_writer_reserve_(u32|u64|i32|i64|varint)\(' src/ > /tmp/reserves.txt  (expect ~249 lines)
      2. grep -rEn 'sf_binary_writer_fill_(u32|u64|i32|i64|varint)\(' src/ > /tmp/fills.txt  (expect ~239 lines)
      3. Run audit script that pairs by (name string, type) — output unmatched.
      4. Manually classify each unmatched reserve via reading the surrounding control flow.
      5. Write evidence file with the table.
    Expected Result: 249 reserve rows accounted for; ≤10 unmatched explained; verdict GO (no bugs) or BUGFOUND=N (Wave 1.8 has N tasks).
    Evidence: .sisyphus/evidence/reserve-fill-audit.md + /tmp/reserves.txt + /tmp/fills.txt.

  Scenario: Audit reveals a buggy unfilled reserve
    Tool: Bash + Read
    Preconditions: a reserve site exists where fill is missed on a non-error path.
    Steps:
      1. Pair-up reveals reserve at e.g. src/archive/bnd4.c:NNN with no matching fill.
      2. Inspect surrounding code: confirm fill is on a code path that's always taken (not an error branch).
      3. Verify upstream `BND4.cs` has the corresponding fill in the same logical position.
      4. Document as Bug; write Wave 1.8 task entry.
    Expected Result: Bug recorded with file:line, expected fill location, upstream alignment.
    Evidence: .sisyphus/evidence/reserve-fill-audit.md contains a Bug row.
  ```

  **Commit**: YES (groups standalone)
  - Message: `audit(writer): reserve/fill 10-entry mismatch classification`
  - Files: `.sisyphus/evidence/reserve-fill-audit.md`
  - Pre-commit: none (no code changed)

- [ ] 0.3 **PARAMDEF apply caller-pattern audit**

  **What to do**:
  1. Find all callers of `sf_param_apply_paramdef(...)` in `src/`, `tests/`, and `examples/` (the public entry point per `include/souls_formats/sf_param.h`).
  2. For each caller, classify the usage pattern:
     - **One-shot**: one call with `(def, rows[])`, no second call with same def. Precompute cache amortizes ZERO; don't bother caching.
     - **Repeat-same-def**: same `def` reused for multiple `apply` calls. Cache amortizes across calls.
     - **Hot-loop**: `apply` inside a loop over rows or params. Cache yields biggest wins.
  3. Specifically inspect: PARAM-row decoding utilities, `examples/sf_param_dump.c`, `tests/param/test_param_apply_paramdef*.c`, and any e2e helpers.
  4. Also check whether the public API expects the `def` to be **immutable** post-apply or whether callers may mutate it (which would invalidate the cache).
  5. Write `.sisyphus/evidence/paramdef-apply-callers.md` with a usage-pattern table and a verdict: GO if at least one Repeat-same-def or Hot-loop caller exists; NO-GO otherwise.

  **Must NOT do**:
  - Do NOT add a cache field to `sf_paramdef_t` in Wave 0 (this is Wave 4.2).
  - Do NOT change the public API to require immutability — if mutability is currently allowed, plan around it (cache key includes a generation counter, or cache stored adjacent to caller).

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T4.2 (paramdef precompute)
  - **Blocked By**: None.

  **References**:
  - **API/Type**: `include/souls_formats/sf_param.h` — `sf_param_apply_paramdef` signature; `include/souls_formats/sf_paramdef.h` — `sf_paramdef_t` opaque pointer + field accessors.
  - **Pattern**: `src/param/paramdef_apply.c` — current per-call field-layout interpretation; tests' usage of apply API.
  - **External**: Upstream `Formats/PARAM/PARAM.cs::ApplyParamdefCarefully` + `PARAMDEF.cs` field-bit-offset computation.
  - **WHY**: A precompute cache only wins if there's reuse. Building it without measurement is dead code. Also: if callers mutate the def, caching could silently miscompute.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/paramdef-apply-callers.md` exists.
  - [ ] Table lists all call sites of `sf_param_apply_paramdef` with classification + estimated row count.
  - [ ] Verdict line: GO (W4.2 proceed) or NO-GO (W4.2 drop, document why).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Found ≥1 reuse pattern, GO verdict
    Tool: Bash + Read
    Preconditions: src/ + tests/ + examples/ tree intact.
    Steps:
      1. grep -rn 'sf_param_apply_paramdef\s*(' src/ tests/ examples/ > /tmp/callers.txt
      2. For each caller (likely 5-15), Read surrounding function context; classify.
      3. Count callers per category.
      4. Write evidence file.
    Expected Result: ≥1 Repeat-same-def or Hot-loop caller; verdict GO with row-count estimate.
    Evidence: .sisyphus/evidence/paramdef-apply-callers.md

  Scenario: All callers are one-shot, NO-GO
    Tool: Bash + Read
    Preconditions: same
    Steps:
      1. After classification, all callers are One-shot.
      2. Write NO-GO verdict; document that T4.2 is dropped from Wave 4 scope.
      3. Note in evidence file: alternative wins via the same time budget (e.g., one extra T3.X task).
    Expected Result: Wave 4 reduces to T4.1 + T4.3 (if cleared by T0.4).
    Evidence: .sisyphus/evidence/paramdef-apply-callers.md (NO-GO verdict)
  ```

  **Commit**: YES
  - Message: `audit(param): paramdef-apply caller pattern classification`
  - Files: `.sisyphus/evidence/paramdef-apply-callers.md`
  - Pre-commit: none

- [ ] 0.4 **binary_reader endian-toggle audit**

  **What to do**:
  1. Find all in-tree assignments to `r->big_endian` (or via `sf_binary_reader_set_big_endian()`) across `src/` to enumerate **our** mid-stream toggle sites.
  2. Cross-reference each toggle site to its upstream `.cs` counterpart — confirm upstream also toggles at the same logical point.
  3. Count: total toggles; per-format breakdown.
  4. Run a FLVER2 vertex-decode microbenchmark before/after the proposed endian inline fast-path, to determine if the optimization yields ≥5% speedup. (Microbench harness: read a representative `c0000.flver` from ER e2e, time `sf_flver2_decode_mesh`.)
  5. Write `.sisyphus/evidence/endian-toggle-sites.md` with: per-site list, upstream alignment confirmation, microbench % delta, and verdict GO (≥5% win, proceed with fast-path) or NO-GO (skip T4.3).

  **Must NOT do**:
  - Do NOT propose type-specialized readers (separate LE/BE types). Public API requires `sf_binary_reader_set_big_endian(r, true)` to keep working.
  - Do NOT remove the existing mutable-flag path; fast-path is opt-in via the same flag.

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T4.3 (endian fast-path)
  - **Blocked By**: None.

  **References**:
  - **Pattern**: `src/core/binary_reader.c:128` (`r->big_endian = be`), `:247` (the macro that branches on it for every read).
  - **API/Type**: `include/souls_formats/sf_io.h::sf_binary_reader_set_big_endian` public symbol — must stay.
  - **Test**: `tests/core/test_binary_reader.c` — existing BE/LE flip cases; a NEW negative test for mid-stream toggle (Wave 4.3 will add if pursued).
  - **External**: Upstream `Utilities/IO/BinaryReaderEx.cs` — `BigEndian` is a public mutable property; 79 assignments across 30 .cs files (Metis confirmed).
  - **WHY**: Without measurement, "endian specialization" is speculative work. A 1% win on a path that's not bottleneck-bound is not worth the risk of breaking the mutable-flag contract.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/endian-toggle-sites.md` exists.
  - [ ] Per-site list shows file:line + which format + a one-line "why we toggle here" rationale.
  - [ ] Microbench harness source committed (small one-file test, lives in `tests/microbench/test_flver2_decode_endian.c`).
  - [ ] Microbench results table: before/after wall-time on the in-memory synthetic FLVER2 cube (constructed as in `tests/geom/test_flver2_synthetic.c` lines around the `k_cube_vertices`/`k_cube_indices` arrays) plus, if available, the ER `c0000.flver` extracted via `er_extract_from_data0`.
  - [ ] Verdict line: GO with `+N.N%` speedup OR NO-GO with `+N.N% < 5%, skip`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Microbench shows ≥5% wall-time reduction
    Tool: Bash (cmake + execute microbench)
    Preconditions: build-mingw exists; ER e2e fixture available (else use the in-memory synthetic FLVER2 cube constructed inline in tests/microbench/test_flver2_decode_endian.c, mirroring tests/geom/test_flver2_synthetic.c's k_cube_vertices/k_cube_indices arrays).
    Steps:
      1. Build the microbench: cmake --build build-mingw --target souls_formats_microbench_endian
      2. Run baseline (current code): ./build-mingw/.../microbench_endian.exe baseline 3>/tmp/baseline.csv
      3. Apply a prototype fast-path in-place (DO NOT commit yet — purely for measurement).
      4. Run optimized: ./build-mingw/.../microbench_endian.exe optimized 3>/tmp/optimized.csv
      5. Compute mean delta; if ≥5%, write GO verdict; else NO-GO.
      6. Revert prototype changes; commit only the microbench source + evidence file.
    Expected Result: Verdict line shows numeric % delta and GO/NO-GO.
    Evidence: .sisyphus/evidence/endian-toggle-sites.md + /tmp/baseline.csv + /tmp/optimized.csv

  Scenario: Mid-stream toggle is rampant (specialization would break)
    Tool: Bash
    Preconditions: same
    Steps:
      1. grep -rEn 'sf_binary_reader_set_big_endian|->big_endian\s*=' src/ > /tmp/toggles.txt
      2. Count toggles in BXF4, MSBE, NVM, ESD, FMG (formats Metis flagged).
      3. If toggles > 5 across our coverage, document that type-specialization is forbidden.
      4. Even if microbench shows wins, the audit caveats: "fast-path only, no type split".
    Expected Result: Evidence file explicitly documents mid-stream-toggle requirement.
    Evidence: .sisyphus/evidence/endian-toggle-sites.md (constraint section)
  ```

  **Commit**: YES
  - Message: `audit(core): endian-toggle sites + FLVER2 microbench`
  - Files: `tests/microbench/test_flver2_decode_endian.c tests/CMakeLists.txt .sisyphus/evidence/endian-toggle-sites.md`
  - Pre-commit: `cmake --build build-mingw --target souls_formats_microbench_endian`

- [ ] 0.5 **MSB scaffolding-vs-subtype LOC breakdown**

  **What to do**:
  1. For each of `src/map/msbs/*.c`, `src/map/msbe/*.c`, `src/map/msbvi/*.c`, classify each function body into:
     - **Scaffolding**: offset table read, count check, alloc loop, index backfill (entry-list traversal mechanics).
     - **Subtype-specific**: per-subtype field reads/writes (e.g., for parts, the per-subtype branch tables).
  2. Use `wc -l` on each function body (via ast-grep or manual range extraction).
  3. Produce a table per file: `Function | Scaffolding LOC | Subtype LOC | Shareable? (Y/N)`.
  4. Aggregate per module: total Scaffolding LOC vs total Subtype LOC; estimate "extractable LOC" if Wave 5 lands.
  5. Specifically validate Metis's claim that `parts_param.c` 627→132→99 LOC difference is mostly subtype divergence (Sekiro has subtypes ER doesn't).
  6. Write `.sisyphus/evidence/msb-scaffold-vs-subtype.md` with the breakdown and a verdict: GO with `~N LOC shared scaffolding extractable per module` or NO-GO if Scaffolding LOC is too small to bother (<50 LOC per module).

  **Must NOT do**:
  - Do NOT propose touching per-subtype field reads. Wave 5 limit = scaffolding only.
  - Do NOT propose a unified "MSB" header with all subtypes — that's a v2.0 concern.

  **Recommended Agent Profile**:
  - **Category**: `deep` — requires reading 22 .c files and classifying functions semantically.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T5.1 (msb_common engine extraction)
  - **Blocked By**: None.

  **References**:
  - **Pattern**: `src/map/msb_common.c` — existing shared entry skeleton; `src/map/msbs/parts_param.c` (627 LOC) vs `src/map/msbe/parts_param.c` (132 LOC) vs `src/map/msbvi/parts_param.c` (99 LOC) — case study.
  - **API/Type**: `include/souls_formats/sf_msb.h` — public `sf_msb_part_t / sf_msb_region_t / ...` opaque types (do not touch); the read/write APIs are per-game.
  - **External**: Upstream `Formats/MSB/MSBS/`, `MSBE/`, `MSBVI/` — generic `Param<T>` class in C# is what we'd be C-ifying.
  - **WHY**: Metis already estimated the shared portion at ~50-150 LOC per file. We need to validate with concrete numbers before committing to T5.1's design.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/msb-scaffold-vs-subtype.md` exists.
  - [ ] Table covers all 19 MSB .c files matching `src/map/msb{s,e,vi}/*.c` (6 msbs + 6 msbe + 7 msbvi as of the current tree; if the count changes during refactor it must still equal the union of those globs).
  - [ ] Per-module aggregate row: Scaffolding total, Subtype total, % extractable.
  - [ ] Verdict line: GO with target LOC reduction estimate, or NO-GO with reason.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: ~50–150 LOC per file is extractable scaffolding
    Tool: Bash + ast-grep + Read
    Preconditions: src/map/ tree intact.
    Steps:
      1. ls src/map/msb{s,e,vi}/*.c | xargs -n1 wc -l > /tmp/msb-loc.txt
      2. For each file, extract function bodies via ast-grep '$RET $FN($$$) { $$$ }' and Read each.
      3. Classify each function as Scaffolding/Subtype based on what it reads/writes.
      4. Aggregate per module.
    Expected Result: Shared scaffolding ~50–150 LOC per file; subtype divergence dominates.
    Evidence: .sisyphus/evidence/msb-scaffold-vs-subtype.md

  Scenario: Less than 50 LOC scaffolding per file (NO-GO)
    Tool: Same
    Preconditions: same
    Steps:
      1. After classification, scaffolding shareable LOC is too small to justify the extraction's overhead (header / function-pointer table / X-macro).
      2. Write NO-GO verdict; T5.1/T5.2 dropped from Wave 5.
      3. T5.3 retained: just document why MSB is divergent in POLICY.md.
    Expected Result: Wave 5 reduces to just the doc task.
    Evidence: .sisyphus/evidence/msb-scaffold-vs-subtype.md (NO-GO with concrete LOC numbers)
  ```

  **Commit**: YES
  - Message: `audit(map): MSB scaffolding-vs-subtype LOC breakdown`
  - Files: `.sisyphus/evidence/msb-scaffold-vs-subtype.md`
  - Pre-commit: none

- [ ] 0.6 **Per-entry vs per-archive alloc-site audit**

  **What to do**:
  1. For each of the top alloc-heavy files (`fxr3_xml_read.c`, `fxr3.c`, `bnd3.c`, `bxf3.c`, `bxf4.c`, `bnd4.c`, `tae.c`, `bhd5.c`, `encoding_win32.c`, `esd.c`, `matbin.c`, `binary_reader.c`, `flver2.c`, `tpf.c`, `dcx.c`), enumerate each `sf_xalloc`/`sf_xrealloc`/`sf_strdup` site.
  2. Classify each site:
     - **Per-archive** / **Per-file** (~1 call per object): not a perf concern at our scale.
     - **Per-entry inside loop** (N calls per object, N = entries): primary Wave 3 target.
     - **Per-element nested loop** (M×N calls): potential Wave 3 target if M×N is large.
  3. For each per-entry site, estimate the typical entry count (e.g., BND4 chrbnd ≈ 5–50, BHD5 Data0 ≈ 25,000, FXR3 nodes per effect ≈ 50–500).
  4. Rank top-15 sites by `entries × call_size_estimate`.
  5. Write `.sisyphus/evidence/alloc-site-audit.md` with the table and a per-file proposed remedy ({name-pool, scratch-buffer, bulk-alloc-with-count, no-op}).

  **Must NOT do**:
  - Do NOT propose changes that would intern caller-returned strings (public contract).
  - Do NOT modify the public allocator signature.
  - Do NOT batch unrelated allocations into one buffer (lifetime mismatch).

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — broad survey across 15 files, careful classification.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T3.1 – T3.5 (per-entry alloc reduction)
  - **Blocked By**: None.

  **References**:
  - **Pattern**: `src/archive/bnd4.c:366, 771` — confirmed per-entry `sf_strdup(b->alloc, headers[i].name_utf8)` in loop.
  - **API/Type**: `sf_xalloc / sf_xfree / sf_strdup` in `src/internal/sf_internal.h:25-95`.
  - **External**: Upstream BND4.cs entry-list parsing — per-entry string allocation is the same pattern; C extension here is the bulk-pool, which we document in `extensions.md`.
  - **WHY**: Saying "997 alloc calls" is a vibe; ranking sites by `entries × cost` is what produces wins. Many of the 44 calls in bnd3.c are once-per-archive; ignoring them is correct.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/alloc-site-audit.md` exists.
  - [ ] Top-15 sites ranked with proposed remedy + estimated steady-state reduction.
  - [ ] Per-file action list ready for Wave 3 task drafting.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Audit produces actionable Wave 3 task seeds
    Tool: Bash + Read
    Preconditions: src/ tree intact.
    Steps:
      1. grep -rEn 'sf_xalloc\(|sf_xrealloc\(|sf_strdup\(' src/archive/*.c src/effects/*.c src/core/*.c > /tmp/alloc-sites.txt
      2. For each site, Read surrounding function; classify loop context.
      3. Look up typical entry counts from existing tests' fixture data (e.g., `tests/archive/test_bnd4_synthetic.c` uses N=3 entries; real ER bnd has 5-50; document range).
      4. Rank and write evidence file with proposed remedies.
    Expected Result: 10-15 actionable sites identified; each has a concrete Wave 3 task seed.
    Evidence: .sisyphus/evidence/alloc-site-audit.md

  Scenario: Most "hot" sites are actually one-shot (downgrade Wave 3 scope)
    Tool: Same
    Preconditions: same
    Steps:
      1. After classification, only 2-3 truly per-entry sites found.
      2. Document; downgrade Wave 3 from 5 tasks to 2-3 tasks.
    Expected Result: Wave 3 task list adjusted before Wave 3 starts.
    Evidence: .sisyphus/evidence/alloc-site-audit.md (downgrade verdict)
  ```

  **Commit**: YES
  - Message: `audit(alloc): per-entry vs per-archive site classification`
  - Files: `.sisyphus/evidence/alloc-site-audit.md`
  - Pre-commit: none

- [ ] 0.7 **`emevd_internal.h` cross-module use audit**

  **What to do**:
  1. Grep all `#include` lines anywhere in `src/`, `include/`, `tests/`, `examples/` that reference `emevd_internal.h`.
  2. If only files under `src/script/` reference it → it's correctly format-local; LEAVE in place; document in evidence as "no relocation needed".
  3. If any non-script file references it → it's genuinely cross-module; **relocate** to `src/internal/emevd_internal.h` in Wave 1.7.
  4. Write `.sisyphus/evidence/emevd-internal-use.md` with includer list and verdict.

  **Must NOT do**:
  - Do NOT relocate the other 9 format-local `*_internal.h` headers (msbs_internal.h, tae_internal.h, etc.) — those are deliberate per Metis.
  - Do NOT relocate if only `src/script/` files use it (it's format-local, that's the convention).

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 0
  - **Blocks**: T1.7 (apply relocation decision)
  - **Blocked By**: None.

  **References**:
  - **Pattern**: existing convention — `src/map/msbs/msbs_internal.h` etc., kept format-local.
  - **External**: Upstream has no equivalent of these private headers (each game has its own .cs).
  - **WHY**: Metis flagged `emevd_internal.h` as the only one in `src/script/`. Confirming intent before relocating prevents pointless churn.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/emevd-internal-use.md` exists.
  - [ ] Verdict line: RELOCATE-TO-INTERNAL, or KEEP-IN-PLACE, with includer list as evidence.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Only script/ files include it (KEEP-IN-PLACE)
    Tool: Bash
    Preconditions: src/ tree intact.
    Steps:
      1. grep -rln 'emevd_internal.h' src/ include/ tests/ examples/ > /tmp/emevd-includers.txt
      2. All paths match src/script/* → write KEEP-IN-PLACE verdict.
    Expected Result: T1.7 becomes a no-op (just update evidence file).
    Evidence: .sisyphus/evidence/emevd-internal-use.md

  Scenario: Cross-module use found (RELOCATE)
    Tool: Same
    Preconditions: same
    Steps:
      1. Find e.g. src/archive/some.c includes emevd_internal.h.
      2. Write RELOCATE-TO-INTERNAL verdict; T1.7 will move file + update all includers.
    Expected Result: T1.7 has concrete instructions: source path, target path, includer list.
    Evidence: .sisyphus/evidence/emevd-internal-use.md (RELOCATE verdict + actions)
  ```

  **Commit**: YES
  - Message: `audit(internal): emevd_internal.h cross-module use`
  - Files: `.sisyphus/evidence/emevd-internal-use.md`
  - Pre-commit: none

### Wave 1 — BASELINES + CMAKE REORG + CHEAP FIXES

> Wave 1 establishes the baselines every later wave verifies against, does the mandatory CMake
> reorg, and lands all the cheap independent fixes Metis surfaced. Runs in parallel after Wave 0.

- [ ] 1.1 **Capture symbol / test / skip / sanitizer baselines**

  **What to do**:
  1. Build clean: `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw`.
  2. Capture symbol export baseline:
     `x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | awk '/^\s*\[\s*[0-9]+\]\s+sf_/{print $NF}' | sort -u > .sisyphus/evidence/symbols-baseline.txt`
     Expected count: ≥400.
  3. Capture per-label test count baseline:
     `for L in core compression crypto archive param script map geom anim hygiene golden e2e e2e_er e2e_ac6 e2e_sekiro e2e_nightreign; do ctest --test-dir build-mingw -L "^${L}$" -N 2>/dev/null | awk '/^\s+Test\s+#/{c++} END{print "'$L': " (c?c:0)}'; done > .sisyphus/evidence/test-counts-baseline.txt`
  4. Capture e2e skip-count baseline (Oodle or game files absent):
     `ctest --test-dir build-mingw -L 'e2e|e2e_er|e2e_ac6|e2e_sekiro|e2e_nightreign' -V --output-on-failure 2>&1 | grep -cE 'SKIP:|gracefully skipping|TEST_IGNORE_MESSAGE' > .sisyphus/evidence/skip-count-baseline.txt`
  5. Build under sanitizer:
     `cmake -B build-asan -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug -DSF_ENABLE_SANITIZERS=ON && cmake --build build-asan`
     Run `ctest --test-dir build-asan -L core --output-on-failure | tee .sisyphus/evidence/sanitizer-baseline.txt`.
  6. Capture golden hash baseline: re-run `tests/golden/capture_golden.c` and commit the resulting `tests/golden/golden_hashes.h` if it's regenerated (or document that current values are pinned).
  7. Commit all evidence files in one commit `chore(evidence): capture wave-0 baselines`.

  **Must NOT do**:
  - Do NOT modify any source code in this task.
  - Do NOT regenerate `golden_hashes.h` if the existing pinned values match the current build's outputs — that's the safer signal.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (after Wave 0)
  - **Parallel Group**: Wave 1 (with T1.2 through T1.8)
  - **Blocks**: all subsequent wave acceptance gates (they `diff` against these baselines)
  - **Blocked By**: Wave 0 complete (so we baseline against a clean tree).

  **References**:
  - **Pattern**: `tests/CMakeLists.txt:1-39` (sf_add_test helper + DLL copy step + ENVIRONMENT PATH).
  - **API/Type**: existing `souls_formats_test_golden` target lives in `tests/golden/capture_golden.c`.
  - **External**: GNU objdump format for PE imports/exports; CTest -N (dry-run) for counting registered tests.
  - **WHY**: Every later wave acceptance gate diffs against these files. Capturing them at a clean head is the contract.

  **Acceptance Criteria**:
  - [ ] All 6 evidence files exist under `.sisyphus/evidence/`.
  - [ ] `symbols-baseline.txt` has ≥400 lines.
  - [ ] `test-counts-baseline.txt` has 16 lines (one per label) with non-zero counts for at least core, compression, crypto, archive, param, script, map, geom, anim.
  - [ ] `skip-count-baseline.txt` is a single integer.
  - [ ] `sanitizer-baseline.txt` ends with `100% tests passed` line.
  - [ ] Golden hashes regenerated or confirmed pinned (commit log says which).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Baselines captured cleanly
    Tool: Bash
    Preconditions: tree at clean HEAD; Oodle DLL and ER game files present.
    Steps:
      1. cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build-mingw 2>&1 | tee /tmp/build.log; test ! grep -q 'warning:' /tmp/build.log
      3. Run all 6 capture commands above.
      4. Verify each output file exists and is non-empty.
    Expected Result: 6 evidence files, all with sensible content.
    Evidence: .sisyphus/evidence/symbols-baseline.txt + 5 others

  Scenario: Sanitizer build fails at baseline
    Tool: Bash
    Preconditions: same
    Steps:
      1. cmake --build build-asan reveals an existing ASan-triggered bug.
      2. Stop the plan; report the bug; treat as Wave 0.5 prerequisite — must be fixed before Wave 1 continues.
    Expected Result: Baseline is honest. If sanitizer was never clean, Wave acceptance cannot use sanitizer as a gate — adjust plan.
    Evidence: .sisyphus/evidence/sanitizer-baseline.txt (with the failure recorded)
  ```

  **Commit**: YES
  - Message: `chore(evidence): capture wave-0 baselines (symbols/tests/skip/sanitizer)`
  - Files: `.sisyphus/evidence/symbols-baseline.txt .sisyphus/evidence/test-counts-baseline.txt .sisyphus/evidence/skip-count-baseline.txt .sisyphus/evidence/sanitizer-baseline.txt`
  - Pre-commit: build + ctest must already be green at HEAD

- [ ] 1.2 **CMake: `BUILD_TESTING` default `${PROJECT_IS_TOP_LEVEL}` + `SF_BUILD_TESTS` alias**

  **What to do**:
  1. In top-level `CMakeLists.txt`, replace the `option(SF_BUILD_TESTS ... ON)` line (line 12) with:
     ```cmake
     # CTest convention: enable_testing only when project is top-level.
     include(CTest)  # provides BUILD_TESTING, defaulting based on PROJECT_IS_TOP_LEVEL via the version-2 behavior
     option(SF_BUILD_TESTS "Backwards-compatible alias for BUILD_TESTING" ${BUILD_TESTING})
     if(DEFINED SF_BUILD_TESTS AND NOT BUILD_TESTING)
         set(BUILD_TESTING ${SF_BUILD_TESTS} CACHE BOOL "" FORCE)
         message(DEPRECATION "SF_BUILD_TESTS is deprecated; use -DBUILD_TESTING=ON")
     endif()
     ```
     (Exact form: ensure `include(CTest)` is called before `add_subdirectory(tests)`; ensure the deprecation message only emits when SF_BUILD_TESTS is explicitly passed, not when it inherits BUILD_TESTING.)
  2. Replace `option(SF_BUILD_EXAMPLES ... OFF)` with a guarded variant:
     ```cmake
     option(SF_BUILD_EXAMPLES "Build examples (requires PROJECT_IS_TOP_LEVEL)" OFF)
     ```
     Make `add_subdirectory(examples)` conditional on `SF_BUILD_EXAMPLES AND PROJECT_IS_TOP_LEVEL`.
  3. Make `add_subdirectory(tests)` conditional on `BUILD_TESTING AND PROJECT_IS_TOP_LEVEL`.
  4. Update the configuration-summary `message(STATUS ...)` block to reflect the new behavior.
  5. Add a doc note to AGENTS.md §3 noting the new default and the legacy alias.

  **Must NOT do**:
  - Do NOT call `enable_testing()` outside the CTest include (CTest does it correctly).
  - Do NOT remove `SF_BUILD_TESTS` — it's the backwards-compat alias and CI relies on it.
  - Do NOT auto-enable tests when consumed via add_subdirectory.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — CMake state machines are subtle; the `option/cache/force/include(CTest)` ordering matters.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (mostly disjoint from other Wave 1 tasks)
  - **Parallel Group**: Wave 1
  - **Blocks**: T1.3 (CI workflow update), T1.5 (probes/labels touch tests/CMakeLists)
  - **Blocked By**: T1.1 (baselines must exist before we move test build behavior)

  **References**:
  - **Pattern**: top-level `CMakeLists.txt:9-14` (current options block); `CMakeLists.txt:231-241` (current add_subdirectory blocks).
  - **API/Type**: CMake `BUILD_TESTING` is a convention from `include(CTest)`; `PROJECT_IS_TOP_LEVEL` is from CMake 3.21 (we require 3.24).
  - **External**: CMake docs: `https://cmake.org/cmake/help/latest/module/CTest.html` and `https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html`.
  - **WHY**: This is the user's mandatory deliverable. Without correct `include(CTest)` ordering, `enable_testing()` won't propagate, and `ctest` won't find tests.

  **Acceptance Criteria**:
  - [ ] `cmake -B build-consumer -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake` from a wrapper project (`add_subdirectory(souls-formats-c sf)`) does NOT build tests/examples (verify via `! test -d build-consumer/sf/tests`).
  - [ ] `cmake -B build-default -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake` from top-level builds tests by default (verify ≥40 test exes under `build-default/tests/`).
  - [ ] `cmake -B build-legacy -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_TESTS=OFF` does NOT build tests; build log mentions DEPRECATION message.
  - [ ] `cmake -B build-explicit -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=OFF` does NOT build tests.
  - [ ] `cmake -B build-ex -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_EXAMPLES=ON` builds examples (top-level path).
  - [ ] All wave-1 acceptance gates pass.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Top-level default builds tests
    Tool: Bash
    Preconditions: clean tree post-T1.1.
    Steps:
      1. rm -rf build-default && cmake -B build-default -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
      2. cmake --build build-default 2>&1 | tee /tmp/build-default.log
      3. find build-default/tests -name 'souls_formats_test_*.exe' | wc -l  # >= 40
      4. ctest --test-dir build-default -L core --output-on-failure
    Expected Result: tests built and at least core label passes.
    Evidence: .sisyphus/evidence/task-1.2-default-build.log

  Scenario: Consumer add_subdirectory does NOT build tests
    Tool: Bash
    Preconditions: same
    Steps:
      1. rm -rf /tmp/sf-consumer && mkdir -p /tmp/sf-consumer && cd /tmp/sf-consumer
      2. Write a minimal consumer CMakeLists.txt with add_subdirectory(/home/soar/src/souls-formats-c sf EXCLUDE_FROM_ALL).
      3. cmake -B build -G Ninja --toolchain /home/soar/src/souls-formats-c/cmake/toolchain-mingw-w64.cmake
      4. cmake --build build
      5. test ! -d build/sf/tests
    Expected Result: Consumer builds without tests; library targets resolve.
    Evidence: .sisyphus/evidence/task-1.2-consumer-build.log
  ```

  **Commit**: YES
  - Message: `build(cmake): default BUILD_TESTING/SF_BUILD_EXAMPLES to PROJECT_IS_TOP_LEVEL; alias SF_BUILD_TESTS`
  - Files: `CMakeLists.txt AGENTS.md`
  - Pre-commit: all four acceptance build variants run

- [ ] 1.3 **CI workflow: update to BUILD_TESTING flag**

  **What to do**:
  1. Read `.github/workflows/ci.yml`. Find every line passing `-DSF_BUILD_TESTS=ON` (or `=OFF`).
  2. Replace with `-DBUILD_TESTING=ON` (or `=OFF` accordingly).
  3. Keep the legacy alias `SF_BUILD_TESTS` available — if some CI step explicitly tests the deprecation path, leave one job exercising the alias.
  4. Ensure the linux-cross job (build-only sanity) still runs with `BUILD_TESTING=ON` so the cross-compile sanity is preserved.
  5. Run the workflow locally if `act` is available; otherwise commit and trust GitHub Actions to verify.

  **Must NOT do**:
  - Do NOT remove the linux-cross job — it's the only sanity for non-Windows host builds.
  - Do NOT introduce new CI matrix entries in this task; that's outside scope.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: NO (depends on T1.2 landing — the alias must exist)
  - **Parallel Group**: Wave 1 (but sequential after T1.2)
  - **Blocks**: none
  - **Blocked By**: T1.2

  **References**:
  - **Pattern**: `.github/workflows/ci.yml:111` (current `-DSF_BUILD_TESTS=ON`) and surrounding job definitions.
  - **External**: GitHub Actions workflow syntax.
  - **WHY**: Without this, the next CI run after T1.2 will SILENTLY still build tests via the alias and not exercise the new default. We want CI to use the new path explicitly.

  **Acceptance Criteria**:
  - [ ] `grep -c SF_BUILD_TESTS .github/workflows/ci.yml` returns 0 or 1 (the 1 case = a deliberate alias-deprecation test job).
  - [ ] `grep -c BUILD_TESTING .github/workflows/ci.yml` returns ≥1.
  - [ ] GitHub Actions run on the commit completes green for all 5 jobs (msvc, clang-cl-asan, mingw, linux-cross, plus any sanitizer matrix). [Verify via `gh run list --workflow=ci.yml --limit 1` post-push.]

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: CI passes on new flag names
    Tool: Bash + gh
    Preconditions: T1.2 has landed; commit pushed.
    Steps:
      1. After commit, `gh run watch <RUN_ID>` or wait for completion.
      2. All matrix jobs show conclusion=success.
    Expected Result: 5/5 CI jobs green.
    Evidence: .sisyphus/evidence/task-1.3-ci-run.log (gh run view output)

  Scenario: Local cross-toolchain build still works
    Tool: Bash
    Preconditions: WSL2 host.
    Steps:
      1. rm -rf build-mingw && cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=ON
      2. cmake --build build-mingw
      3. ctest --test-dir build-mingw -L core --output-on-failure
    Expected Result: full local build green; matches CI linux-cross job's behavior.
    Evidence: .sisyphus/evidence/task-1.3-local-cross.log
  ```

  **Commit**: YES
  - Message: `ci: update workflow to BUILD_TESTING flag (post SF_BUILD_TESTS alias)`
  - Files: `.github/workflows/ci.yml`
  - Pre-commit: `cmake -B /tmp/wave1-ci-check -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=ON && cmake --build /tmp/wave1-ci-check --target souls_formats_static`

- [ ] 1.4 **`sf_sl2.h` add to `SF_PUBLIC_HEADERS` + examples target alias harmonization**

  **What to do**:
  1. Open `CMakeLists.txt`. In `SF_PUBLIC_HEADERS` (lines 46-84), add `include/souls_formats/sf_sl2.h` alphabetically-or-categorically-ordered with the other crypto-adjacent headers (next to `sf_regulation.h`).
  2. Verify the install rules (if any) pick up the new header. Run `cmake --install build-mingw --prefix /tmp/sf-install` and confirm `sf_sl2.h` appears in `/tmp/sf-install/include/souls_formats/`.
  3. Open `examples/CMakeLists.txt`. Change `target_link_libraries(sf_param_dump PRIVATE souls_formats)` to use `souls_formats_static` for consistency with `sf_bnd_extract`.
  4. Build examples and verify both link successfully.

  **Must NOT do**:
  - Do NOT change `sf_sl2.h` content — it's already correct, just missing from the install manifest.
  - Do NOT remove the `souls_formats` alias (other consumers may rely on it).
  - Do NOT change the link visibility from PRIVATE.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (independent of T1.2/T1.3/T1.5/T1.6/T1.7/T1.8)
  - **Parallel Group**: Wave 1
  - **Blocks**: none
  - **Blocked By**: None.

  **References**:
  - **Pattern**: `CMakeLists.txt:46-84` (SF_PUBLIC_HEADERS list); `examples/CMakeLists.txt:1-7` (target_link_libraries lines).
  - **API/Type**: `include/souls_formats/sf_sl2.h` — exists, declares the SL2 archive crypto API per Phase 2.
  - **WHY**: Pre-existing bug surfaced by Metis: `sf_sl2.h` exists but isn't shipped on install; example link inconsistency is cosmetic but worth fixing while we're here.

  **Acceptance Criteria**:
  - [ ] `grep -c 'sf_sl2.h' CMakeLists.txt` returns 1.
  - [ ] After `cmake --install build-mingw --prefix /tmp/sf-install`, `test -f /tmp/sf-install/include/souls_formats/sf_sl2.h` succeeds.
  - [ ] `examples/CMakeLists.txt` shows both targets linking `souls_formats_static`.
  - [ ] `cmake --build build-mingw --target sf_bnd_extract sf_param_dump` succeeds with `-DSF_BUILD_EXAMPLES=ON`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: sf_sl2.h installs correctly
    Tool: Bash
    Preconditions: T1.2 has landed.
    Steps:
      1. cmake --install build-mingw --prefix /tmp/sf-install --component Development 2>/dev/null || cmake --install build-mingw --prefix /tmp/sf-install
      2. ls /tmp/sf-install/include/souls_formats/sf_sl2.h
    Expected Result: file present.
    Evidence: .sisyphus/evidence/task-1.4-install-listing.log

  Scenario: Both example binaries build
    Tool: Bash
    Preconditions: same; SF_BUILD_EXAMPLES=ON
    Steps:
      1. cmake -B build-ex -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_EXAMPLES=ON
      2. cmake --build build-ex --target sf_bnd_extract sf_param_dump
      3. test -f build-ex/examples/sf_bnd_extract.exe && test -f build-ex/examples/sf_param_dump.exe
    Expected Result: both exes present.
    Evidence: .sisyphus/evidence/task-1.4-examples-build.log
  ```

  **Commit**: YES
  - Message: `build(headers,examples): add sf_sl2.h to SF_PUBLIC_HEADERS; unify examples link target`
  - Files: `CMakeLists.txt examples/CMakeLists.txt`
  - Pre-commit: install dry-run + examples build

- [ ] 1.5 **Probe gating behind `SF_BUILD_PROBES=OFF` + CTest label unification**

  **What to do**:
  1. Add to top-level `CMakeLists.txt`: `option(SF_BUILD_PROBES "Build one-shot diagnostic probes (debug aids)" OFF)`.
  2. In `tests/CMakeLists.txt`, wrap the existing `probe_nightreign_msb` and `probe_matbin_paramtypes` `add_executable` blocks (and the included `tests/probes/CMakeLists.txt` block) inside `if(SF_BUILD_PROBES AND PROJECT_IS_TOP_LEVEL) ... endif()`.
  3. Audit every `set_tests_properties(... LABELS ...)` for label drift. The intent (per Metis):
     - **`e2e` is the umbrella label**: every e2e test carries it.
     - **`e2e_<game>` is the per-game subtag** (e2e_er, e2e_ac6, e2e_sekiro, e2e_nightreign).
     - **A test that's specifically ER e2e carries BOTH** `e2e` AND `e2e_er` so `ctest -L ^e2e$` runs everything and `ctest -L e2e_er` runs only ER.
  4. Update every `sf_add_test()` and `set_tests_properties()` call accordingly. Specifically: e2e tests that currently only have `e2e_er` (or only `e2e`) get the second label.
  5. Add a `tests/CMakeLists.txt` helper `sf_add_e2e_test(name source game)` that registers both labels at once.
  6. Run `ctest --test-dir build-mingw -L 'e2e' -N` and verify the count matches T1.1's baseline-or-higher.

  **Must NOT do**:
  - Do NOT delete the probe binaries.
  - Do NOT change the probe binaries' contents.
  - Do NOT collapse `e2e_er` and `e2e_ac6` into one — they're per-game on purpose.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — CMake fan-out across ~30 e2e tests.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: NO (touches the same `tests/CMakeLists.txt` as T1.2 and would conflict)
  - **Parallel Group**: Wave 1 (sequential after T1.2)
  - **Blocks**: T2.4 (e2e helper consolidation builds on the label unification)
  - **Blocked By**: T1.2

  **References**:
  - **Pattern**: `tests/CMakeLists.txt:105-176, 354-415` (e2e test registrations with split labels); `tests/CMakeLists.txt:417-438` (probe `add_executable` blocks).
  - **WHY**: Without label unification, `ctest -L e2e` misses Sekiro/AC6/NR tests; analysis of skip counts and gap analysis hangs on consistent labeling.

  **Acceptance Criteria**:
  - [ ] `grep -c 'SF_BUILD_PROBES' CMakeLists.txt tests/CMakeLists.txt` ≥3 (option + 2 guards).
  - [ ] `cmake -B build-noprobes -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=ON -DSF_BUILD_PROBES=OFF && cmake --build build-noprobes` — no probe targets built.
  - [ ] `cmake -B build-probes -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=ON -DSF_BUILD_PROBES=ON && cmake --build build-probes --target probe_nightreign_msb` succeeds.
  - [ ] Every `*_e2e_*` test target carries both `e2e` AND `e2e_<game>` labels: `ctest --test-dir build-default -L 'e2e' -N | grep -c 'Test #'` ≥ sum of all per-game counts (verify via shell math).
  - [ ] `ctest --test-dir build-default -L e2e_er -N | grep -c 'Test #'` ≥ 5 (ER-specific subset still works).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Probes excluded by default
    Tool: Bash
    Preconditions: T1.2 landed.
    Steps:
      1. cmake -B build-default -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DBUILD_TESTING=ON
      2. cmake --build build-default
      3. test ! -f build-default/tests/probes/probe_nightreign_msb.exe
    Expected Result: probe binary NOT built.
    Evidence: .sisyphus/evidence/task-1.5-no-probes.log

  Scenario: Label unification picks up all e2e
    Tool: Bash
    Preconditions: same
    Steps:
      1. PRE=$(cat .sisyphus/evidence/test-counts-baseline.txt | grep '^e2e_' | awk -F: '{s+=$2}END{print s}')
      2. NOW=$(ctest --test-dir build-default -L '^e2e$' -N | grep -c 'Test #')
      3. test "$NOW" -ge "$PRE"  # umbrella catches all per-game tests + any e2e-without-game
    Expected Result: umbrella `e2e` covers >= sum of per-game counts.
    Evidence: .sisyphus/evidence/task-1.5-label-coverage.log
  ```

  **Commit**: YES
  - Message: `build(tests): probe gating + e2e label unification`
  - Files: `CMakeLists.txt tests/CMakeLists.txt tests/probes/CMakeLists.txt`
  - Pre-commit: both build variants + ctest -L e2e -N

- [ ] 1.6 **`SF_ENABLE_PHASE7` CHANGELOG drift cleanup + stale-build-dir doc trail**

  **What to do**:
  1. Read `CHANGELOG.md`. Find every mention of `SF_ENABLE_PHASE7`.
  2. Verify in `CMakeLists.txt` that NO such option exists (Phase 7 is unconditionally compiled per current lines 168-173).
  3. Decide policy (default: doc-only): document in CHANGELOG that `SF_ENABLE_PHASE7` was never landed as an option; Phase 7 is permanently in-build.
  4. Add a brief note to README.md or AGENTS.md §3 about cleaning up old build dirs (`rm -rf build-on build-off`) — not as a script, just a hint in prose.
  5. Confirm `.gitignore` covers `build*/` (it should already).

  **Must NOT do**:
  - Do NOT add `SF_ENABLE_PHASE7` to CMakeLists.txt — the option was retroactively dropped as a design decision.
  - Do NOT auto-delete user-local build dirs.
  - Do NOT rewrite history in CHANGELOG entries from older versions; just add a clarifying entry under `[Unreleased]`.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1
  - **Blocks**: none
  - **Blocked By**: None.

  **References**:
  - **Pattern**: existing CHANGELOG.md structure.
  - **External**: keep-a-changelog convention.
  - **WHY**: Documentation drift creates user confusion. Killing the SF_ENABLE_PHASE7 ghost reference makes future contributors less likely to look for an option that doesn't exist.

  **Acceptance Criteria**:
  - [ ] `grep -c 'SF_ENABLE_PHASE7' CHANGELOG.md` is 0 OR the only remaining mention is the explicit `[Unreleased]` clarifying entry.
  - [ ] `grep -c 'SF_ENABLE_PHASE7' CMakeLists.txt` is 0.
  - [ ] `.gitignore` includes `build*/` pattern.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: CHANGELOG cleaned
    Tool: Bash
    Preconditions: tree clean.
    Steps:
      1. Read CHANGELOG.md; identify all SF_ENABLE_PHASE7 mentions.
      2. Either remove them or replace with a clarifying note in [Unreleased].
      3. Verify final grep count is 0 OR 1 (the clarifying note).
    Expected Result: ghost reference resolved.
    Evidence: CHANGELOG.md diff in commit.

  Scenario: gitignore covers stale build dirs
    Tool: Bash
    Preconditions: same
    Steps:
      1. cat .gitignore | grep -E '^build'
      2. If missing, add `build*/`.
    Expected Result: build dirs ignored by git.
    Evidence: .gitignore diff (if any change).
  ```

  **Commit**: YES
  - Message: `docs(changelog,docs): drop SF_ENABLE_PHASE7 ghost reference; cleanup-note for stale builds`
  - Files: `CHANGELOG.md .gitignore? AGENTS.md?`
  - Pre-commit: none

- [ ] 1.7 **Apply `emevd_internal.h` relocation decision from T0.7**

  **What to do**:
  1. Read `.sisyphus/evidence/emevd-internal-use.md` produced by T0.7.
  2. If verdict is **KEEP-IN-PLACE**: this task is a no-op; just add a comment line at top of the header saying "format-local internal header; keep colocated with format implementation". Commit that one-liner.
  3. If verdict is **RELOCATE-TO-INTERNAL**:
     - `git mv src/script/emevd_internal.h src/internal/emevd_internal.h`
     - Update every `#include "emevd_internal.h"` (or relative path) to `#include "internal/emevd_internal.h"`.
     - Verify `cmake --build build-mingw` succeeds with zero warnings.
     - Verify `ctest --test-dir build-mingw -L script` passes.

  **Must NOT do**:
  - Do NOT relocate any other `*_internal.h` (msbs_internal.h, tae_internal.h, etc.).
  - Do NOT modify the header's contents — only its location and the includer paths.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (touches files unrelated to other Wave 1 tasks if action = relocate)
  - **Parallel Group**: Wave 1
  - **Blocks**: none
  - **Blocked By**: T0.7 (audit must produce verdict)

  **References**:
  - **Pattern**: `src/internal/sf_internal.h` etc. — convention for cross-module internals.
  - **WHY**: Either confirms a convention (KEEP) or aligns with the rest (RELOCATE). Either way, makes intent explicit.

  **Acceptance Criteria**:
  - [ ] If KEEP: header has a leading comment documenting the convention; commit message references T0.7 evidence.
  - [ ] If RELOCATE: file is at `src/internal/emevd_internal.h`, no longer at `src/script/emevd_internal.h`; all includers updated; full ctest -L script passes.
  - [ ] No new warnings under `-Werror`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Relocation succeeds with includer updates (RELOCATE path)
    Tool: Bash
    Preconditions: T0.7 verdict = RELOCATE.
    Steps:
      1. git mv src/script/emevd_internal.h src/internal/emevd_internal.h
      2. grep -rln 'emevd_internal.h' src/ | xargs sed -i 's,"emevd_internal.h","internal/emevd_internal.h",g'  (or use Edit per-file)
      3. cmake --build build-mingw 2>&1 | tee /tmp/move.log
      4. ctest --test-dir build-mingw -L script --output-on-failure
    Expected Result: build green, tests green.
    Evidence: .sisyphus/evidence/task-1.7-relocate.log

  Scenario: No-op when KEEP-IN-PLACE
    Tool: Bash
    Preconditions: T0.7 verdict = KEEP-IN-PLACE.
    Steps:
      1. Add one-line comment to top of src/script/emevd_internal.h.
      2. cmake --build build-mingw; verify zero warnings.
    Expected Result: tree builds; one-line doc addition only.
    Evidence: git diff src/script/emevd_internal.h
  ```

  **Commit**: YES
  - Message (RELOCATE): `refactor(internal): relocate emevd_internal.h to src/internal/`
  - Message (KEEP): `docs(internal): annotate emevd_internal.h as format-local`
  - Files: per verdict
  - Pre-commit: build + script ctest

- [ ] 1.8 **Fix reserve/fill bugs surfaced by T0.2**

  **What to do**:
  1. Read `.sisyphus/evidence/reserve-fill-audit.md`. Pull out the "Bug" rows.
  2. For each bug: read the upstream `.cs` to confirm the correct fix (per AGENTS.md STRICT UPSTREAM REFERENCE rule).
  3. Apply the fix (add the missing `fill_*` call on the correct control-flow path).
  4. Add a regression test under `tests/<area>/test_<format>_reserve_fill_<bug>.c` that reproduces the bug and verifies the fix (writes a fixture, calls the affected writer, asserts `sf_binary_writer_finish` returns SF_OK).
  5. Re-run `ctest -L core -L archive -L param -L script -L map -L geom -L anim`. All green.

  **Must NOT do**:
  - Do NOT extract the reserve/fill scaffold macro here — that's Wave 2.3. This task only fixes bugs.
  - Do NOT change `sf_binary_writer_t` internals beyond what the fix requires.
  - Do NOT silence the existing `SF_ERR_INTERNAL` safety net.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (each bug is in a separate file)
  - **Parallel Group**: Wave 1
  - **Blocks**: T2.3 (scaffold macro is now safe to extract)
  - **Blocked By**: T0.2

  **References**:
  - **Pattern**: existing `sf_binary_writer_fill_*` patterns; per-format reader/writer pairings.
  - **External**: per-bug, the corresponding upstream `.cs` file (cited in evidence).
  - **WHY**: Freezing bugs into a macro abstraction would make them harder to find later. Fix first, abstract after.

  **Acceptance Criteria**:
  - [ ] If T0.2 found ≥1 bug: each is fixed with a corresponding regression test added; ctest covers the regressions.
  - [ ] If T0.2 found 0 bugs: this task is a no-op; document confirmation in commit message.
  - [ ] `ctest --test-dir build-mingw -L 'core|archive|param|script|map|geom|anim' --output-on-failure` 100% pass.
  - [ ] New regression tests appear in `tests/` and are registered via `sf_add_test`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Bug present, fix lands cleanly
    Tool: Bash
    Preconditions: T0.2 evidence file lists ≥1 Bug row.
    Steps:
      1. For each Bug row: read its file:line; read upstream .cs; apply the missing fill_*.
      2. Add regression test that constructs minimal fixture, runs writer, asserts SF_OK.
      3. cmake --build build-mingw; ctest --test-dir build-mingw -L <area> --output-on-failure
    Expected Result: regression tests pass; no other tests regress.
    Evidence: .sisyphus/evidence/task-1.8-fix-<N>.log

  Scenario: T0.2 found no bugs
    Tool: Bash
    Preconditions: T0.2 evidence: 0 Bug rows.
    Steps:
      1. Commit a doc entry to CHANGELOG noting the audit cleared.
      2. Verify nothing else changes.
    Expected Result: no source changes; CHANGELOG entry only.
    Evidence: .sisyphus/evidence/task-1.8-no-bugs.log
  ```

  **Commit**: per-bug (one commit per fix)
  - Message: `fix(<area>): pair reserve_<type> with fill_<type> on <control-flow-path> (upstream <CS file>:line)`
  - Files: per bug
  - Pre-commit: `ctest --test-dir build-mingw -L <area> --output-on-failure`

### Wave 2 — DEDUP HELPERS

> Wave 2 extracts cross-module helpers — but only those that survived a Wave 0 audit OR a per-task
> mini-audit at the start of the wave. The principle is **evidence before extraction**.

- [ ] 2.1 **Adopt `sf_get_decompressed_reader` at the 7 caller sites**

  **What to do**:
  1. For each of the 7 callers found by Wave 0 (`src/compression/dcx.c`, `src/core/sf_util.c`, `src/archive/bxf3.c`, `src/archive/bxf4.c`, `src/archive/tpf.c`, `src/archive/bnd4.c`, `src/archive/bnd3.c`), inspect the surrounding code that decides between "DCX-wrapped input" and "raw input".
  2. If the caller's pattern matches the helper signature (sniff → allocate → decompress → wrap in new reader), replace the inline duplication with a call to `sf_get_decompressed_reader`.
  3. If the caller has additional side effects (logging, custom error mapping), preserve those at the call site (extract only the unique-to-the-pattern code).
  4. Run the relevant ctest labels (`archive`, `compression`, `e2e_er`) — all must remain green.

  **Must NOT do**:
  - Do NOT change `sf_get_decompressed_reader` signature (would break public API).
  - Do NOT force a fit where the caller's error-handling diverges substantially.
  - Do NOT inline the helper back at any site.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — 7 sites; per-site context required.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES (one task per file, fan out)
  - **Parallel Group**: Wave 2
  - **Blocks**: none
  - **Blocked By**: T1.1 (baseline so we can verify zero symbol change), T0.2 (writer audit complete to know reader paths are safe to touch)

  **References**:
  - **Pattern**: `src/core/sf_util.c:18-74` — the helper itself.
  - **API/Type**: `include/souls_formats/sf_dcx.h::sf_dcx_compression_info_t` and `include/souls_formats/sf_io.h::sf_binary_reader_t`.
  - **External**: Upstream `Utilities/SFUtil.cs::GetDecompressedBinaryReader` — the canonical mirror.
  - **WHY**: 7 callers duplicate ~15-30 LOC each ≈ 100-200 LOC of dedup opportunity, all matching an existing helper we already shipped.

  **Acceptance Criteria**:
  - [ ] After adoption: `grep -rEn 'sf_dcx_decompress_from_buffer\(|sf_dcx_decompress_from_stream\(' src/archive/ src/compression/dcx.c` shows ≥4 fewer direct-call sites than baseline.
  - [ ] All 7 caller files compile clean; their tests pass.
  - [ ] `ctest --test-dir build-mingw -L 'archive|compression|e2e_er' --output-on-failure` green.
  - [ ] Symbol export unchanged.
  - [ ] CHANGELOG `[Unreleased]/Internal` entry mentions the dedup with caller-site count delta.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Adoption at 7 callers preserves byte output
    Tool: Bash
    Preconditions: golden hashes pinned (W1.1).
    Steps:
      1. For each caller, apply the adoption edit.
      2. cmake --build build-mingw
      3. ctest --test-dir build-mingw -L golden --output-on-failure
      4. ctest --test-dir build-mingw -L 'archive|compression' --output-on-failure
    Expected Result: golden hashes unchanged; format tests green.
    Evidence: .sisyphus/evidence/task-2.1-ctest.log

  Scenario: Caller pattern diverges, skip extraction
    Tool: Read
    Preconditions: same
    Steps:
      1. Inspect one of the 7 callers; find it does custom logging between sniff and decompress.
      2. Leave the inline code in that one caller; document in commit message which caller(s) were skipped and why.
    Expected Result: 5-6 callers adopted; 1-2 documented as divergent.
    Evidence: commit message + .sisyphus/evidence/task-2.1-skipped-callers.md
  ```

  **Commit**: per-caller (one commit per file)
  - Message: `refactor(<area>): use sf_get_decompressed_reader (-NN LOC dup)`
  - Files: per caller
  - Pre-commit: `ctest --test-dir build-mingw -L <area> --output-on-failure`

- [ ] 2.2 **Magic-check helper macro (only if ≥80% sites are uniform after a quick audit)**

  **What to do**:
  1. Sample 20 of the 123 `SF_ERR_BAD_MAGIC` sites across diverse formats (BND3/BND4/BXF3/BXF4/BHD5/TPF/FMG/PARAM/MSBE/FLVER2/EMEVD/ESD/TAE/FXR3 — pick widely).
  2. For each, classify: does it call `sf_binary_reader_assert_ascii` or `assert_u32`? Are the surrounding diagnostics uniform?
  3. If ≥80% are pattern-identical (same assert call, same error code, same return-on-fail), extract a macro `SF_ASSERT_MAGIC(reader, magic_string_or_u32, sizeof_macro)`.
  4. If <80%, document the divergence in `.sisyphus/evidence/magic-check-sites.md` and SKIP the macro extraction — leave inline.
  5. If extracted: define macro in `src/internal/sf_internal.h`; replace the qualifying sites; verify tests.

  **Must NOT do**:
  - Do NOT force-uniform divergent diagnostics.
  - Do NOT change the user-facing error code (`SF_ERR_BAD_MAGIC` stays).
  - Do NOT add overload-style macros (`_Generic` is allowed only if it doesn't break MinGW under `-Werror`).

  **Recommended Agent Profile**:
  - **Category**: `deep` — careful pattern matching across many files.
  - **Skills**: none.

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 2
  - **Blocks**: none
  - **Blocked By**: T1.1 (baselines).

  **References**:
  - **Pattern**: e.g., `src/archive/bnd4.c` opening `read_from_*` functions (the magic-check pattern in ~5 lines at function entry).
  - **API/Type**: `sf_binary_reader_assert_ascii / assert_u32` in `sf_io.h`.
  - **External**: Upstream `Utilities/IO/BinaryReaderEx.cs::AssertASCII / AssertInt32` — the upstream aligned counterparts.
  - **WHY**: 123 sites of near-identical code. If uniform, big LOC reduction; if divergent, leaving them inline is safer than forcing a hostile abstraction.

  **Acceptance Criteria**:
  - [ ] `.sisyphus/evidence/magic-check-sites.md` exists.
  - [ ] If extracted: macro defined; ≥80% of the 20-sample sites updated; ctest unchanged; symbol export unchanged.
  - [ ] If skipped: evidence file documents why; no source change.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: ≥80% uniform, macro extracted successfully
    Tool: Bash
    Preconditions: W1.1 baselines.
    Steps:
      1. Survey 20 sites manually (Read each); classify.
      2. If ≥16/20 uniform, write macro in sf_internal.h.
      3. Replace qualifying sites (use ast-grep or per-file Edit).
      4. cmake --build build-mingw && ctest --test-dir build-mingw -L 'archive|param|script|map|geom|anim' --output-on-failure
    Expected Result: macro lands; tests pass; LOC delta measurable.
    Evidence: .sisyphus/evidence/task-2.2-macro-adoption.log

  Scenario: Divergence too high; skip
    Tool: Read
    Preconditions: same
    Steps:
      1. Survey shows divergent error codes or extra logging at >4/20 sites.
      2. Document in evidence file with example divergences.
      3. No source change; CHANGELOG note explaining the skip.
    Expected Result: evidence file with verdict SKIP and concrete examples.
    Evidence: .sisyphus/evidence/magic-check-sites.md
  ```

  **Commit**: YES (single commit if extracted; one doc commit if skipped)
  - Message (extracted): `refactor(internal): SF_ASSERT_MAGIC helper macro (-NN LOC dup across <K> sites)`
  - Message (skipped): `docs(audit): magic-check divergence prevents macro extraction`
  - Files: per outcome
  - Pre-commit: `ctest --test-dir build-mingw -L 'archive|param|script|map|geom|anim'`

- [ ] 2.3 **Reserve/fill scaffold macro (gated by T0.2 clearance + T1.8 fixes)**

  **What to do**: After T0.2 verdict shows the 10-entry mismatch is fully explained AND T1.8 has fixed any bugs, extract a `SF_RESERVE_FILL_PAIR(writer, name, type, value)` style macro/inline helper that bundles the common reserve/fill convention. Replace ≥80% of the 249 sites if uniform; document any skipped sites. Verify ctest covers all affected formats.

  **Must NOT do**: Do NOT mask the `SF_ERR_INTERNAL` safety net on `_finish`; do NOT alter the `name+type` pairing semantics (the upstream contract).

  **Agent**: `deep` — same risk profile as T2.2.

  **Parallelization**: YES; Wave 2; blocks: none; blocked by: T0.2 + T1.8.

  **References**:
  - Pattern: `src/core/binary_writer.c` reserve/fill impl; example call sites `src/archive/bnd4.c` write path.
  - External: Upstream `BinaryWriterEx.cs::Reserve*` + `Fill*` paired pattern.
  - **WHY**: 249 paired sites; ≥80% likely uniform per T0.2 audit.

  **Acceptance**:
  - [ ] Macro defined in `src/internal/sf_internal.h`.
  - [ ] LOC delta measurable (CHANGELOG note with before/after count).
  - [ ] Symbol export unchanged.
  - [ ] `ctest -L 'archive|param|script|map|geom|anim'` 100% pass.

  **QA Scenarios**:
  ```
  Scenario: Macro extraction preserves byte output
    Tool: Bash; Preconditions: T0.2 cleared.
    Steps: extract macro → replace sites → ctest -L 'golden|archive|param|map|geom|anim'
    Expected: golden + format tests green; LOC delta ≥ 200 saved.
    Evidence: .sisyphus/evidence/task-2.3-extraction.log

  Scenario: Edge-case site refuses to fit (skip)
    Tool: Read; Preconditions: same.
    Steps: identify divergent site → leave inline → CHANGELOG note.
    Expected: doc'd skips ≤ 20% of sites.
    Evidence: .sisyphus/evidence/task-2.3-skipped.md
  ```

  **Commit**: `refactor(internal): SF_RESERVE_FILL macro (-NN LOC across <K> sites)` — `src/internal/sf_internal.h` + per-file replacements — pre-commit: full ctest.

- [ ] 2.4 **e2e test helper consolidation into a static lib target**

  **What to do**: Currently each e2e test target uses `target_sources(... e2e/er_test_helper.c)` (recompiling the helper per target — ~30 recompilations). Create a CMake static lib `souls_formats_e2e_helpers` containing all of `er_test_helper.c`, `nightreign_test_helper.c`, `ac6_test_helper.c`, `sekiro_test_helper.c`. Each e2e test target instead `target_link_libraries(... PRIVATE souls_formats_e2e_helpers)`. Verify build time delta (`time cmake --build build-mingw` before vs after; report in CHANGELOG note). Verify all e2e tests still build and pass.

  **Must NOT do**: Do NOT make the helper lib public (it's test-only); do NOT remove the `SF_E2E_*` compile definitions from individual test targets (those carry the per-test path constants).

  **Agent**: `unspecified-high` — CMake fan-out + verify build-time win.

  **Parallelization**: YES; Wave 2; blocks: none; blocked by: T1.5 (label unification).

  **References**:
  - Pattern: `tests/CMakeLists.txt:105-176, 354-415` (per-target `target_sources(... er_test_helper.c)`).
  - **WHY**: Removes ~30 lines of CMake boilerplate; cuts compile work substantially for the e2e portion of the test suite.

  **Acceptance**:
  - [ ] `souls_formats_e2e_helpers` target exists in tests/CMakeLists.txt.
  - [ ] No remaining `target_sources(... e2e/er_test_helper.c)` or analogous lines.
  - [ ] `ctest --test-dir build-mingw -L 'e2e|e2e_er|e2e_ac6|e2e_sekiro|e2e_nightreign'` matches baseline skip-count.
  - [ ] Build time for `--target souls_formats_test_*_e2e_*` reduces vs baseline (report in CHANGELOG).

  **QA Scenarios**:
  ```
  Scenario: e2e helpers in static lib, all e2e tests build
    Tool: Bash; Preconditions: T1.5 landed.
    Steps: rebuild from clean; verify all e2e .exe present.
    Expected: same set of tests as before; same skip-count.
    Evidence: .sisyphus/evidence/task-2.4-build.log

  Scenario: Per-test SF_E2E_* macros still flow through
    Tool: Bash; Preconditions: same.
    Steps: run ER e2e test; verify it reads from `C:/Games/ELDEN RING`.
    Expected: helper compiles once, used by N targets, each with its own macro defs.
    Evidence: .sisyphus/evidence/task-2.4-defs-flow.log
  ```

  **Commit**: `build(tests): consolidate e2e helpers into souls_formats_e2e_helpers static lib` — `tests/CMakeLists.txt tests/e2e/CMakeLists.txt` — pre-commit: full e2e ctest.

- [ ] 2.5 **Document goto-cleanup convention (no global macro; documentation only)**

  **What to do**: With 812 `goto cleanup` / `fail` / `err` sites across src/, a global cleanup-macro abstraction would be too invasive. Instead: write a `docs/api-mapping/POLICY.md §X "Error cleanup convention"` documenting the established pattern (the prevalent name → file:line:N typical occurrences) so future contributors know not to invent new ad-hoc patterns. No source changes.

  **Must NOT do**: Do NOT extract a global macro; do NOT touch any source file.

  **Agent**: `quick`.

  **Parallelization**: YES; Wave 2; blocks: none; blocked by: T1.1.

  **References**:
  - Pattern: sample any 5-10 well-formed `goto cleanup` blocks (e.g., `src/archive/bnd4.c`) and document the convention.
  - **WHY**: 812 sites is too many to abstract safely; documenting the pattern is the same value at zero risk.

  **Acceptance**:
  - [ ] `docs/api-mapping/POLICY.md` has a new section "Error cleanup convention" with one canonical example.

  **QA Scenarios**:
  ```
  Scenario: Documentation lands; no source diff
    Tool: Bash; Steps: git diff src/ → empty; git diff docs/api-mapping/POLICY.md → has section addition.
    Expected: docs change only.
    Evidence: git diff in commit.

  Scenario: Canonical example actually matches at least 3 existing sites
    Tool: Bash; Steps: grep -A20 for the example pattern; expect ≥3 hits.
    Expected: validation passes.
    Evidence: .sisyphus/evidence/task-2.5-pattern-validation.log
  ```

  **Commit**: `docs(policy): document goto-cleanup convention` — `docs/api-mapping/POLICY.md` — pre-commit: none.

### Wave 3 — PER-ENTRY ALLOC REDUCTION

> 5 tasks; each operates only on a hot path approved by T0.6's alloc-site audit.
> If T0.6 says a path is per-archive (not per-entry), the corresponding task is downgraded to a no-op.

- [ ] 3.1 **BND3/BND4: bulk-allocate names array in single block**

  **What to do**: In `src/archive/bnd3.c` and `src/archive/bnd4.c`, the read paths allocate `name_copy` via `sf_strdup` per entry inside a loop (confirmed at bnd4.c:366, :771). Replace the per-entry `sf_strdup` with a single bulk allocation: compute `Σ strlen(headers[i].name_utf8)+1` first, allocate one block, copy each name into the block, store offsets. Update `_destroy` to free the single block instead of per-entry `sf_xfree`. Same change on the write path.

  **Must NOT do**: Do NOT change the public field type (still `char *name`); only change ownership semantics internally. Do NOT intern across multiple bnd reads (no global pool).

  **Agent**: `deep`.

  **Parallelization**: YES; Wave 3; blocks: T3.2 (BXF mirrors this); blocked by: T0.6.

  **References**:
  - Pattern: `src/archive/bnd4.c:366, :771` (per-entry strdup); `src/internal/sf_internal.h::sf_strdup`.
  - External: Upstream `Formats/Binder/BND4/BND4.cs::Read` — per-entry name is fine in C# (GC); C extension is the bulk-pool, documented in `extensions.md`.
  - **WHY**: BND4 chrbnd with N=50 entries = 50 alloc calls just for names. Bulk-alloc reduces to 1 per archive.

  **Acceptance**:
  - [ ] Per-archive alloc count for BND4 read path reduces by ≥(N-1) where N is entry count (microbenchmark on synthetic + ER c0000.chrbnd).
  - [ ] `ctest -L archive -L e2e_er -R bnd` 100% pass.
  - [ ] Golden hashes unchanged.
  - [ ] `_destroy` correctly frees the bulk block (verify under ASan).
  - [ ] `docs/api-mapping/extensions.md` row added for the name-pool optimization.

  **QA Scenarios**:
  ```
  Scenario: Alloc count drops for synthetic BND4 with N=10 entries
    Tool: Bash + custom counting allocator; Preconditions: T0.6 audit cleared.
    Steps: build with counting allocator override; read synthetic; report alloc count.
    Expected: per-entry strdup count drops from 10 → 1.
    Evidence: .sisyphus/evidence/task-3.1-alloc-count.log

  Scenario: ASan still clean after _destroy redesign
    Tool: Bash; Preconditions: same.
    Steps: ctest --test-dir build-asan -L archive --output-on-failure.
    Expected: zero leaks, zero UB.
    Evidence: .sisyphus/evidence/task-3.1-asan.log
  ```

  **Commit**: `perf(archive): bulk-allocate BND3/BND4 name array (-N allocs/archive)` — `src/archive/bnd3.c src/archive/bnd4.c docs/api-mapping/extensions.md` — pre-commit: ctest + asan ctest.

- [ ] 3.2 **BXF3/BXF4: mirror BND3/BND4 optimization**

  **What to do**: Same pattern as T3.1 applied to `src/archive/bxf3.c` and `src/archive/bxf4.c`. The BHD/BDT split doesn't change the per-entry alloc pattern materially.

  **Must NOT do**: Same constraints as T3.1.

  **Agent**: `deep`.

  **Parallelization**: YES (after T3.1 establishes the pattern); Wave 3; blocks: none; blocked by: T3.1.

  **References**:
  - Pattern: T3.1's implementation; `src/archive/bxf4.c` analogous loops.
  - **WHY**: BXF files mirror BND structure; same win.

  **Acceptance**:
  - [ ] Alloc count drops similarly to T3.1 on synthetic + e2e BXF4.
  - [ ] `ctest -L archive -L e2e_er -R bxf` 100% pass.
  - [ ] Golden + ASan green.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Alloc count drops for synthetic BXF4 with N=10 entries (mirrors T3.1)
    Tool: Bash + counting allocator override.
    Preconditions: T3.1 landed; counting allocator helper from T3.1 reused.
    Steps:
      1. cmake --build build-mingw --target souls_formats_test_bxf4_synthetic
      2. Run with counting allocator; capture per-entry strdup call count for bxf4 read.
      3. Compare to baseline captured in T0.6 evidence file.
    Expected Result: per-entry strdup count drops from 10 → 1 (matching BND4 reduction shape).
    Evidence: .sisyphus/evidence/task-3.2-alloc-count.log

  Scenario: ASan + golden roundtrip preserved after _destroy redesign
    Tool: Bash.
    Preconditions: same.
    Steps:
      1. cmake --build build-asan && ctest --test-dir build-asan -L archive -R bxf --output-on-failure
      2. ctest --test-dir build-mingw -L golden -R bxf --output-on-failure
    Expected Result: zero leaks, zero UB, golden hashes unchanged.
    Evidence: .sisyphus/evidence/task-3.2-asan-golden.log
  ```

  **Commit**: `perf(archive): bulk-allocate BXF3/BXF4 name array` — `src/archive/bxf3.c src/archive/bxf4.c` — pre-commit: ctest -L 'archive|golden' + asan -L archive.

- [ ] 3.3 **BHD5: entry-list bulk alloc + name pool**

  **What to do**: `src/archive/bhd5.c` (37 alloc calls) is the canonical large-N case (ER Data0 has ~25K entries per bhd). Replace per-bucket / per-entry `sf_xalloc` calls with a single bulk allocation per bucket array AND a single contiguous name pool for all entry names in the file. Update `sf_bhd5_destroy` to free the two blocks instead of N+M per-entry frees.

  **Must NOT do**: Do NOT change the public field types (`entry->name` still `const char *`); do NOT cause iteration-order changes in the entry list (golden hash protection).

  **Agent**: `deep`.

  **Parallelization**: YES (parallel with T3.4, T3.5); Wave 3; blocks: T4.1 (BHD5 khash builds on cleaned-up bhd5.c); blocked by: T0.6.

  **References**:
  - Pattern: `src/archive/bhd5.c` (existing reader and destroy fns); `tests/archive/test_bhd5_synthetic.c` (synthetic fixture).
  - External: Upstream `Formats/BHD5.cs::Read` (top-level standalone file, not in a subdirectory) — same per-entry alloc in C#; same C-extension exception.
  - **WHY**: 25K entries × ~30 bytes alloc overhead each ≈ 750 KB of allocator-call overhead per Data0.bhd parse. Bulk alloc reduces to ~2 calls total.

  **Acceptance**:
  - [ ] BHD5 read for ER Data0 shows ≥99% reduction in alloc count (one big alloc + one big string pool replaces 25K per-entry).
  - [ ] `ctest -L archive -L e2e_er -R bhd5` 100% pass.
  - [ ] Golden hashes unchanged.
  - [ ] ASan clean.

  **QA Scenarios**:
  ```
  Scenario: ER Data0 BHD5 read goes from ~25k allocs to <10
    Tool: Bash + counting allocator; Preconditions: ER game files present.
    Steps: run e2e test with counting allocator; capture diff.
    Expected: count reduction ≥ 25000.
    Evidence: .sisyphus/evidence/task-3.3-alloc-count.log

  Scenario: Synthetic BHD5 round-trip preserves bytes
    Tool: Bash; Preconditions: golden hashes pinned.
    Steps: ctest -L golden -R bhd5.
    Expected: pass.
    Evidence: .sisyphus/evidence/task-3.3-golden.log
  ```

  **Commit**: `perf(archive): BHD5 bulk-alloc entries + name pool (-Nk allocs/bhd)` — `src/archive/bhd5.c docs/api-mapping/extensions.md` — pre-commit: ctest + golden.

- [ ] 3.4 **FXR3 XML read path: scratch buffer for transient strings**

  **What to do**: `src/effects/fxr3_xml_read.c` has 76 alloc calls — many for transient strings consumed during mxml parsing then immediately copied into final POD. Introduce a per-parse scratch arena (single `sf_xalloc` of estimated size, bump-pointer allocation, freed all at once on parse complete or error). Persistent fields (strings stored in the final FXR3 tree) keep using `sf_strdup`.

  **Must NOT do**: Do NOT cache the scratch across parses (one arena per parse); do NOT change the public FXR3 API.

  **Agent**: `unspecified-high`.

  **Parallelization**: YES; Wave 3; blocks: none; blocked by: T0.6.

  **References**:
  - Pattern: `src/effects/fxr3_xml_read.c` — heaviest alloc file.
  - External: Upstream FXR3 XML reader.
  - **WHY**: 76 alloc/free pairs per FXR3 XML parse is non-trivial; arena reduces to ~1 per parse for transient strings.

  **Acceptance**:
  - [ ] FXR3 XML synthetic test alloc count drops by ≥40% on the parse path.
  - [ ] `ctest -L anim` 100% pass.
  - [ ] ASan clean.
  - [ ] Golden hashes unchanged.

  **QA Scenarios**:
  ```
  Scenario: Synthetic FXR3 XML parse drops transient allocs
    Tool: Bash + counting allocator. Steps: run XML synthetic test; report.
    Expected: ≥40% fewer allocator calls.
    Evidence: .sisyphus/evidence/task-3.4-alloc.log

  Scenario: ASan and roundtrip still clean
    Tool: Bash. Steps: ctest --test-dir build-asan -L anim.
    Expected: pass.
    Evidence: .sisyphus/evidence/task-3.4-asan.log
  ```

  **Commit**: `perf(effects): FXR3 XML scratch arena (-NN%/parse)` — `src/effects/fxr3_xml_read.c docs/api-mapping/extensions.md` — pre-commit: ctest + asan.

- [ ] 3.5 **PARAM row data: contiguous arena per param (audited subset only)**

  **What to do**: If T0.6 audit confirms PARAM rows are allocated per-row (likely), introduce a single bulk allocation for all row data of one PARAM. Update `sf_param_destroy` accordingly. Apply only if profitable per audit (PARAMs with ≥1000 rows).

  **Must NOT do**: Do NOT change public row API; do NOT batch across multiple PARAMs.

  **Agent**: `deep`.

  **Parallelization**: YES; Wave 3; blocks: none; blocked by: T0.6.

  **References**: `src/param/param.c`; upstream `PARAM.cs::Read`. **WHY**: large PARAMs amortize a single bulk alloc to ~1/PARAM vs N/PARAM.

  **Acceptance**:
  - [ ] `ctest -L param -L e2e_er -R param` 100% pass.
  - [ ] Golden hashes unchanged.
  - [ ] If T0.6 audit downgrades this task → no-op with CHANGELOG note.
  - [ ] ASan clean.

  **QA Scenarios**:
  ```
  Scenario: Large PARAM read alloc count drops
    Tool: Bash + counting allocator. Steps: read SpEffectParam from regulation.bin; count.
    Expected: per-row alloc reduces by ≥N-1.
    Evidence: .sisyphus/evidence/task-3.5-alloc.log

  Scenario: Audit downgrades (no-op)
    Tool: Bash. Steps: confirm evidence file; CHANGELOG note.
    Expected: zero source change.
    Evidence: CHANGELOG diff.
  ```

  **Commit**: `perf(param): PARAM row bulk-alloc (if T0.6 cleared)` — `src/param/param.c docs/api-mapping/extensions.md` — pre-commit: param ctest + asan.

### Wave 4 — ALGORITHMIC OPTIMIZATIONS (each task gated by Wave 0)

> Three high-risk perf changes. Each task is GO/NO-GO based on its Wave 0 audit.

- [ ] 4.1 **klib khash adoption in `src/archive/bhd5.c` entry lookup** (gated by T0.1 GO)

  **What to do**:
  1. If T0.1 verdict is NO-GO → skip task with CHANGELOG note explaining why.
  2. If GO: include klib via `cmake/deps/klib.cmake` (which is already in tree, just unused). Instantiate `KHASH_MAP_INIT_INT64(bhd5_lookup, sf_bhd5_entry_t *)` (or appropriate value type) in `src/archive/bhd5.c`.
  3. Build the hash on `sf_bhd5_open` (populate after entries are parsed); use it to back `sf_bhd5_find_by_hash` or equivalent. Preserve byte-identical write order (iterate insertion or sorted on write, NOT khash iteration order).
  4. Add `tests/archive/test_bhd5_lookup_perf.c` microbench measuring lookup time on a 25K-entry synthetic.
  5. Document in `docs/api-mapping/extensions.md`.

  **Must NOT do**: Do NOT adopt klib in any other module. Do NOT change `sf_bhd5_*` public API. Do NOT change on-disk byte format.

  **Agent**: `deep`.

  **Parallelization**: YES; Wave 4; blocks: none; blocked by: T0.1 GO + T3.3 (cleaned-up bhd5.c).

  **References**:
  - Pattern: `src/archive/bhd5.c` (existing linear search). klib README example: `KHASH_MAP_INIT_INT64`.
  - External: Upstream `BHD5.cs` uses `Dictionary<long, BucketEntry>` — same semantics.
  - **WHY**: O(1) worst-case lookup; even at ~25K entries, predictable lookup latency matters for downstream tools that scan thousands of paths.

  **Acceptance**:
  - [ ] `cmake --build build-mingw` clean under all 3 toolchains.
  - [ ] `ctest -L archive -L e2e_er -R bhd5 -L golden` 100% pass.
  - [ ] Microbench: lookup time on 25K synthetic ≤ 10µs/lookup (vs the current linear scan baseline).
  - [ ] `docs/api-mapping/extensions.md` row added.
  - [ ] ASan clean.

  **QA Scenarios**:
  ```
  Scenario: khash lookup matches linear scan results bit-identically
    Tool: Bash; Steps: run both impls on the same 100 random paths; compare results.
    Expected: 100/100 match.
    Evidence: .sisyphus/evidence/task-4.1-equiv.log

  Scenario: Write path preserves byte order
    Tool: Bash; Steps: ctest -L golden -R bhd5.
    Expected: golden hashes unchanged.
    Evidence: .sisyphus/evidence/task-4.1-golden.log
  ```

  **Commit**: `perf(archive): adopt klib khash for BHD5 entry lookup (O(1) worst-case)` — `src/archive/bhd5.c cmake/deps/klib.cmake docs/api-mapping/extensions.md tests/archive/test_bhd5_lookup_perf.c` — pre-commit: full archive + golden + asan ctest.

- [ ] 4.2 **PARAMDEF apply-path field-layout precompute** (gated by T0.3 GO)

  **What to do**:
  1. If T0.3 verdict is NO-GO → skip with CHANGELOG note.
  2. If GO: introduce an internal `sf_paramdef_field_layout_t` struct cached inside the def (or alongside it). Compute once on first apply; reuse on subsequent applies. Use a generation counter or immutability assertion to invalidate if the def changes.
  3. Verify the precompute is invisible to callers (same outputs, same alloc semantics from their perspective).
  4. Add benchmark comparing per-row apply time before/after on the largest PARAM in regulation.bin.

  **Must NOT do**: Do NOT change public def or apply API; do NOT cache across def-mutation boundaries.

  **Agent**: `deep`.

  **Parallelization**: YES; Wave 4; blocks: none; blocked by: T0.3 GO.

  **References**: `src/param/paramdef_apply.c`; upstream `PARAM.cs::ApplyParamdefCarefully`. **WHY**: amortizes O(F) field-layout cost across N rows (F=fields-per-def, N=rows-per-param).

  **Acceptance**:
  - [ ] `ctest -L param -L e2e_er -R apply` 100% pass.
  - [ ] Per-row apply wall-time on a 10000-row PARAM reduces by ≥30%.
  - [ ] No new public symbols (verify via objdump).
  - [ ] `docs/api-mapping/extensions.md` row.

  **QA Scenarios**:
  ```
  Scenario: Precompute reduces per-row time
    Tool: Bash + bench; Steps: bench before/after.
    Expected: ≥30% drop.
    Evidence: .sisyphus/evidence/task-4.2-bench.log

  Scenario: Mutation correctness
    Tool: Bash; Steps: apply def → mutate def → apply again; verify second result.
    Expected: cache invalidated correctly.
    Evidence: .sisyphus/evidence/task-4.2-mutation.log
  ```

  **Commit**: `perf(param): precompute PARAMDEF field layout (-NN% per-row)` — `src/param/paramdef_apply.c src/internal/paramdef_internal.h docs/api-mapping/extensions.md` — pre-commit: param ctest + bench.

- [ ] 4.3 **binary_reader endian inline fast-path** (gated by T0.4 GO)

  **What to do**:
  1. If T0.4 verdict is NO-GO (<5% win) → skip with CHANGELOG note.
  2. If GO: in `src/core/binary_reader.c`, replace the per-call `if (r->big_endian) {...}` macro branch with a branch annotated with `__builtin_expect` (LE = expected) AND cache the flag into a stack-local at loop entry where the compiler benefits. NO type-specialization (no new reader type). Public `sf_binary_reader_set_big_endian()` remains usable.
  3. Verify mid-stream toggle test still passes (T0.4 added the regression test).
  4. Re-run the FLVER2 microbench; commit results showing the ≥5% win.

  **Must NOT do**: Do NOT type-specialize readers. Do NOT silently change `set_big_endian` behavior. Do NOT remove the mutable-flag path.

  **Agent**: `ultrabrain` (perf-sensitive microbench + decision).

  **Parallelization**: YES; Wave 4; blocks: none; blocked by: T0.4 GO.

  **References**: `src/core/binary_reader.c:247`; upstream `BinaryReaderEx.cs::BigEndian` setter. **WHY**: Hot path; if ≥5% win, real production benefit.

  **Acceptance**:
  - [ ] FLVER2 decode bench reduces wall-time by ≥5% on representative input.
  - [ ] `ctest -L core` 100% pass; the toggle regression test passes.
  - [ ] `objdump` symbol export unchanged.

  **QA Scenarios**:
  ```
  Scenario: Bench shows ≥5% wall-time reduction
    Tool: Bash + microbench; Steps: rebench; compare to T0.4 baseline.
    Expected: ≥5% improvement.
    Evidence: .sisyphus/evidence/task-4.3-bench.log

  Scenario: Mid-stream toggle still works
    Tool: Bash; Steps: ctest -R binary_reader_endian.
    Expected: pass.
    Evidence: .sisyphus/evidence/task-4.3-toggle.log
  ```

  **Commit**: `perf(core): inline endian fast-path in binary_reader (+N% FLVER2)` — `src/core/binary_reader.c tests/core/test_binary_reader_endian_toggle.c` — pre-commit: core ctest + bench.

### Wave 5 — MSB SHARED SCAFFOLDING EXTRACTION (gated by T0.5)

> Strict scope limit: skeleton only. Per-subtype field reads are off-limits.

- [ ] 5.1 **Extract `msb_entry_list_read` / `msb_entry_list_write` into `msb_common.c`**

  **What to do**:
  1. If T0.5 verdict is NO-GO (extracted LOC <50 per module) → skip; T5.2/T5.3 also skip; CHANGELOG note.
  2. If GO: extract the scaffold (offset-table read, count check, allocator setup, index backfill) into helper functions in `src/map/msb_common.c`. Sign them carefully: `sf_msb_entry_list_read(...)` accepting a callback for per-entry parsing OR a tagged dispatch table.
  3. Choose technique (function-pointer table vs X-macro) and document in `docs/api-mapping/POLICY.md` as a generic-method C-style adaptation.

  **Must NOT do**: Do NOT touch per-subtype field reads. Do NOT change msbs/msbe/msbvi public APIs.

  **Agent**: `deep`.

  **Parallelization**: NO (T5.2 fans out after this); Wave 5; blocks: T5.2, T5.3; blocked by: T0.5 GO.

  **References**: `src/map/msb_common.c` (existing common); `src/map/msbs/parts_param.c:scaffold` etc.; upstream `MSB.cs::Param<T>` generic. **WHY**: documented per-module LOC savings from T0.5 audit.

  **Acceptance**:
  - [ ] `src/map/msb_common.c` has new helpers (verify via lsp_symbols).
  - [ ] `ctest -L map` 100% pass.
  - [ ] No public symbols added (helpers are internal).
  - [ ] POLICY.md updated.

  **QA Scenarios**:
  ```
  Scenario: Helpers compile and tests still pass
    Tool: Bash; Steps: build + ctest -L map.
    Expected: green.
    Evidence: .sisyphus/evidence/task-5.1-build.log

  Scenario: Helpers are used by ≥1 msb_* (smoke before T5.2 fan-out)
    Tool: Bash; Steps: refactor msbs/event_param.c as pilot; verify e2e ER.
    Expected: pilot passes; pattern is sound.
    Evidence: .sisyphus/evidence/task-5.1-pilot.log
  ```

  **Commit**: `refactor(map): extract msb_entry_list_read/write scaffolding` — `src/map/msb_common.c include/souls_formats/sf_msb.h docs/api-mapping/POLICY.md` — pre-commit: map ctest.

- [ ] 5.2 **Apply `msb_entry_list_*` to msbs/msbe/msbvi**

  **What to do**: For each of the 19 MSB .c files matching `src/map/msb{s,e,vi}/*.c` (6 msbs + 6 msbe + 7 msbvi), replace the per-file scaffold copy with calls to the new helpers from T5.1. Do this in three sub-PRs (one per game variant) so each is reversible. Per-subtype field reads remain untouched.

  **Must NOT do**: Do NOT touch the per-subtype tables. Do NOT collapse msbs/msbe/msbvi into one file. Do NOT change the per-format header (sf_msbs.h / sf_msbe.h / sf_msbvi.h).

  **Agent**: `unspecified-high` (3-way fan-out).

  **Parallelization**: 3 sub-tasks in parallel after T5.1; Wave 5; blocks: T5.3; blocked by: T5.1.

  **References**: T5.1 helpers; per-file scaffold positions from T0.5 evidence file. **WHY**: realize the LOC reduction.

  **Acceptance**:
  - [ ] Aggregate `src/map/` LOC reduces by 15–25% (NOT 50%+; that would mean creep).
  - [ ] `ctest -L map -L e2e_er -L e2e_sekiro -L e2e_ac6 -L e2e_nightreign` matches baseline counts and PASS/SKIP distribution.
  - [ ] Golden hashes for MSB synthetic round-trips unchanged.

  **QA Scenarios**:
  ```
  Scenario: All 4 e2e game variants stay green/skipped
    Tool: Bash; Steps: ctest matrix.
    Expected: skip-count stable; pass-count stable.
    Evidence: .sisyphus/evidence/task-5.2-matrix.log

  Scenario: LOC delta lands in 15–25% range
    Tool: Bash + cloc; Steps: cloc src/map/ before/after.
    Expected: within target.
    Evidence: .sisyphus/evidence/task-5.2-loc.log
  ```

  **Commit**: 3 commits — `refactor(map/msbs): use msb_entry_list_* helpers`, ditto msbe, ditto msbvi — files per game — pre-commit: per-game ctest.

- [ ] 5.3 **Document MSB shared-engine technique in POLICY.md / extensions.md**

  **What to do**: Write a section in `docs/api-mapping/POLICY.md` (or a new entry in `extensions.md`) explaining the chosen technique (callback-table vs X-macro), why it was chosen, and how it maps to upstream's C# `Param<T>` generic. Include the per-module LOC delta as evidence.

  **Must NOT do**: Do NOT make this a tutorial; keep it terse policy-level.

  **Agent**: `writing`.

  **Parallelization**: YES; Wave 5; blocks: none; blocked by: T5.2.

  **References**: T5.1/T5.2 final source; AGENTS.md §5.x; existing POLICY.md sections.

  **Acceptance**:
  - [ ] POLICY.md or extensions.md has new section with technique + rationale + LOC delta.
  - [ ] `docs/api-mapping/README.md` updated if a new section was added.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Documentation lands with required structure
    Tool: Bash.
    Preconditions: T5.1 + T5.2 landed.
    Steps:
      1. grep -qE '^## MSB shared-engine|^### MSB shared scaffolding' docs/api-mapping/POLICY.md docs/api-mapping/extensions.md
      2. grep -qE 'callback table|X-macro|function-pointer table' docs/api-mapping/POLICY.md docs/api-mapping/extensions.md  # technique cited
      3. grep -qE 'LOC|lines saved|reduction' docs/api-mapping/POLICY.md docs/api-mapping/extensions.md  # delta cited
      4. git diff --stat HEAD~1 HEAD -- src/   # should be empty (doc-only)
    Expected Result: new section exists, names the chosen technique, cites LOC delta; zero src/ changes.
    Evidence: .sisyphus/evidence/task-5.3-doc-validation.log

  Scenario: Cross-link to upstream Param<T> generic preserved
    Tool: Bash.
    Steps:
      1. grep -qE 'Param<T>|generic method|upstream MSBE|upstream MSBS|upstream MSBVI' docs/api-mapping/POLICY.md docs/api-mapping/extensions.md
    Expected Result: upstream generic cited in the policy doc per STRICT UPSTREAM REFERENCE.
    Evidence: .sisyphus/evidence/task-5.3-upstream-cite.log
  ```

  **Commit**: `docs(policy): document MSB shared-engine extraction technique` — `docs/api-mapping/POLICY.md docs/api-mapping/extensions.md docs/api-mapping/README.md?` — pre-commit: doc validation grep.

### Wave 6 — UPSTREAM GAP ANALYSIS → 10 CLUSTER PLANS

> Wave 6 runs AFTER Waves 1-5 land. Produces 10 self-contained `.sisyphus/plans/next-batch-*.md`
> planning files, each schedulable independently. Cluster boundaries fixed during interview.

#### Shared cluster-plan QA validator (`SHARED-CLUSTER-VALIDATOR`)

> Every T6.1–T6.10 task references this validator block by name. Run AFTER the cluster .md file is written.

```bash
# SHARED-CLUSTER-VALIDATOR — execute this against the cluster file under test.
# Pass argument: the cluster file path, e.g.: .sisyphus/plans/next-batch-legacy-binder.md
CLUSTER_FILE="$1"
test -f "$CLUSTER_FILE"

# 1) All 9 required sections are present in the file.
for SEC in \
  '^## TL;DR' \
  '^## Upstream formats covered' \
  '^## Must Have' \
  '^## Must NOT Have' \
  '^## Dependencies on prior clusters' \
  '^## Acceptance criteria' \
  '^## STRICT UPSTREAM REFERENCE' \
  '^## Estimated effort' \
  '^## Risk'; do
    grep -qE "$SEC" "$CLUSTER_FILE" || { echo "MISSING $SEC in $CLUSTER_FILE"; exit 1; }
done

# 2) At least one citation of an upstream .cs path inside SoulsFormats/.
grep -qE 'SoulsFormats/.+\.cs' "$CLUSTER_FILE"

# 3) Acceptance criteria block contains at least one executable bash command (lines beginning with
#    a command verb commonly used in our plans).
grep -qE '^\s*(cmake|ctest|grep|test |find |awk|comm|wc|gh|git|x86_64-w64-mingw32-objdump)\b' "$CLUSTER_FILE"

# 4) Cross-coverage: every upstream .cs path this cluster claims to cover is mapped to this cluster
#    in T6.0's evidence inventory (.sisyphus/evidence/upstream-inventory.md).
awk -v cluster="$(basename "$CLUSTER_FILE" .md | sed 's/^next-batch-//')" '
    /^path: SoulsFormats\/.+\.cs/ {p=$2}
    /^cluster: / && p { if ($2==cluster) print p; p="" }
' .sisyphus/evidence/upstream-inventory.md \
  | sort -u > /tmp/inv-claims-${cluster}.txt
grep -oE 'SoulsFormats/[A-Za-z0-9_/.]+\.cs' "$CLUSTER_FILE" \
  | sort -u > /tmp/cluster-cites-${cluster}.txt
# Cluster file must cite >= 80% of the upstream files inventory assigns to this cluster.
INV=$(wc -l < /tmp/inv-claims-${cluster}.txt)
CIT=$(comm -12 /tmp/inv-claims-${cluster}.txt /tmp/cluster-cites-${cluster}.txt | wc -l)
test "$INV" -eq 0 || awk -v inv="$INV" -v cit="$CIT" 'BEGIN { exit !(cit >= 0.8 * inv) }'

echo "VALIDATOR PASS: $CLUSTER_FILE"
```

> Save this snippet as `tests/cluster-plan-validator.sh` (committed by T6.0) so every T6.X task and
> the F1 final review can invoke it the same way:
> `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-<cluster>.md`
>
> Evidence file convention: every T6.X writes `.sisyphus/evidence/task-6.<n>-validator.log` containing
> the validator's stdout + exit code.

- [ ] 6.0 **Enumerate upstream Formats/*.cs surface inventory**

  **What to do**:
  1. Walk `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/` and produce an exhaustive inventory: for each `.cs` file or subdirectory, list its public class(es), public methods, approximate LOC, and a one-line description.
  2. Walk `Utilities/` similarly for ancillary surface (split it into the actual sub-trees: `Utilities/IO/`, `Utilities/Collections/`, `Utilities/Compression/`, `Utilities/Cryptography/`, `Utilities/Exceptions/`, `Utilities/Formats/`, `Utilities/Guessing/`, `Utilities/Text/`, `Utilities/Xml/`, plus the top-level standalone .cs files such as `Utilities/SFUtil.cs`, `Utilities/HexHelper.cs`, etc.). The upstream tree has NO `Other/` directory — confirm via `ls /home/soar/src/SoulsFormatsNEXT/SoulsFormats/` and document the upstream-root listing as part of the inventory's preamble.
  3. Cross-reference each upstream file with `docs/api-mapping/format-*.md` rows to determine: IMPLEMENTED (in our v1) / PARTIAL (some methods missing) / NOT IMPLEMENTED.
  4. Assign each NOT-IMPLEMENTED file to one of the 10 clusters (T6.1-T6.10). Document any file not fitting cleanly.
  5. Write `.sisyphus/evidence/upstream-inventory.md` with the full table.

  **Must NOT do**: Do NOT modify the upstream tree. Do NOT classify a format as IMPLEMENTED if any non-trivial method is missing.

  **Agent**: `librarian` (best at cross-repo surface mapping).

  **Parallelization**: NO (T6.1-T6.10 depend on this); Wave 6; blocks: T6.1-T6.10; blocked by: Waves 1-5 complete.

  **References**:
  - Pattern: `docs/api-mapping/README.md` index; `docs/api-mapping/format-*.md` rows.
  - External: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/` (read-only).
  - **WHY**: All cluster plans must reference real upstream paths + LOC; this enumeration is the source of truth.

  **Also part of T6.0**: Create `tests/cluster-plan-validator.sh` containing the `SHARED-CLUSTER-VALIDATOR` script defined above; make it executable (`chmod +x`); verify it self-runs on a minimal hand-written sample cluster file (`tests/cluster-plan-validator-self-test.md`) producing a PASS line and exit 0.

  **Acceptance**:
  - [ ] `.sisyphus/evidence/upstream-inventory.md` exists with every `.cs` file mapped.
  - [ ] Total row count ≈ 413 (upstream's `.cs` count); deviations explained.
  - [ ] Every NOT-IMPLEMENTED entry has a `cluster: <one of 10>` field.
  - [ ] Zero entries with `cluster: ?` (uncategorized go to `uncategorized-deferred`).
  - [ ] `tests/cluster-plan-validator.sh` exists, is executable, and passes self-test against `tests/cluster-plan-validator-self-test.md`.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Inventory complete and consistent (happy)
    Tool: Bash
    Preconditions: /home/soar/src/SoulsFormatsNEXT clone present.
    Steps:
      1. find /home/soar/src/SoulsFormatsNEXT/SoulsFormats -name '*.cs' | wc -l > /tmp/upstream-cs-count.txt   # baseline ~413
      2. awk '/^path:/{c++}END{print c}' .sisyphus/evidence/upstream-inventory.md > /tmp/inventory-row-count.txt
      3. diff /tmp/upstream-cs-count.txt /tmp/inventory-row-count.txt  # ≤ 5% deviation allowed
      4. awk '/^cluster: $/{print "MISSING";exit 1}END{print "OK"}' .sisyphus/evidence/upstream-inventory.md  # zero homeless rows
    Expected Result: every row has a cluster value; total ≈ 413; OK printed.
    Evidence: .sisyphus/evidence/upstream-inventory.md + /tmp/upstream-cs-count.txt + /tmp/inventory-row-count.txt

  Scenario: Validator script self-test (negative)
    Tool: Bash
    Preconditions: tests/cluster-plan-validator-self-test.md is a hand-written minimal sample.
    Steps:
      1. bash tests/cluster-plan-validator.sh tests/cluster-plan-validator-self-test.md ; echo $?
      2. Mutate the sample (remove the `## Must Have` line); rerun; expect non-zero exit.
      3. Restore.
    Expected Result: validator returns exit 0 on full sample, non-zero on missing-section sample.
    Evidence: .sisyphus/evidence/task-6.0-validator-self-test.log
  ```

  **Commit**: `audit(upstream): enumerate Formats/ and Utilities/ surface; add cluster-plan-validator.sh` — `.sisyphus/evidence/upstream-inventory.md tests/cluster-plan-validator.sh tests/cluster-plan-validator-self-test.md` — pre-commit: `bash tests/cluster-plan-validator.sh tests/cluster-plan-validator-self-test.md`.

- [ ] 6.1 **Write `next-batch-legacy-binder.md`**

  **What to do**: Per T6.0 inventory, draft the cluster plan covering BND, BND2, and any legacy Binder helpers missing from our v1 (the BinderHashTable variants for older games, BNDDataEntry layout differences). Each plan has these sections: TL;DR, Upstream formats covered (with .cs paths + LOC + one-line description), Must Have (formats, methods, fixtures), Must NOT Have, Dependencies on prior clusters, Acceptance criteria (executable build/test commands), STRICT UPSTREAM REFERENCE table, and an estimated effort + risk note.

  **Must NOT do**: Do NOT include MSB legacy variants (those go to T6.2); do NOT include BHD5 (already in v1).

  **Agent**: `writing`.

  **Parallelization**: YES (parallel with T6.2-T6.10); Wave 6; blocks: none; blocked by: T6.0.

  **References**: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/BND/` + `/BND2/`. **WHY**: legacy binder is the smallest cluster — good first follow-on plan.

  **Acceptance**:
  - [ ] File `.sisyphus/plans/next-batch-legacy-binder.md` exists with all 9 sections enforced by `SHARED-CLUSTER-VALIDATOR`.
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-binder.md` exits 0.
  - [ ] Cites every legacy-binder upstream `.cs` file from T6.0 inventory (`SoulsFormats/Formats/Binder/BND/*.cs`, `SoulsFormats/Formats/Binder/BND2/*.cs`).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SHARED-CLUSTER-VALIDATOR passes on legacy-binder cluster
    Tool: Bash
    Preconditions: T6.0 has written upstream-inventory.md and validator script.
    Steps:
      1. bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-binder.md 2>&1 | tee .sisyphus/evidence/task-6.1-validator.log
      2. test ${PIPESTATUS[0]} -eq 0
    Expected Result: validator prints "VALIDATOR PASS: .sisyphus/plans/next-batch-legacy-binder.md"; exit 0.
    Evidence: .sisyphus/evidence/task-6.1-validator.log

  Scenario: Coverage of upstream legacy Binder formats
    Tool: Bash
    Preconditions: same
    Steps:
      1. grep -oE 'SoulsFormats/Formats/Binder/(BND/|BND2/)[^ )]+\.cs' .sisyphus/plans/next-batch-legacy-binder.md | sort -u > /tmp/cluster-cites.txt
      2. find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/{BND,BND2} -name '*.cs' -printf '%P\n' | sed 's,^,SoulsFormats/Formats/Binder/,' | sort -u > /tmp/upstream-cs.txt
      3. comm -23 /tmp/upstream-cs.txt /tmp/cluster-cites.txt | wc -l   # missing citations
    Expected Result: ≤ 20% of upstream files unaccounted for (the rest must appear in the cluster cite list).
    Evidence: .sisyphus/evidence/task-6.1-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-legacy-binder.md` — `.sisyphus/plans/next-batch-legacy-binder.md` — pre-commit: `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-binder.md`.

- [ ] 6.2 **Write `next-batch-legacy-msb.md`**

  **What to do**: Cover MSB1, MSB2, MSB3, MSBAC4, MSBB, MSBD, MSBDR, MSBFA, MSBN, MSBV, MSBVD. Note interdependencies (MSBB is similar to MSB3; MSBDR extends MSBD; etc.). Acceptance commands include enumerating per-game .cs file paths and the corresponding game-fixture availability matrix.

  **Must NOT do**: Do NOT include MSBS/MSBE/MSBVI (in v1). Do NOT include MsbBoundingBox.cs or Shape.cs (those are shared headers — they're already partially covered).

  **Agent**: `writing`.

  **Parallelization**: YES; Wave 6; blocks: none; blocked by: T6.0.

  **References**: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/{MSB1,MSB2,...}/`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-msb.md` exits 0.
  - [ ] Cites every legacy-MSB upstream `.cs` (MSB1, MSB2, MSB3, MSBAC4, MSBB, MSBD, MSBDR, MSBFA, MSBN, MSBV, MSBVD subdirs).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: SHARED-CLUSTER-VALIDATOR passes on legacy-msb cluster
    Tool: Bash
    Preconditions: T6.0 outputs present.
    Steps:
      1. bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-msb.md 2>&1 | tee .sisyphus/evidence/task-6.2-validator.log
      2. test ${PIPESTATUS[0]} -eq 0
    Expected Result: VALIDATOR PASS line printed; exit 0.
    Evidence: .sisyphus/evidence/task-6.2-validator.log

  Scenario: Coverage of all 11 legacy MSB variants
    Tool: Bash
    Steps:
      1. for v in MSB1 MSB2 MSB3 MSBAC4 MSBB MSBD MSBDR MSBFA MSBN MSBV MSBVD; do
           grep -q "$v" .sisyphus/plans/next-batch-legacy-msb.md || { echo "MISSING $v"; exit 1; }
         done
    Expected Result: every variant name appears in the cluster .md.
    Evidence: .sisyphus/evidence/task-6.2-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-legacy-msb.md` — `.sisyphus/plans/next-batch-legacy-msb.md` — pre-commit: `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-msb.md`.

- [ ] 6.3 **Write `next-batch-legacy-flver.md`**

  **What to do**: Cover FLVER0, Edge Geometry / SPU vertex format / RSX vertex format (PS3-era console-specific), and any FLVER0-vs-FLVER2 layout divergences. Note that FLVER0 likely shares header/Node infrastructure with FLVER2 — call out reuse opportunities.

  **Must NOT do**: Do NOT include FLVER2 internals; do NOT scope future vertex formats not in upstream.

  **Agent**: `writing`. **Parallelization**: YES; Wave 6; blocked by: T6.0.

  **References**: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/FLVER0/`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-flver.md` exits 0.
  - [ ] Cites FLVER0/ subdir contents; documents shared infrastructure with FLVER2 (Node.cs, LayoutMember.cs, Vertex.cs).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash
    Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-legacy-flver.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.3-validator.log

  Scenario: FLVER0/FLVER2 reuse opportunity documented
    Tool: Bash
    Steps: grep -qE 'FLVER2|Node\.cs|LayoutMember\.cs' .sisyphus/plans/next-batch-legacy-flver.md
    Expected Result: reuse opportunity explicitly called out.
    Evidence: .sisyphus/evidence/task-6.3-reuse.log
  ```

  **Commit**: `docs(plan): next-batch-legacy-flver.md` — pre-commit: validator.

- [ ] 6.4 **Write `next-batch-tae-templates.md`**

  **What to do**: Cover the TAE Template subsystem (`Template.cs` 801 LOC: `ApplyTemplate / BankTemplate / EventTemplate / ParameterTemplate`) plus the non-SDT TAE format dispatches (DS1, SOTFS, DS3, BB, DES, DESR). Note that this was explicitly deferred in PLAN.md to v1.1+/v1.2.

  **Must NOT do**: Do NOT include FXR3 (in v1); do NOT include FXR1 (that goes to effects-misc).

  **Agent**: `writing`. **Parallelization**: YES; Wave 6.

  **References**: `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/Template.cs` and TAE/ format dispatch files.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-tae-templates.md` exits 0.
  - [ ] Cites `Template.cs`, `BankTemplate`, `EventTemplate`, `ParameterTemplate`; enumerates non-SDT TAE format-bytes (DS1=0x1000B etc).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-tae-templates.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.4-validator.log

  Scenario: Template subsystem completeness
    Tool: Bash; Steps: grep -qE 'ApplyTemplate|BankTemplate|EventTemplate|ParameterTemplate' .sisyphus/plans/next-batch-tae-templates.md
    Expected Result: all 4 template names cited.
    Evidence: .sisyphus/evidence/task-6.4-templates.log
  ```

  **Commit**: `docs(plan): next-batch-tae-templates.md` — pre-commit: validator.

- [ ] 6.5 **Write `next-batch-lighting.md`**

  **What to do**: Cover BTAB, BTL, BTPB, GPARAM (DS3+ lighting param), PMDCL. Note that GPARAM is non-trivial.

  **Must NOT do**: Do NOT include BHD5 (in v1).

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: upstream `BTAB.cs BTL.cs BTPB.cs GPARAM.cs PMDCL.cs`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-lighting.md` exits 0.
  - [ ] All 5 lighting formats cited (BTAB, BTL, BTPB, GPARAM, PMDCL).
  - [ ] Notes GPARAM non-triviality (largest LOC in cluster) as a separate effort estimate.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-lighting.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.5-validator.log

  Scenario: All 5 formats cited
    Tool: Bash; Steps: for F in BTAB BTL BTPB GPARAM PMDCL; do grep -q "$F" .sisyphus/plans/next-batch-lighting.md || exit 1; done
    Expected Result: every format name present.
    Evidence: .sisyphus/evidence/task-6.5-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-lighting.md` — pre-commit: validator.

- [ ] 6.6 **Write `next-batch-navmesh.md`**

  **What to do**: Cover NVA, NVM, NGP, MCG, MCP, EDGE. Note that EDGE is shared geometry compression used in multiple formats.

  **Must NOT do**: Do NOT scope BHD5 (in v1).

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: upstream `NVA.cs NVM.cs NGP.cs MCG.cs MCP.cs EDGE.cs`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-navmesh.md` exits 0.
  - [ ] All 6 navmesh formats cited.
  - [ ] Notes EDGE-as-shared-compression dependency for other formats (not just navmesh).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-navmesh.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.6-validator.log

  Scenario: All 6 formats cited
    Tool: Bash; Steps: for F in NVA NVM NGP MCG MCP EDGE; do grep -q "$F" .sisyphus/plans/next-batch-navmesh.md || exit 1; done
    Expected Result: every format name present.
    Evidence: .sisyphus/evidence/task-6.6-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-navmesh.md` — pre-commit: validator.

- [ ] 6.7 **Write `next-batch-text-script-misc.md`**

  **What to do**: Cover LUAGNL, LUAINFO, EMELD, FMB. Note relationship to existing EMEVD/ESD (in v1).

  **Must NOT do**: Do NOT include EMEVD/ESD/FMG (in v1).

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: upstream `LUAGNL.cs LUAINFO.cs EMELD.cs FMB.cs`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-text-script-misc.md` exits 0.
  - [ ] All 4 formats cited (LUAGNL, LUAINFO, EMELD, FMB).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-text-script-misc.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.7-validator.log

  Scenario: All 4 formats cited
    Tool: Bash; Steps: for F in LUAGNL LUAINFO EMELD FMB; do grep -q "$F" .sisyphus/plans/next-batch-text-script-misc.md || exit 1; done
    Expected Result: every format name present.
    Evidence: .sisyphus/evidence/task-6.7-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-text-script-misc.md` — pre-commit: validator.

- [ ] 6.8 **Write `next-batch-effects-misc.md`**

  **What to do**: Cover FXR1, FFXDLSE, ANI, MQB (ER cutscene format), Morpheme.

  **Must NOT do**: Do NOT include FXR3 (in v1).

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: upstream `FXR1/ FFXDLSE/ ANI.cs MQB/ Morpheme/`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-effects-misc.md` exits 0.
  - [ ] All 5 effect-family formats cited (FXR1, FFXDLSE, ANI, MQB, Morpheme).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-effects-misc.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.8-validator.log

  Scenario: All 5 formats cited
    Tool: Bash; Steps: for F in FXR1 FFXDLSE ANI MQB Morpheme; do grep -q "$F" .sisyphus/plans/next-batch-effects-misc.md || exit 1; done
    Expected Result: every format name present.
    Evidence: .sisyphus/evidence/task-6.8-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-effects-misc.md` — pre-commit: validator.

- [ ] 6.9 **Write `next-batch-ac-specific.md`**

  **What to do**: Cover AcParts (variants per AC game), MLB_AC4 / MLB_AC5, FSDATA, FSLIBLZS. These are AC-series-specific formats deferred to v3.

  **Must NOT do**: Do NOT include AC6-specific files (msbvi etc are in v1).

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: upstream `AcParts/ MLB/ FSDATA.cs FSLIBLZS.cs`.

  **Acceptance**:
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-ac-specific.md` exits 0.
  - [ ] All 4 AC-family clusters cited (AcParts variants, MLB_AC4/5, FSDATA, FSLIBLZS).

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes
    Tool: Bash; Steps: bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-ac-specific.md ; test $? -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.9-validator.log

  Scenario: AC formats covered (per-variant matrix)
    Tool: Bash; Steps: for F in AcParts MLB FSDATA FSLIBLZS; do grep -q "$F" .sisyphus/plans/next-batch-ac-specific.md || exit 1; done
    Expected Result: every family name present.
    Evidence: .sisyphus/evidence/task-6.9-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-ac-specific.md` — pre-commit: validator.

- [ ] 6.10 **Write `next-batch-uncategorized-deferred.md` (10th catch-all cluster)**

  **What to do**: Cover the stragglers Metis flagged: DRB (UI layout), ACB, CCM, RMB, GRASS, F2TR, EDD, AIP, SMD4, CLM2. Plus anything else T6.0 inventory failed to assign cleanly.

  **Must NOT do**: Do NOT defer anything to "future cluster" — every leftover upstream format must be IN this file.

  **Agent**: `writing`. **Parallelization**: YES.

  **References**: T6.0 inventory; upstream standalone `.cs` files.

  **Acceptance**:
  - [ ] Every remaining upstream `.cs` from T6.0 is covered here OR explicitly marked as "Implemented in v1" / "Implemented in cluster X".
  - [ ] `bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-uncategorized-deferred.md` exits 0.
  - [ ] Union check: every NOT-IMPLEMENTED row in `.sisyphus/evidence/upstream-inventory.md` appears in at least one of the 10 cluster files.

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Validator passes on uncategorized-deferred
    Tool: Bash
    Steps:
      1. bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-uncategorized-deferred.md 2>&1 | tee .sisyphus/evidence/task-6.10-validator.log
      2. test ${PIPESTATUS[0]} -eq 0
    Expected Result: exit 0.
    Evidence: .sisyphus/evidence/task-6.10-validator.log

  Scenario: Cross-cluster union covers every NOT-IMPLEMENTED upstream format
    Tool: Bash
    Steps:
      1. awk '/^path:/{p=$2}/^state: NOT-IMPLEMENTED/{print p}' .sisyphus/evidence/upstream-inventory.md | sort -u > /tmp/not-implemented.txt
      2. cat .sisyphus/plans/next-batch-*.md | grep -oE 'SoulsFormats/[A-Za-z0-9_/.]+\.cs' | sort -u > /tmp/cluster-union.txt
      3. comm -23 /tmp/not-implemented.txt /tmp/cluster-union.txt > /tmp/homeless.txt
      4. test ! -s /tmp/homeless.txt
    Expected Result: zero homeless upstream formats; every NOT-IMPLEMENTED format is referenced by ≥1 cluster.
    Evidence: .sisyphus/evidence/task-6.10-union-coverage.log
  ```

  **Commit**: `docs(plan): next-batch-uncategorized-deferred.md` — pre-commit: validator + union-coverage check (both bash scenarios above).

> **Wave-6 closure gate**: After T6.0–T6.10 land, run a coverage-completeness audit:
> ```bash
> # Verify every NOT-IMPLEMENTED upstream format is covered by ≥1 cluster plan
> awk '/^cluster:/{c=$2}/^path:/{print c, $2}' .sisyphus/evidence/upstream-inventory.md \
>   | grep -v 'IMPLEMENTED' \
>   | awk '{print $1}' | sort -u  # all 10 cluster names should appear
> ```
> Plus the structural validator on each `.sisyphus/plans/next-batch-*.md` (per Wave 7 F1).

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get
> explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work
> complete.**
> **Never mark F1-F4 as checked before getting user's okay.** Rejection or user feedback → fix → re-run → present again → wait.

- [ ] F1. **Plan Compliance Audit** — `oracle`

  **What to do**: Read this plan end-to-end. For each "Must Have": verify implementation exists (read file, run command, inspect evidence). For each "Must NOT Have": grep codebase for the forbidden pattern — reject with file:line if found. Check every evidence file exists in `.sisyphus/evidence/`. Compare deliverables against the plan.

  **Required commands**:
  ```bash
  test -f .sisyphus/evidence/symbols-baseline.txt
  test -f .sisyphus/evidence/test-counts-baseline.txt
  test -f .sisyphus/evidence/skip-count-baseline.txt
  test -f .sisyphus/evidence/reserve-fill-audit.md
  test -f .sisyphus/evidence/klib-toolchain-spike.md
  test -f .sisyphus/evidence/paramdef-apply-callers.md
  test -f .sisyphus/evidence/endian-toggle-sites.md
  test -f .sisyphus/evidence/msb-scaffold-vs-subtype.md
  test -f .sisyphus/evidence/alloc-site-audit.md
  test -f .sisyphus/evidence/wave3-alloc-counts.md
  test $(ls .sisyphus/plans/next-batch-*.md | wc -l) -eq 10
  grep -c '^SF_API ' include/souls_formats/sf_sl2.h
  grep -qE 'sf_sl2\.h' CMakeLists.txt
  grep -q '0.4.1' CMakeLists.txt
  ! grep -q 'SF_ENABLE_PHASE7' CHANGELOG.md  # OR add back to CMakeLists
  ```

  **Output format**: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`. Evidence path: `.sisyphus/evidence/review-oracle-constraints.md`.

- [ ] F2. **Code Quality + Symbol Stability + Sanitizer Review** — `unspecified-high`

  **What to do**: Run `tsc-equivalent` (build with `-Werror`), lint, full `ctest`, sanitizer build. Review all changed files via `git diff main...HEAD` for: AI slop (excessive comments, over-abstraction, generic names), suppression annotations (`#pragma warning`, unused-suppress), empty catch-equivalents, commented-out code, unused includes. Verify symbol export stability via `comm -23 baseline now` showing zero removals.

  **Required commands**:
  ```bash
  cmake --build build-mingw 2>&1 | grep -cE 'warning:' | xargs -I{} test {} -eq 0
  ctest --test-dir build-mingw -L 'core|compression|crypto|archive|param|script|map|geom|anim|hygiene|golden' --output-on-failure
  cmake --build build-asan && ctest --test-dir build-asan -L core --output-on-failure
  comm -23 .sisyphus/evidence/symbols-baseline.txt <(x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | awk '/^\s*\[\s*[0-9]+\]\s+sf_/{print $NF}' | sort -u) | wc -l  # expect 0
  git diff main...HEAD --stat | tail -1  # report LOC delta
  ```

  **Output format**: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail/N skip] | Sanitizer [PASS/FAIL] | Symbols removed [0] | Files [N clean/N issues] | VERDICT`. Evidence path: `.sisyphus/evidence/review-quality.md`.

- [ ] F3. **Real Manual QA** — `unspecified-high`

  **What to do**: From a freshly cleaned checkout, execute every wave's acceptance commands in order (`W1` then `W2` then …). Save outputs. Verify all six Wave-Acceptance gates pass for each wave. Specifically re-run from scratch: library-only consumer build, BUILD_TESTING toggle, golden hash check, e2e skip-count stability. Note any flakiness.

  **Output format**: `Waves [N/N] | Consumer-build [PASS/FAIL] | BUILD_TESTING toggle [PASS/FAIL] | Golden [PASS/FAIL] | e2e skip-count [STABLE/DRIFTED] | Flakes [count] | VERDICT`. Evidence path: `.sisyphus/evidence/review-qa.md`.

- [ ] F4. **Scope Fidelity Check** — `deep`

  **What to do**: For each Wave task: read its "What to do", read the actual git diff for that task's commits. Verify 1:1 — everything in the spec was implemented (no missing), nothing beyond the spec was implemented (no creep). Specifically watch for: MSB per-subtype field touches (W5 forbidden), endian type-specialization (W4 forbidden), klib outside BHD5 (W4 forbidden), removed exported symbols (any wave forbidden), changes to `include/souls_formats/*.h` declarations (any wave forbidden).

  **Required commands**:
  ```bash
  git log --oneline main...HEAD | wc -l
  git diff main...HEAD --stat -- include/souls_formats/  # should show only sf_sl2.h additions to CMakeLists, no header content changes
  git diff main...HEAD -- src/map/msbs/parts_param.c src/map/msbe/parts_param.c src/map/msbvi/parts_param.c | grep -cE '^[-+][^-+]' | xargs -I{} test {} -le 50  # minimal touches
  ```

  **Output format**: `Tasks [N/N compliant] | Per-subtype touched [CLEAN/N issues] | Endian-type-specialization [CLEAN/N] | klib outside BHD5 [CLEAN/N] | Header decl changes [CLEAN/N] | VERDICT`. Evidence path: `.sisyphus/evidence/review-scope.md`.

> After F1–F4 return verdicts: present the consolidated report to the user. **Wait for user's explicit "okay"** before marking F1–F4 checked. If any F returns REJECT, fix the issues and re-run that specific F (and any downstream Fs) — do NOT bypass.

---

## Commit Strategy

> Group commits by task. Each commit cites the upstream `.cs` file (if applicable) per AGENTS.md §5.x.
> Pre-commit hook: build + targeted ctest.

| Wave-task | Commit prefix | Files (canonical pattern) | Pre-commit verification |
|---|---|---|---|
| T0.* | `audit(<area>): <description>` | `.sisyphus/evidence/*.md` only | none (no code) |
| T1.1 | `chore(evidence): capture baselines` | `.sisyphus/evidence/*.txt` | none |
| T1.2 | `build(cmake): default BUILD_TESTING to PROJECT_IS_TOP_LEVEL; alias SF_BUILD_TESTS` | `CMakeLists.txt` | `cmake -B build-mingw && cmake --build build-mingw && ctest --test-dir build-mingw -L core` |
| T1.3 | `ci: update workflow to BUILD_TESTING flag` | `.github/workflows/ci.yml` | `cmake -B build-mingw -DBUILD_TESTING=ON --toolchain ...` |
| T1.4 | `build(headers): add sf_sl2.h to public headers; harmonize example link` | `CMakeLists.txt examples/CMakeLists.txt` | `cmake --build build-mingw && cmake --install build-mingw --prefix /tmp/sf-install && test -f /tmp/sf-install/include/souls_formats/sf_sl2.h` |
| T1.5 | `build(cmake): probe gating + ctest label unification` | `CMakeLists.txt tests/CMakeLists.txt tests/probes/CMakeLists.txt` | `ctest -N -L e2e \| grep -c 'Test'` |
| T1.6 | `docs(changelog): drop SF_ENABLE_PHASE7 reference; build-dir cleanup note` | `CHANGELOG.md .gitignore?` | none |
| T1.7 | `refactor(internal): relocate emevd_internal.h (if T0.7 confirms)` | `src/script/emevd_internal.h → ?` | `ctest -L script` |
| T1.8 | `fix(writer): resolve N reserve/fill mismatch (per T0.2)` | `src/...` per audit | `ctest -L core -L archive` |
| T2.1 | `refactor(archive,compression): adopt sf_get_decompressed_reader at <N> sites` | `src/archive/*.c src/compression/dcx.c` | `ctest -L archive -L compression` |
| T2.2 | `refactor(core): magic-check helper SF_ASSERT_MAGIC()` | `src/internal/sf_internal.h src/<modules>` | `ctest -L core -L archive -L param -L script -L map -L geom -L anim` |
| T2.3 | `refactor(core): reserve/fill scaffold macro` | `src/internal/sf_internal.h src/<modules>` | `ctest -L core -L archive -L param -L script -L map -L geom` |
| T2.4 | `build(tests): consolidate e2e helpers into static lib` | `tests/CMakeLists.txt tests/e2e/CMakeLists.txt` | `ctest -L e2e` |
| T2.5 | `docs(internal): document goto-cleanup convention` | `docs/api-mapping/POLICY.md` | none |
| T3.* | `perf(<area>): bulk-alloc <feature> in <module>` | `src/<area>/<file>.c` | `ctest -L <area>; ctest -L golden` |
| T4.1 | `perf(archive): adopt klib khash for BHD5 entry lookup` | `src/archive/bhd5.c third_party/klib include` | `ctest -L archive -L e2e_er; ctest -L golden` |
| T4.2 | `perf(param): precompute PARAMDEF field layout` | `src/param/paramdef_apply.c` | `ctest -L param` |
| T4.3 | `perf(core): inline endian fast-path in binary_reader` (skip if <5% wins) | `src/core/binary_reader.c` | `ctest -L core` + microbench |
| T5.1 | `refactor(map): extract msb_entry_list_read/write scaffolding` | `src/map/msb_common.c include/souls_formats/sf_msb.h` | `ctest -L map` |
| T5.2 | `refactor(map): apply msb_entry_list_* to msbs/msbe/msbvi` | `src/map/msbs/*.c src/map/msbe/*.c src/map/msbvi/*.c` | `ctest -L map -L e2e_er -L e2e_sekiro -L e2e_ac6 -L e2e_nightreign` |
| T5.3 | `docs(policy): document MSB shared-engine technique` | `docs/api-mapping/POLICY.md docs/api-mapping/extensions.md` | none |
| T6.* | `docs(plan): next-batch-<cluster>.md` | `.sisyphus/plans/next-batch-*.md` | none |
| **Final** | `release: 0.4.1` | `CMakeLists.txt CHANGELOG.md` | full CI matrix |

---

## Success Criteria

### Verification Commands

```bash
# Library-only build (consumer experience)
rm -rf /tmp/sf-consumer && mkdir -p /tmp/sf-consumer && cd /tmp/sf-consumer
cat > CMakeLists.txt <<EOF
cmake_minimum_required(VERSION 3.24)
project(consumer LANGUAGES C)
add_subdirectory(/home/soar/src/souls-formats-c sf EXCLUDE_FROM_ALL)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE souls_formats::static)
EOF
echo 'int main(void){return 0;}' > main.c
cmake -B build -G Ninja --toolchain /home/soar/src/souls-formats-c/cmake/toolchain-mingw-w64.cmake
cmake --build build
test ! -d build/sf/tests
test ! -d build/sf/examples
test ! -d build/sf/tests/probes
test -f build/consumer.exe   # consumer linked successfully

# Standalone dev loop still builds tests
cd /home/soar/src/souls-formats-c
rm -rf build-default
cmake -B build-default -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-default
test $(find build-default/tests -name 'souls_formats_test_*.exe' | wc -l) -ge 40

# Legacy SF_BUILD_TESTS alias works
rm -rf build-legacy
cmake -B build-legacy -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DSF_BUILD_TESTS=OFF
cmake --build build-legacy
test ! -d build-legacy/tests

# Symbol export — zero removals
comm -23 .sisyphus/evidence/symbols-baseline.txt \
    <(x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll \
      | awk '/^\s*\[\s*[0-9]+\]\s+sf_/{print $NF}' | sort -u) \
  | wc -l   # expect: 0

# All tests pass
ctest --test-dir build-mingw -L 'core|compression|crypto|archive|param|script|map|geom|anim|hygiene|golden' --output-on-failure

# Sanitizer clean
ctest --test-dir build-asan -L core --output-on-failure

# 10 cluster plans exist
test $(ls .sisyphus/plans/next-batch-*.md | wc -l) -eq 10

# Version bumped
grep 'VERSION 0.4.1' CMakeLists.txt

# CHANGELOG finalized
grep '^## \[0.4.1\]' CHANGELOG.md
```

### Final Checklist
- [ ] All "Must Have" items present (verified by F1)
- [ ] All "Must NOT Have" items absent (verified by F1 + F4)
- [ ] All wave acceptance gates pass for W1–W5
- [ ] All 10 cluster plans exist and pass structural validation
- [ ] User explicitly says "okay" to F1–F4 reviewer reports
- [ ] CHANGELOG ## [0.4.1] block finalized with date stamp
- [ ] CMakeLists.txt `VERSION 0.4.1`
- [ ] Draft file `.sisyphus/drafts/refactor-and-gap-analysis.md` deleted
