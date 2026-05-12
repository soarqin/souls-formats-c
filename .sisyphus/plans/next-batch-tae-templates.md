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
