# T0.7 — emevd_internal.h Cross-Module Use Audit

## Verdict: KEEP-IN-PLACE

## Includer List

All files that include `emevd_internal.h`:

```
src/script/emevd_parameter.c
src/script/emevd_event.c
src/script/emevd_instruction.c
src/script/emevd_layer.c
src/script/emevd.c
tests/script/test_emevd_write.c
tests/script/test_emevd_synthetic.c
```

All 7 includers are under `src/script/` or `tests/script/`. No cross-module use found.

## Rationale

`emevd_internal.h` is correctly format-local. It is only used by the EMEVD format
implementation and its tests. This matches the established convention for format-local
internal headers (same as `msbs_internal.h`, `tae_internal.h`, etc.).

## Action for T1.7

T1.7 is a **no-op** (KEEP-IN-PLACE path):
- Add a one-line comment at the top of `src/script/emevd_internal.h` documenting the
  format-local convention.
- No file relocation needed.

## Audit Command

```bash
grep -rln 'emevd_internal.h' src/ include/ tests/ examples/
# Output: 7 files, all under src/script/ or tests/script/
```
