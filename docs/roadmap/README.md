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
| 0 | Project scaffolding | ✅ done | 0.5 wk | (in PLAN.md) |
| 1 | Runtime (IO, encoding, math, hash) | ✅ done | 1.5 wk | (in PLAN.md) |
| 2 | Compression + crypto | ✅ done | 2 wk | [phase-2-compression-crypto.md](phase-2-compression-crypto.md) — 13/13 PASS across 13 binaries (2026-05-10) |
| 3 | Archive containers | ⏳ pending | 2 wk | [phase-3-archive-containers.md](phase-3-archive-containers.md) |
| 4 | Param + text | ⏳ pending | 1.5 wk | [phase-4-param-text.md](phase-4-param-text.md) |
| 5 | Script + map | ⏳ pending | 2.5 wk | [phase-5-script-map.md](phase-5-script-map.md) |
| 6 | Geometry + material | ⏳ pending | 3 wk | [phase-6-geometry-material.md](phase-6-geometry-material.md) |
| 7 | Animation + effects (optional / v1.1) | ⏳ pending | 2 wk | [phase-7-animation-effects.md](phase-7-animation-effects.md) |
| v2+ | Legacy games | ⏳ post v1 GA | — | [post-v1.md](post-v1.md) |

Total v1 (Phases 2-6) effort: **~11 weeks**, plus Phase 7 (~2 weeks) if shipped in v1.0.

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
2. Read the **Upstream references** list and skim those `.cs` files in
   `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/`.
3. Implement headers → sources → tests in the file order shown under
   **File structure**.
4. Wire each new test into [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)
   via `sf_add_test()`.
5. Run the **QA scenarios** at the bottom of the doc until all green.
6. Tick the corresponding boxes in [`PLAN.md`](../../.sisyphus/plans/PLAN.md)
   `### Phase N` section with timestamp + concrete pass count.
7. If the phase materially changed scope, re-run Momus on PLAN.md.

---

## What lives in PLAN.md vs. here

| Topic | PLAN.md | docs/roadmap/ |
|---|---|---|
| Strategic decisions, license, scope boundaries | ✅ canonical | brief reference |
| Test data hardcoded paths | ✅ canonical (§8.4) | brief reference |
| Risk register | ✅ canonical (§11) | per-phase risks only |
| Phase deliverable checklist | ✅ tick-able | duplicated for working convenience |
| File structure (concrete paths) | high-level (§6) | ✅ per-phase detail |
| Upstream `.cs` files to read | implicit | ✅ explicit list |
| Public API sketches | conventions (§5) | ✅ per-format signatures |
| Implementation notes (gotchas) | — | ✅ here |
| QA scenarios | ✅ canonical | duplicated + may add details |

When the two disagree, **PLAN.md wins** — it is Momus-audited.
