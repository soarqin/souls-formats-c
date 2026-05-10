# API Realignment Decisions

## Architectural Decisions
- No deprecation/shim layer — clean break at v0.x
- No mapping-table auto-generation tooling — markdown by hand
- No upstream-test port — keep Unity-based tests, augment
- No new format implementation inside this plan — Phase 3+ formats get mapping rows only
- No abstraction inflation

## Naming Decisions
- Version bump: 0.1.0 → 0.2.0 (minor signals "rebuild required")
- sf_dcx_compression_info_t: tagged union with 9 variants
- IsFlexible: per-reader flag + separate global default setter
- OodleLZ_* enums: public as sf_oodle_lz_*

## Wave Dependencies
- Wave 2 tasks T8,T9,T10,T11,T12,T14,T18 are ALL parallel (no cross-deps)
- T18 moved to Wave 2 to unblock T15 (DCX needs sf_oodle_lz_compressor_t)
- T13 moved to Wave 3 (needs T15's sf_dcx_compression_info_t)
- T15 (DCX) is head of Wave 3 — must complete before T13, T16, T17, T19, T20
