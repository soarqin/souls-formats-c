# Cluster Plan: Navmesh (T6.6)

## TL;DR

Implement the navigation mesh formats used across FromSoftware titles, including area data (NVA), polygon meshes (NVM), group/path containers (NGP), and the map collision graph/points (MCG/MCP). This cluster also includes the shared EDGE geometry compression used by these formats.

## Upstream formats covered

- `SoulsFormats/Formats/EDGE.cs`
- `SoulsFormats/Formats/MCG.cs`
- `SoulsFormats/Formats/MCP.cs`
- `SoulsFormats/Formats/NGP.cs`
- `SoulsFormats/Formats/NVA.cs`
- `SoulsFormats/Formats/NVM.cs`

## Must Have

- Full read/write support for NVA, NVM, NGP, MCG, and MCP.
- Implementation of EDGE geometry decompression/compression as required by the formats.
- Support for both legacy (32-bit) and modern (64-bit) variants where applicable.
- Unit tests for each format using representative samples from Sekiro and Elden Ring.

## Must NOT Have

- Integration with physics engines (Havok/PhysX) — this is a data-only library.
- Pathfinding algorithms — only the data structures are implemented.

## Dependencies on prior clusters

- Phase 1 (Core IO): `sf_binary_reader_t`, `sf_binary_writer_t`.
- Phase 6 (Geometry): Shared math types (`sf_vector3_t`, etc.).

## Acceptance criteria

- All 6 formats pass the validator:
```bash
bash tests/cluster-plan-validator.sh .sisyphus/plans/next-batch-navmesh.md
```
- Build succeeds with new modules:
```bash
cmake --build build-mingw
```
- New tests pass:
```bash
ctest --test-dir build-mingw -L navmesh
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| EDGE | `SoulsFormats/Formats/EDGE.cs` |
| MCG | `SoulsFormats/Formats/MCG.cs` |
| MCP | `SoulsFormats/Formats/MCP.cs` |
| NGP | `SoulsFormats/Formats/NGP.cs` |
| NVA | `SoulsFormats/Formats/NVA.cs` |
| NVM | `SoulsFormats/Formats/NVM.cs` |

## Estimated effort

- 3 days (Medium complexity due to geometry data in NVM/EDGE).

## Risk

- Medium. EDGE compression is non-trivial and must match upstream bit-for-bit for round-trip stability.
