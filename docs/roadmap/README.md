# Phase Roadmap

This directory holds **per-phase implementation guides** for souls-formats-c.

Each guide is the working spec a developer (or AI agent) opens when they
sit down to execute a phase. It expands the high-level summaries in
[`.sisyphus/plans/PLAN.md`](../../.sisyphus/plans/PLAN.md) with concrete
file paths, upstream references, public API sketches, and the full QA
contract for the phase.

> **Strategic plan**: [`.sisyphus/plans/PLAN.md`](../../.sisyphus/plans/PLAN.md) (Momus-audited).
> **Project navigation for agents**: [`AGENTS.md`](../../AGENTS.md).

---

## Phase index

| Phase | Title | State | Estimate | Doc |
|---|---|---|---|---|
| 0 | Project scaffolding | ✅ done | 0.5 wk | [phase-0-scaffolding.md](phase-0-scaffolding.md) |
| 1 | Runtime (IO, encoding, math, hash) | ✅ done | 1.5 wk | [phase-1-runtime.md](phase-1-runtime.md) |
| 2 | Compression + crypto | ✅ done | 2 wk | [phase-2-compression-crypto.md](phase-2-compression-crypto.md): 17/17 PASS (2026-05-10) |
| 3 | Archive containers | ✅ done | 2 wk | [phase-3-archive-containers.md](phase-3-archive-containers.md): 32/32 PASS (2026-05-10) |
| 4 | Param + text | ✅ done | 1.5 wk | [phase-4-param-text.md](phase-4-param-text.md): 20/20 PASS (2026-05-11) |
| 5 | Script + map | ⏳ pending | 3 wk | [phase-5-script-map.md](phase-5-script-map.md) |
| 6 | Geometry + material | ⏳ pending | 3 wk | [phase-6-geometry-material.md](phase-6-geometry-material.md) |
| 7 | Animation + effects (optional / v1.1) | ⏳ pending | 2 wk | [phase-7-animation-effects.md](phase-7-animation-effects.md) |
| v2+ | Legacy games | ⏳ post v1 GA | ... | [post-v1.md](post-v1.md) |

Total v1 (Phases 2-6) effort: **~11 weeks**, plus Phase 7 (~2 weeks) if shipped in v1.0.

---

## Strict upstream alignment

Every code change in this repository must follow the mandatory rules defined in [`AGENTS.md`](../../AGENTS.md) §5.x.

1. **STRICT UPSTREAM REFERENCE**: Every implementation must reference upstream code at the pinned commit. Guessing at semantics or wire formats is forbidden.
2. **API MIRRORS UPSTREAM**: Public C API design must mirror upstream as closely as possible. Functional differences are forbidden.

Refer to the [API Mapping](../api-mapping/README.md) directory for row-level alignment status, [POLICY.md](../api-mapping/POLICY.md) for adaptation rules, and [UPSTREAM.md](../api-mapping/UPSTREAM.md) for the source baseline.

---

## Dependency graph

```
Phase 1 (runtime)
   │
   ▼
Phase 2 (compression + crypto)
   │
   ├──> Phase 3 (archive containers)
   │       │
   │       ▼
   │    er_extract_from_data0() helper  ◀── all downstream e2e depends on this
   │       │
   │       ├──> Phase 4 (param + text)
   │       ├──> Phase 5 (script + map)
   │       ├──> Phase 6 (geometry + material)
   │       └──> Phase 7 (animation + effects, optional)
   │
   └──> regulation.bin pipeline (used by Phase 4 too)
```

**Hard rule**: do not start phase N+1 until phase N's QA scenarios pass on
the current branch. Phases 4-6 specifically require Phase 3's
`er_extract_from_data0` helper because Elden Ring ships every loose asset
inside `Data0-3.bhd/bdt`, never as a standalone `.dcx`.

---

## How to use a phase doc

1. Open the matching `phase-N-*.md`.
2. Open the relevant mapping docs in [`docs/api-mapping/`](../api-mapping/README.md) before writing any code. Verify the Status column for every method you plan to touch.
3. Read the **Upstream references** list and skim those `.cs` files in
   `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/`.
4. Implement headers → sources → tests in the file order shown under
   **File structure**.
5. Wire each new test into [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)
   via `sf_add_test()`.
6. Run the **QA scenarios** at the bottom of the doc until all green.
7. Tick the corresponding boxes in [`PLAN.md`](../../.sisyphus/plans/PLAN.md)
   `### Phase N` section with timestamp + concrete pass count.
8. If the phase materially changed scope, re-run Momus on PLAN.md.

---

## What lives in PLAN.md vs. here

| Topic | PLAN.md | docs/roadmap/ | docs/api-mapping/ |
|---|---|---|---|
| Strategic decisions, license, scope | ✅ canonical | brief reference | ... |
| Test data hardcoded paths | ✅ canonical (§8.4) | brief reference | ... |
| Risk register | ✅ canonical (§11) | per-phase risks only | ... |
| Phase deliverable checklist | ✅ tick-able | duplicated | ... |
| File structure (concrete paths) | high-level (§6) | ✅ per-phase detail | ... |
| Upstream `.cs` files to read | implicit | ✅ explicit list | ... |
| Public API sketches | conventions (§5) | ✅ per-format signatures | ... |
| Row-level symbol mapping | ... | ... | ✅ canonical |
| Implementation notes (gotchas) | ... | ✅ here | ... |
| QA scenarios | ✅ canonical | duplicated + details | ... |
When the two disagree, **PLAN.md wins**. It is Momus-audited.
