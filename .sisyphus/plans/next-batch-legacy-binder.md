## TL;DR

Implement legacy binder formats BND and BND2 used in Demon's Souls, Dark Souls 1, Dark Souls 2, Bloodborne, and early Armored Core titles. This cluster completes the binder coverage by adding the older variants that preceded BND3 and BND4.

## Upstream formats covered

- BND (Legacy BND binder)
- BND2 (Legacy BND2 binder)

## Must Have

- Full implementation of `sf_bnd_t` and `sf_bnd2_t` mirroring upstream `BND.cs` and `BND2.cs`.
- Streaming reader support for BND2 via `sf_bnd2_reader_t`.
- Support for both big-endian and little-endian variants.
- Integration with `sf_binder_t` interface where applicable.

## Must NOT Have

- MSB legacy variants (deferred to `legacy-msb` cluster).
- BHD5/BND3/BND4 (already implemented in v1).

## Dependencies on prior clusters

- Phase 1 (Core IO/Stream)
- Phase 2 (Compression/DCX)

## Acceptance criteria

- All unit tests for BND and BND2 pass.
- `libsouls_formats.dll` exports `sf_bnd_*` and `sf_bnd2_*` symbols.
- Verification via:
```bash
cmake --build build-mingw
ctest --test-dir build-mingw -L legacy-binder --output-on-failure
x86_64-w64-mingw32-objdump -p build-mingw/libsouls_formats.dll | grep -E 'sf_bnd2?_'
```

## STRICT UPSTREAM REFERENCE

| Format | Upstream Path |
|--------|---------------|
| BND | SoulsFormats/Formats/Binder/BND/BND.cs |
| BND2 | SoulsFormats/Formats/Binder/BND2/BND2.cs |
| BND2 Header | SoulsFormats/Formats/Binder/BND2/BND2FileHeader.cs |
| BND2 Reader | SoulsFormats/Formats/Binder/BND2/BND2Reader.cs |
| BND2 Interface | SoulsFormats/Formats/Binder/BND2/IBND2.cs |

## Estimated effort

- 3 days (BND is simple, BND2 has more moving parts with the streaming reader).

## Risk

- Low. These are well-understood formats with stable upstream implementations.
