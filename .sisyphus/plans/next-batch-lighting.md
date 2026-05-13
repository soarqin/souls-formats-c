## TL;DR

Implement lighting and environment parameter formats including BTAB, BTL, BTPB, GPARAM, and PMDCL. This cluster covers the data used for light placement, baked lighting, and global environment settings (sky, fog, etc.).

## Upstream formats covered

- BTAB (Bake atlas table)
- BTL (Map light placement)
- BTPB (Baked lighting prebake buffer)
- GPARAM (Global environment parameters)
- PMDCL (Precomputed map data)

## Must Have

- Full implementation of `sf_gparam_t` mirroring upstream `GPARAM.cs`.
- Support for BTL light placement data.
- Support for baked lighting metadata (BTAB, BTPB).
- Implementation of PMDCL for precomputed map data.

## Must NOT Have

- MTD/MATBIN (already in v1).

## Dependencies on prior clusters

- Phase 1 (Core IO)

## Acceptance criteria

- GPARAM files pass read/write round-trip tests.
- BTL light data is correctly parsed.
- Verification via:
```bash
cmake --build build-mingw
ctest --test-dir build-mingw -L lighting --output-on-failure
grep -E "sf_(gparam|btl)_" include/souls_formats/sf_common.h || true
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| BTAB | SoulsFormats/Formats/BTAB.cs |
| BTL | SoulsFormats/Formats/BTL.cs |
| BTPB | SoulsFormats/Formats/BTPB.cs |
| GPARAM | SoulsFormats/Formats/GPARAM.cs |
| PMDCL | SoulsFormats/Formats/PMDCL.cs |

## Estimated effort

- 6 days (GPARAM is complex with many nested groups and parameters).

## Risk

- Medium. GPARAM has a non-trivial structure with many optional fields and game-specific variations.

## Completion

**Completed: 2026-05-13**

All acceptance criteria met. Lighting batch landed as v0.5.0:
- Public headers shipped: sf_btab.h, sf_btl.h, sf_gparam.h, sf_pmdcl.h
- Tests pass: 8/8 (4 synthetic + 4 e2e with graceful skip)
- BTPB dropped: not present in v1 games per Wave-0 probe
- See CHANGELOG.md ## [0.5.0] and docs/api-mapping/extensions.md "Post-v1: Lighting" for details.
