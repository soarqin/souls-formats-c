# API Realignment with SoulsFormatsNEXT

## TL;DR

> **Quick Summary**: Adopt a strict upstream-alignment policy, build a tiered upstream→ours API mapping table, then refactor already-shipped Phase 0/1/2 code to close every drift item the table reveals.
>
> **Deliverables**:
> - `AGENTS.md` extended with two mandatory alignment rules + reconciled ABI-break clause
> - `docs/api-mapping/` — pinned upstream commit, mapping policy, ~25 Tier-A row-level docs, 1 Tier-B legacy inventory, 1 extensions doc, README index
> - `CHANGELOG.md` (new) recording every removed/renamed/added public symbol
> - Phase 0/1/2 source/headers refactored to align with upstream (DCX tagged-union, plural reads/writes, Get* / Assert* coverage, Reserve/Fill 12-type completeness, ReadEnum, PathHelper, IsPrime, Regulation/SL2 public APIs, Oodle enum exposure)
> - All `docs/roadmap/*.md` rewritten under the new alignment policy
> - `.sisyphus/plans/PLAN.md` synced (§5.x policy clause, §7 retrospective notes, §12 mapping cross-refs, §11 drift risk)
> - Pre-refactor golden hashes capture ensures behavior preservation
>
> **Estimated Effort**: Large (two solid weeks of focused work, but parallelizable across ~6-8 concurrent agents per wave)
>
> **Parallel Execution**: YES — 7 waves with 6-8 tasks each (except foundation + final review)
>
> **Critical Path**: Wave 1 preconditions → Wave 2 Phase 1 module fixes → Wave 3 Phase 2 module fixes → Wave 4 Phase 3-7 mapping → Wave 6 roadmap rewrite → Wave 7 PLAN.md sync → Final reviews

---

## Context

### Original Request
User reported that Phase 2 was completed but the implementation drifted from upstream SoulsFormatsNEXT, requiring manual hardcode patches. They want:
1. Two new alignment rules in `AGENTS.md` (no guessing; mirror upstream API).
2. An upstream→ours API mapping table covering the entire upstream library.
3. Roadmap rewrite under the new conventions.
4. Use the table to fix already-implemented Phase 0/1/2 code.

### Interview Summary
- **API table scope**: ENTIRE upstream repo (incl. v2+ legacy). Unimplemented marked `未实现`.
- **Fix scope**: Full alignment of Phase 0/1/2 — add ALL missing methods, rename to match upstream.
- **Stream architecture**: KEEP 2-layer (`sf_istream_t` + `sf_binary_reader_t`); document as deliberate C-style adaptation in extensions doc.
- **Mapping table org**: SPLIT into `docs/api-mapping/*.md` per upstream logical type (NOT per `.cs` — partial-class groups consolidated).
- **Already-completed roadmap docs**: REWRITE Phase 0/1/2 docs as completion retrospective + actual API delivered + alignment status.
- **DCX `CompressionInfo` model**: REFACTOR to tagged union — `sf_dcx_compression_info_t` with discriminant + per-type union (DcpDflt/DcpEdge/DcxEdge/DcxDflt/DcxKrak/DcxZstd/Zlib/None/Unk).
- **Oodle enums**: PUBLICIZE the entire `OodleLZ_*` family as `sf_oodle_lz_*`.
- **Non-upstream additions** (`sf_dcx_unwrap`, `sf_ostream_detach_buffer`, dual-layer stream): KEEP, but track in a SEPARATE `docs/api-mapping/extensions.md` so the main mapping table contains only entries with an upstream counterpart.
- **PLAN.md sync**: UPDATE in this plan.
- **Regulation/SL2 public API**: ADD byte-buffer-level public APIs in this plan (`sf_regulation.h`, `sf_sl2.h`); BND4-aware overloads come later in Phase 3.

### Research Findings
- Upstream layout (real paths): `SoulsFormats/Utilities/{IO,Text,Cryptography,Compression}/...` + `SoulsFormats/Formats/...` (~413 `.cs` files, ~152K LOC).
- Phase 1 drift catalogued: missing plural arrays, Get* coverage, Assert* multi-option, 7 of 12 Reserve/Fill types, GetASCII variants, PadFF, ReadEnum8/16/32/64, IsFlexible, Stream getter, ToArray/FinishBytes, PathHelper, IsPrime, SFUtil.GetDecompressedBinaryReader.
- Phase 2 drift catalogued: flat `sf_dcx_params_t` vs upstream `interface CompressionInfo` + 9 concrete structs; missing presets (DfltCompressionPreset / KrakCompressionPreset / DefaultType); missing Stream/path overloads; missing OodleLZ_* enum family; entirely missing public Regulation/SL2 APIs.

### Metis Review (Pre-Plan Gap Analysis)
**Critical findings folded into this plan**:
- **G1 — Pin upstream commit** (no commit pinning ⇒ silent invalidation when upstream advances).
- **G2 — Partial-class problem** (30 logical types split across multiple `.cs`, e.g., AcParts4=38, FXR1=14, FLVER2=13, PARAMDEF=3). Refined "one .md per .cs" → "one .md per logical type with all sibling files referenced in header".
- **G3 — Mapping table schema** needs 6 columns: `Upstream signature | Upstream loc (file:line) | Kind | Our API (or 未实现 / _删除_) | Status | Notes`.
- **G4 — Pre-refactor golden hashes mandatory** (round-trip tests can hide symmetric encoder/decoder bugs).
- **G5 — PLAN.md §12 numbering conflict** (already has §12附录A and §13下一步; extend §12 in-place, don't introduce a new collision).
- **G6 — Stale numbers** in 4 places (AGENTS.md status table claims `Phase 2 ⏳ next` and `49/49 across 5 binaries`; reality is Phase 2 done with 66 tests across 13 binaries).
- **G7 — AGENTS.md §7 line 202 reconciliation** (current text forbids API change without patch bump; alignment refactor needs explicit allowance with CHANGELOG + tests + minor bump).
- **Tier A/B mapping split** (Tier A = ~25 row-level docs for util + v1 in-scope formats; Tier B = single `legacy.md` inventory for v2+ legacy). Saves ~70% doc work.
- **Per-module interleaving** within Wave 2/3 (map → fix → test → ship per module, not global mapping → global fixing).
- **5th review agent** added: cross-doc consistency reviewer.
- **Version bump 0.1.0 → 0.2.0** (minor signals "rebuild required"); `CHANGELOG.md` is new artifact.
- **Static `IsFlexible`** sketchy in C lib; recommend per-reader flag with separate global default setter, document divergence.
- **Shift-JIS round-trip identity** test required (SJ → UTF-8 → SJ identical bytes).

### Decisions for AI-Slop Avoidance
- No deprecation/shim layer — clean break at v0.x.
- No mapping-table auto-generation tooling — markdown by hand for v1.
- No upstream-test port — keep Unity-based tests, augment.
- No new format implementation inside this plan — Phase 3+ formats get **mapping rows only**, no `.h`/`.c`.
- No abstraction inflation — POLICY.md is plain markdown decisions, not a "framework".

---

## Work Objectives

### Core Objective
Bring the souls-formats-c public API into strict, traceable alignment with SoulsFormatsNEXT by (a) pinning the upstream baseline, (b) producing an exhaustive cross-reference table, (c) closing every documented drift item in already-shipped Phase 0/1/2 code, and (d) propagating the new alignment policy through `AGENTS.md`, `PLAN.md`, and all roadmap docs.

### Concrete Deliverables
- `AGENTS.md` updated: §5 (or new §5.x) "Strict upstream alignment policy" with two mandatory rules; §7 line 202 reconciled to allow v0.x ABI breaks under conditions; status table refreshed (Phase 2 ✅, real test count); narrative test/symbol counts updated.
- `docs/api-mapping/UPSTREAM.md` — pinned commit SHA, date, line count, re-audit policy.
- `docs/api-mapping/POLICY.md` — C-mapping policies (properties, overloads, generics, default args, params arrays, null vs empty, static-vs-instance, internal-vs-public).
- `docs/api-mapping/README.md` — index with Tier A/B explanation, status legend, table of contents.
- `docs/api-mapping/extensions.md` — schema + entries for every C-library convenience helper that has no upstream analogue (sf_dcx_unwrap, sf_ostream_detach_buffer, dual-layer stream, sf_oodle_set_search_path, sf_oodle_unload, sf_get_decompressed_reader, etc.).
- `docs/api-mapping/util-io-binary-reader-ex.md` — full row-level mapping for `BinaryReaderEx` (≥103 rows).
- `docs/api-mapping/util-io-binary-writer-ex.md` — full row-level mapping for `BinaryWriterEx` (≥75 rows).
- `docs/api-mapping/util-io-path-helper.md`, `util-text-sf-encoding.md`, `util-cryptography-hash-helper.md`, `util-cryptography-regulation-decryptor.md`, `util-cryptography-sl2-decryptor.md`, `util-compression-zlib-helper.md`, `util-compression-zstd-helper.md`, `util-compression-oodle.md` (consolidates Oodle/26/28/29/IOodleCompressor/NoOodleException), `util-sf-util.md`, `math.md` (Vector/Quaternion/Color).
- `docs/api-mapping/format-dcx.md` — DCX with all 9 CompressionInfo subclasses.
- Tier A mapping for in-scope future formats (Phase 3-7): `format-bnd3.md`, `format-bnd4.md`, `format-binder-common.md` (consolidates `Formats/Binder/*`), `format-bxf3.md`, `format-bxf4.md`, `format-bhd5.md`, `format-tpf.md`, `format-enfl.md`, `format-param.md` (consolidates `Formats/PARAM/*` partial-class), `format-paramdef.md` (consolidates 3 PARAMDEF files), `format-paramtdf.md`, `format-fmg.md`, `format-emevd.md` (consolidates `Formats/EMEVD/*`), `format-esd.md`, `format-msb-common.md`, `format-msbs.md`, `format-msbe.md`, `format-msbvi.md` (each consolidates the partial-class subfolder), `format-flver-common.md`, `format-flver2.md` (consolidates 13-file FLVER2 subfolder), `format-mtd.md`, `format-matbin.md`, `format-tae.md`, `format-fxr3.md`.
- `docs/api-mapping/legacy.md` — Tier B inventory: one row per legacy/v2+ format type (DS1/2/3/BB/DeS, AC4/ACFA/ACV/ACVD, King's Field, Kuon, Otogi, etc.), columns: `Class | First file | File count | Defer-to-version | Notes`.
- `docs/api-mapping/drift-checklist.md` — every Phase 0/1/2 drift item from the analysis as a tickable list, ticked as fixes land.
- `CHANGELOG.md` (new repo root file) — sections "Breaking changes" (every removed/renamed symbol with replacement), "New APIs", "Notes". 0.1.0 → 0.2.0.
- Refactored Phase 1 sources/headers: `sf_io.h`, `binary_reader.c`, `binary_writer.c`, `sf_hash.h`, `filename_hash.c`, new `sf_path.h` + `path.c` + tests.
- Refactored Phase 2 sources/headers: `sf_dcx.h` (tagged union + presets + overloads), `dcx.c`, `sf_oodle.h` (publicized OodleLZ_* enums), `oodle_loader.c`, new `sf_regulation.h` + `sf_sl2.h` + minimal source-level renames.
- Updated Phase 1 + 2 tests asserting newly-added APIs (Reserve/Fill new types, plural reads, Assert multi-option, ReadEnum, GetASCII, IsPrime, regulation byte-level, SL2 byte-level, Shift-JIS round-trip identity, golden-hash regression suite).
- All `docs/roadmap/*.md` rewritten under the alignment policy (README + 8 per-phase docs).
- `.sisyphus/plans/PLAN.md` synced (§5 policy clause, §7 retrospective notes, §11 risk addition, §12 extension).

### Definition of Done
- [ ] `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure` exits 0.
- [ ] `grep -r "Phase 2.*next" --include='*.md' --exclude-dir=.sisyphus .` returns no matches outside `CHANGELOG.md` retrospective entries (the plan file itself in `.sisyphus/plans/` is excluded as the historical record of the stale state being fixed).
- [ ] `find docs/api-mapping -name "*.md" | wc -l` returns ≥ 25 (Tier A docs).
- [ ] Every drift item from the original analysis closed in `docs/api-mapping/drift-checklist.md`.
- [ ] Pre-refactor golden hashes regenerated and identical post-refactor for every existing test fixture.
- [ ] Final 5-agent review wave returns all PASS verdicts and user explicitly approves.

### Must Have
- Pin a specific upstream commit SHA before any mapping row is written.
- Pre-refactor SHA-256 golden hashes captured for every test's deterministic byte output.
- Mapping table schema includes `Upstream loc` column with `file:line` references.
- Partial-class groups consolidated into one mapping doc each.
- Tier A row-level mapping for in-scope (util + Phase 1-7 formats); Tier B inventory-only for legacy.
- Per-module interleave (map+fix+test as a unit per module).
- AGENTS.md §7 line 202 reconciled to authorize v0.x ABI breaks with CHANGELOG + tests + minor bump.
- Version bump 0.1.0 → 0.2.0 in `CMakeLists.txt`, `sf_common.h`, AGENTS.md narrative, CHANGELOG.md.
- Cross-doc consistency reviewer in final wave.

### Must NOT Have (Guardrails)
- **No new format code** (Phase 3+ format implementations) inside this plan; mapping rows only.
- **No deprecation/shim layer**; clean break.
- **No auto-generation tooling** for mapping tables; plain markdown.
- **No upstream-test port** to Unity; existing tests stay, augmented with new-API tests only.
- **No bindgen / Python / Rust binding work**.
- **No re-architecture** of `sf_binary_reader_t` internals beyond additive changes (add fields/methods, don't repack existing layout).
- **No abstraction inflation** ("mapping framework", doc generator, etc.).
- **No Linux backend / Wine port** while touching crypto.
- **No row-level method mapping** for legacy/v2+ formats (Tier B inventory only).
- **No silent behavior change** during refactor — every behavioral delta MUST be a deliberate mapping-row delta with rationale OR an entry in `extensions.md`.
- **No assumption that "tests pass" = "behavior preserved"** without golden hashes.
- **No Phase 7 expansion** from optional to required.

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.
> Acceptance criteria requiring "user manually tests/confirms" are FORBIDDEN.

### Test Decision
- **Infrastructure exists**: YES (Unity 2.6.1 via CPM)
- **Automated tests**: YES (Tests-after for new APIs; Golden-hash regression for existing).
- **Framework**: Unity (unchanged)
- **TDD/RED-GREEN-REFACTOR**: NOT used here — refactor pattern dominates. Each module's task includes adding tests for newly-exposed surface.

### QA Policy
Every task MUST include agent-executed QA scenarios (see TODO template).
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Library/Module fixes**: Use `bash` to run `cmake --build` + `ctest -L <label>` and capture output. Compare diff against pre-refactor baseline.
- **Markdown deliverables**: Use `bash` to verify file existence, line counts, grep specific markers (e.g., `grep -c '^|' docs/api-mapping/util-io-binary-reader-ex.md ≥ 105`).
- **Golden hashes**: Use `bash` to recompute SHA-256 of every test's deterministic output and compare against `.sisyphus/evidence/golden-hashes/*.sha256`.
- **Cross-doc consistency**: Use `grep`/`bash` to verify that AGENTS.md, PLAN.md, roadmap, mapping all agree on numbers/policies (e.g., grep alignment rule appears in all four; version 0.2.0 appears wherever stated).

---

## Execution Strategy

### Parallel Execution Waves

> Each wave completes before the next begins. Within a wave, tasks run in parallel.
> Per-module interleaving (map+fix+test as one task per module) honors Metis's
> guidance to keep each module independently shippable.

```
Wave 1 (Preconditions — parallel, all required before mapping/fixing):
├── Task 1: Pin upstream commit + create UPSTREAM.md       [quick]
├── Task 2: Capture pre-refactor golden hashes             [unspecified-high]
├── Task 3: Stale-number sweep                              [quick]
├── Task 4: AGENTS.md alignment rules + §7 reconciliation   [writing]
├── Task 5: Create POLICY.md                                [writing]
├── Task 6: Bump version 0.1.0 → 0.2.0 + CHANGELOG.md skel  [quick]
└── Task 7: docs/api-mapping/{README,extensions}.md skel    [writing]

Wave 2 (Phase 1 modules + Oodle enums — interleaved map+fix+test, parallel across modules):
├── Task 8:  BinaryReaderEx map + fix + test               [deep]
├── Task 9:  BinaryWriterEx map + fix + test               [deep]
├── Task 10: SFEncoding map + verify                        [unspecified-high]
├── Task 11: PathHelper map + sf_path.h + test              [unspecified-high]
├── Task 12: HashHelper map + IsPrime + test                [quick]
├── Task 14: Math (Vector/Quaternion/Color) map + verify    [quick]
└── Task 18: Oodle (+26/28/29/IOodleCompressor/NoOodleEx) map + publicize enums [deep]
            ⬅ MOVED EARLY: T15 (DCX) needs sf_oodle_lz_compressor_t

Wave 3 (Phase 2 modules + SFUtil — after Wave 2 settles):
├── Task 15: DCX map + tagged union refactor + presets + overloads [ultrabrain]
├── Task 13: SFUtil map + sf_get_decompressed_reader + test [unspecified-high]
            ⬅ MOVED LATE: needs sf_dcx_compression_info_t from T15
├── Task 16: ZlibHelper map + verify                        [quick]
├── Task 17: ZstdHelper map + verify                        [quick]
├── Task 19: RegulationDecryptor map + sf_regulation.h + test [unspecified-high]
└── Task 20: SL2Decryptor map + sf_sl2.h + test             [unspecified-high]

Wave 4 (Tier A mapping for Phase 3-7 future formats — no code, parallel):
├── Task 21: Binder family (BND3/4 + BXF3/4 + BHD5)         [writing]
├── Task 22: TPF + ENFL                                     [writing]
├── Task 23: PARAM family (PARAM + PARAMDEF + PARAMTDF + FMG) [writing]
├── Task 24: EMEVD + ESD                                    [writing]
├── Task 25: MSB family (common + MSBS + MSBE + MSBVI)      [writing]
├── Task 26: FLVER family (common + FLVER2 + vertex)        [writing]
├── Task 27: MTD + MATBIN                                   [writing]
└── Task 28: TAE + FXR3 (Phase 7)                           [writing]

Wave 5 (Tier B legacy inventory):
└── Task 29: legacy.md single inventory doc                 [writing]

Wave 6 (Roadmap rewrite — AFTER Wave 3 settles, parallel):
├── Task 30: roadmap/README.md rewrite                      [writing]
├── Task 31: phase-0/1/2 retrospective rewrites             [writing]
├── Task 32: phase-3 full rewrite                           [writing]
├── Task 33: phase-4 full rewrite                           [writing]
├── Task 34: phase-5 full rewrite                           [writing]
├── Task 35: phase-6 full rewrite                           [writing]
└── Task 36: phase-7 + post-v1 rewrites                     [writing]

Wave 7 (PLAN.md sync — after Wave 6):
└── Task 37: PLAN.md §5/§7/§11/§12 sync                     [writing]

Wave FINAL (5 parallel reviews — ALL must APPROVE; then user explicit okay):
├── Task F1: Plan compliance audit              (oracle)
├── Task F2: Code quality review                (unspecified-high)
├── Task F3: Real manual QA / build + tests     (unspecified-high)
├── Task F4: Scope fidelity check               (deep)
└── Task F5: Cross-doc consistency check        (deep)         ← NEW per Metis
-> Present results -> Get explicit user okay
```

**Critical Path**: T1 → T2 → T8 → T15 → T18 → T21 → T30 → T37 → F1-F5 → user okay.
**Parallel Speedup**: ~75% faster than sequential.
**Max Concurrent**: 8 (Waves 4 & 6).

### Dependency Matrix (abbreviated, post-Momus cycle break)

- **1-7**: — → 8-20 (preconditions block all module work)
- **8**: 1, 2, 4, 5 → 21-28 mapping refs
- **9**: 1, 2, 4, 5 → 21-28
- **10, 11, 12, 14**: 1, 2, 4, 5 → 21-28
- **18** (moved to Wave 2): 1, 2, 4, 5 → **15** (T15's DcxKrakCompressionInfo uses `sf_oodle_lz_compressor_t`), 21-28
- **15**: 1, 2, 4, 5, 8, 9, **18** → 13, 16, 17, 19, 20
- **13** (moved to Wave 3): 1, 2, 4, 5, 8, **15** (uses new tagged-union DCX info type)
- **16, 17, 19, 20**: 15, 8, 9 (no T18 dep — already settled in Wave 2)
- **21-28**: 8-20 (need finalized fix state to cite correctly)
- **29**: 21-28
- **30-36**: 8-20 (all fixes settled), 21-29 (mapping refs)
- **37**: 30-36
- **F1-F5**: 37

**Cycle break rationale**:
- Original draft had T15 → T18 (DCX uses Oodle types) and T18 → T15 (Oodle "blocks 15-37"). Resolved by moving T18 (Oodle enum publication) to Wave 2 — it has no real dependency on DCX and its enums are pure type declarations.
- Original draft had T13 (SFUtil) in Wave 2 but using T15's tagged-union DCX info type (Wave 3). Resolved by moving T13 to Wave 3 after T15 lands; T13 uses the post-refactor `sf_dcx_compression_info_t` cleanly.

### Agent Dispatch Summary (post-cycle-break)

- **Wave 1 (7)**: T1 → `quick`, T2 → `unspecified-high`, T3 → `quick`, T4 → `writing`, T5 → `writing`, T6 → `quick`, T7 → `writing`
- **Wave 2 (7)**: T8 → `deep`, T9 → `deep`, T10 → `unspecified-high`, T11 → `unspecified-high`, T12 → `quick`, T14 → `quick`, **T18 → `deep`** (moved here so T15 can use Oodle enums)
- **Wave 3 (6)**: T15 → `ultrabrain`, **T13 → `unspecified-high`** (moved here, depends on T15 tagged union), T16 → `quick`, T17 → `quick`, T19 → `unspecified-high`, T20 → `unspecified-high`
- **Wave 4 (8)**: T21-T28 → `writing`
- **Wave 5 (1)**: T29 → `writing`
- **Wave 6 (7)**: T30-T36 → `writing`
- **Wave 7 (1)**: T37 → `writing`
- **FINAL (5)**: F1 → `oracle`, F2-F3 → `unspecified-high`, F4-F5 → `deep`

---

## TODOs

> Implementation + Test = ONE Task. Per-module interleave (map+fix+test as a unit per module).
> EVERY task MUST have: Recommended Agent Profile + Parallelization info + QA Scenarios.
> **A task WITHOUT QA Scenarios is INCOMPLETE.**

- [x] 1. **Pin upstream commit + create UPSTREAM.md**

  **What to do**:
  - In `/home/soar/src/SoulsFormatsNEXT`, capture the actual checked-out state:
    - `git rev-parse --abbrev-ref HEAD` (the live branch name — verified at plan time as `master`, but use whatever the repo reports at execution time).
    - `git rev-parse HEAD` (40-char commit SHA).
    - `git log -1 --format='%H %ci %s'` (commit subject + date).
    - `find SoulsFormats -name '*.cs' | wc -l` (file count).
    - Total LOC via `find SoulsFormats -name '*.cs' -print0 | xargs -0 wc -l | tail -1`.
  - Create `docs/api-mapping/UPSTREAM.md` recording exactly what `git` reports — do NOT hardcode a branch name; use the live `--abbrev-ref HEAD` output. Include: pinned SHA, commit date, commit subject, branch name (whatever was actually checked out), file count, total LOC. Append a "Re-audit policy" paragraph: "When upstream advances past 50 commits on the recorded branch, re-survey BinaryReaderEx, BinaryWriterEx, DCX.cs, RegulationDecryptor.cs, SL2Decryptor.cs, HashHelper.cs, Oodle.cs only. Other files re-audited on next phase mapping."

  **Must NOT do**:
  - Do NOT clone, fetch, or check out a different branch/commit; pin exactly what `/home/soar/src/SoulsFormatsNEXT` currently reports.
  - Do NOT hardcode `net9.0`, `master`, or any branch name in this plan or in UPSTREAM.md content templates — read it from `git rev-parse --abbrev-ref HEAD` at execution time.
  - Do NOT include any binary or game-data references.

  **Recommended Agent Profile**:
  - **Category**: `quick` — Trivial single-file write with shell capture.
  - **Skills**: `[]` — No skill needed.

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1 (with Tasks 2, 3, 4, 5, 6, 7)
  - Blocks: 8-37 (every other task references this commit)
  - Blocked By: None

  **References**:
  - **Pattern References**: None.
  - **External References**: `/home/soar/src/SoulsFormatsNEXT/.git` — Read-only git repo to pin from.

  **WHY each reference matters**: This is the "trust anchor" for the entire alignment work; without a fixed commit, any mapping row can be silently invalidated by an upstream push.

  **Acceptance Criteria**:
  - [ ] `docs/api-mapping/UPSTREAM.md` exists.
  - [ ] `grep -E '^[a-f0-9]{40}$' docs/api-mapping/UPSTREAM.md` returns ≥ 1 line.
  - [ ] `grep -c 'Re-audit policy' docs/api-mapping/UPSTREAM.md` ≥ 1.

  **QA Scenarios**:
  ```
  Scenario: UPSTREAM.md present and well-formed
    Tool: Bash
    Preconditions: Working dir is repo root.
    Steps:
      1. `test -f docs/api-mapping/UPSTREAM.md && echo PRESENT`
      2. `grep -oE '\b[a-f0-9]{40}\b' docs/api-mapping/UPSTREAM.md | head -1`
      3. `grep -c 'Re-audit policy' docs/api-mapping/UPSTREAM.md`
    Expected Result: PRESENT printed; one 40-char SHA returned; count >= 1.
    Failure Indicators: file missing, no SHA, missing policy paragraph.
    Evidence: .sisyphus/evidence/task-1-upstream-md-shape.txt

  Scenario: SHA matches actual upstream HEAD
    Tool: Bash
    Preconditions: /home/soar/src/SoulsFormatsNEXT/.git exists.
    Steps:
      1. `cd /home/soar/src/SoulsFormatsNEXT && git rev-parse HEAD` → captured.
      2. `grep -oE '\b[a-f0-9]{40}\b' docs/api-mapping/UPSTREAM.md | head -1` → captured.
      3. Diff the two strings.
    Expected Result: Identical SHA.
    Failure Indicators: SHA in doc differs from upstream HEAD.
    Evidence: .sisyphus/evidence/task-1-sha-match.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): pin upstream SoulsFormatsNEXT commit baseline`
  - Files: `docs/api-mapping/UPSTREAM.md`
  - Pre-commit: none (doc only)

- [x] 2. **Capture pre-refactor golden hashes**

  **What to do**:
  - Build current branch: `cmake --build build-mingw`.
  - For every test that produces deterministic byte output (binary writer round-trips, DCX (de)compress, encoding converters, regulation decrypt of synthetic fixtures), instrument the test (or a one-shot harness `tests/golden/capture_golden.c`) to compute SHA-256 over the produced buffer and emit `<test-name>:<hash>` to stdout.
  - Persist captured hashes as `tests/golden/golden_hashes.h` constants and as a checked-in mirror at `.sisyphus/evidence/golden-hashes/baseline.txt`.
  - Add a CMake target `souls_formats_test_golden` (Unity test) that, given the same inputs, recomputes and `TEST_ASSERT_EQUAL_HEX8_ARRAY`s against the saved hashes.
  - Register golden test in `ctest` with label `golden`.

  **Must NOT do**:
  - Do NOT modify any existing source code outside `tests/golden/`.
  - Do NOT skip a test if its output is non-deterministic — instead, document its non-determinism source in a comment (e.g., "uses time-based IV"), and skip with `TEST_IGNORE_MESSAGE`.
  - Do NOT include real game files; only synthetic fixtures from existing tests.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high` — Non-trivial test scaffolding with deterministic output discipline.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1 (with Tasks 1, 3-7)
  - Blocks: 8-20 (all module fixes require golden baseline so refactor is safe)
  - Blocked By: None

  **References**:
  - **Pattern References**:
    - `tests/core/test_binary_writer.c:130-180` — Existing writer round-trip test pattern.
    - `tests/compression/test_dcx_dflt.c` — DCX synthetic fixtures.
    - `tests/crypto/test_regulation_decrypt.c:50-100` — Regulation flow.
  - **API References**:
    - `include/souls_formats/sf_io.h:200-280` — Binary writer API used in golden buffer construction.
  - **External References**:
    - Win32 BCrypt SHA-256: `BCryptOpenAlgorithmProvider("SHA256")`. Already wired in `src/crypto/md5_cng.c` for MD5; pattern is identical for SHA-256.

  **WHY each reference matters**: Existing tests show the synthetic-fixture style; the writer pattern is reused to deterministically build buffers; BCrypt SHA-256 is the only crypto we should pull into tests (no third-party deps).

  **Acceptance Criteria**:
  - [ ] `tests/golden/capture_golden.c` exists.
  - [ ] `tests/golden/golden_hashes.h` exists with ≥ 8 hash constants (one per existing deterministic test).
  - [ ] `.sisyphus/evidence/golden-hashes/baseline.txt` exists with same hashes.
  - [ ] `souls_formats_test_golden` registered in `tests/CMakeLists.txt` with label `golden`.
  - [ ] `ctest --test-dir build-mingw -L golden --output-on-failure` returns 0.

  **QA Scenarios**:
  ```
  Scenario: Golden test passes on baseline
    Tool: Bash
    Preconditions: Phase 0/1/2 build succeeds; tests/golden/ added.
    Steps:
      1. `cmake --build build-mingw --target souls_formats_test_golden`
      2. `ctest --test-dir build-mingw -L golden --output-on-failure`
    Expected Result: All hash assertions pass; exit 0.
    Failure Indicators: Any TEST_ASSERT_EQUAL_HEX8_ARRAY mismatch.
    Evidence: .sisyphus/evidence/task-2-golden-baseline.txt

  Scenario: Tampered output is detected
    Tool: Bash
    Preconditions: Golden test passes.
    Steps:
      1. Edit one byte of one synthetic-fixture writer setup in `tests/golden/capture_golden.c`.
      2. Rebuild and re-run golden test.
      3. Revert the edit and re-build.
    Expected Result: Step 2 fails with hash mismatch; step 3 returns to PASS.
    Failure Indicators: Step 2 still passes (test is no-op).
    Evidence: .sisyphus/evidence/task-2-golden-tamper.txt
  ```

  **Commit**: YES
  - Message: `test(golden): capture pre-refactor SHA-256 baseline for deterministic outputs`
  - Files: `tests/golden/*.c`, `tests/golden/golden_hashes.h`, `tests/CMakeLists.txt`, `.sisyphus/evidence/golden-hashes/baseline.txt`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L golden`

- [x] 3. **Stale-number sweep**

  **What to do**:
  - Replace `Phase 2 ⏳ next` with `Phase 2 ✅ done` in: `AGENTS.md` §2 status table, `docs/roadmap/README.md` index table, `docs/roadmap/phase-2-compression-crypto.md` header.
  - Run `find tests -name 'test_*.c' | wc -l` and `ctest --test-dir build-mingw -N | grep '^  Test ' | wc -l` to get accurate test counts; replace `49/49 across 5 binaries` in AGENTS.md and any other doc that quotes a count with the new accurate count plus "(verified at <date>)".
  - Replace AGENTS.md narrative `137 sf_* symbols` with a procedural placeholder: `<DLL exports — verify with x86_64-w64-mingw32-objdump -p libsouls_formats.dll | grep -c 'sf_'>` and remove the hard count (it will re-rot).
  - In `docs/roadmap/README.md` index table, add Phase 2 actual completion date and test count.

  **Must NOT do**:
  - Do NOT touch `.sisyphus/plans/PLAN.md` (handled in Task 37).
  - Do NOT remove the historical retrospective Phase 2 commit-time test counts inside Phase 2's "Completion" subsection (those are dated facts).
  - Do NOT change wording outside numbers/state markers.

  **Recommended Agent Profile**:
  - **Category**: `quick` — Targeted single-pattern replacements across 3 docs.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: 30-36 (roadmap rewrite assumes correct status)
  - Blocked By: None

  **References**:
  - **Pattern References**:
    - `AGENTS.md:26-43` — `## 2. Current status` heading + Phase status table + "Current artifacts" narrative (where `49/49 across 5 binaries` lives).
    - `docs/roadmap/README.md:18-30` — Phase index table.
    - `docs/roadmap/phase-2-compression-crypto.md:3` — Header status.

  **WHY each reference matters**: These are the three locations Metis identified as containing stale state markers; updating them keeps agent-facing docs honest before any further mapping work begins.

  **Acceptance Criteria** (greps explicitly exclude `.sisyphus/` since the plan file itself documents the stale state being fixed and would otherwise self-match):
  - [ ] `grep -r 'Phase 2.*next' --include='*.md' --exclude-dir=.sisyphus . | grep -v CHANGELOG.md` returns empty.
  - [ ] `grep -c '49/49' AGENTS.md` returns 0.
  - [ ] `grep -c '137 sf_\*' AGENTS.md` returns 0.
  - [ ] `grep -c 'Phase 2.*done' AGENTS.md docs/roadmap/README.md docs/roadmap/phase-2-compression-crypto.md` ≥ 3.

  **QA Scenarios**:
  ```
  Scenario: All stale markers removed (in production docs, not the plan)
    Tool: Bash
    Steps:
      1. `grep -rn 'Phase 2.*next' --include='*.md' --exclude-dir=.sisyphus .`
      2. `grep -rn '49/49' --include='*.md' --exclude-dir=.sisyphus .`
      3. `grep -rn '137 sf_\*' --include='*.md' --exclude-dir=.sisyphus .`
    Expected Result: Each grep returns 0 matches (or only matches in CHANGELOG retrospective).
    Failure Indicators: any match outside `.sisyphus/` and CHANGELOG.
    Evidence: .sisyphus/evidence/task-3-stale-grep.txt

  Scenario: New status reflects reality
    Tool: Bash
    Steps:
      1. `grep -A1 'Phase 2' AGENTS.md | head -20`
      2. `ctest --test-dir build-mingw -N | grep '^  Test ' | wc -l`
      3. Compare AGENTS.md test count to actual ctest count.
    Expected Result: Documented count matches actual count (or is documented procedurally).
    Failure Indicators: still says 49 or 5 binaries.
    Evidence: .sisyphus/evidence/task-3-status-match.txt
  ```

  **Commit**: YES
  - Message: `docs: refresh stale Phase 2 status markers and test counts`
  - Files: `AGENTS.md`, `docs/roadmap/README.md`, `docs/roadmap/phase-2-compression-crypto.md`
  - Pre-commit: none (doc only)

- [x] 4. **AGENTS.md alignment rules + §7 reconciliation**

  **What to do**:
  - In `AGENTS.md` §5 ("API conventions"), append a clearly-marked subsection `### 5.x STRICT UPSTREAM REFERENCE / API MIRRORS UPSTREAM` with the two user-supplied rules (verbatim Chinese + English translation):
    1. **STRICT UPSTREAM REFERENCE**: Every code implementation must strictly reference upstream code at the pinned commit (see `docs/api-mapping/UPSTREAM.md`). Guessing at semantics, signatures, or wire formats is FORBIDDEN. When in doubt, read the `.cs` file.
    2. **API MIRRORS UPSTREAM**: Public C API design must mirror upstream as closely as possible. Minor C-style adjustments (out-param error returns, pointer-based ownership, snake_case) are explicitly allowed. Functional differences are FORBIDDEN. Every divergence must be either (a) a documented C-style adaptation in `docs/api-mapping/POLICY.md`, or (b) a tracked extension in `docs/api-mapping/extensions.md`.
  - Append a sub-bullet linking to `docs/api-mapping/README.md` and `docs/api-mapping/POLICY.md`.
  - Reconcile §7 hard-constraint line 202 (currently "Do not change the public API of an existing function in v0.x without bumping the patch version and updating every test that uses it"). Replace with: "v0.x ABI breaks ARE permitted but require, in the same commit (or contiguous PR): (a) every removed or renamed symbol recorded in `CHANGELOG.md`, (b) every test that referenced the symbol updated, (c) version bump 0.x.y → 0.(x+1).0 (minor, not patch)."

  **Must NOT do**:
  - Do NOT remove or weaken any other §7 hard constraint.
  - Do NOT modify §1, §2, §3, §4, §6, §8 (out of scope).
  - Do NOT introduce new sections beyond what is specified.
  - Do NOT translate `STRICT UPSTREAM REFERENCE` / `API MIRRORS UPSTREAM` markers; they're searchable anchors.

  **Recommended Agent Profile**:
  - **Category**: `writing` — Doc-only edit with precise wording.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: 8-37 (every downstream task quotes these rules)
  - Blocked By: None

  **References**:
  - **Pattern References**:
    - `AGENTS.md:144-178` — §5 conventions, just before where new subsection lands.
    - `AGENTS.md:194-210` — §7 hard constraints; line 202 is the one to reconcile.

  **WHY each reference matters**: AGENTS.md is the agent-facing entry point; downstream agents will quote these rules verbatim.

  **Acceptance Criteria**:
  - [ ] `grep -c 'STRICT UPSTREAM REFERENCE' AGENTS.md` ≥ 1.
  - [ ] `grep -c 'API MIRRORS UPSTREAM' AGENTS.md` ≥ 1.
  - [ ] §7 line referencing patch-bump no longer present (`grep -c 'patch version' AGENTS.md` returns 0).
  - [ ] §7 contains new minor-bump clause (`grep -c '0.(x+1).0' AGENTS.md` ≥ 1).

  **QA Scenarios**:
  ```
  Scenario: New rules anchored
    Tool: Bash
    Steps:
      1. `grep -c 'STRICT UPSTREAM REFERENCE' AGENTS.md`
      2. `grep -c 'API MIRRORS UPSTREAM' AGENTS.md`
    Expected Result: Both >= 1.
    Failure Indicators: 0 for either marker.
    Evidence: .sisyphus/evidence/task-4-rules-anchored.txt

  Scenario: §7 ABI clause reconciled
    Tool: Bash
    Steps:
      1. `grep -n 'patch version' AGENTS.md`     # should be empty
      2. `grep -n '0.(x+1).0' AGENTS.md`         # should match new clause
      3. `grep -B2 -A2 'CHANGELOG.md' AGENTS.md` # should reference CHANGELOG
    Expected Result: 1 returns nothing; 2 and 3 return matches.
    Failure Indicators: stale clause still present.
    Evidence: .sisyphus/evidence/task-4-abi-clause.txt
  ```

  **Commit**: YES
  - Message: `docs(agents): add strict upstream alignment rules and reconcile §7 ABI clause`
  - Files: `AGENTS.md`
  - Pre-commit: none (doc only)

- [x] 5. **Create POLICY.md**

  **What to do**:
  - Create `docs/api-mapping/POLICY.md` with these explicit policies (each as a heading + 1-3 sentence rule + ≥1 example row):
    1. **Properties** (`{ get; set; }`): split into `_get_` and `_set_` functions. Side effects (e.g., `BinaryReaderEx.Position` triggers seek) documented inline.
    2. **Method overloads by parameter type**: suffix the C name with the param-type tag (e.g., `Foo(int)` → `sf_foo_i32`, `Foo(string)` → `sf_foo_str`).
    3. **Generic methods** (e.g., `T ReadEnum<T>()`): expand into typed variants (`_8 / _16 / _32 / _64`).
    4. **Default parameter values**: C has none; document the upstream default in the Notes column; never silently change behavior.
    5. **`params Xxx[]` (variadic arrays)**: map to `(size_t count, const T*)` pair; first parameter is count.
    6. **`out` / `ref`**: pointer parameter; `out` is unspecified-on-input, set on success.
    7. **Static state** (e.g., `BinaryReaderEx.IsFlexible`): per-instance flag with separate `sf_*_set_<flag>_default()` setter for global default. Document divergence.
    8. **`null` C# string vs empty**: NULL pointer = absent / not provided; empty string `""` = present-and-empty. Document per-API.
    9. **`internal` upstream APIs**: SKIP at policy level. Map only `public`. Exception: if `internal` is the only path to public behavior, escalate to extension entry.
    10. **Throwing exceptions**: convert to `sf_result_t` error code; document specific upstream exception type in Notes (e.g., `NoOodleFoundException` → `SF_ERR_OODLE_NOT_FOUND`).
    11. **Encoding boundary**: upstream uses `string` (UTF-16 internally); we expose UTF-8 at boundary, internally convert via Win32. Round-trip identity tested per encoding.
    12. **Round-trip invariant**: every format module must satisfy `read(write(x)) == x` and `write(read(b)) == b` byte-for-byte where applicable; tested per format.
    13. **Status legend**: `✓ aligned` / `~ partial` / `✗ deviation` / `+ extension` / `未实现` / `_skipped_`.

  **Must NOT do**:
  - Do NOT introduce policies beyond these 13.
  - Do NOT propose any auto-generation tooling.
  - Do NOT mandate a specific naming for type-tagged suffixes; leave the convention reasonable but flexible.

  **Recommended Agent Profile**:
  - **Category**: `writing` — Reference doc.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: 8-29 (mapping work cites POLICY)
  - Blocked By: None

  **References**:
  - **Pattern References**: None — new file.
  - **External References**: AGENTS.md §5 conventions (existing).

  **WHY each reference matters**: POLICY.md is the agreed grammar for every mapping row; without it, each row is ad-hoc.

  **Acceptance Criteria**:
  - [ ] `docs/api-mapping/POLICY.md` exists.
  - [ ] All 13 policy headings present (`grep -c '^### ' docs/api-mapping/POLICY.md` ≥ 13).
  - [ ] Status legend present (`grep -c '✓ aligned' docs/api-mapping/POLICY.md` ≥ 1).

  **QA Scenarios**:
  ```
  Scenario: All 13 policies present
    Tool: Bash
    Steps:
      1. `test -f docs/api-mapping/POLICY.md && echo PRESENT`
      2. `grep -c '^### ' docs/api-mapping/POLICY.md`
      3. `grep -E '^### ' docs/api-mapping/POLICY.md`
    Expected Result: PRESENT; count >= 13; titles cover Properties / Overloads / Generics / Defaults / params / out-ref / Static / null / internal / Exceptions / Encoding / Round-trip / Status.
    Failure Indicators: missing any of 13.
    Evidence: .sisyphus/evidence/task-5-policy-headings.txt

  Scenario: Each policy has at least one example
    Tool: Bash
    Steps:
      1. For each of 13 sections, count code blocks or example rows: `awk '/^### /{section=$0; count[section]=0} /```/||/^\| /{count[section]++} END {for (s in count) print count[s], s}' docs/api-mapping/POLICY.md`
    Expected Result: Each section has count >= 1.
    Failure Indicators: section without example.
    Evidence: .sisyphus/evidence/task-5-policy-examples.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): add POLICY.md defining C-mapping conventions`
  - Files: `docs/api-mapping/POLICY.md`
  - Pre-commit: none (doc only)

- [x] 6. **Bump version 0.1.0 → 0.2.0 + CHANGELOG.md skeleton**

  **What to do**:
  - Bump `CMakeLists.txt` `project(... VERSION 0.1.0)` → `0.2.0`.
  - Bump `include/souls_formats/sf_common.h` macros: `SF_VERSION_MINOR 1` → `2`, `SF_VERSION_PATCH 0` (unchanged), `SF_VERSION_STRING "0.1.0"` → `"0.2.0"`.
  - Create `CHANGELOG.md` (repo root) following Keep-A-Changelog format with sections:
    - `## [0.2.0] - <date>`
    - `### Breaking changes` (placeholder list to be populated by Tasks 8-20)
    - `### New APIs` (placeholder list)
    - `### Notes` — link to `docs/api-mapping/UPSTREAM.md`, mention strict-alignment policy, mention "Tier A row-level mapping for in-scope formats; Tier B inventory for legacy".
    - `## [0.1.0] - 2026-05-10` retrospective entry summarizing Phase 0/1/2 deliverables.
  - Update AGENTS.md narrative wherever `0.1.0` or `v0.1.0` is mentioned to reference the post-refactor 0.2.0 baseline.

  **Must NOT do**:
  - Do NOT bump major to 1.0.0 (still pre-1.0).
  - Do NOT remove the 0.1.0 retrospective from CHANGELOG.
  - Do NOT add other version-related metadata files (no `.version`, no `VERSION` file).

  **Recommended Agent Profile**:
  - **Category**: `quick` — Touch 3-4 files with surgical edits.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: 8-20 (CHANGELOG entries land per task)
  - Blocked By: None

  **References**:
  - **Pattern References**:
    - `CMakeLists.txt:1-20` — top-level project() call.
    - `include/souls_formats/sf_common.h:21-24` — version macros.
  - **External References**:
    - https://keepachangelog.com/en/1.1.0/ — CHANGELOG format reference.

  **WHY each reference matters**: Version is sourced from two places (CMake + header) that must agree; CHANGELOG follows a community-standard format so consumers can parse it.

  **Acceptance Criteria**:
  - [ ] `grep -c '0.2.0' CMakeLists.txt include/souls_formats/sf_common.h CHANGELOG.md AGENTS.md` ≥ 4 (one match per file).
  - [ ] `CHANGELOG.md` has `## [0.2.0]`, `## [0.1.0]`, `### Breaking changes`, `### New APIs`, `### Notes` headings.
  - [ ] Build still succeeds after bump: `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug && cmake --build build-mingw`.

  **QA Scenarios**:
  ```
  Scenario: Version bump applied consistently
    Tool: Bash
    Steps:
      1. `grep '0.2.0' CMakeLists.txt include/souls_formats/sf_common.h CHANGELOG.md AGENTS.md`
      2. `grep '0.1.0' include/souls_formats/sf_common.h CMakeLists.txt`  # should be 0
    Expected Result: Step 1 returns >=4 matches; step 2 returns 0.
    Failure Indicators: any 0.1.0 left in CMakeLists or sf_common.h.
    Evidence: .sisyphus/evidence/task-6-version.txt

  Scenario: Build still works after bump
    Tool: Bash
    Steps:
      1. `rm -rf build-mingw`
      2. `cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug`
      3. `cmake --build build-mingw 2>&1 | tail -30`
      4. `ctest --test-dir build-mingw --output-on-failure 2>&1 | tail -20`
    Expected Result: configure + build succeed; ctest exits 0; SF_VERSION_STRING printed in any banner is "0.2.0".
    Failure Indicators: build fails or test count drops.
    Evidence: .sisyphus/evidence/task-6-build.txt
  ```

  **Commit**: YES
  - Message: `chore: bump version 0.1.0 → 0.2.0 and add CHANGELOG.md`
  - Files: `CMakeLists.txt`, `include/souls_formats/sf_common.h`, `CHANGELOG.md`, `AGENTS.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`

- [x] 7. **docs/api-mapping/{README, extensions, drift-checklist}.md skeletons**

  **What to do**:
  - Create `docs/api-mapping/README.md` with: top-level overview, link to UPSTREAM.md and POLICY.md, table-of-contents listing every Tier-A doc (`util-io-binary-reader-ex.md`, `util-io-binary-writer-ex.md`, …) and the single Tier-B `legacy.md`, status-legend reproduction, "How to add a new mapping doc" mini-guide.
  - Create `docs/api-mapping/extensions.md` with: schema definition (columns: `Symbol | Module | Rationale | Lifecycle | Source loc`), seed entries for every C-library convenience helper currently in code that has no upstream analogue: `sf_dcx_unwrap`, `sf_ostream_detach_buffer`, `sf_oodle_set_search_path`, `sf_oodle_unload`, `sf_oodle_load`, `sf_istream_t` and `sf_ostream_t` as a layered abstraction (single row each).
  - Create `docs/api-mapping/drift-checklist.md` listing every drift item from this plan's Context section (Phase 1 + Phase 2) as `- [ ]` checkboxes grouped by module. Each item has the form `[Module] <description> (closes when Task NN ships)`.

  **Must NOT do**:
  - Do NOT pre-fill any Tier-A row content; only the index entry.
  - Do NOT mark any drift item as completed yet.
  - Do NOT add per-format files yet (Tasks 21-29 own those).

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 1
  - Blocks: 8-29 (downstream tasks edit these skeletons)
  - Blocked By: None

  **References**:
  - **Pattern References**:
    - `docs/roadmap/README.md` — doc-tree-index pattern to mirror.

  **WHY each reference matters**: The roadmap README is the existing index style; api-mapping/README.md should match the project's house style.

  **Acceptance Criteria**:
  - [ ] All three files exist.
  - [ ] `docs/api-mapping/README.md` lists ≥ 25 Tier-A doc entries (one per future-task mapping doc).
  - [ ] `extensions.md` has at least 6 seed entries (sf_dcx_unwrap, sf_ostream_detach_buffer, sf_oodle_set_search_path, sf_oodle_unload, sf_oodle_load, layered-stream).
  - [ ] `drift-checklist.md` has ≥ 25 unchecked items grouped by module heading.

  **QA Scenarios**:
  ```
  Scenario: Skeletons present and well-shaped
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/{README,extensions,drift-checklist}.md`
      2. `grep -c '^| ' docs/api-mapping/extensions.md`           # >= 7 (header + 6+ rows)
      3. `grep -c '^- \[ \]' docs/api-mapping/drift-checklist.md` # >= 25
      4. `grep -c '\[.*\](.*\.md)' docs/api-mapping/README.md`    # >= 25 internal links
    Expected Result: All counts meet thresholds.
    Failure Indicators: missing files or thin skeletons.
    Evidence: .sisyphus/evidence/task-7-skeletons.txt

  Scenario: Drift checklist organisation matches Context
    Tool: Bash
    Steps:
      1. `grep -E '^## ' docs/api-mapping/drift-checklist.md`
    Expected Result: Headings include "Phase 1 / BinaryReaderEx", "Phase 1 / BinaryWriterEx", "Phase 1 / PathHelper", "Phase 1 / HashHelper", "Phase 1 / SFUtil", "Phase 2 / DCX", "Phase 2 / Oodle", "Phase 2 / RegulationDecryptor", "Phase 2 / SL2Decryptor".
    Failure Indicators: heading missing.
    Evidence: .sisyphus/evidence/task-7-checklist-headings.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): scaffold README, extensions, drift-checklist`
  - Files: `docs/api-mapping/README.md`, `docs/api-mapping/extensions.md`, `docs/api-mapping/drift-checklist.md`
  - Pre-commit: none (doc only)

- [x] 8. **BinaryReaderEx — map + fix + test**

  **What to do**:
  - **Map**: create `docs/api-mapping/util-io-binary-reader-ex.md`. Six-column table (Upstream signature | Upstream loc | Kind | Our API | Status | Notes). Cover ALL ~103 public methods + 9 properties + 3 ctors. Read `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryReaderEx.cs` end to end; do NOT cherry-pick.
  - **Fix** in `include/souls_formats/sf_io.h` + `src/core/binary_reader.c`:
    1. Add plural reads for every primitive: `sf_binary_reader_read_i8s/u8s/i16s/u16s/i32s/u32s/i64s/u64s/f32s/f64s/varints/bools` — signature `(sf_binary_reader_t*, size_t n, T *out_array)`.
    2. Add `Get*` coverage: `get_bool/i8/u8/u16/i16/f32/f64/varint` + plural `get_*s` (each takes `int64_t off`).
    3. Add `GetASCII / GetUTF16 / GetShiftJIS` (with and without length).
    4. Refactor `assert_*` to multi-option: `(sf_binary_reader_t*, size_t n_options, const T *options, T *out_value)`. Keep single-option convenience wrappers (`assert_u32_one(r, expect)`).
    5. Add `assert_i8 / sbyte / i16 / boolean / single / double / i64 / varint`.
    6. Add `read_enum_8 / 16 / 32 / 64`: validates that the value is a defined enum case (consumer-supplied option list).
    7. Add `is_flexible` per-reader flag + `sf_binary_reader_set_flexible_default(bool)` global default.
    8. Expose `sf_binary_reader_stream(reader) -> sf_istream_t*` (read-only access).
    9. Rename `read_vec3_11_11_10` → `read_11_11_10_vec3` to match upstream `Read11_11_10Vector3`. **Clean break**: do NOT keep the old name as an alias (per the "no deprecation/shim layer" guardrail). Record in `CHANGELOG.md` "Breaking changes" with the explicit rename so consumers (none yet at v0.x) can grep+replace.
  - **Test** in `tests/core/test_binary_reader.c`: add ≥ 30 sub-cases covering every new method. Each `assert_*` multi-option case uses 3 valid + 1 invalid option set. Add Shift-JIS round-trip identity test.
  - Update `CHANGELOG.md` `### Breaking changes` (multi-option Assert sig) and `### New APIs` (everything else).
  - Tick the corresponding items in `docs/api-mapping/drift-checklist.md`.

  **Must NOT do**:
  - Do NOT change the existing `sf_binary_reader_t` opaque struct layout in a non-additive way (additive only — append fields at end).
  - Do NOT remove existing single-arg `assert_*`; convert to convenience wrappers.
  - Do NOT add `IsFlexible` as a global without per-reader override.
  - Do NOT touch `BinaryWriterEx` work (Task 9).

  **Recommended Agent Profile**:
  - **Category**: `deep` — Multi-step refactor + comprehensive map + tests; needs to read 1255-line upstream file thoroughly.
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (with Tasks 9-14)
  - Parallel Group: Wave 2
  - Blocks: 21-37 (downstream mapping cites finalized signatures)
  - Blocked By: 1, 2, 4, 5

  **References**:
  - **Pattern References**:
    - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryReaderEx.cs:1-1255` — upstream surface (entire file).
    - `src/core/binary_reader.c:1-end` — current implementation.
    - `include/souls_formats/sf_io.h:104-200` — current public API.
    - `tests/core/test_binary_reader.c` — test patterns.
  - **API References**:
    - `docs/api-mapping/POLICY.md` (Task 5) — generics, params, properties policy.
    - `tests/golden/golden_hashes.h` (Task 2) — preserve byte-output identity.

  **WHY each reference matters**: BinaryReaderEx is the workhorse; missing methods will block every Phase 3+ format module. Reading the upstream file in full is non-negotiable per the new policy.

  **Acceptance Criteria**:
  - [ ] `docs/api-mapping/util-io-binary-reader-ex.md` exists; `grep -c '^|' docs/api-mapping/util-io-binary-reader-ex.md ≥ 105`.
  - [ ] Every row's `Upstream loc` references `BinaryReaderEx.cs:<line>`.
  - [ ] Build passes: `cmake --build build-mingw` (Werror).
  - [ ] `ctest --test-dir build-mingw -L core --output-on-failure` exits 0; new sub-cases pass.
  - [ ] `ctest --test-dir build-mingw -L golden` exits 0 (no regression in deterministic outputs).
  - [ ] `objdump -p libsouls_formats.dll | grep -c 'sf_binary_reader_'` ≥ 60 (was ~40).
  - [ ] `CHANGELOG.md` updated with the breaking-change + new-API entries.
  - [ ] `drift-checklist.md` BinaryReaderEx items all `- [x]`.

  **QA Scenarios**:
  ```
  Scenario: New plural reads round-trip
    Tool: Bash + Unity
    Preconditions: Build green.
    Steps:
      1. `ctest --test-dir build-mingw -R test_binary_reader -V` capture output.
      2. Confirm sub-cases cover read_i32s/u16s/booleans/varints/floats/etc.
      3. Confirm Shift-JIS round-trip identity sub-case present (`Shift-JIS round-trip identity`).
    Expected Result: All sub-cases PASS.
    Failure Indicators: any failure or missing sub-case.
    Evidence: .sisyphus/evidence/task-8-reader-tests.txt

  Scenario: Multi-option Assert rejects invalid + accepts valid
    Tool: Bash
    Steps:
      1. Inspect test source for `assert_u32` calls with options array.
      2. Run sub-case targeting an invalid value; confirm SF_ERR_BAD_MAGIC.
      3. Run sub-case with valid value; confirm SF_OK.
    Expected Result: Both behaviors observed.
    Failure Indicators: Assert returns OK on invalid value.
    Evidence: .sisyphus/evidence/task-8-assert-multi.txt

  Scenario: Mapping doc rows resolve at upstream
    Tool: Bash
    Steps:
      1. `awk -F '|' '/BinaryReaderEx.cs:/{print $3}' docs/api-mapping/util-io-binary-reader-ex.md | head -10`
      2. For each line ref, `sed -n '<line>p' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryReaderEx.cs`
      3. Verify content matches the cited signature.
    Expected Result: Sampled refs all resolve to the cited symbol.
    Failure Indicators: ref points at unrelated code.
    Evidence: .sisyphus/evidence/task-8-loc-spotcheck.txt
  ```

  **Commit**: YES (multi-file commit)
  - Message: `feat(io)!: align sf_binary_reader with upstream BinaryReaderEx (G/Assert/plural/enum/flexible)`
  - Files: `include/souls_formats/sf_io.h`, `src/core/binary_reader.c`, `tests/core/test_binary_reader.c`, `docs/api-mapping/util-io-binary-reader-ex.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L 'core|golden' --output-on-failure`

- [x] 9. **BinaryWriterEx — map + fix + test**

  **What to do**:
  - **Map**: create `docs/api-mapping/util-io-binary-writer-ex.md`. Six-column table covering all ~75 public methods + properties + ctors. Read `BinaryWriterEx.cs` (896 lines) in full.
  - **Fix** in `sf_io.h` + `src/core/binary_writer.c`:
    1. Add Reserve/Fill for the 7 missing types: `bool / i8 / u8 / i16 / u16 / f32 / f64`. Match upstream's `(name, typeName)` keying — typeName is the C type-tag string (`"i32"`, `"f32"`, etc.).
    2. Add plural writes: `write_*s` for every primitive (12 types).
    3. Add `pad_FF(align)` shorthand (calls `pad_byte(align, 0xFF)` internally).
    4. Rename `pattern` → `write_pattern` to match upstream `WritePattern`.
    5. Add 3-mode finish: keep current `finish()` (verify), add `to_array()` (snapshot bytes; doesn't close), add `finish_bytes()` (snapshot + verify + close). Each returns `(uint8_t **out, size_t *out_size)` via out-param + status.
    6. Expose `sf_binary_writer_stream(writer) -> sf_ostream_t*` (read-only).
  - **Test** in `tests/core/test_binary_writer.c`: ≥ 25 new sub-cases. Test reservation list integrity (e.g., `reserve_f32` then `fill_f32` round-trips a float; mixing types raises error). Test 3-mode finish semantics.
  - Update `CHANGELOG.md` and `drift-checklist.md`.

  **Must NOT do**:
  - Do NOT change reservation byte-storage layout if upstream stores reserved bytes in-band; if upstream zero-fills then patches on Fill, mirror exactly.
  - Do NOT touch reader code (Task 8).
  - Do NOT remove `sf_ostream_detach_buffer` (it's an extension; keep, document).

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (with Task 8, 10-14)
  - Parallel Group: Wave 2
  - Blocks: 15, 19, 20, 21-37
  - Blocked By: 1, 2, 4, 5

  **References**:
  - **Pattern References**:
    - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/BinaryWriterEx.cs:1-896`
    - `src/core/binary_writer.c:1-end`
    - `include/souls_formats/sf_io.h:200-end`
    - `tests/core/test_binary_writer.c`

  **WHY each reference matters**: Reservation byte-layout MUST match upstream exactly so files written by upstream tools and us are bit-identical.

  **Acceptance Criteria**:
  - [ ] Mapping doc ≥ 75 rows.
  - [ ] All 12 types have Reserve/Fill pairs (`grep -c 'reserve_' include/souls_formats/sf_io.h ≥ 12`).
  - [ ] `pad_FF`, `write_pattern`, `to_array`, `finish_bytes` exposed.
  - [ ] Build green; tests green; golden hashes unchanged.
  - [ ] CHANGELOG + drift-checklist updated.

  **QA Scenarios**:
  ```
  Scenario: Reserve/Fill all 12 types
    Tool: Bash
    Steps:
      1. `ctest --test-dir build-mingw -R test_binary_writer -V`
      2. Inspect output for sub-cases per Reserve type (12 expected).
    Expected Result: 12 Reserve sub-cases all PASS.
    Failure Indicators: missing type or test failure.
    Evidence: .sisyphus/evidence/task-9-reserve-fill.txt

  Scenario: 3-mode finish behaves
    Tool: Bash
    Steps:
      1. Run sub-case writing data, calling `to_array` (snapshot), continuing to write, then `finish_bytes`.
      2. Confirm snapshot is shorter than final bytes; final bytes contain snapshot prefix.
    Expected Result: Snapshot length < final length; prefix matches.
    Failure Indicators: snapshot mutates or finish_bytes fails.
    Evidence: .sisyphus/evidence/task-9-finish-modes.txt
  ```

  **Commit**: YES
  - Message: `feat(io)!: align sf_binary_writer with upstream BinaryWriterEx (Reserve/Fill 12, plurals, 3-mode finish)`
  - Files: `include/souls_formats/sf_io.h`, `src/core/binary_writer.c`, `tests/core/test_binary_writer.c`, `docs/api-mapping/util-io-binary-writer-ex.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L 'core|golden'`

- [x] 10. **SFEncoding map + verify**

  **What to do**:
  - Create `docs/api-mapping/util-text-sf-encoding.md` mapping the 4 public-static-readonly Encoding fields (ASCII / ShiftJIS / UTF16 / UTF16BE) to our `sf_*_to_utf8` and `sf_utf8_to_*` converter pairs. Document this as a deliberate C-style adaptation: upstream exposes singletons of `System.Text.Encoding`; we cannot mirror that idiom in C, so we expose imperative converter functions instead. This adaptation is described once in POLICY.md (Encoding boundary policy); reference it.
  - Verify: ensure all 8 conversion directions (4 encodings × {to_utf8, from_utf8}) are present and tested. The current `sf_encoding.h` already has them; if any direction is missing, add.
  - Add round-trip identity test in `tests/core/test_encoding.c` for each non-ASCII encoding.
  - Tick drift-checklist items for SFEncoding.

  **Must NOT do**:
  - Do NOT add encoding singletons or Encoding-object factories; we use imperative functions.
  - Do NOT change function signatures of existing converters.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 2
  - Blocks: 21-37
  - Blocked By: 1, 4, 5

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Text/SFEncoding.cs` (full file)
  - `include/souls_formats/sf_encoding.h` + `src/core/encoding_win32.c`
  - `tests/core/test_encoding.c`
  - `docs/api-mapping/POLICY.md` Encoding boundary section

  **Acceptance Criteria**:
  - [ ] Mapping doc exists with 4 rows (one per encoding) + a "C-style adaptation" header note.
  - [ ] All 8 conversion functions present.
  - [ ] Round-trip identity test passes for ASCII / Shift-JIS / UTF-16 LE / UTF-16 BE.

  **QA Scenarios**:
  ```
  Scenario: Round-trip identity for all encodings
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_encoding -V`
      2. Confirm sub-cases for ASCII, Shift-JIS, UTF-16 LE, UTF-16 BE round-trip.
    Expected Result: 4 sub-cases PASS.
    Failure Indicators: round-trip diverges (byte mismatch).
    Evidence: .sisyphus/evidence/task-10-encoding-roundtrip.txt

  Scenario: Mapping doc structure
    Tool: Bash
    Steps:
      1. `grep -c '^|' docs/api-mapping/util-text-sf-encoding.md`  # >= 6
      2. `grep 'C-style adaptation' docs/api-mapping/util-text-sf-encoding.md`
    Expected Result: rows present; adaptation note present.
    Evidence: .sisyphus/evidence/task-10-encoding-doc.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map SFEncoding; add round-trip identity tests`
  - Files: `docs/api-mapping/util-text-sf-encoding.md`, `tests/core/test_encoding.c`, `docs/api-mapping/drift-checklist.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L core`

- [x] 11. **PathHelper — map + add sf_path.h + test**

  **What to do**:
  - **Map**: create `docs/api-mapping/util-io-path-helper.md` covering 3 upstream public methods (full body cited from `PathHelper.cs:13-41` of pinned commit):
    - `static string Backup(string file, bool overwrite = false)` (lines 13-19): `bak = file + ".bak"; if (overwrite || !File.Exists(bak)) File.Copy(file, bak, overwrite); return bak;`
    - `static string GetRealExtension(string path)` (lines 24-30): if rightmost extension is `.dcx`, return next-level extension instead.
    - `static string GetRealFileName(string path)` (lines 35-41): if rightmost extension is `.dcx`, return name minus both `.dcx` and the inner extension; otherwise return name minus single extension.
  - **Fix**: create new public header `include/souls_formats/sf_path.h` and source `src/core/path.c`:
    - `sf_result_t sf_path_backup(const char *utf8_path, bool overwrite, char **out_backup_path, const sf_allocator_t*)`. **Behavior MUST mirror upstream byte-for-byte**: backup path is always `<utf8_path> + ".bak"` (literal single-`.bak` suffix; **no numeric increment**). If `overwrite==false` AND `<file>.bak` already exists → do NOT copy (still return the same `.bak` path). If `overwrite==true` → forcibly copy via `CopyFileW(..., FALSE)` semantics matching `File.Copy(src, dst, true)` (overwrite). Use Win32 `CopyFileW` + `GetFileAttributesW`. Map `default-arg overwrite=false` per `POLICY.md` (no defaults in C; document in header).
    - `sf_result_t sf_path_get_real_extension(const char *utf8_path, char **out_ext, const sf_allocator_t*)`. Mirrors `PathHelper.cs:24-30`. Examples: `bar.flver.dcx → .flver`, `bar.txt → .txt`, `bar → "" (empty)`.
    - `sf_result_t sf_path_get_real_file_name(const char *utf8_path, char **out_name, const sf_allocator_t*)`. Mirrors `PathHelper.cs:35-41`. Examples: `bar.flver.dcx → bar`, `bar.txt → bar`, `/path/to/bar → bar`.
  - **Test** in `tests/core/test_path.c`: synthetic fixtures covering each method:
    - `Backup` happy: temp file → `<temp>.bak` exists, contents identical, returned path == `<temp>.bak`.
    - `Backup` no-overwrite-when-exists: temp file → first call creates `.bak`; touch `<temp>.bak.original-marker`; second call with `overwrite=false` → `.bak` is untouched (modification time unchanged or marker still exists), returned path still `<temp>.bak`.
    - `Backup` overwrite-when-exists: same setup, second call with `overwrite=true` → `.bak` content matches latest source.
    - `GetRealExtension` cases: `bar.flver.dcx → .flver`, `bar.txt → .txt`, `bar → ""`, `/path/bar.flver.dcx → .flver`.
    - `GetRealFileName` cases: `bar.flver.dcx → bar`, `bar.txt → bar`, `/path/bar.flver.dcx → bar`.
  - Add to `souls_formats.h` umbrella, `tests/CMakeLists.txt`, drift-checklist.

  **Must NOT do**:
  - Do NOT use stdio (`fopen`); use Win32 `CopyFileW`.
  - Do NOT invent suffix incrementing (`.bak1` / `.bak2`) or any other behavior absent from `PathHelper.cs`. Strict alignment.
  - Do NOT silently change the "do nothing if .bak exists and overwrite==false" semantic — faithfully mirror it.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 2
  - Blocks: 21-37
  - Blocked By: 1, 4, 5

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/IO/PathHelper.cs` (43 lines, full file)
  - `src/core/encoding_win32.c` — UTF-8 ↔ UTF-16 helper pattern
  - Win32 `CopyFileW`, `GetFileAttributesW`

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_path.h` + `src/core/path.c` exist.
  - [ ] All 3 functions exported (`objdump -p libsouls_formats.dll | grep sf_path_`).
  - [ ] `test_path` registered with label `core`; PASS.
  - [ ] Mapping doc has all 3 rows.

  **QA Scenarios**:
  ```
  Scenario: Real-extension and file-name stripping
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_path -V`
      2. Inspect sub-cases: `bar.flver.dcx → .flver`; `bar.txt → .txt`; `bar → ""`; `/path/bar.flver.dcx → bar`.
    Expected Result: All sub-cases PASS, results match upstream `PathHelper.cs:24-41` byte-for-byte.
    Failure Indicators: any deviation from upstream rule (e.g., `bar.dcx` returning `.dcx` instead of `""`).
    Evidence: .sisyphus/evidence/task-11-real-ext.txt

  Scenario: Backup with overwrite=false preserves existing .bak (mirrors upstream exactly)
    Tool: Bash + Unity
    Steps:
      1. Sub-case creates temp file `/tmp/sf_test/orig.bin` (under WSL temp dir; PE accessed via UNC `\\wsl.localhost\...`).
      2. Calls `sf_path_backup(orig.bin, overwrite=false)` → returns `/tmp/sf_test/orig.bin.bak` (literal `.bak` suffix).
      3. Mutates `orig.bin` to a new content; touches `orig.bin.bak` to record current mtime.
      4. Calls `sf_path_backup(orig.bin, overwrite=false)` again → confirm returned path is still `orig.bin.bak`; confirm `orig.bin.bak` content has NOT changed (still matches step-2 content); confirm mtime unchanged.
    Expected Result: Same returned path; `.bak` file untouched (no `.bak1` created); content + mtime preserved.
    Failure Indicators: any `.bak1` / numeric suffix appears; `.bak` content silently overwritten.
    Evidence: .sisyphus/evidence/task-11-backup-no-overwrite.txt

  Scenario: Backup with overwrite=true overwrites existing .bak
    Tool: Bash + Unity
    Steps:
      1. Setup as previous scenario (orig.bin + orig.bin.bak from initial backup).
      2. Mutate orig.bin to new content `0xCAFE`.
      3. Call `sf_path_backup(orig.bin, overwrite=true)` → returns `orig.bin.bak`.
      4. Read `orig.bin.bak` content → confirm == new content `0xCAFE`.
    Expected Result: `.bak` content matches latest source; returned path still `orig.bin.bak`.
    Failure Indicators: `.bak` content stale; new file path created.
    Evidence: .sisyphus/evidence/task-11-backup-overwrite.txt
  ```

  **Commit**: YES
  - Message: `feat(path): add sf_path.h with PathHelper-equivalent helpers`
  - Files: `include/souls_formats/sf_path.h`, `src/core/path.c`, `tests/core/test_path.c`, `tests/CMakeLists.txt`, `include/souls_formats/souls_formats.h`, `docs/api-mapping/util-io-path-helper.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L core`

- [x] 12. **HashHelper — map + IsPrime + test**

  **What to do**:
  - **Map**: create `docs/api-mapping/util-cryptography-hash-helper.md` covering 2 public methods: `FromPathHash(string)`, `IsPrime(uint)`.
  - **Fix** in `include/souls_formats/sf_hash.h` + `src/core/filename_hash.c`:
    - Verify `sf_path_hash` (existing) maps to `FromPathHash`. Update mapping row to `✓ aligned`.
    - Add `bool sf_is_prime(uint32_t candidate)` matching upstream `IsPrime`. Read `HashHelper.cs:IsPrime` to confirm exact algorithm.
  - **Test** in `tests/core/test_filename_hash.c`: add ≥ 8 sub-cases for IsPrime covering small primes (2, 3, 5, 7, 11), composites (4, 6, 9, 15), boundary (0, 1), and a known large prime (e.g., 65537).

  **Must NOT do**:
  - Do NOT change `sf_path_hash` semantics (it's already aligned).
  - Do NOT use stdlib's mathematical libraries; pure C bit ops.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 2
  - Blocks: 21-37 (used by BHD5/BND4 hash-table sizing in future phases)
  - Blocked By: 1, 4, 5

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/HashHelper.cs` (42 lines, full file)
  - `include/souls_formats/sf_hash.h`
  - `src/core/filename_hash.c`

  **Acceptance Criteria**:
  - [ ] Mapping doc 2 rows.
  - [ ] `sf_is_prime` exported; matches upstream output for ≥ 8 test inputs.
  - [ ] `ctest -R test_filename_hash` PASS.

  **QA Scenarios**:
  ```
  Scenario: IsPrime matches upstream for samples
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_filename_hash -V`
      2. Inspect IsPrime sub-cases: small primes/composites/boundaries/large prime.
    Expected Result: All sub-cases PASS.
    Evidence: .sisyphus/evidence/task-12-isprime.txt

  Scenario: Mapping doc resolves at upstream loc
    Tool: Bash
    Steps:
      1. `grep 'HashHelper.cs:' docs/api-mapping/util-cryptography-hash-helper.md`
      2. For each line, `sed -n '<n>p' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/HashHelper.cs`
    Expected Result: Line refs resolve to FromPathHash and IsPrime headers.
    Evidence: .sisyphus/evidence/task-12-loc.txt
  ```

  **Commit**: YES
  - Message: `feat(hash): add sf_is_prime; map HashHelper`
  - Files: `include/souls_formats/sf_hash.h`, `src/core/filename_hash.c`, `tests/core/test_filename_hash.c`, `docs/api-mapping/util-cryptography-hash-helper.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L core`

- [x] 13. **SFUtil — map + sf_get_decompressed_reader + test**

  **What to do**:
  - **Map**: create `docs/api-mapping/util-sf-util.md` covering ALL 3 upstream public methods of `SoulsFormats.SFUtil` (verified at pinned commit `Utilities/SFUtil.cs:1-49`):
    - Row 1: `GetDecompressedBinaryReader(BinaryReaderEx br, out DCX.CompressionInfo compression)` (lines 13-25). Status: `未实现` → `✓ aligned` after fix below.
    - Row 2: `ConcatAll<T>(params IEnumerable<T>[] lists)` (lines 30-36). Status: `_skipped_`. Notes: ".NET-LINQ helper for collection concatenation. C consumers manage their own collections; if equivalent primitive is needed, klib `kvec_t` push-loops or `memcpy` into a sized `T*` buffer give the same result. Not a C-language idiom — intentional skip per `POLICY.md` §9 (internal-helper-bridge equivalent: skipped at policy level)."
    - Row 3: `Dictionize<T>(List<T> items)` (lines 41-47). Status: `_skipped_`. Notes: ".NET helper to build `Dictionary<int, T>` from `List<T>` keyed by index. C consumers can either treat the array's index as the key directly (no dict needed) or use klib `khash_t` if hash semantics required. Not a C-language idiom — intentional skip per `POLICY.md` §9."
  - **Fix**: add public `sf_result_t sf_get_decompressed_reader(sf_binary_reader_t *in, sf_binary_reader_t **out_reader, sf_dcx_compression_info_t *out_info, const sf_allocator_t*)` in `sf_io.h` (do NOT create a new `sf_util.h` — keep header count manageable; matches Wave-3 placement after T15 lands the tagged union). Behavior mirrors `SFUtil.cs:13-25`: probe input via `sf_dcx_is_*` (Task 15-introduced). If DCX, decompress + build new reader on top of memory-backed istream, fill `out_info` with concrete variant via `out` parameter; *out_reader is heap-owned by caller (free via reader_destroy + the new istream + the heap buffer). If not DCX, set `*out_reader = in` (caller treats as borrow; no destruction needed for that pointer) and fill `out_info->type = SF_DCX_TYPE_NONE` (NoCompressionInfo equivalent). Document ownership semantics inline in the header doc-comment with the exact "borrow vs new-allocation" rule.
  - Do NOT add C-side `sf_concat_all` or `sf_dictionize`; those rows stay `_skipped_` in the mapping doc.
  - **Test** in `tests/core/test_sf_util.c` (new): synthetic Zlib-wrapped buffer + non-DCX buffer; verify both paths. No tests for `ConcatAll` / `Dictionize` (intentionally not implemented).

  **Must NOT do**:
  - Do NOT close the input reader on the not-DCX path; clearly document borrow vs new-allocation semantics.
  - Do NOT introduce a new `sf_util.h`; reuse `sf_io.h`.
  - Do NOT add C-side `ConcatAll` / `Dictionize` equivalents; mark them `_skipped_` in the mapping doc with the rationale captured above.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: `[]`

  **Parallelization** (post-Momus cycle break):
  - Can Run In Parallel: YES (with T16, T17, T19, T20 in Wave 3)
  - Parallel Group: **Wave 3** (moved here from Wave 2 to break T13 ↔ T15 cycle)
  - Blocks: 21-37 (Phase 3+ format modules use this convenience)
  - Blocked By: 1, 2, 4, 5, **8** (BinaryReaderEx fixes), **15** (DCX tagged-union info type — `sf_dcx_compression_info_t` is filled by T13's out-param)
  - **Rationale**: T13 was previously Wave 2 but its `sf_get_decompressed_reader` API uses the new tagged-union `sf_dcx_compression_info_t` from T15. Doing T13 after T15 keeps the API consistent on first commit (no retrofit churn).

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/SFUtil.cs` (49 lines)
  - `src/compression/dcx.c` (existing decompress path)
  - `include/souls_formats/sf_io.h` + `sf_dcx.h`

  **Acceptance Criteria**:
  - [ ] `sf_get_decompressed_reader` exported.
  - [ ] `test_sf_util` registered; PASS.
  - [ ] Mapping doc has **3 rows** (one per upstream public method): `GetDecompressedBinaryReader` → `✓ aligned`; `ConcatAll<T>` → `_skipped_`; `Dictionize<T>` → `_skipped_`. Each row includes a `SFUtil.cs:<line>` reference and rationale in Notes column.
  - [ ] Ownership note in `sf_io.h` doc-comment explicitly distinguishes the borrow-vs-new-allocation paths.

  **QA Scenarios**:
  ```
  Scenario: Decompress path returns new reader; consumer destroys both
    Tool: Bash + Unity
    Steps:
      1. Sub-case constructs Zlib-wrapped buffer; calls sf_get_decompressed_reader.
      2. Reads decompressed bytes via new reader.
      3. Destroys new reader, original reader, allocator buffers.
    Expected Result: Round-trip succeeds; no leaks reported by ASan if enabled.
    Evidence: .sisyphus/evidence/task-13-decomp-path.txt

  Scenario: Non-DCX path borrows input reader
    Tool: Bash + Unity
    Steps:
      1. Sub-case constructs raw bytes (no DCX magic).
      2. Confirms returned reader pointer == input reader pointer.
      3. Confirms out_info->type == SF_DCX_TYPE_NONE.
    Expected Result: Returned pointer identical; type=NONE.
    Evidence: .sisyphus/evidence/task-13-nondcx-path.txt
  ```

  **Commit**: YES
  - Message: `feat(util): add sf_get_decompressed_reader (SFUtil-equivalent)`
  - Files: `include/souls_formats/sf_io.h`, `src/core/sf_util.c` (or merge into existing `binary_reader.c`), `tests/core/test_sf_util.c`, `tests/CMakeLists.txt`, `docs/api-mapping/util-sf-util.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L core`

- [x] 14. **Math (Vector / Quaternion / Color) — map + verify**

  **What to do**:
  - Create `docs/api-mapping/math.md` mapping `System.Numerics.Vector2/3/4`, `System.Numerics.Quaternion`, `System.Numerics.Matrix4x4`, `System.Drawing.Color` to our `sf_vec2/3/4_t`, `sf_quat_t`, `sf_mat4_t`, `sf_color_t`. Note layout equivalence (X/Y/Z/W order; row-major Matrix4x4; ARGB color).
  - Verify our `sf_math.h` POD struct field order matches upstream byte order. Add `_Static_assert(sizeof(sf_vec3_t) == 12, ...)` and similar for every type.

  **Must NOT do**:
  - Do NOT add operator-overload-equivalent helpers (no `sf_vec3_add`); we are not a math library.
  - Do NOT change struct field order (would break alignment with binary_reader/writer).

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 2
  - Blocks: 21-37
  - Blocked By: 1, 4, 5

  **References**:
  - `include/souls_formats/sf_math.h`
  - `src/core/binary_reader.c` — existing vec/quat/color reads (confirm field order)
  - https://learn.microsoft.com/dotnet/api/system.numerics.vector3 — System.Numerics docs

  **Acceptance Criteria**:
  - [ ] Mapping doc with at least 6 rows (vec2/3/4, quat, mat4, color).
  - [ ] `_Static_assert` lines added in `sf_math.h` for each type's size.
  - [ ] Build still green.

  **QA Scenarios**:
  ```
  Scenario: Static asserts hold
    Tool: Bash
    Steps:
      1. `cmake --build build-mingw 2>&1 | grep -i 'static_assert' | head -10`
    Expected Result: No assertion failures; build succeeds.
    Evidence: .sisyphus/evidence/task-14-asserts.txt

  Scenario: Mapping doc complete
    Tool: Bash
    Steps:
      1. `grep -c '^|' docs/api-mapping/math.md`  # >= 8 (header + 6+)
    Expected Result: count >= 8.
    Evidence: .sisyphus/evidence/task-14-math-doc.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map math types; add layout static asserts`
  - Files: `include/souls_formats/sf_math.h`, `docs/api-mapping/math.md`, `docs/api-mapping/drift-checklist.md`
  - Pre-commit: `cmake --build build-mingw`

- [x] 15. **DCX — map + tagged union refactor + presets + overloads + tests**

  **What to do**:
  - **Read in full** `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/DCX.cs` (1145 lines). Extract every public class/struct/enum/method.
  - **Map**: create `docs/api-mapping/format-dcx.md`. Six-column table covering:
    - Public static methods: `Is(Stream/byte[]/string)` (3), `Decompress(...)` (6 overloads), `Compress(...)` (2 overloads).
    - Public enums: `DCX.Type` (9 values), `DefaultType` (8 values), `DfltCompressionPreset` (5 values), `KrakCompressionPreset` (2 values).
    - Public interface: `CompressionInfo`.
    - Public structs: `UnkCompressionInfo / NoCompressionInfo / DcpDfltCompressionInfo / DcpEdgeCompressionInfo / ZlibCompressionInfo / DcxEdgeCompressionInfo / DcxDfltCompressionInfo / DcxKrakCompressionInfo / DcxZstdCompressionInfo` with all properties and ctors.
  - **Refactor** in `include/souls_formats/sf_dcx.h`:
    - Replace flat `sf_dcx_params_t` with **tagged union** `sf_dcx_compression_info_t`:
      ```c
      typedef enum sf_dcx_type { SF_DCX_TYPE_UNKNOWN, SF_DCX_TYPE_NONE,
        SF_DCX_TYPE_ZLIB, SF_DCX_TYPE_DCP_EDGE, SF_DCX_TYPE_DCP_DFLT,
        SF_DCX_TYPE_DCX_EDGE, SF_DCX_TYPE_DCX_DFLT, SF_DCX_TYPE_DCX_KRAK,
        SF_DCX_TYPE_DCX_ZSTD, SF_DCX_TYPE_COUNT_ } sf_dcx_type_t;
      typedef struct { /* empty */ } sf_dcx_unk_info_t;
      typedef struct { /* empty */ } sf_dcx_none_info_t;
      typedef struct { /* empty */ } sf_dcx_zlib_info_t;
      typedef struct { /* empty */ } sf_dcx_dcp_edge_info_t;
      typedef struct { /* empty */ } sf_dcx_dcp_dflt_info_t;
      typedef struct { /* empty */ } sf_dcx_dcx_edge_info_t;
      typedef struct { int32_t unk04, unk10, unk14; uint8_t unk30, unk38; } sf_dcx_dcx_dflt_info_t;
      typedef struct { uint8_t compression_level; sf_oodle_lz_compressor_t oodle_compressor_type; } sf_dcx_dcx_krak_info_t;
      typedef struct { uint8_t compression_level; } sf_dcx_dcx_zstd_info_t;
      typedef struct sf_dcx_compression_info {
          sf_dcx_type_t type;
          union {
              sf_dcx_unk_info_t      unk;
              sf_dcx_none_info_t     none;
              sf_dcx_zlib_info_t     zlib;
              sf_dcx_dcp_edge_info_t dcp_edge;
              sf_dcx_dcp_dflt_info_t dcp_dflt;
              sf_dcx_dcx_edge_info_t dcx_edge;
              sf_dcx_dcx_dflt_info_t dcx_dflt;
              sf_dcx_dcx_krak_info_t dcx_krak;
              sf_dcx_dcx_zstd_info_t dcx_zstd;
          } u;
      } sf_dcx_compression_info_t;
      ```
    - Add preset enums: `sf_dcx_default_type_t` (8 values), `sf_dcx_dflt_compression_preset_t` (5 values), `sf_dcx_krak_compression_preset_t` (2 values).
    - Add factory helpers: `sf_dcx_compression_info_from_default_type(sf_dcx_default_type_t, sf_dcx_compression_info_t *out)`, `sf_dcx_compression_info_from_dflt_preset(...)`, `sf_dcx_compression_info_from_krak_preset(...)`.
    - Add overloads: `sf_dcx_decompress_from_path(const char *utf8_path, ...)`, `sf_dcx_decompress_from_stream(sf_istream_t*, ...)`, `sf_dcx_compress_to_path(...)`, `sf_dcx_compress_to_stream(...)`.
    - Add `sf_dcx_is(...)` family (`_from_buffer / _from_stream / _from_path`) returning `bool` via out-param.
    - Keep `sf_dcx_unwrap` (extension); annotate header doc-comment as `/* extension: see docs/api-mapping/extensions.md */`.
    - DELETE `sf_dcx_params_t` and `sf_dcx_read_params` (replaced by sf_dcx_compression_info_t + sf_dcx_decompress out-info).
  - **Refactor source**: rewrite `src/compression/dcx.c` paths that used `sf_dcx_params_t`. Update `tests/compression/test_dcx_*.c` to use new tagged union.
  - **Test**: every variant of CompressionInfo gets a sub-case (round-trip a small buffer with each type that doesn't require Oodle; KRAK case uses oodle skip-if-missing).
  - Update CHANGELOG (breaking change), drift-checklist (Phase 2 / DCX items).

  **Must NOT do**:
  - Do NOT skip any of the 9 CompressionInfo variants — adding a 10th later is an ABI break.
  - Do NOT change wire format; golden hashes must still match.
  - Do NOT drop `sf_dcx_unwrap`; it's an explicit extension.
  - Do NOT introduce per-variant `_create()` / `_destroy()` — `sf_dcx_compression_info_t` is value type, no allocation.

  **Recommended Agent Profile**:
  - **Category**: `ultrabrain` — Largest refactor in this plan; needs careful read of 1145-line upstream + cross-cutting changes.
  - **Skills**: `[]`

  **Parallelization** (post-Momus cycle break):
  - Can Run In Parallel: NO (within Wave 3 — too cross-cutting; T13/T16/T17/T19/T20 follow)
  - Parallel Group: Wave 3 — head of wave.
  - Blocks: 13, 16, 17, 19, 20, 21-37 (every downstream uses the new info type)
  - Blocked By: 8, 9 (binary reader/writer multi-option Assert + Reserve/Fill needed for DCX read/write paths), **18** (DcxKrakCompressionInfo struct field `oodle_compressor_type` requires `sf_oodle_lz_compressor_t` from T18)
  - **Rationale**: T18 must complete BEFORE T15 because the tagged union's `sf_dcx_dcx_krak_info_t` variant references the publicized `sf_oodle_lz_compressor_t` enum. T18 is therefore in Wave 2.

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/DCX.cs:1-1145` — read in full
  - `include/souls_formats/sf_dcx.h` (current)
  - `src/compression/dcx.c` (current)
  - `tests/compression/test_dcx_*.c`
  - `docs/api-mapping/POLICY.md` for naming conventions
  - `tests/golden/golden_hashes.h` — must continue to match

  **Acceptance Criteria**:
  - [ ] Mapping doc has rows for every CompressionInfo variant + every public method/property.
  - [ ] `sf_dcx_compression_info_t` defined as tagged union with 9 variants.
  - [ ] All 3 preset enums + 3 factory helpers exposed.
  - [ ] `sf_dcx_is_*`, `sf_dcx_decompress_from_*`, `sf_dcx_compress_to_*` overloads exposed.
  - [ ] Build green; existing DCX tests still pass; new variant tests pass.
  - [ ] Golden hashes match.
  - [ ] CHANGELOG breaking-change entry; drift-checklist DCX items ticked.

  **QA Scenarios**:
  ```
  Scenario: Round-trip every CompressionInfo variant
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_dcx -V`
      2. Inspect: NoCompression, Zlib, DcpDflt, DcpEdge, DcxEdge, DcxDflt, DcxZstd round-trip a 4 KB synthetic buffer; KRAK runs if oodle present, else SKIP.
    Expected Result: All non-Oodle variants PASS; KRAK PASS or SKIP.
    Failure Indicators: any variant fails or is missing.
    Evidence: .sisyphus/evidence/task-15-dcx-variants.txt

  Scenario: Tagged-union discriminant correct
    Tool: Bash + Unity
    Steps:
      1. Sub-case decompresses each fixture and reads `info.type`.
      2. Asserts type matches fixture-built type.
    Expected Result: Discriminant matches.
    Evidence: .sisyphus/evidence/task-15-discriminant.txt

  Scenario: Golden hashes preserved
    Tool: Bash
    Steps:
      1. `ctest --test-dir build-mingw -L golden --output-on-failure`
    Expected Result: All hashes still match baseline.
    Evidence: .sisyphus/evidence/task-15-golden.txt
  ```

  **Commit**: YES
  - Message: `feat(dcx)!: refactor to tagged-union CompressionInfo + presets + overloads`
  - Files: `include/souls_formats/sf_dcx.h`, `src/compression/dcx.c`, `tests/compression/test_dcx_*.c`, `docs/api-mapping/format-dcx.md`, `docs/api-mapping/drift-checklist.md`, `docs/api-mapping/extensions.md` (sf_dcx_unwrap row), `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L 'compression|golden'`

- [x] 16. **ZlibHelper — map + verify**

  **What to do**:
  - Create `docs/api-mapping/util-compression-zlib-helper.md` covering upstream `ZlibHelper.cs` public methods (likely `ReadZlib(BinaryReaderEx, int compressedSize, ...)` and `WriteZlib(byte[], ...)`). Read the file in full.
  - Verify our `src/compression/deflate_zlibng.c` covers functional parity at the byte level (raw deflate + zlib wrapper, both directions). Mark as `+ extension` (kept internal — no public sf_zlib_* needed; it's hidden behind sf_dcx_*).
  - Mark drift-checklist Zlib items as ticked or N/A.

  **Must NOT do**:
  - Do NOT publicize zlib helpers; they're DCX implementation detail.
  - Do NOT introduce a separate `sf_zlib.h`.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (Wave 3)
  - Parallel Group: Wave 3
  - Blocks: 21-37
  - Blocked By: 15

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/ZlibHelper.cs`
  - `src/compression/deflate_zlibng.c`

  **Acceptance Criteria**:
  - [ ] Mapping doc exists with all upstream rows + Status column noting "internal-only by design (used by sf_dcx_*)".
  - [ ] Drift-checklist updated.

  **QA Scenarios**:
  ```
  Scenario: Mapping doc completeness
    Tool: Bash
    Steps:
      1. `grep -c 'ZlibHelper.cs:' docs/api-mapping/util-compression-zlib-helper.md`
    Expected Result: count >= number of public methods in upstream file (verify by `grep -c 'public ' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/ZlibHelper.cs`).
    Evidence: .sisyphus/evidence/task-16-zlib-rows.txt

  Scenario: No new public API
    Tool: Bash
    Steps:
      1. `git diff include/souls_formats | grep -c '^+'`
    Expected Result: 0 lines added to public headers.
    Evidence: .sisyphus/evidence/task-16-no-public.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map ZlibHelper as internal DCX implementation detail`
  - Files: `docs/api-mapping/util-compression-zlib-helper.md`, `docs/api-mapping/drift-checklist.md`
  - Pre-commit: none (doc only)

- [x] 17. **ZstdHelper — map + verify**

  **What to do**:
  - Create `docs/api-mapping/util-compression-zstd-helper.md` covering `ZstdHelper.cs` public methods (`ReadZstd(BinaryReaderEx, int compressedSize)` and `WriteZstd(byte[], int level)`). Read the file in full.
  - Verify our `src/compression/zstd_wrap.c` covers parity. Mark internal-only.
  - Mark drift-checklist Zstd items as ticked or N/A.

  **Must NOT do**:
  - Do NOT publicize zstd helpers; DCX implementation detail.

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 3
  - Blocks: 21-37
  - Blocked By: 15

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/ZstdHelper.cs`
  - `src/compression/zstd_wrap.c`

  **Acceptance Criteria**:
  - [ ] Mapping doc with all upstream rows.
  - [ ] No new public API.
  - [ ] Drift-checklist updated.

  **QA Scenarios**:
  ```
  Scenario: Mapping doc rows
    Tool: Bash
    Steps:
      1. `grep -c 'ZstdHelper.cs:' docs/api-mapping/util-compression-zstd-helper.md`
    Expected Result: count >= number of public methods.
    Evidence: .sisyphus/evidence/task-17-zstd-rows.txt

  Scenario: zstd round-trip still works
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_dcx_zstd -V`
    Expected Result: PASS.
    Evidence: .sisyphus/evidence/task-17-zstd-test.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map ZstdHelper as internal DCX implementation detail`
  - Files: `docs/api-mapping/util-compression-zstd-helper.md`, `docs/api-mapping/drift-checklist.md`
  - Pre-commit: `ctest --test-dir build-mingw -R test_dcx_zstd`

- [x] 18. **Oodle (and Oodle26/28/29 + IOodleCompressor + NoOodleException) — map + publicize enums**

  **What to do**:
  - Create `docs/api-mapping/util-compression-oodle.md` consolidating ALL of `Oodle.cs / Oodle26.cs / Oodle28.cs / Oodle29.cs / IOodleCompressor.cs / NoOodleException.cs`. Six-column table covering:
    - `enum OodleVersion` (3 values)
    - `enum OodleLZ_CompressionLevel`, `OodleLZ_Compressor`, `OodleLZ_CompressOptions`, `OodleLZ_CheckCRC`, `OodleLZ_Decode_ThreadPhase`, `OodleLZ_FuzzSafe`, `OodleLZ_Profile`, `OodleLZ_Verbosity`. **Read each enum upstream and faithfully reproduce all values + numeric assignments.** Do NOT guess.
    - Static dictionaries / pointers (`OodlePtrs`, `Oodle9Ptr`, `Oodle8Ptr`, `Oodle6Ptr`)
    - `static IOodleCompressor GetOodleCompressor()`, `GetOodleCompressor2()`, `IntPtr GetOodlePtr()`
    - Interface `IOodleCompressor` (Compress / Decompress)
    - Per-version P/Invoke externs (Oodle26/28/29) — note as `_skipped_` (private interop)
    - `class NoOodleFoundException` → `SF_ERR_OODLE_NOT_FOUND`
  - **Refactor** `include/souls_formats/sf_oodle.h`:
    - Add `typedef enum sf_oodle_version { SF_OODLE_VERSION_UNKNOWN, SF_OODLE_VERSION_6, SF_OODLE_VERSION_8, SF_OODLE_VERSION_9 } sf_oodle_version_t;` (replace integer return of current `sf_oodle_version()`).
    - Add `sf_oodle_lz_compressor_t`, `sf_oodle_lz_compression_level_t`, `sf_oodle_lz_check_crc_t`, `sf_oodle_lz_decode_thread_phase_t`, `sf_oodle_lz_fuzz_safe_t`, `sf_oodle_lz_profile_t`, `sf_oodle_lz_verbosity_t` typedef'd enums with all upstream values + numeric assignments.
    - Add `typedef struct sf_oodle_lz_compress_options { ... }` mirroring upstream struct field-for-field.
    - Keep `sf_oodle_set_search_path / sf_oodle_load / sf_oodle_unload`. Mark these in `extensions.md` (no upstream analogue).
    - Update `sf_oodle_version()` return type from `int` → `sf_oodle_version_t`.
  - **Refactor source**: `src/compression/oodle/oodle_loader.c` — return enum; rename internal helpers per POLICY.
  - **Test** in `tests/compression/test_dcx_krak.c`: confirm `sf_oodle_version()` returns the new enum; add a sub-case verifying `OodleVersion → DLL probe order` per upstream (Nightreign=9, AC6=8, ER/Sekiro=6).

  **Must NOT do**:
  - Do NOT inline Oodle SDK headers; only public-facing enums (FromSoft community-known values).
  - Do NOT publicize the per-version P/Invoke externs.
  - Do NOT change the search-path order.

  **Recommended Agent Profile**:
  - **Category**: `deep`
  - **Skills**: `[]`

  **Parallelization** (post-Momus cycle break):
  - Can Run In Parallel: YES (with 8, 9, 10, 11, 12, 14 in Wave 2)
  - Parallel Group: **Wave 2** (moved here from Wave 3 to break T15 ↔ T18 cycle)
  - Blocks: **15** (DCX `sf_dcx_dcx_krak_info_t` references `sf_oodle_lz_compressor_t`), 21-37
  - Blocked By: 1, 2, 4, 5
  - **Rationale**: T18 only declares enum types and reorganizes existing oodle_loader code; it has no dependency on DCX. Moving it earlier removes the apparent cycle and gives T15 a stable enum to reference on its first commit.

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/Oodle/Oodle.cs:1-311`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/Oodle/Oodle26.cs / Oodle28.cs / Oodle29.cs / IOodleCompressor.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Exceptions/NoOodleException.cs`
  - `include/souls_formats/sf_oodle.h`
  - `src/compression/oodle/{oodle_loader.c,h, oodle_v6.c, oodle_v8.c, oodle_v9.c}`

  **Acceptance Criteria**:
  - [ ] Mapping doc rows for every public enum + every public method/property.
  - [ ] All 8 OodleLZ_* enums + sf_oodle_version_t exposed.
  - [ ] sf_oodle_lz_compress_options_t struct defined with fields matching upstream.
  - [ ] Build green; KRAK round-trip test still passes (or SKIPs cleanly when DLL absent).
  - [ ] CHANGELOG breaking-change entry (return type of sf_oodle_version changed).
  - [ ] Drift-checklist Oodle items ticked.

  **QA Scenarios**:
  ```
  Scenario: All OodleLZ_* enums exposed
    Tool: Bash
    Steps:
      1. `grep -c '^typedef enum sf_oodle_lz_' include/souls_formats/sf_oodle.h`
    Expected Result: count >= 7 (Compressor, CompressionLevel, CheckCRC, ThreadPhase, FuzzSafe, Profile, Verbosity).
    Evidence: .sisyphus/evidence/task-18-enums.txt

  Scenario: KRAK still works (or skips)
    Tool: Bash + Unity
    Preconditions: ~/dev/oodle/ may or may not contain oo2core_*_win64.dll.
    Steps:
      1. `ctest --test-dir build-mingw -R test_dcx_krak -V`
    Expected Result: PASS or SKIP with "oodle dll missing" message.
    Failure Indicators: FAIL or unexpected error.
    Evidence: .sisyphus/evidence/task-18-krak.txt

  Scenario: Enum values match upstream numeric assignments
    Tool: Bash
    Steps:
      1. `grep -A 20 'enum OodleLZ_Compressor' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Compression/Oodle/Oodle.cs`
      2. `grep -A 20 'sf_oodle_lz_compressor_t' include/souls_formats/sf_oodle.h`
      3. Diff numerical values.
    Expected Result: Numerical values identical.
    Failure Indicators: any divergence.
    Evidence: .sisyphus/evidence/task-18-enum-values.txt
  ```

  **Commit**: YES
  - Message: `feat(oodle)!: publicize OodleLZ_* enums and OodleVersion`
  - Files: `include/souls_formats/sf_oodle.h`, `src/compression/oodle/*.{c,h}`, `tests/compression/test_dcx_krak.c`, `docs/api-mapping/util-compression-oodle.md`, `docs/api-mapping/extensions.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L compression`

- [x] 19. **RegulationDecryptor — map + sf_regulation.h + tests**

  **What to do**:
  - Read `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs` in full.
  - Create `docs/api-mapping/util-cryptography-regulation-decryptor.md` covering `enum RegulationKey` (4 values), 4 game-specific decrypt + 4 encrypt + generic `DecryptBndWithKey` / `EncryptRegulationWithKey`. Faithfully reproduce the upstream `EncryptERNRRegulation` quirk (calls `RegulationKey.EldenRing` instead of `Nightreign`) — document explicitly in mapping notes; the C-side fix matches upstream behavior bit-identically (no silent fix; document the Nightreign caveat).
  - Create `include/souls_formats/sf_regulation.h`:
    - `typedef enum sf_regulation_key { SF_REGULATION_KEY_DARK_SOULS_3 = 0, SF_REGULATION_KEY_ELDEN_RING = 1, SF_REGULATION_KEY_ARMORED_CORE_6 = 2, SF_REGULATION_KEY_ELDEN_RING_NIGHTREIGN = 3 } sf_regulation_key_t;`
    - Byte-buffer-level (NOT BND4 yet): `sf_regulation_decrypt(const uint8_t *in, size_t in_size, sf_regulation_key_t key, uint8_t **out, size_t *out_size, const sf_allocator_t *)` and `sf_regulation_encrypt(const uint8_t *in, size_t in_size, sf_regulation_key_t key, uint8_t **out, size_t *out_size, const sf_allocator_t *)`.
    - Game-specific convenience wrappers: `sf_regulation_decrypt_ds3 / _er / _ac6 / _ernr` (each forwards to the generic with the matching key). 4 encrypt counterparts.
    - Document in header doc-comment: "BND4-aware overloads (sf_regulation_*_bnd4) will land in Phase 3."
  - Move `src/crypto/regulation.c` and `regulation_keys.c` content to satisfy the new public API; keep internal helper symbols hidden.
  - Update `include/souls_formats/souls_formats.h` umbrella.
  - **Test** `tests/crypto/test_regulation_decrypt.c`: convert existing test to use new public API; add a synthetic round-trip case (encrypt + decrypt of a small known plaintext) for each key.
  - Tick drift-checklist Regulation items.

  **Must NOT do**:
  - Do NOT silently fix the Nightreign-key quirk in `EncryptERNR`; faithfully port the upstream behavior (mark in mapping notes; if user later wants a fix, that's a separate task).
  - Do NOT introduce BND4-aware overloads now (Phase 3 territory).
  - Do NOT expose the raw key bytes.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (with 16, 17, 18, 20)
  - Parallel Group: Wave 3
  - Blocks: 21-37
  - Blocked By: 9 (Reserve/Fill needed if encrypt path uses writer; otherwise just 1, 4, 5)

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/RegulationDecryptor.cs:1-195`
  - `src/crypto/regulation.c`, `src/crypto/regulation_keys.c` (current internal)
  - `src/crypto/aes_cng.c`

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_regulation.h` exists, exposed via umbrella.
  - [ ] All 9 functions exported (1 enum + 2 generic + 8 game-specific = 10 symbols).
  - [ ] Synthetic round-trip test PASS for each key.
  - [ ] If `regulation.bin` available at `/mnt/c/Games/ELDEN RING/Game/`, e2e decrypt → BND4 magic check passes (existing test).
  - [ ] Mapping doc rows present + Nightreign quirk noted.
  - [ ] Drift-checklist + CHANGELOG updated.

  **QA Scenarios**:
  ```
  Scenario: Synthetic round-trip per key
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_regulation_decrypt -V`
      2. Inspect 4 round-trip sub-cases (one per key) on a 1 KB plaintext.
    Expected Result: All 4 round-trips PASS byte-identical.
    Evidence: .sisyphus/evidence/task-19-roundtrip.txt

  Scenario: ER e2e regulation.bin decrypt unchanged
    Tool: Bash + Unity
    Preconditions: /mnt/c/Games/ELDEN RING/Game/regulation.bin present.
    Steps:
      1. `ctest --test-dir build-mingw -R test_regulation_decrypt -V`
      2. Inspect e2e sub-case (decrypt ER regulation.bin → check `BND4` magic).
    Expected Result: PASS or SKIP with "regulation.bin not found".
    Evidence: .sisyphus/evidence/task-19-er-e2e.txt

  Scenario: Public API symbol presence
    Tool: Bash
    Steps:
      1. `x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep sf_regulation_`
    Expected Result: >= 10 sf_regulation_ symbols.
    Evidence: .sisyphus/evidence/task-19-symbols.txt
  ```

  **Commit**: YES
  - Message: `feat(regulation)!: publicize sf_regulation byte-level API`
  - Files: `include/souls_formats/sf_regulation.h`, `include/souls_formats/souls_formats.h`, `src/crypto/regulation.c`, `src/crypto/regulation.h`, `tests/crypto/test_regulation_decrypt.c`, `docs/api-mapping/util-cryptography-regulation-decryptor.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L crypto`

- [x] 20. **SL2Decryptor — map + sf_sl2.h + tests**

  **What to do**:
  - Read `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/SL2Decryptor.cs` (104 lines).
  - Create `docs/api-mapping/util-cryptography-sl2-decryptor.md` covering 3 key getters (DS2 / Scholar / DS3) + 2 file ops + 3 known 16-byte hex keys.
  - Create `include/souls_formats/sf_sl2.h`:
    - `sf_result_t sf_sl2_get_ds2_key(const uint8_t **out_key_16);` (returns pointer to static 16-byte key).
    - `sf_result_t sf_sl2_get_scholar_key(const uint8_t **out_key_16);`
    - `sf_result_t sf_sl2_get_ds3_key(const uint8_t **out_key_16);`
    - `sf_result_t sf_sl2_decrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16, uint8_t **out, size_t *out_size, const sf_allocator_t *);`
    - `sf_result_t sf_sl2_encrypt(const uint8_t *in, size_t in_size, const uint8_t *key_16, uint8_t **out, size_t *out_size, const sf_allocator_t *);`
  - Move/expose `src/crypto/sl2.c` content to satisfy the new public API.
  - Update umbrella header.
  - **Test** new `tests/crypto/test_sl2_kat.c`: synthetic round-trip per key, plus byte-level KAT against known SL2 fixtures (constructed inline; do NOT include real save files).

  **Must NOT do**:
  - Do NOT include real DS2/DS3 save files.
  - Do NOT change MD5 leading-hash format.

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 3
  - Blocks: 21-37
  - Blocked By: 1, 4, 5

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Utilities/Cryptography/SL2Decryptor.cs:1-104`
  - `src/crypto/sl2.c` (current internal)
  - `src/crypto/aes_cng.c`, `src/crypto/md5_cng.c`

  **Acceptance Criteria**:
  - [ ] `include/souls_formats/sf_sl2.h` exists.
  - [ ] 5 functions exported.
  - [ ] Synthetic round-trip per key passes.
  - [ ] Mapping doc + drift-checklist + CHANGELOG updated.

  **QA Scenarios**:
  ```
  Scenario: Round-trip per key
    Tool: Bash + Unity
    Steps:
      1. `ctest --test-dir build-mingw -R test_sl2 -V`
    Expected Result: 3 sub-cases PASS (one per key).
    Evidence: .sisyphus/evidence/task-20-sl2-roundtrip.txt

  Scenario: Key bytes match upstream constants
    Tool: Bash
    Steps:
      1. Extract DS2 key bytes from upstream `SL2Decryptor.cs` (B7 FD 46 3E ...).
      2. Extract our `sf_sl2_get_ds2_key`-returned bytes via test stub.
      3. Diff.
    Expected Result: Identical 16 bytes.
    Evidence: .sisyphus/evidence/task-20-key-bytes.txt
  ```

  **Commit**: YES
  - Message: `feat(sl2): publicize sf_sl2 byte-level API + key getters`
  - Files: `include/souls_formats/sf_sl2.h`, `include/souls_formats/souls_formats.h`, `src/crypto/sl2.c`, `src/crypto/sl2.h`, `tests/crypto/test_sl2_kat.c`, `tests/CMakeLists.txt`, `docs/api-mapping/util-cryptography-sl2-decryptor.md`, `docs/api-mapping/drift-checklist.md`, `CHANGELOG.md`
  - Pre-commit: `cmake --build build-mingw && ctest --test-dir build-mingw -L crypto`

> **WAVE 4 NOTE — Tier A mapping for Phase 3-7 future formats**:
> Tasks 21-28 produce mapping docs ONLY. Status column for our API is `未实现` for every row.
> NO source code changes. NO public-header additions. Each task per the same recipe:
> (1) Read upstream `.cs` files in full (consolidate partial-class groups). (2) Write
> `docs/api-mapping/format-<name>.md` with the 6-column schema (Upstream signature |
> Upstream loc (file:line) | Kind | Our API (`未实现`) | Status | Notes). (3) For
> partial-class groups, the doc header lists ALL contributing `.cs` files. (4) Include
> a "Phase target" note (which Phase this format lands in) and a "Dependencies" note
> (which other formats it relies on, e.g., FLVER2 depends on Binder).

- [x] 21. **Binder family mapping (Binder common + BND3 + BND4 + BXF3 + BXF4 + BHD5)**

  **What to do**:
  - Produce 6 docs: `format-binder-common.md`, `format-bnd3.md`, `format-bnd4.md`, `format-bxf3.md`, `format-bxf4.md`, `format-bhd5.md`.
  - Read upstream files (paths verified against pinned commit):
    - **Binder common** (consolidate 6 files): `Formats/Binder/Binder.cs`, `Formats/Binder/BinderFile.cs`, `Formats/Binder/BinderFileHeader.cs`, `Formats/Binder/BinderHashTable.cs`, `Formats/Binder/BinderReader.cs`, `Formats/Binder/IBinder.cs`.
    - **BND3** (consolidate 2 files): `Formats/Binder/BND3/BND3.cs`, `Formats/Binder/BND3/BND3Reader.cs`.
    - **BND4** (consolidate 2 files): `Formats/Binder/BND4/BND4.cs`, `Formats/Binder/BND4/BND4Reader.cs`.
    - **BXF3** (consolidate 2 files): `Formats/Binder/BXF3/BXF3.cs`, `Formats/Binder/BXF3/BXF3Reader.cs`.
    - **BXF4** (consolidate 2 files): `Formats/Binder/BXF4/BXF4.cs`, `Formats/Binder/BXF4/BXF4Reader.cs`.
    - **BHD5**: `Formats/BHD5.cs` (root level — NOT under Binder/).
  - Each doc: header table listing contributing `.cs` files; six-column rows for every public method/property/enum/nested type. All Status column entries are `未实现`. Add "Phase target: 3" + "Dependencies: DCX, AES (BHD5 only), HashHelper".
  - Update `docs/api-mapping/README.md` index to link these 6 docs.

  **Must NOT do**:
  - Do NOT add any C source files or public headers; mapping only.
  - Do NOT design our future C API beyond the planned naming convention; just mark our column `未实现`.

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (with 22-28)
  - Parallel Group: Wave 4
  - Blocks: 30-37 (roadmap rewrite cites these mapping docs)
  - Blocked By: 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/{Binder,BinderFile,BinderFileHeader,BinderHashTable,BinderReader,IBinder}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/BND3/{BND3,BND3Reader}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/BND4/{BND4,BND4Reader}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/BXF3/{BXF3,BXF3Reader}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/Binder/BXF4/{BXF4,BXF4Reader}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/BHD5.cs` (root, **not** under Binder/)
  - To find any further partial-class siblings: `grep -lR "partial class \(BND3\|BND4\|BXF3\|BXF4\|BHD5\|Binder\)" /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/` at the pinned commit.

  **Acceptance Criteria**:
  - [ ] 6 docs exist; each has header file table + ≥ 10 rows.
  - [ ] Every row has line ref `<File>.cs:<line>`.
  - [ ] README index updated.

  **QA Scenarios**:
  ```
  Scenario: All 6 docs exist with required structure
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{binder-common,bnd3,bnd4,bxf3,bxf4,bhd5}.md`
      2. For each: `grep -c '^|' file >= 12` (header + 10 rows minimum) and `grep -c '\.cs:' file >= 10`
    Expected Result: All present; minimums met.
    Evidence: .sisyphus/evidence/task-21-binder-docs.txt

  Scenario: Index links work
    Tool: Bash
    Steps:
      1. `grep -E 'format-(binder-common|bnd3|bnd4|bxf3|bxf4|bhd5)\.md' docs/api-mapping/README.md | wc -l`
    Expected Result: 6.
    Evidence: .sisyphus/evidence/task-21-index.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map Binder family (Binder common, BND3/4, BXF3/4, BHD5) [Phase 3]`
  - Files: 6 new mapping docs + `docs/api-mapping/README.md`
  - Pre-commit: none (doc only)

- [x] 22. **TPF + ENFL mapping**

  **What to do**:
  - Produce 2 docs: `format-tpf.md`, `format-enfl.md`.
  - Read upstream `Formats/TPF/` (consolidate partial-class) and `Formats/ENFL.cs`.
  - Six-column schema; all Status `未实现`; Phase target 3.
  - Update README index.

  **Must NOT do**:
  - Do NOT design DDS-pixel decoders; TPF transports DDS opaquely (per PLAN.md §2.1).

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 4
  - Blocks: 30-37
  - Blocked By: 8-20

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TPF/`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/ENFL.cs`

  **Acceptance Criteria**:
  - [ ] 2 docs exist with file-table headers + ≥ 5 rows each.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Docs present
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{tpf,enfl}.md`
      2. `grep -c '^|' docs/api-mapping/format-tpf.md >= 7`
      3. `grep -c '^|' docs/api-mapping/format-enfl.md >= 7`
    Expected Result: All true.
    Evidence: .sisyphus/evidence/task-22-tpf-enfl.txt

  Scenario: Loc refs resolve
    Tool: Bash
    Steps:
      1. Sample 3 line refs per doc; `sed -n '<n>p' upstream`
    Expected Result: Each cites a real upstream symbol.
    Evidence: .sisyphus/evidence/task-22-loc.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map TPF and ENFL [Phase 3]`
  - Files: `format-tpf.md`, `format-enfl.md`, `README.md`
  - Pre-commit: none (doc only)

- [x] 23. **PARAM family mapping (PARAM + PARAMDEF + PARAMTDF + FMG)**

  **What to do**:
  - Produce 4 docs: `format-param.md`, `format-paramdef.md`, `format-paramtdf.md`, `format-fmg.md`.
  - Read upstream files (paths verified at pinned commit):
    - **PARAM** (consolidate partial-class siblings): primary file `Formats/PARAM/PARAM/PARAM.cs`. Discover full sibling list at the pinned commit via `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM -name '*.cs'` and `grep -lR 'partial class PARAM' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/`.
    - **PARAMDEF** (consolidate partial-class siblings): subfolder `Formats/PARAM/PARAMDEF/` with primary file `PARAMDEF.cs`; full sibling list via `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF -name '*.cs'`.
    - **PARAMTDF**: `Formats/PARAM/PARAMTDF.cs` (sibling files: `Formats/PARAM/{ParamUtil,ParamDbpUtil}.cs` are utility helpers — review and decide per row whether they belong in this mapping doc or as separate utility rows).
    - **FMG**: `Formats/FMG.cs` (root level).
  - Header table on each doc lists ALL contributing files (discovered, not assumed). Six-column schema; Status `未实现`; Phase target 4.
  - For PARAMDEF: document the XML-deserialization API surface (`ApplyParamdefCarefully`, etc.) explicitly in mapping rows.
  - Note: `Formats/PARAM/Deprecated/` and `Formats/PARAM/PARAMDBP/` exist at the pinned commit — these are out of scope for this task (legacy/deprecated; they go in `legacy.md` Tier B per Task 29).
  - Update README index.

  **Must NOT do**:
  - Do NOT split PARAMDEF into multiple docs; consolidate.
  - Do NOT design PARAMDEF XML schema yet.

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES
  - Parallel Group: Wave 4
  - Blocks: 30-37
  - Blocked By: 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAM/PARAM.cs` plus any partial-class siblings discovered with `find` (NOT root-level `Formats/PARAM.cs` — that does not exist).
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMDEF/PARAMDEF.cs` plus any partial-class siblings (NOT `Formats/PARAMDEF/`).
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/PARAMTDF.cs` (NOT root-level `Formats/PARAMTDF.cs` — that does not exist).
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/PARAM/{ParamUtil,ParamDbpUtil}.cs` — utility helpers, evaluate inclusion per row.
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FMG.cs` (root level — this one is correct).
  - **Out of scope here**: `Formats/PARAM/Deprecated/`, `Formats/PARAM/PARAMDBP/` — go to `legacy.md` (Task 29).

  **Acceptance Criteria**:
  - [ ] 4 docs exist; each has file-table header + ≥ 10 rows.
  - [ ] PARAM and PARAMDEF docs each list multiple `.cs` files in header.
  - [ ] README index updated.

  **QA Scenarios**:
  ```
  Scenario: Partial-class consolidation visible in headers
    Tool: Bash
    Steps:
      1. `awk '/^# / && /PARAM/' docs/api-mapping/format-param.md` (or similar header check)
      2. Header file table lists multiple `.cs` files (>=2).
    Expected Result: yes.
    Evidence: .sisyphus/evidence/task-23-partial-class.txt

  Scenario: All 4 docs structurally complete
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{param,paramdef,paramtdf,fmg}.md`
      2. `for d in param paramdef paramtdf fmg; do echo "$d: $(grep -c '^|' docs/api-mapping/format-$d.md)"; done`
    Expected Result: each count >= 12.
    Evidence: .sisyphus/evidence/task-23-rows.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map PARAM family (PARAM, PARAMDEF, PARAMTDF, FMG) [Phase 4]`
  - Files: 4 new mapping docs + `README.md`
  - Pre-commit: none (doc only)

- [x] 24. **EMEVD + ESD mapping**

  **What to do**:
  - Produce 2 docs: `format-emevd.md`, `format-esd.md`.
  - Read upstream files (paths verified at pinned commit):
    - **EMEVD** (consolidate ≥ 5 files): `Formats/EMEVD/{EMEVD,Event,Instruction,Layer,Parameter}.cs`. Discover any further partial-class siblings via `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD -name '*.cs'`.
    - **ESD**: `Formats/ESD.cs` (root). Discover siblings via `grep -lR 'partial class ESD' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/`.
  - Six-column schema; Status `未实现`; Phase target 5.
  - Document any game-specific dispatch (DS3/Sekiro/ER/AC6 EMEVD format variants) in Notes column.
  - Update README index.

  **Must NOT do**:
  - Do NOT design opcode tables yet.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 4; Blocks 30-37; Blocked By 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/EMEVD/{EMEVD,Event,Instruction,Layer,Parameter}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/ESD.cs` (root)

  **Acceptance Criteria**:
  - [ ] 2 docs exist; each ≥ 10 rows; partial-class header table where applicable.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Both docs present + structured
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{emevd,esd}.md`
      2. Verify line counts and loc refs.
    Expected Result: present, conformant.
    Evidence: .sisyphus/evidence/task-24-emevd-esd.txt

  Scenario: Game-specific notes captured
    Tool: Bash
    Steps:
      1. `grep -i 'DS3\|Sekiro\|ER\|AC6' docs/api-mapping/format-emevd.md`
    Expected Result: at least 1 mention; ideally 4 (one per game).
    Evidence: .sisyphus/evidence/task-24-game-notes.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map EMEVD and ESD [Phase 5]`
  - Files: `format-emevd.md`, `format-esd.md`, `README.md`
  - Pre-commit: none (doc only)

- [x] 25. **MSB family mapping (MSB common + MSBS + MSBE + MSBVI)**

  **What to do**:
  - Produce 4 docs: `format-msb-common.md`, `format-msbs.md`, `format-msbe.md`, `format-msbvi.md`.
  - Read upstream files (paths verified at pinned commit):
    - **MSB common** (consolidate 6 files): `Formats/MSB/{MSB,IMsb,FormatReference,MSBReference,MsbBoundingBox,Shape}.cs`.
    - **MSBS** (Sekiro): subfolder `Formats/MSB/MSBS/` with `MSBS.cs` + any partial-class siblings.
    - **MSBE** (ER + Nightreign): subfolder `Formats/MSB/MSBE/` with `MSBE.cs` + any partial-class siblings (Metis estimated 6 files; verify via `find Formats/MSB/MSBE -name '*.cs'`).
    - **MSBVI** (AC6): subfolder `Formats/MSB/MSBVI/` with `MSBVI.cs` + any partial-class siblings.
  - In each game-specific doc, the header table lists every contributing `.cs` file (verified via `find` at the pinned commit; do not assume Metis's count).
  - In `format-msb-common.md`, document the inheritance / shared base (MSB.cs) and where each game-specific class extends.
  - Six-column schema; Status `未实现`; Phase target 5.
  - Cross-link the four docs.
  - **Out of scope here** (legacy, → `legacy.md` Task 29): MSB1/MSB2/MSB3/MSBAC4/MSBB/MSBD/MSBDR/MSBFA/MSBN/MSBV/MSBVD subfolders.

  **Must NOT do**:
  - Do NOT design our future MSB inheritance model in C; just describe upstream.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 4; Blocks 30-37; Blocked By 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/{MSB,IMsb,FormatReference,MSBReference,MsbBoundingBox,Shape}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/MSBS/` (entire subfolder; `find ... -name '*.cs'`)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/MSBE/` (entire subfolder)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MSB/MSBVI/` (entire subfolder)

  **Acceptance Criteria**:
  - [ ] 4 docs exist; each game-specific doc lists ≥ 6 contributing files in the header.
  - [ ] Cross-links between docs working.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Partial-class consolidation visible
    Tool: Bash
    Steps:
      1. For each of msbs/msbe/msbvi: `awk '/Contributing files/,/^$/' docs/api-mapping/format-<name>.md`
    Expected Result: each header lists ≥ 4 .cs files (MSBE expected ≥ 6).
    Evidence: .sisyphus/evidence/task-25-msb-files.txt

  Scenario: Common doc references game-specific
    Tool: Bash
    Steps:
      1. `grep -E 'format-(msbs|msbe|msbvi)\.md' docs/api-mapping/format-msb-common.md`
    Expected Result: each link present.
    Evidence: .sisyphus/evidence/task-25-msb-xrefs.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map MSB family (common, MSBS, MSBE, MSBVI) [Phase 5]`
  - Files: 4 new mapping docs + `README.md`
  - Pre-commit: none (doc only)

- [x] 26. **FLVER family mapping (common + FLVER2 + vertex)**

  **What to do**:
  - Produce 2 docs: `format-flver-common.md`, `format-flver2.md`.
  - Read upstream files (paths verified at pinned commit):
    - **FLVER common** (consolidate 8 files): `Formats/FLVER/{Dummy,IFlver,LayoutMember,Node,Vertex,VertexBoneIndices,VertexBoneWeights,VertexColor}.cs`.
    - **FLVER2** (consolidate all files in subfolder): `Formats/FLVER/FLVER2/` (note: FLVER2 is INSIDE FLVER/, NOT at root). Actual file count discovered via `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/FLVER2 -name '*.cs' | wc -l`. Metis estimated 13; trust the live `find` count.
  - Header table on FLVER2 doc lists ALL contributing files (live-discovered count, not Metis-estimated).
  - Document the vertex-element-layout enumeration explicitly with all values; it's huge but critical.
  - Document the vertex-format dispatch table (per-game vertex layouts) with notes on completeness.
  - Six-column schema; Status `未实现`; Phase target 6.
  - **Out of scope here** (legacy, → `legacy.md` Task 29): `Formats/FLVER/FLVER0/` subfolder.

  **Must NOT do**:
  - Do NOT design the future host-friendly vertex decoder (Phase 6 work).

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 4; Blocks 30-37; Blocked By 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/{Dummy,IFlver,LayoutMember,Node,Vertex,VertexBoneIndices,VertexBoneWeights,VertexColor}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/FLVER2/` (entire subfolder; **path is `FLVER/FLVER2/`, NOT `FLVER2/` at root**)

  **Acceptance Criteria**:
  - [ ] FLVER2 doc lists 13 .cs files in header (or actual count if different from Metis estimate).
  - [ ] Vertex-element-layout enum reproduced as a table within the FLVER2 doc.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Big partial-class consolidated correctly
    Tool: Bash
    Steps:
      1. `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FLVER/FLVER2 -name '*.cs' | wc -l`  # actual count (path: FLVER/FLVER2, NOT FLVER2 at root)
      2. Inspect `format-flver2.md` header file table; count rows; compare.
    Expected Result: file table count == filesystem count.
    Failure Indicators: count mismatch or any file omitted from header.
    Evidence: .sisyphus/evidence/task-26-flver2-files.txt

  Scenario: Vertex enum present
    Tool: Bash
    Steps:
      1. `grep -c 'VertexAttribute\|VertexFormat\|LayoutType' docs/api-mapping/format-flver2.md`
    Expected Result: >= 5.
    Evidence: .sisyphus/evidence/task-26-vertex-enum.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map FLVER family (common + FLVER2) [Phase 6]`
  - Files: `format-flver-common.md`, `format-flver2.md`, `README.md`
  - Pre-commit: none (doc only)

- [x] 27. **MTD + MATBIN mapping**

  **What to do**:
  - Produce 2 docs: `format-mtd.md`, `format-matbin.md`.
  - Read `Formats/MTD.cs` (and siblings) + `Formats/MATBIN.cs` (and siblings).
  - Six-column schema; Status `未实现`; Phase target 6.
  - Note: MTD is Sekiro-and-earlier; MATBIN is ER/AC6/Nightreign.

  **Must NOT do**:
  - Do NOT design shader-resolution semantics yet.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 4; Blocks 30-37; Blocked By 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MTD.cs` (root)
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/MATBIN.cs` (root)
  - Discover any partial-class siblings via `grep -lR 'partial class \(MTD\|MATBIN\)' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/`

  **Acceptance Criteria**:
  - [ ] 2 docs exist; each ≥ 8 rows.
  - [ ] Game-applicability notes (Sekiro vs ER/AC6/Nightreign) clear.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Docs structurally complete
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{mtd,matbin}.md`
      2. Verify rows + loc refs.
    Expected Result: present + conformant.
    Evidence: .sisyphus/evidence/task-27-mtd-matbin.txt

  Scenario: Game applicability noted
    Tool: Bash
    Steps:
      1. `grep -i 'Sekiro\|Elden Ring\|ER\|AC6' docs/api-mapping/format-mtd.md docs/api-mapping/format-matbin.md`
    Expected Result: each doc mentions applicable games.
    Evidence: .sisyphus/evidence/task-27-game-applicability.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map MTD and MATBIN [Phase 6]`
  - Files: `format-mtd.md`, `format-matbin.md`, `README.md`
  - Pre-commit: none (doc only)

- [x] 28. **TAE + FXR3 mapping (Phase 7)**

  **What to do**:
  - Produce 2 docs: `format-tae.md`, `format-fxr3.md`.
  - Read `Formats/TAE/` (consolidate partial-class) and `Formats/FXR3.cs` (and siblings).
  - Six-column schema; Status `未实现`; Phase target 7.
  - For FXR3: document XML serialization surface (mxml-driven in our future impl).

  **Must NOT do**:
  - Do NOT design TAE animation event opcodes yet.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 4; Blocks 30-37; Blocked By 8-20

  **References** (path-verified at the pinned commit):
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE/{TAE,Animation,Event,EventGroup,Template}.cs`
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/FXR3.cs` (root)
  - Discover further siblings via `find /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/TAE -name '*.cs'` and `grep -lR 'partial class \(TAE\|FXR3\)' /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/`

  **Acceptance Criteria**:
  - [ ] 2 docs exist; ≥ 8 rows each.
  - [ ] FXR3 doc mentions XML serialization rows.
  - [ ] README updated.

  **QA Scenarios**:
  ```
  Scenario: Docs present
    Tool: Bash
    Steps:
      1. `ls docs/api-mapping/format-{tae,fxr3}.md`
      2. Row counts + loc refs.
    Expected Result: conformant.
    Evidence: .sisyphus/evidence/task-28-tae-fxr3.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): map TAE and FXR3 [Phase 7]`
  - Files: `format-tae.md`, `format-fxr3.md`, `README.md`
  - Pre-commit: none (doc only)

- [x] 29. **Tier B legacy inventory (legacy.md)**

  **What to do**:
  - Create `docs/api-mapping/legacy.md` with a single inventory table covering ALL legacy/v2+/v3 formats: DS1/2/3/BB/DeS-only formats, AC4/ACFA/ACV/ACVD-only formats, King's Field/Kuon/Otogi/Dreamcast formats, navmesh formats (NVA/NVM/NGP/MCG/MCP/EDGE), lighting (BTAB/BTL/BTPB/PMDCL), misc (ACB/CCM/DRB/RMB/AIP/FMB/EDD/EMELD/LUAGNL/LUAINFO), legacy archives (BND2/DVDBND/FSDATA/LDMU/MGF/Zero3/ACE3/AC3SL/Kuon BND), legacy geometry (FLVER0/MDL/MDL0/MDL4/SMD4/OM2/CLM2/F2TR/GRASS), legacy maps (MSB1/2/3/B/D/N/V/VD/FA/AC4), legacy params (GPARAM, FFXDLSE, FXR1, ANI, MQB).
  - Inventory schema (5 columns): `Class | First file (relative path) | File count (1 or N for partial-class) | Defer-to-version (v2/v3) | Notes`.
  - Use `grep -lR "public class <Name>" /home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/` style discovery to count partial-class siblings. Don't miss any.
  - At top of doc: brief explanation of "Tier B = inventory only; no row-level method mapping. When a v2/v3 plan begins, the matching row gets promoted to a Tier-A doc."
  - Update README index with single `legacy.md` link.

  **Must NOT do**:
  - Do NOT row-map any individual method for these formats.
  - Do NOT skip a class because it looks niche; if it's `public class` somewhere, it goes in.

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**:
  - Can Run In Parallel: YES (independent of 21-28 mostly)
  - Parallel Group: Wave 5
  - Blocks: 30-37
  - Blocked By: 21-28 (need Tier-A docs settled to avoid double-counting in-scope formats)

  **References**:
  - `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/Formats/` (entire tree)
  - `/home/soar/src/SoulsFormatsNEXT/FORMATS.md` (upstream's own format catalog if present)

  **Acceptance Criteria**:
  - [ ] `legacy.md` exists.
  - [ ] Row count covers ≥ 60 distinct legacy classes.
  - [ ] No overlap with Tier-A scope (compare class names against `format-*.md` filenames).
  - [ ] README updated with link.

  **QA Scenarios**:
  ```
  Scenario: Coverage of legacy formats
    Tool: Bash
    Steps:
      1. `grep -c '^|' docs/api-mapping/legacy.md`  # >= 62 (header + 60+)
      2. `awk -F '|' '{print $2}' docs/api-mapping/legacy.md | sort -u | wc -l`  # >= 60
    Expected Result: row count >= 60.
    Evidence: .sisyphus/evidence/task-29-legacy-rows.txt

  Scenario: No double-counting with Tier A
    Tool: Bash
    Steps:
      1. `awk -F '|' '/^\| /{print tolower($2)}' docs/api-mapping/legacy.md | tr -d ' ' | sort -u > /tmp/legacy.txt`
      2. `ls docs/api-mapping/format-*.md | sed 's|.*format-||;s|\.md$||' | sort -u > /tmp/tier-a.txt`
      3. `comm -12 /tmp/legacy.txt /tmp/tier-a.txt`
    Expected Result: empty intersection.
    Evidence: .sisyphus/evidence/task-29-no-overlap.txt
  ```

  **Commit**: YES
  - Message: `docs(api-mapping): add Tier-B legacy.md inventory of v2+/v3 formats`
  - Files: `docs/api-mapping/legacy.md`, `docs/api-mapping/README.md`
  - Pre-commit: none (doc only)

> **WAVE 6 NOTE — Roadmap rewrite under the new alignment policy**.
> All Wave 6 tasks gate on Wave 3 fixes settling (Metis-mandated; sequential, not parallel
> with Wave 3). Each rewritten doc must include:
> (a) "Upstream references" section: explicit `.cs` paths and partial-class siblings
>     for every task in the phase.
> (b) "API alignment checklist" inline with each task: bullets that cite
>     `docs/api-mapping/format-<name>.md` rows the task fulfills.
> (c) "Strict alignment policy reminder" header pointing to `AGENTS.md §5.x` rules.
> (d) For Phase 0/1/2 (already done): a "Completion Retrospective" section with
>     actual API delivered + alignment status (`✓ aligned` after Wave 3 fixes).

- [x] 30. **roadmap/README.md rewrite**

  **What to do**:
  - Rewrite `docs/roadmap/README.md` under the new policy:
    - Update Phase 2 status to ✅ done with date + test count from filesystem reality.
    - Add a new section "Strict upstream alignment" with a quick reference to AGENTS.md §5.x rules and links to `docs/api-mapping/{README,POLICY,UPSTREAM}.md`.
    - Update "How to use a phase doc" to add a step: "Open `docs/api-mapping/format-<name>.md` BEFORE writing any code; verify Status column for each method you'll touch."
    - Keep existing "What lives in PLAN.md vs. here" table; add a row for `docs/api-mapping/`.
    - Refresh dependency-graph diagram if Wave 2/3 changed any cross-phase dependency (e.g., sf_get_decompressed_reader added in Phase 1 may simplify Phase 3 e2e helpers).

  **Must NOT do**:
  - Do NOT change the existing phase numbering or date estimates.
  - Do NOT delete the existing dependency-graph or vs-PLAN.md table.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES (with Tasks 31-36); Wave 6; Blocks: 37; Blocked By: 8-29.

  **References**:
  - `docs/roadmap/README.md` (current)
  - `docs/api-mapping/README.md` (Task 7)
  - `AGENTS.md` (after Task 4)

  **Acceptance Criteria**:
  - [ ] Phase 2 row shows ✅ done + concrete test count.
  - [ ] "Strict upstream alignment" section present (`grep -c 'Strict upstream alignment' docs/roadmap/README.md ≥ 1`).
  - [ ] Link to `docs/api-mapping/` present.

  **QA Scenarios**:
  ```
  Scenario: Index reflects new state
    Tool: Bash
    Steps:
      1. `grep 'Phase 2' docs/roadmap/README.md | head -3`
      2. `grep 'api-mapping' docs/roadmap/README.md`
    Expected Result: Phase 2 ✅ done; api-mapping referenced.
    Evidence: .sisyphus/evidence/task-30-roadmap-readme.txt

  Scenario: How-to step refers to mapping
    Tool: Bash
    Steps:
      1. `grep -A 8 'How to use' docs/roadmap/README.md | grep api-mapping`
    Expected Result: at least 1 reference.
    Evidence: .sisyphus/evidence/task-30-howto.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite README under strict alignment policy`
  - Files: `docs/roadmap/README.md`
  - Pre-commit: none (doc only)

- [x] 31. **phase-0/1/2 retrospective rewrites**

  **What to do**:
  - Rewrite `docs/roadmap/phase-2-compression-crypto.md` (and add new files for phase-0 / phase-1 if they don't exist; per current state, phase 0/1 only live inside PLAN.md so create `phase-0-scaffolding.md` and `phase-1-runtime.md`):
    - "Status: ✅ done" header with completion date.
    - "Completion Retrospective" section: list of actual deliverables (sources + tests) with file paths, alignment-status snapshot for each (link to specific mapping doc rows).
    - "Alignment Status" table: every drift item the phase had + how it was closed (link to drift-checklist.md).
    - "Follow-up fixes pulled in by Wave 2/3" subsection: list of mapping/code changes Tasks 8-20 made.
    - "Lessons learned" section: 3-5 bullets capturing what to do differently in Phase 3+.

  **Must NOT do**:
  - Do NOT recreate the original "Goal/Deliverables/QA" sections; this is retrospective, not aspirational.
  - Do NOT silently rewrite history; cite what was actually shipped.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `.sisyphus/plans/PLAN.md §7` Phase 0/1/2 sections (canonical history)
  - `docs/api-mapping/drift-checklist.md` (after Tasks 8-20)
  - Current `docs/roadmap/phase-2-compression-crypto.md`

  **Acceptance Criteria**:
  - [ ] 3 phase docs (phase-0, phase-1, phase-2) exist, each with retrospective sections.
  - [ ] Each links to ≥ 3 mapping docs.
  - [ ] Status header shows ✅ done.

  **QA Scenarios**:
  ```
  Scenario: Retrospective sections present
    Tool: Bash
    Steps:
      1. `for f in phase-0-scaffolding phase-1-runtime phase-2-compression-crypto; do grep -c 'Completion Retrospective' docs/roadmap/$f.md; done`
    Expected Result: each returns >= 1.
    Evidence: .sisyphus/evidence/task-31-retro-sections.txt

  Scenario: Mapping links present
    Tool: Bash
    Steps:
      1. `grep -c 'api-mapping/' docs/roadmap/phase-{0-scaffolding,1-runtime,2-compression-crypto}.md`
    Expected Result: each >= 3.
    Evidence: .sisyphus/evidence/task-31-mapping-links.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): retrospective rewrites for completed Phase 0/1/2`
  - Files: `docs/roadmap/phase-0-scaffolding.md` (new), `docs/roadmap/phase-1-runtime.md` (new), `docs/roadmap/phase-2-compression-crypto.md` (rewrite)
  - Pre-commit: none (doc only)

- [x] 32. **phase-3 full rewrite**

  **What to do**:
  - Rewrite `docs/roadmap/phase-3-archive-containers.md` under new policy:
    - Header: "Strict upstream alignment policy applies — see AGENTS.md §5.x".
    - Each task gets an explicit "Upstream references" sub-block with file paths + line-anchored cross-refs to `docs/api-mapping/format-binder-common.md`, `format-bnd3.md`, `format-bnd4.md`, `format-bxf3.md`, `format-bxf4.md`, `format-bhd5.md`, `format-tpf.md`, `format-enfl.md`.
    - "API alignment checklist" inline per task: bullets pointing at specific mapping rows the task must satisfy.
    - Preserve existing QA scenarios; augment with mapping-row-coverage check (e.g., "after BND4 implementation, mapping-doc Status column for every BND4 row must transition `未实现` → `✓ aligned` or `~ partial`").
    - Note: `er_extract_from_data0` helper now lives in Phase 3 still, but its design must reference Task 13's `sf_get_decompressed_reader`.

  **Must NOT do**:
  - Do NOT design BND4 or any other format here; these are roadmap docs, not implementation specs.
  - Do NOT alter the phase's scope (BND3/4 + BXF3/4 + BHD5 + TPF + ENFL).

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES (with 30, 31, 33-36); Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `docs/roadmap/phase-3-archive-containers.md` (current)
  - All Phase-3 mapping docs from Task 21-22

  **Acceptance Criteria**:
  - [ ] Each Phase-3 task in the doc has an `Upstream references` sub-block.
  - [ ] At least 8 cross-refs to `docs/api-mapping/format-*.md`.
  - [ ] Strict-alignment header present.

  **QA Scenarios**:
  ```
  Scenario: Per-task upstream references
    Tool: Bash
    Steps:
      1. `grep -c 'Upstream references' docs/roadmap/phase-3-archive-containers.md`  # >= 6 (one per format task)
    Expected Result: >= 6.
    Evidence: .sisyphus/evidence/task-32-phase3-refs.txt

  Scenario: Mapping cross-refs
    Tool: Bash
    Steps:
      1. `grep -cE 'docs/api-mapping/format-(binder-common|bnd3|bnd4|bxf3|bxf4|bhd5|tpf|enfl)\.md' docs/roadmap/phase-3-archive-containers.md`
    Expected Result: >= 8.
    Evidence: .sisyphus/evidence/task-32-phase3-xrefs.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite phase-3 under strict alignment policy`
  - Files: `docs/roadmap/phase-3-archive-containers.md`
  - Pre-commit: none (doc only)

- [x] 33. **phase-4 full rewrite**

  **What to do**:
  - Rewrite `docs/roadmap/phase-4-param-text.md`. Same recipe as Task 32, but for PARAM/PARAMDEF/PARAMTDF/FMG. Cross-refs to `format-param.md`, `format-paramdef.md`, `format-paramtdf.md`, `format-fmg.md`.
  - Note PARAMDEF XML reading is in scope; XML writing is deferred (per PLAN.md §2.2).

  **Must NOT do**:
  - Do NOT pull XML write back into v1 scope.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `docs/roadmap/phase-4-param-text.md` (current)
  - Phase-4 mapping docs (Task 23)

  **Acceptance Criteria**:
  - [ ] Per-task `Upstream references` sub-blocks present for PARAM/PARAMDEF/PARAMTDF/FMG.
  - [ ] At least 4 mapping cross-refs.

  **QA Scenarios**:
  ```
  Scenario: Cross-refs all four formats
    Tool: Bash
    Steps:
      1. `grep -cE 'docs/api-mapping/format-(param|paramdef|paramtdf|fmg)\.md' docs/roadmap/phase-4-param-text.md`
    Expected Result: >= 4.
    Evidence: .sisyphus/evidence/task-33-phase4-xrefs.txt

  Scenario: XML write still deferred
    Tool: Bash
    Steps:
      1. `grep -i 'XML write' docs/roadmap/phase-4-param-text.md`
    Expected Result: text indicates "deferred to v1.1" or similar.
    Evidence: .sisyphus/evidence/task-33-xml-write-deferred.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite phase-4 under strict alignment policy`
  - Files: `docs/roadmap/phase-4-param-text.md`
  - Pre-commit: none (doc only)

- [x] 34. **phase-5 full rewrite**

  **What to do**:
  - Rewrite `docs/roadmap/phase-5-script-map.md`. Cross-refs to `format-emevd.md`, `format-esd.md`, `format-msb-common.md`, `format-msbs.md`, `format-msbe.md`, `format-msbvi.md`.
  - Add explicit per-task notes about MSBE applying to BOTH ER and Nightreign (one C implementation, two e2e test suites).

  **Must NOT do**:
  - Do NOT add legacy MSB1/2/3 mapping back into scope.

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `docs/roadmap/phase-5-script-map.md` (current)
  - Phase-5 mapping docs (Tasks 24-25)

  **Acceptance Criteria**:
  - [ ] Per-task `Upstream references` blocks for EMEVD/ESD/MSB-common/MSBS/MSBE/MSBVI.
  - [ ] At least 6 mapping cross-refs.
  - [ ] Nightreign-uses-MSBE note present.

  **QA Scenarios**:
  ```
  Scenario: Cross-refs cover all 6 formats
    Tool: Bash
    Steps:
      1. `grep -cE 'docs/api-mapping/format-(emevd|esd|msb-common|msbs|msbe|msbvi)\.md' docs/roadmap/phase-5-script-map.md`
    Expected Result: >= 6.
    Evidence: .sisyphus/evidence/task-34-phase5-xrefs.txt

  Scenario: Nightreign note present
    Tool: Bash
    Steps:
      1. `grep -i 'Nightreign.*MSBE\|MSBE.*Nightreign' docs/roadmap/phase-5-script-map.md`
    Expected Result: at least 1 match.
    Evidence: .sisyphus/evidence/task-34-nightreign-msbe.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite phase-5 under strict alignment policy`
  - Files: `docs/roadmap/phase-5-script-map.md`
  - Pre-commit: none (doc only)

- [x] 35. **phase-6 full rewrite**

  **What to do**:
  - Rewrite `docs/roadmap/phase-6-geometry-material.md`. Cross-refs to `format-flver-common.md`, `format-flver2.md`, `format-mtd.md`, `format-matbin.md`.
  - Document the per-game vertex-layout dispatch (FLVER2 contains different layout types depending on Sekiro/ER/AC6); cite the mapping-doc rows.

  **Must NOT do**:
  - Do NOT include FLVER0 (legacy, Tier B).

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `docs/roadmap/phase-6-geometry-material.md` (current)
  - Phase-6 mapping docs (Tasks 26-27)

  **Acceptance Criteria**:
  - [ ] Per-task `Upstream references` blocks for FLVER-common/FLVER2/MTD/MATBIN.
  - [ ] At least 4 mapping cross-refs.
  - [ ] Per-game vertex-layout note present.

  **QA Scenarios**:
  ```
  Scenario: Cross-refs cover 4 formats
    Tool: Bash
    Steps:
      1. `grep -cE 'docs/api-mapping/format-(flver-common|flver2|mtd|matbin)\.md' docs/roadmap/phase-6-geometry-material.md`
    Expected Result: >= 4.
    Evidence: .sisyphus/evidence/task-35-phase6-xrefs.txt

  Scenario: Vertex-layout dispatch noted
    Tool: Bash
    Steps:
      1. `grep -ic 'vertex.*layout\|layout.*type' docs/roadmap/phase-6-geometry-material.md`
    Expected Result: >= 1.
    Evidence: .sisyphus/evidence/task-35-vertex-layout.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite phase-6 under strict alignment policy`
  - Files: `docs/roadmap/phase-6-geometry-material.md`
  - Pre-commit: none (doc only)

- [x] 36. **phase-7 + post-v1 rewrites**

  **What to do**:
  - Rewrite `docs/roadmap/phase-7-animation-effects.md`. Cross-refs to `format-tae.md`, `format-fxr3.md`. Preserve "v1.1 / optional" status; do NOT promote to required.
  - Rewrite `docs/roadmap/post-v1.md` to reflect new alignment policy: every legacy/v2+ format has a row in `docs/api-mapping/legacy.md`; promotion to Tier A happens when a legacy game's plan begins (PLAN_v2.md, PLAN_v3.md). Update prose to emphasize the alignment-with-upstream commitment carries forward.

  **Must NOT do**:
  - Do NOT promote Phase 7 from optional to required.
  - Do NOT design v2 plans inline (PLAN_v2.md is a separate future document).

  **Recommended Agent Profile**: `writing` / `[]`

  **Parallelization**: YES; Wave 6; Blocks 37; Blocked By 8-29.

  **References**:
  - `docs/roadmap/phase-7-animation-effects.md` (current)
  - `docs/roadmap/post-v1.md` (current)
  - Phase-7 mapping docs (Task 28)
  - Tier B `legacy.md` (Task 29)

  **Acceptance Criteria**:
  - [ ] phase-7 rewrites preserve "optional / v1.1" status.
  - [ ] phase-7 cross-refs `format-tae.md` and `format-fxr3.md`.
  - [ ] post-v1 references `legacy.md` and outlines Tier A promotion path.

  **QA Scenarios**:
  ```
  Scenario: Phase 7 stays optional
    Tool: Bash
    Steps:
      1. `grep -i 'optional\|v1.1\|deferred' docs/roadmap/phase-7-animation-effects.md`
    Expected Result: at least 1 match.
    Evidence: .sisyphus/evidence/task-36-phase7-optional.txt

  Scenario: post-v1 references legacy.md
    Tool: Bash
    Steps:
      1. `grep -c 'legacy.md' docs/roadmap/post-v1.md`
    Expected Result: >= 1.
    Evidence: .sisyphus/evidence/task-36-postv1-legacy.txt
  ```

  **Commit**: YES
  - Message: `docs(roadmap): rewrite phase-7 + post-v1 under strict alignment policy`
  - Files: `docs/roadmap/phase-7-animation-effects.md`, `docs/roadmap/post-v1.md`
  - Pre-commit: none (doc only)

- [x] 37. **PLAN.md sync (§5 / §7 / §11 / §12)**

  **What to do**:
  - In `.sisyphus/plans/PLAN.md`:
    - In §5 ("公共 API 设计原则"), add a new subsection `### 5.x Strict upstream alignment` referencing AGENTS.md §5.x rules verbatim and pointing to `docs/api-mapping/`.
    - In §7 ("阶段化里程碑"), append "Completion Retrospective" notes inline within Phase 0/1/2 entries — list new APIs added by this plan, cite the `docs/api-mapping/drift-checklist.md` line for each closed item.
    - In §11 ("风险与未决问题"), add a new row "API drift over time" — risk: upstream advances, our mapping goes stale; mitigation: re-audit policy in `UPSTREAM.md` (every 50 commits, re-survey core 6 files).
    - In existing §12 ("附录 A：上游格式完整对照清单"), add a new "row → mapping doc" column linking each upstream format to its `docs/api-mapping/format-*.md` (Tier A) or its `legacy.md` row (Tier B). Per Metis G5: do NOT introduce a new §12; extend the existing one.
    - Anywhere PLAN.md says version 0.1.0, change to 0.2.0 with a footnote "(post-API-realignment minor bump)".

  **Must NOT do**:
  - Do NOT change Phase 3-7 QA contracts or scope.
  - Do NOT renumber any section.
  - Do NOT remove the existing §12 content; extend it.
  - Do NOT touch §1 / §2 / §3 / §4 / §6 / §8 / §9 / §10 (out of scope).

  **Recommended Agent Profile**:
  - **Category**: `writing`
  - **Skills**: `[]`

  **Parallelization**: NO (final pre-review consolidation); Wave 7; Blocks F1-F5; Blocked By 8-36.

  **References**:
  - `.sisyphus/plans/PLAN.md` (full file)
  - `AGENTS.md` (after Task 4)
  - `docs/api-mapping/README.md` + `drift-checklist.md` + `UPSTREAM.md`

  **Acceptance Criteria**:
  - [ ] §5.x present (`grep -c '5\.[0-9].*Strict upstream alignment\|Strict upstream alignment' .sisyphus/plans/PLAN.md ≥ 1`).
  - [ ] §11 has new "API drift over time" row.
  - [ ] §12 contains link column with at least one `docs/api-mapping/` reference per upstream format.
  - [ ] No `0.1.0` left as the post-refactor version (`grep -c '0\.1\.0' .sisyphus/plans/PLAN.md` returns 0 outside historical/retrospective entries).

  **QA Scenarios**:
  ```
  Scenario: Sections updated
    Tool: Bash
    Steps:
      1. `grep -n 'Strict upstream alignment' .sisyphus/plans/PLAN.md`
      2. `grep -n 'API drift over time\|API 漂移' .sisyphus/plans/PLAN.md`
      3. `grep -n 'docs/api-mapping' .sisyphus/plans/PLAN.md`
    Expected Result: each grep returns >= 1 line.
    Evidence: .sisyphus/evidence/task-37-plan-sections.txt

  Scenario: Version reflects 0.2.0
    Tool: Bash
    Steps:
      1. `grep '0\.[12]\.0' .sisyphus/plans/PLAN.md`
    Expected Result: 0.2.0 references present; any 0.1.0 reference is in retrospective context.
    Evidence: .sisyphus/evidence/task-37-version.txt

  Scenario: §12 extension is in-place (no collision)
    Tool: Bash
    Steps:
      1. `grep -E '^## 1[0-9]\.|^## [0-9]+\.' .sisyphus/plans/PLAN.md | head -20`
    Expected Result: §12 still present; §13 still present; no duplicate numbering.
    Evidence: .sisyphus/evidence/task-37-no-renumber.txt
  ```

  **Commit**: YES
  - Message: `docs(plan): sync PLAN.md with strict alignment policy + mapping cross-refs`
  - Files: `.sisyphus/plans/PLAN.md`
  - Pre-commit: none (doc only)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 5 review agents run in PARALLEL (one more than baseline — Metis-mandated cross-doc consistency reviewer). ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.
>
> **Do NOT auto-proceed after verification. Wait for user's explicit approval before marking work complete.**
> **Never mark F1-F5 as checked before getting user's okay.** Rejection or user feedback → fix → re-run → present again → wait for okay.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read this plan end-to-end. For each "Must Have": verify deliverable exists (read file, run command, grep marker). For each "Must NOT Have": search codebase/docs for forbidden patterns — reject with file:line if found. Check `.sisyphus/evidence/` for evidence files per task. Compare deliverables against plan TODOs.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `cmake --build build-mingw` (Werror) and `ctest --test-dir build-mingw -L core --output-on-failure`. Lint via clang-format check. Review all changed files (`git diff --name-only`) for: `as any`-equivalents (forced casts), commented-out code, unused imports, AI slop (excessive comments, generic names like `data/result/item/temp` in new code). Confirm `_Static_assert` guards on every new enum table.
  Output: `Build [PASS/FAIL] | Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real Manual QA** — `unspecified-high`
  Start from clean build dir. Configure + build + run ALL tests (`ctest --test-dir build-mingw --output-on-failure`). Verify golden-hash regression suite passes. Spot-check 3 mapping docs by sampling rows and confirming the upstream `file:line` reference resolves correctly (open `/home/soar/src/SoulsFormatsNEXT/...` at the cited line). Verify `objdump -p libsouls_formats.dll` exports include all newly-added `sf_*` symbols. Save evidence to `.sisyphus/evidence/final-qa/`.
  Output: `Build [N/N] | Tests [N/N pass] | Spot-checks [N/N] | Symbol export [N added] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (`git log --oneline + git diff`). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance per task. Detect cross-task contamination: Task N touching Task M's files. Flag unaccounted changes.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

- [x] F5. **Cross-Doc Consistency Check** — `deep`
  Verify these documents agree pairwise:
  (a) `AGENTS.md` ↔ `PLAN.md` ↔ `docs/api-mapping/POLICY.md`: alignment policy text is consistent (no contradictory rules).
  (b) `AGENTS.md §2 status table` ↔ `docs/roadmap/README.md table` ↔ filesystem reality (Phase 2 ✅, real test count from `ctest -N`).
  (c) `CHANGELOG.md` ↔ `git log` ↔ `objdump -p`: every "Breaking change" entry corresponds to an actual removal/rename; every "New API" entry is an actually-exported symbol.
  (d) Every mapping row's upstream `file:line` ref resolves at the pinned commit (sample 20 rows; spot-check).
  (e) Drift checklist all ticked; no orphan items.
  Output: `Pairs checked [N/N consistent] | Sampled refs [N/N resolve] | Orphans [N] | VERDICT`

---

## Commit Strategy

> One commit per task. Commit messages follow `type(scope): summary` with the upstream-loc rationale in the body.
> Per-task commit details are in each TODO's "Commit" section.

Commit groups:
- T1-T7: Wave 1 preconditions, 7 commits.
- T8-T14: Wave 2 Phase 1 module fixes, 7 commits.
- T15-T20: Wave 3 Phase 2 module fixes, 6 commits.
- T21-T28: Wave 4 mapping docs, 8 commits.
- T29: Wave 5 legacy inventory, 1 commit.
- T30-T36: Wave 6 roadmap rewrites, 7 commits.
- T37: Wave 7 PLAN.md sync, 1 commit.
- Final wave: zero commits (review only); user-approved squash optional at handoff.

Pre-commit hook: `cmake --build build-mingw && ctest --test-dir build-mingw -L core --output-on-failure` MUST pass before any commit in Waves 2/3 lands.

---

## Success Criteria

### Verification Commands
```bash
# 1. Build + test passes (Werror, Unity tests)
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
# Expected: 0 failures, real test count (>= 66 + new-API tests)

# 2. Golden-hash regression suite passes
ctest --test-dir build-mingw -L golden --output-on-failure
# Expected: every recorded SHA-256 matches

# 3. Mapping doc count + structure
find docs/api-mapping -name "*.md" -type f | wc -l
# Expected: >= 25 (Tier A) + 1 (legacy.md) + 4 (UPSTREAM/POLICY/README/extensions/drift-checklist) = >= 30

# 4. No stale "Phase 2 next" outside CHANGELOG and outside this plan file
grep -r "Phase 2.*next" --include="*.md" --exclude-dir=.sisyphus . | grep -v CHANGELOG.md
# Expected: empty
# (.sisyphus/ is excluded because this plan file itself documents the
#  stale state being fixed and would self-match otherwise.)

# 5. Version bump landed
grep -c "0.2.0" CMakeLists.txt include/souls_formats/sf_common.h CHANGELOG.md AGENTS.md
# Expected: each file has at least 1 match

# 6. Strict alignment rules in AGENTS.md
grep -c "STRICT UPSTREAM REFERENCE" AGENTS.md
grep -c "API MIRRORS UPSTREAM" AGENTS.md
# Expected: >= 1 each

# 7. Symbol export verification
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -c "sf_"
# Expected: > 137 (new APIs added)

# 8. PLAN.md cross-link
grep -c "api-mapping" .sisyphus/plans/PLAN.md
# Expected: >= 1
```

### Final Checklist
- [ ] All "Must Have" deliverables present and verified by command.
- [ ] All "Must NOT Have" guardrails respected (no new format code, no shims, etc.).
- [ ] All Phase 0/1/2 drift items closed in `drift-checklist.md`.
- [ ] All 5 review agents (F1-F5) returned APPROVE.
- [ ] User explicitly approved final state.
