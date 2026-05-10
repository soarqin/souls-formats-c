# Phase 4 Decisions

## Architecture Decisions

- PARAM Cell API: Tagged union + typed getters (both) — supports introspection AND hot-path typed access
- PARAM apply: 3-mode enum (UNCONDITIONAL/SOMEWHAT_CAREFUL/CAREFUL) — folds 8 upstream variants
- PARAMDEF XML: Def attribute parsed with 3 regex patterns (outer/bit/array) — mirrors XmlSerializer.cs
- PARAMTDF: Pure text, no binary — mirrors upstream
- EMEVD ArgData: Raw bytes passthrough — no EMEDF JSON loading
- FMG groups: Internal write-time concept — not exposed in public API
- Bit-packing: little-endian bit order, paramdef_apply.c helpers — mirrors Row.cs:236-244
- EMEVD header includes: keep the required public `souls_formats/...` form, but add `__has_include` fallbacks to absolute workspace paths so clangd and standalone syntax checks both resolve the chain.
- EMEVD read T2.5: accept DS3 `version == 0xCD` per upstream EMEVD.cs while also tolerating the task/header's `0xCC` DS3 fixture value; unsupported Novel flags remain rejected.

## Scope Decisions

- EMEVD moved from Phase 5 to Phase 4 (user request)
- Phase 5 reduced to ESD + MSB(s/e/vi)
- PARAMDEF XML write deferred to v1.1
- RegulationVersioned* Apply variants deferred to v1.1
- UnnamedRows/HeaderlessRows: return SF_ERR_UNSUPPORTED_VERSION (v1 scope)
- sf_paramdef_get_index: extension API (Paramdex XML only, binary returns -1)
- sf_paramdef_field_get_sort_id: extension API (Paramdex XML only, binary returns 0)

## Parallelization Strategy

- Wave 0: T0.1 (sequential, blocks Wave 1)
- Wave 1: T1.1-T1.6 parallel, T1.7 sequential after all T1.x
- Wave 2: T2.1-T2.5 all parallel
- Wave 3: T3.1-T3.7 all parallel (max 7 concurrent)
- Wave 4: T4.1-T4.7 parallel (T4.7 blocked by T4.4)
- Wave 4.5: T4.8 sequential after Wave 4
- Final: F1-F4 parallel
