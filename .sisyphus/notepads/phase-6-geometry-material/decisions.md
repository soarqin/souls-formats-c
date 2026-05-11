# Phase 6 Decisions

## [2026-05-12] Session Start

### Architecture
- Edge geometry / SPU / RSX: OUT-of-scope v1, push to v2 legacy
- FLVER2 BE: reject at offset 0x06 with SF_ERR_UNSUPPORTED_VERSION
- EdgeCompression flag: reject both read and write
- `sf_flver2_decode_mesh`: EXTENSION (not upstream API)

### Mapping notes
- FLVER2 Edge / SPU / RSX sub-tables are marked `_skipped_` in `docs/api-mapping/format-flver2.md`; main FLVER2 tables remain `未实现` for the v1 wave.

### File layout
- `include/souls_formats/sf_flver.h` — FLVER common types (Dummy/Node/LayoutMember/half-float)
- `include/souls_formats/sf_flver2.h` — FLVER2 opaque types + accessors
- `include/souls_formats/sf_mtd.h` — MTD types
- `include/souls_formats/sf_matbin.h` — MATBIN types + 8 ParamTypes
- `src/geom/` — all geometry source files
- `tests/geom/` — all geometry unit + e2e tests
- `tests/probes/` — one-shot empirical probes (T4, T5)

### [2026-05-12] Documentation
- Seeded `docs/api-mapping/extensions.md` with detailed entries for `sf_flver2_decode_mesh`, BE refusal, and EdgeCompression refusal.
