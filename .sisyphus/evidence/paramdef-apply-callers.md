# T0.3 — PARAMDEF Apply Caller-Pattern Audit

## Verdict: GO (W4.2 proceed — repeat-same-def pattern found)

## Call Sites

| File:Line | Function | Pattern | Estimated row count | Notes |
|-----------|----------|---------|---------------------|-------|
| src/param/paramdef_apply.c:583 | sf_param_apply_paramdef | DEFINITION | N/A | Public entry point |
| src/param/paramdef_apply.c:617 | sf_param_apply_paramdef | Repeat-same-def | N (all rows) | Calls itself recursively for multi-def apply |
| tests/param/test_param_apply_paramdef.c:199 | test_apply_basic | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:265 | test_apply_careful | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:291 | test_roundtrip | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:301 | test_roundtrip | One-shot | ~10 | Unit test (roundtrip) |
| tests/param/test_param_apply_paramdef.c:316 | test_field_types | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:330 | test_bitfield | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:349 | test_error_cases | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:364 | test_padding | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef.c:418 | test_apply_all | One-shot | ~10 | Unit test |
| tests/param/test_param_apply_paramdef_e2e.c:110 | e2e_apply | Hot-loop | ~10,000+ | ER regulation.bin — applies same def to all rows |
| tests/param/test_param_apply_paramdef_e2e.c:221 | e2e_wrong_def | One-shot | ~10 | Error case test |

## Key Finding

`tests/param/test_param_apply_paramdef_e2e.c:110` applies the same `def` to all rows of a
PARAM from ER's regulation.bin. SpEffectParam has ~10,000+ rows. This is a **Hot-loop** pattern
where precomputing the field layout once and reusing it across all rows would yield significant
savings.

Additionally, `src/param/paramdef_apply.c:617` calls itself recursively for multi-def apply,
which is a **Repeat-same-def** pattern.

## Mutability Check

Callers do NOT mutate the `def` after calling `sf_param_apply_paramdef`. The def is passed as
`const sf_paramdef_t *` — the API already enforces immutability at the type level. A precompute
cache is safe without a generation counter.

## Downstream Impact

- **T4.2**: GO — precompute cache is justified by the e2e hot-loop pattern (~10k rows × field-layout cost).
- Cache can be stored inside `sf_paramdef_t` (opaque, so no ABI break).
- No generation counter needed (const pointer enforces immutability).
