## TL;DR

Implement the Time Act Editor (TAE) template subsystem and complete the TAE format support for non-SDT variants. This cluster enables structured event data parsing for all TAE versions.

## Upstream formats covered

- TAE (Time Act Editor) — remaining non-SDT variants and template integration.
- TAE Template subsystem (BankTemplate, EventTemplate, ParameterTemplate).

## Must Have

- Full implementation of `sf_tae_template_t` mirroring upstream `Template.cs`.
- Support for parsing TAE events using templates to determine parameter types and names.
- Completion of `sf_tae_t` to support all legacy TAE variants found in DS1, DS2, DS3, and Bloodborne.

## Must NOT Have

- FXR3 (already in v1).

## Dependencies on prior clusters

- Phase 1 (Core IO)
- Phase 7 (TAE SDT infrastructure)

## Acceptance criteria

- TAE files from legacy games are correctly parsed using templates.
- Event parameters are correctly typed and named according to the template.
- Verification via:
```bash
cmake --build build-mingw
ctest --test-dir build-mingw -L tae-templates --output-on-failure
test -f include/souls_formats/sf_tae_template.h
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| TAE Root | SoulsFormats/Formats/TAE/TAE.cs |
| TAE Template | SoulsFormats/Formats/TAE/Template.cs |

## Estimated effort

- 4 days (Template logic is non-trivial and requires careful mapping to C).

## Risk

- Low. The template system is well-defined in upstream.

## TODOs

- [x] Create `include/souls_formats/sf_tae_template.h` — public header with ParamType enum, opaque types, XML read API, accessor API
- [x] Create `src/effects/tae_template_internal.h` — internal struct definitions
- [x] Create `src/effects/tae_template.c` — XML parsing (mxml) and accessor implementations
- [x] Add `sf_tae_apply_template()` to `sf_tae.h` and implement in `tae.c`
- [x] Implement non-SDT TAE variants (DS1/SOTFS/DS3/BB/DES/DESR) read/write in `tae.c`
- [x] Update `souls_formats.h` umbrella to include `sf_tae_template.h`
- [x] Update `tests/CMakeLists.txt` with 4 new test targets (label: `tae-templates`)
- [x] Write `tests/anim/test_tae_template_xml.c` — synthetic XML parsing test
- [x] Write `tests/anim/test_tae_template_apply.c` — apply template to synthetic TAE
- [x] Write `tests/anim/test_tae_legacy_ds1r.c` — DS1R e2e TAE read test
- [x] Write `tests/anim/test_tae_legacy_ds3.c` — DS3 e2e TAE read test

## Final Verification Wave

- [x] `cmake --build build-mingw` → exit 0
- [x] `ctest --test-dir build-mingw -L tae-templates --output-on-failure` → 4/4 PASS
- [x] `test -f include/souls_formats/sf_tae_template.h` → file exists
- [x] No regressions: 84/84 tests pass across all labels

## Completion

**Completed: 2026-05-13**

All acceptance criteria met:
- `sf_tae_template_t` fully implemented mirroring upstream `Template.cs` (801 LOC)
- XML parsing via mxml for all TAE game formats (DS1/SOTFS/DS3/BB/SDT/DES/DESR)
- `sf_tae_apply_template()` validates template compatibility and resizes event parameters
- Non-SDT TAE variants (DS1/SOTFS/DS3/BB/DES/DESR) read/write implemented in `tae.c`
- 4 new tests with label `tae-templates` all pass
- 84/84 total tests pass (no regressions)
