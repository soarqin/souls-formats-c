# Phase 6 Learnings

## [2026-05-12] Session Start

### Project Context
- Phase 0-5 complete. Phase 6 is geometry+material: FLVER2, MTD, MATBIN, FLVER common.
- Build: `cmake --build build-mingw && ctest --test-dir build-mingw --output-on-failure`
- Toolchain: MinGW-w64 cross-compile from WSL2; .exe runs via binfmt_misc interop
- Upstream: `/home/soar/src/SoulsFormatsNEXT` (read-only)
- Test framework: Unity ThrowTheSwitch + ctest + `sf_add_test()` labels

### Key Conventions (DO NOT DRIFT)
- Every public symbol: `sf_` prefix, `_t` type suffix, `SF_API` decoration
- Error path: `sf_result_t` return codes, output via pointer params
- Memory: every "create" takes `const sf_allocator_t *alloc` (NULL = default malloc/free)
- Strings: UTF-8 at boundary, Win32 for Shift-JIS/UTF-16 bridging
- `_Static_assert` after every enum table
- No Edge geometry API exposed in public headers

### Existing patterns to reuse
- `src/archive/bnd4.c` — reserve/fill top-level dispatch
- `src/map/msb_common.c` — list-of-lists top-level skeleton
- `src/core/encoding_win32.c` — Shift-JIS / UTF-16 string reading
- Phase 5 probes: `tests/probes/probe_nightreign_msb.c` — one-shot probe pattern
- `tests/e2e/er_test_helper.c` — BHD5 + DCX + BND4 extraction chain

### Critical Design Decisions (from Metis review)
1. Vertex dispatch MUST mirror `Vertex.cs:112-390` foreach + semantic-first if/else ladder (NOT static table)
2. GXItem.Data is opaque `uint8_t*` + size (NOT structured)
3. MATBIN ParamType: 8 non-consecutive values: Bool=0, Int=4, Int2=5, Float=8..Float5=12
4. FaceSet restart symbol: ONLY 0xFFFF (u16), not u32
5. SkeletonSet: NO v1/v2 hierarchy branch; simple `Version >= 0x2001A` gate + BaseSkeleton + AllSkeletons
6. FLVER2 BE byte at offset 0x06; v1 REJECTS BE → SF_ERR_UNSUPPORTED_VERSION
7. `sf_flver2_decode_mesh` is EXTENSION (must be in extensions.md)
8. BufferLayout/VertexBuffer are mesh-shared via global index (NOT mesh 1:1)
9. BufferLayout.Size uses SpecialModifier == -32768 sentinel for zero-byte members
10. EdgeCompression flag: v1 rejects on read + write
11. Half-float helpers are Wave 1 HARD requirement
12. UV factor / AC6 normalization flag: threaded context param (not registry)
13. AC6 UShort4 normals: special normalization via threaded flag
14. Triangulation: filter degenerate ON by default
15. Bounding box: write as-is, NO recompute
16. FLVER2 Header version whitelist (from FLVER2.cs:109)

### Status table sync note
- Phase 5 completion text used: `5/5 PASS across 32 test binaries`
- Phase 6 status now reads `🚧 in progress` across AGENTS.md and docs/roadmap/README.md
- PLAN.md Phase 5 checklist was fully checked off without touching Phase 6 scope

## [2026-05-12] T5 MATBIN ParamType empirical survey

- Probe: `tests/probes/probe_matbin_paramtypes.c`; evidence written to `.sisyphus/evidence/task-5-matbin-survey.md` and raw stdout to `.sisyphus/evidence/task-5-matbin-survey.txt`.
- `allmaterial.matbinbnd.dcx` BND4 entry count: 15103.
- First 10 `.matbin` sample histogram: Bool(0)=63, Int(4)=20, Int2(5)=10, Float(8)=58, Float2(9)=0, Float3(10)=10, Float4(11)=0, Float5(12)=28.
- Unknown ParamTypes: none; sampled raw values stayed within `{0,4,5,8,9,10,11,12}`.
- De-duplicated sampler types included C_Detail3Blend and OverlayBlend texture slots; smallest MATBIN found was `S[Ghost].matbin` at 425 bytes.
- Gotcha: current `er_extract_from_data0()` 32-bit/37 hash path did not locate allmaterial in this install; the probe falls back to the BHD5 64-bit `h = c + 133*h` hash used by modern archives, then applies the same one-layer DCX unwrap.

## [2026-05-12] T4 c0000 FLVER2 layout probe

- `/chr/c0000.chrbnd.dcx` is in ER `Data3`, not `Data0`; the probe needs to fan out across `Data0`-`Data3` with the archive-specific ER RSA keys.
- ER Data3 BHD5 uses the 64-bit `h = c + 133*h` path hash and 40-byte ER file headers (`u64 hash`, `i32 padded`, `i32 unpadded`, `i64 file offset`, `i64 SHA offset`, `i64 AES offset`).
- RSA-unwrapped BHD5 blocks must be restored to 255-byte plaintext chunks before parsing, matching `src/archive/bhd5.c`'s padding behavior.
- Empirical result for this install: `N:\GR\data\INTERROOT_win64\chr\c0000\c0000.flver` has version `0x2001A`, endian byte `0`, 510 dummies, 488 bones, but `MESH_COUNT=0`, `BUFFER_LAYOUT_COUNT=0`, and no `(Type, Semantic)` layout pairs/triples.
- Gotcha: local ER `Data0.bhd` contains 5824 files and opens fine, but the current snapshot scan did not expose `/chr/c0000.chrbnd.dcx` or `/material/allmaterial.matbinbnd.dcx`; later archives in the install returned `SF_ERR_OUT_OF_RANGE` from `sf_bhd5_open`.

## [2026-05-12] T4 c0000 FLVER2 layout probe

- `probe_flver2_layouts` must use ER Data3 for `/chr/c0000.chrbnd.dcx`; Data0 does not contain this path in the installed build.
- ER Data3 BHD top-level header uses 32-bit bucket count/offset with 40-byte ER file records; c0000's BHD record has `unpadded_size == 0`, so extraction must use `padded_size`.
- ER 64-bit BHD path hash is the `h = c + 133*h` algorithm, not the public 32-bit `sf_path_hash_64` shim.
- Actual `N:\GR\data\INTERROOT_win64\chr\c0000\c0000.flver` evidence: version `0x2001A`, little-endian, `dummy_count=510`, `bone_count=488`, `mesh_count=0`, `buffer_layout_count=0`; no `(LayoutType, LayoutSemantic, Index)` triples exist for this c0000 skeleton FLVER in the current ER install.

## T15 — FLVER2 FaceSet + triangle-strip decode (2026-05-12)

- **FaceSet compact v1 layout:** current C implementation uses the task-specified
  24-byte record (`u32 flags`, two bools, two u8 unknowns, `i32 count`, `i32 data`
  offset, `u8 index_size`, zero u8/i16/i32) and reads indices from
  `data_offset + index_offset` as u16/u32 widened to u32.
- **Primitive restart is u16-only:** triangle strips split only on `0xFFFF` when
  `index_size == 16`. Do not add a `0xFFFFFFFF` restart in the u32 path.
- **Degenerate filtering default:** internal triangulation helper takes an explicit
  `filter_degenerate`; mesh/decode callers should pass `true` to match upstream
  `Triangulate(..., includeDegenerateFaces: false)` behavior.
- **Writer data section ordering:** top-level `flver2.c` now calls
  `sfi_flver2_face_set_write_indices()` immediately after filling `DataOffset`,
  before final data-section padding/`DataSize` backpatch.
- **EdgeCompressed policy:** `SF_FLVER2_FS_FLAGS_EDGE_COMPRESSED` remains v1
  unsupported and returns `SF_ERR_UNSUPPORTED_VERSION` during read/write.
