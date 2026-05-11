# Phase 5 Decisions

## [2026-05-11] Session ses_1e930abc1ffegcwbNdWbHIbXxm — Plan Analysis

### Wave 0 Parallelization
- T1, T3, T4, T5, T6 can run in parallel
- T2 depends on T1 (needs build to succeed for ctest count)

### Key Architectural Decisions (from plan)
- MSB sub-params: 1 .c file per sub-param per variant (not monolithic)
- ESD bytecode: structured decode (known opcodes → operand tree), NOT opaque blob
- MSB variants: MSBS (Sekiro), MSBE (ER + Nightreign), MSBVI (AC6)
- Test helpers: each game has its own helper, no cross-game pollution
- Nightreign probe needed before MSBE writer to see if ER code is compatible

### Out of Scope
- MSB1/2/3/MSBAC4/MSBB/etc. (v2 legacy)
- MSBVI/MSBE Shape subtypes not used by ER
- New third-party deps
- VM execution of ESD bytecode (only structural decode)
- Nightreign-specific MSB field extensions if incompatible

### [2026-05-11] Phase 4 Documentation Sync
- Use the ctest label `param|script` output as the source of truth for Phase 4 completion counts.
- Record Phase 4 as completed in status tables while keeping Phase 5 as the active in-progress phase.

## 2026-05-11 — MSBS EventParam internal API
- Kept EventParam fields internal by replacing only the opaque `sf_msbs_event` reserved byte with `msbs_event_t data` in `msbs_internal.h`; no public `sf_msbs.h` event accessors were added.
- Enabled root MSBS writing for nonzero EventParam while retaining unsupported guards for region, route, and parts params.
