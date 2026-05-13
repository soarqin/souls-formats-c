# Post-v1 Roadmap

> **Status**: ✅ All post-v1 batches complete (v0.5.0) · v1.0 GA scheduled.

## v0.5.0 — Lighting closure (LATEST)

**Completed: 2026-05-13**

* 4 formats shipped: BTAB / BTL / GPARAM / PMDCL (BTPB dropped per Wave-0 probe)
* All 10 post-v1 batches now complete
* Reference to [next-batch-lighting.md](../../.sisyphus/plans/next-batch-lighting.md) (implemented in v0.5.0 — see CHANGELOG)

---

## v1.1 — Animation + Effects + PARAMDEF XML write

**Estimate**: ~2 weeks. **Trigger**: shortly after v1.0 GA, while context
is fresh.

Scope:
* [Phase 7](phase-7-animation-effects.md) flipped on by default.
* PARAMDEF XML **write-back** (read shipped in [Phase 4](phase-4-param-text.md);
  write deferred to v1.1 to avoid bikeshedding XML-formatting style under
  v1.0 deadline pressure).
* Wider FLVER vertex layout coverage if Phase 6 left gaps logged as
  `KNOWN_LAYOUT_GAP`.

No new format families.

---

## v2.0 — Legacy modern Souls (DS1 / DS2 / DS3 / BB / DeS)

**Estimate**: ~6-8 weeks. **Trigger**: at least one user explicitly asking
for legacy support.

Scope of new format support is tracked in [legacy.md](../api-mapping/legacy.md).

### v2 Process

1. Spin up `docs/roadmap/v2-*.md` per legacy phase, mirroring the v1
   structure (one phase doc per format family).
2. **Tier A Promotion**: Promote relevant formats from `legacy.md` to new Tier A mapping docs with row-level method mapping.
3. Re-run Momus on the new plan additions before starting code.
4. v2 may share code with v1 (DCX, BHD5, BND infrastructure) but adds
   new format modules — the architecture is already designed to absorb
   them without disrupting v1.

### v2 Test data

The user will need to provide additional game copies (DS3 in particular,
since it shares so much infrastructure with ER). Hardcoded paths in
[`.sisyphus/plans/PLAN.md`](../../.sisyphus/plans/PLAN.md) §8.4 will be extended as
copies become available.

---

## v3.0 — Pre-modern Armored Core + King's Field + miscellany

**Estimate**: ~4-6 weeks. **Trigger**: niche; only if community
specifically asks.

Scope: See [legacy.md](../api-mapping/legacy.md) for the inventory of pre-modern formats.

These mostly do not share code with v1/v2 — they need fresh per-format
parsers. v3 is intentionally last because the community demand is small
and the formats are sparsely documented.

---

## What stays the same across v2/v3

* **Architecture** ([PLAN.md](../../.sisyphus/plans/PLAN.md) §3) does not change. New format modules slot
  into existing layers.
* **Public API conventions** ([PLAN.md](../../.sisyphus/plans/PLAN.md) §5, [AGENTS.md](../../AGENTS.md) §5) are immutable in
  v0.x post-1.0; new functions follow the same prefix / error / allocator
  rules.
* **License**: still GPL-3.0. Still no Oodle DLL redistribution.
* **CI matrix**: the Windows × {MSVC, clang-cl, MinGW} × Ubuntu cross
  matrix continues unchanged.

---

## What MIGHT change

* **Build option for "modern only"**: when v2 lands, expose
  `SF_MODERN_ONLY=ON` to skip legacy formats; default ON in v2.0 release
  branch and OFF on main.
* **Threading**: v3 may introduce parallel BHD5 / FLVER decoding for
  large batch tools. v1 keeps strict single-threaded-per-context.
* **Backend swap**: §3.3 of [PLAN.md](../../.sisyphus/plans/PLAN.md) keeps the door open for non-Win32
  crypto/Oodle backends in case of a future Linux-subset build. Not
  scheduled, but the structure is in place.

---

## When to update this doc

* Right after v1.0 GA — sharpen the v1.1 scope based on what shipped.
* When a v2 milestone is committed — split into per-phase docs in this
  directory, mirroring v1's `phase-N-*.md` pattern.
* Never trim existing v1 phase docs — they remain as historical reference.
