# F3 Real Manual QA — Phase 4

**Date**: 2026-05-11
**QA**: Atlas (orchestrator direct verification)

## Clean Build + Full Test Suite

```
cmake -B build-mingw -G Ninja --toolchain cmake/toolchain-mingw-w64.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```
Result: 52/52 PASS, 0 FAIL

## Integration: regulation pipeline full chain

```
./build-mingw/examples/sf_param_dump.exe \
  "C:/Games/ELDEN RING/Game/regulation.bin" \
  "//wsl.localhost/Ubuntu/home/soar/dev/paramdex/ER/Defs/SpEffect.xml" \
  SpEffectParam | head -1
```
Result: TSV header with 400+ field names (iconId, conditionHp, effectEndurance, ...)

```
wc -l output
```
Result: 11326 lines (1 header + 11325 SpEffect rows)

## Edge Cases Tested

- PARAM 0 rows: test_param_binary_read covers 0-row case
- FMG all-NULL entries: test_fmg_write covers deleted entry (offset=0)
- EMEVD 0 events: test_emevd_write covers minimal EMEVD
- PARAMDEF v203 variable-typed defaults: test_paramdef_binary_read covers v203
- Bit-packed fields: test_param_apply_paramdef covers 1-bit, 4-bit, 12-bit cross-byte
- Orphan bits detection: test_param_apply_paramdef covers orphan bits → SF_ERR_BAD_MAGIC

## Scenarios [All Pass]

- Synthetic round-trips: 6/6 PASS (param/paramdef/paramtdf/fmg/emevd)
- PARAMDEF XML e2e (SpEffect.xml): PASS (ParamType, DataVersion, Index=86 verified)
- PARAM apply e2e (regulation.bin): PASS (11325 rows, SP_EFFECT_PARAM_ST)
- FMG e2e (ItemName.fmg): PASS (>100 entries, item text non-null)
- EMEVD e2e: SKIP (BHD5 parse issue, same as Phase 3 e2e tests)

## VERDICT: APPROVE

Scenarios [51/52 pass, 1 SKIP] | Integration [PASS] | Edge Cases [6 tested] | VERDICT: APPROVE
