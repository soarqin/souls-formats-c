# Cluster Plan: Text-Script Misc (T6.7)

## TL;DR

Implement miscellaneous text and script-related formats that were not included in the v1 core. This includes Lua global name lists (LUAGNL), script info tables (LUAINFO), event message localization data (EMELD), and field map binaries (FMB).

## Upstream formats covered

- `SoulsFormats/Formats/EMELD.cs`
- `SoulsFormats/Formats/FMB.cs`
- `SoulsFormats/Formats/LUAGNL.cs`
- `SoulsFormats/Formats/LUAINFO.cs`

## Must Have

- Read/write support for LUAGNL and LUAINFO (used for Lua script management).
- Read/write support for EMELD (localization mapping for event messages).
- Read/write support for FMB (map metadata).
- Proper handling of Shift-JIS and UTF-16 encodings as used in these formats.

## Must NOT Have

- EMEVD/ESD/FMG — these are already implemented in v1.
- Lua bytecode execution or decompilation.

## Dependencies on prior clusters

- Phase 1 (Core IO): `sf_binary_reader_t`, `sf_binary_writer_t`, `sf_encoding_t`.

## Acceptance criteria

- All 4 formats pass the validator:
```bash
bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-text-script-misc.md
```
- Build succeeds with new modules:
```bash
cmake --build build-mingw
```
- New tests pass:
```bash
ctest --test-dir build-mingw -L script
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| EMELD | `SoulsFormats/Formats/EMELD.cs` |
| FMB | `SoulsFormats/Formats/FMB.cs` |
| LUAGNL | `SoulsFormats/Formats/LUAGNL.cs` |
| LUAINFO | `SoulsFormats/Formats/LUAINFO.cs` |

## Estimated effort

- 1 day (Low complexity, mostly simple tables and strings).

## Risk

- Low. These formats are well-understood and stable.
