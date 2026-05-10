2026-05-11: ER phase-4 test helpers can stay test-only in `tests/e2e`; use suffix matching for param entries because DLC-merged paths include extra prefixes.
2026-05-11: For BND payload copies, read the archive into memory, parse with `sf_bnd4_read_from_memory`, then copy the matched entry into a caller-owned buffer.

## T4.2 — Integrated synthetic round-trip (2026-05-11)

### Pattern: self-consistent round-trip
Strict `memcmp(original_fixture, write_output)` is **not achievable** for the
binary formats (PARAM, PARAMDEF, FMG) because the writer emits a canonical
byte layout that may legitimately differ from the human-friendly hand-built
fixture. The strongest available guarantee — and the one upstream C# tests
also rely on — is:

```
fixture → read → write (canonical #1) → read → write (canonical #2)
assert memcmp(canonical #1, canonical #2) == 0
```

This is the **same pattern** used in `tests/archive/test_bnd4_synthetic.c`
(via `roundtrip_assert`), `tests/param/test_param_binary_write.c`
(via `assert_written_round_trip`), and `tests/param/test_paramdef_binary_write.c`
(via `assert_writable_version_round_trips`).

### PARAMTDF text round-trip uses strcmp, not memcmp
The PARAMTDF text writer always emits a trailing `\r\n`, whereas hand-written
inputs typically omit it. Two consecutive canonical writes (both from the
writer) **are** byte-identical, so the same self-consistency contract applies
but with `TEST_ASSERT_EQUAL_STRING` (NUL-terminated comparison).

### EMEVD synthetic tests require internal header access
`sf_emevd.h` is read-only (no builders/setters). To construct a synthetic
EMEVD for round-trip, the test must include `script/emevd_internal.h` and
populate `struct sf_emevd` / `struct sf_emevd_event` / etc. directly via
`sf_xalloc`. This mirrors `tests/script/test_emevd_write.c::make_emevd`.

### FMG public API is sufficient for round-trip
Unlike EMEVD, FMG exposes `sf_fmg_create` + `sf_fmg_add_entry(id, text, alloc)`
in the public header. NULL text → tombstone (deleted) entry; this is the
mechanism used to populate the "5 entries with 1 deleted" fixture in T4.2.

### CMakeLists.txt canonical naming
Tests must use the `souls_formats_test_<name>` target prefix so external
build-system invocations (`cmake --build … --target souls_formats_test_X`)
match the project convention established in Phase 0–3.

### T4.3 — Paramdex XML e2e specifics (2026-05-11)
`SpEffect.xml` is stable enough to probe by metadata (`ParamType`, `DataVersion`,
`Unicode`, `BigEndian`, `FormatVersion`, `Index`) and by field lookup via
`InternalName`; field order should not be assumed.
