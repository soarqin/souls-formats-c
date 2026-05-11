# Phase 5 Learnings

## [2026-05-11] Session ses_1e930abc1ffegcwbNdWbHIbXxm — Initial Analysis

### Codebase State
- Phase 0-4 fully done; EMEVD was moved forward into Phase 4
- Phase 5 scope: ESD + MSB family (MSBS/MSBE/MSBVI)
- No `src/map/` directory exists yet
- `src/script/` exists with only EMEVD files (emevd.c, emevd_event.c, etc.)
- `tests/e2e/` exists with er_test_helper.{c,h} and EMEVD/archive tests

### [2026-05-11] Hygiene Guard Follow-up
- Project-wide `/home/` grep guards must avoid self-matching source text; build the needle from split literals or macros.
- `system()` in the Windows test binary returned a raw status code, not the POSIX wait status; normalize for both `1` and `256`-style returns.
- `SOULS_FORMATS_ROOT_DIR` is useful for tests that need repo-relative runtime paths without hardcoding `/home/...` in source.

### [2026-05-11] ESD Header Surface
- `sf_esd.h` can stay header-only with opaque top-level, state, condition, and command-call handles.
- `ESD.Condition` evaluator and `CommandCall.Arguments` are exposed as raw byte blobs; decode later in implementation/tests, not in the public API.
- C++ compatibility can follow the existing `_Static_assert` redefinition pattern used by `sf_msb.h`.

### Confirmed Bugs
- `include/souls_formats/sf_param.h:14-15` has 2 absolute path includes (/home/soar/...)
- `include/souls_formats/sf_emevd.h:25,28,35,38` has 4 absolute path includes (/home/soar/...)
- Total: 6 bugs confirmed by grep

### Test Infrastructure
- `sf_add_test(name source label)` macro in tests/CMakeLists.txt
- Labels: smoke, core, compression, crypto, archive, param, script, e2e_er
- New labels needed: map, script (ESD), hygiene, e2e_sekiro, e2e_nightreign, e2e_ac6, phase-4-debt

### API-Mapping Docs
- All format-{esd,msb-common,msbs,msbe,msbvi}.md exist already
- All entries have status "未实现" — need updates as implementation progresses

### Build System
- MinGW-w64 cross compiler on WSL2
- Build dir: build-mingw
- Command: cmake --build build-mingw
- Test command: ctest --test-dir build-mingw --output-on-failure

### Project Include Convention
- Correct relative include: `"souls_formats/sf_common.h"` 
- Reference: include/souls_formats/sf_paramdef.h:14-15 uses correct pattern
- Never use absolute paths starting with /home/

### Upstream Reference
- Pinned commit: 9f5848f5f45a7f5b2d4ff841ba05b63ab2e6be0a
- Location: /home/soar/src/SoulsFormatsNEXT/
- ESD: SoulsFormats/Formats/ESD/ESD.cs (+ ESD.State.cs, etc.)
- MSB common: SoulsFormats/Formats/MSB/MSB.cs
- MSBS: SoulsFormats/Formats/MSB/MSBS/MSBS.cs
- MSBE: SoulsFormats/Formats/MSB/MSBE/MSBE.cs
- MSBVI: SoulsFormats/Formats/MSB/MSBVI/MSBVI.cs

### [2026-05-12] Synthetic Round-Trip Test Notes
- ESD synthetic RT can reuse the existing hand-crafted fixture from `test_esd_read.c`; the write/read path stayed stable for both 32-bit and 64-bit fixtures.
- MSBS and MSBE synthetic RTs are easiest when built from the internal variant structs in `msb*_internal.h` and verified through the public count/accessor APIs.
- MSBVI write/read is currently reliable for the empty-root case; keep the synthetic RT conservative unless a richer fixture is proven stable.

### [2026-05-11] T14a — MSBS Root Dispatcher Shell
- Upstream `MSBS.Read()` order is exactly: Models, Events, Regions, Routes, Layers, Parts, PartsPoses, BoneNames.
- Sekiro typed param names/versions: `MODEL_PARAM_ST`, `EVENT_PARAM_ST`, `POINT_PARAM_ST`, `ROUTE_PARAM_ST`, `PARTS_PARAM_ST` all version 35.
- Sekiro empty params are still required in the root chain: `LAYER_PARAM_ST` version 0x23, `MAPSTUDIO_PARTS_POSE_ST` version 0, `MAPSTUDIO_BONE_NAME_STRING` version 0.
- When writing the linked param chain, backpatch each list's `nextParamOffset` with the next list's start offset before writing that next list's body; filling it after the next body skips a param and causes truncated reads.
- `msb_common_read_header()` currently records list names/counts and ignores param version, so variant writers should preserve upstream versions themselves for future compatibility.
- Build verification exposed a pre-existing `src/script/esd_bytecode.c` destructor drift: the implementation freed nonexistent `tree->nodes`; freeing from `tree->root` recursively is the matching layout.

### [2026-05-11] Nightreign E2E Helper
- Nightreign helper mirrors ER almost exactly; the only data-path change is lowercase `data0.bhd` / `data0.bdt` under `C:/Games/ELDEN RING NIGHTREIGN/Game`.
- A local `k_oodle_dir` constant avoids CMake string-escaping issues while keeping the build-time `SF_E2E_OODLE_DIR` define harmless.
- Smoke coverage can stay minimal: availability probe, init idempotence, and shutdown idempotence are enough for the helper target.

### [2026-05-11] MSB Header Surface
- `sf_msb.h` can be kept header-only with opaque forward declarations plus shared kind enums.
- `sf_math.h` is not C++-safe by itself because of raw `_Static_assert` usage; `sf_msb.h` needs a local `_Static_assert`→`static_assert` shim when included from C++.
- Current smoke verification: MinGW C build and C++ header-only compile both pass after the shim.

### [2026-05-11] Phase 4 Status Sync
- Phase 4 verification result: 20/20 PASS across 20 test binaries.
- Status tables aligned to mark Phase 4 as done and Phase 5 as in progress.
- PLAN Phase 4 checkboxes were fully closed out; Phase 5 estimate moved to 3 weeks.

### [2026-05-11] Phase 4 Status Sync
- Phase 4 test evidence: 20/20 PASS across 20 test binaries.
- `ctest --test-dir build-mingw -L 'param|script'` log uses `Start N:` lines, so counting test binaries requires matching those lines rather than `^test [0-9]+`.

### [2026-05-11] ESD Path Correction
- ESD lives in `SoulsFormats/Formats/ESD.cs` (single file), not `SoulsFormats/Formats/ESD/ESD.cs`.
- Inner classes `State`, `Condition`, and `CommandCall` are defined in the same file at lines 435, 584, and 742.
- Upstream file length: 875 lines.
- Synchronized Phase 5 roadmap with actual scope (ESD + MSB family).
- Removed EMEVD from Phase 5 scope as it was completed in Phase 4.
- Updated file structure to include per-variant MSB directories.
- Added 4-game e2e matrix to QA scenarios.
- Cleaned up em dashes and en dashes in roadmap files to follow anti-slop rules.

### [2026-05-11] ER e2e Data0 shard correction
- Hardcoded `/chr/c0000.chrbnd.dcx` paths are brittle for ER Data0-based e2e tests because chr archives live in later shards.
- Candidate-scan helpers keep tests stable across patch variations while still exercising real Data0 content.
- Valid Data0 anchors for this build: event EMEVD, msgbnd, and split-archive `.tpfbhd/.tpfbdt` pairs.

### [2026-05-11] T12: Sekiro helper (multi-shard BHD5 traversal)
- Sekiro dvdbnd layout: `C:/Games/Sekiro/Data{1..5}.bhd` + `.bdt` (no `Data0`, no `Game/` subdir — unlike ER).
- Mirror er_test_helper pattern: lazy singleton, `atexit` shutdown, soft-fail init, strict `is_available()`.
- Init must be tolerant: succeed when at least one shard opens cleanly so partial installs + incomplete key tables still allow extraction from working shards. Strict `is_available()` (all 5 files present on disk) keeps smoke skip clean.
- Extract entry point fans the path hash across every open shard; SF_ERR_NOT_FOUND propagates only when all shards return not-found.
- DCX unwrap remains the helper's responsibility (matches ER pattern); `sf_bhd5_extract_by_path` returns raw BHD payload bytes, not decompressed.
- Empirical finding: with the currently shipped Sekiro RSA key (UXM `SekiroKeys["Data1"]`), all 5 shards return `SF_ERR_OUT_OF_RANGE` from `rsa_unwrap_bhd5` — chunk_size > 255 after decrypt indicates key mismatch. This matches the `bhd5_keys.c` comment ("T10 will add per-archive variants"). Helper degrades gracefully to IGNORE-everything until per-shard keys are added.
- Compile-definition escaping is double-backslash-hell: write `\\\\\\\\wsl.localhost\\\\Ubuntu` in CMake to land `\\wsl.localhost\Ubuntu` in the C macro — same pattern ER uses.
- `sf_free` lives in `sf_io.h`, not `sf_common.h`; both helper.c and smoke test need the include.

### [2026-05-11] T8 — MSB list-of-lists skeleton

#### MSB on-disk layout (confirmed by walking MSBS.cs:Read+Write)
```
[MSB magic 16 bytes]   "MSB " + i32(1) + i32(0x10) + bool(false) + bool(false) + u8(1) + u8(0xFF)
[Param 0 header]
    i32 version
    i32 offsetCount        (== entries.Count + 1)
    i64 nameOffset         (-> UTF-16 string, padded to 8)
    i64 entryOffsets[offsetCount - 1]
    i64 nextParamOffset    (absolute offset of next Param header; 0 = end)
[Param 0 name + entries]
[Param 1 header] ...
```
- The list-of-lists is a LINKED chain via nextParamOffset, NOT a table.
- Final param's nextParamOffset must be 0; upstream MSBS:109 checks `br.Position != 0` after reading the last param.

#### Skeleton API design choices
- `msb_layout_t.data_offset` = where the entry-offsets table STARTS (right after the 16-byte param-header preamble). For 0-entry lists this equals position of nextParamOffset.
- `msb_common_reserve_list` writes a zero-entry placeholder header AND the UTF-16 name. Reserves only the nextParamOffset slot (name_offset is filled inline).
- Reserve names use distinct prefixes: `MsbNameOff<id>` and `MsbNextList<id>` to allow simultaneous concurrent reservations.
- Absolute seeking on the reader uses `sf_istream_seek(sf_binary_reader_stream(r), pos)` — there's no public `seek` API on the reader itself (only step_in/step_out which is stack-based).

#### Pitfalls
- `step_in` PUSHES position; pairing with subsequent absolute jumps requires step_out to balance. For walking a chain without nesting, use stream-level seek directly.
- `sf_binary_reader_get_utf16` reads until 0x0000 terminator, allocates from reader's allocator, caller frees via `sf_free(allocator, ptr)`.
- For 0-entry lists, the `entry_count * 8` skip is zero — handle this trivially.

#### Verification
- `nm libsouls_formats.a | grep msb_common_ | grep " T "` → 7 symbols (≥6 required).
- `grep -c "msb_common_reserve" src/map/msb_common.c` → 1; `grep -c "msb_common_fill"` → 1 (matched pair).
- `ctest -R msb_common_skeleton` → 1/1 PASS (round-trip + bad-magic rejection).
- Full suite: 56/56 PASS (no regressions).

### [2026-05-11] T15 MSBS ModelParam

- Upstream `MSBS/ModelParam.cs` defines exactly 5 Sekiro model type IDs: MapPiece `0`, Object `1`, Enemy `2`, Player `4`, Collision `5`.
- Shared model record fields are relative-offset name, type ID, entry ID, relative-offset SIB path, `InstanceCount`, `Unk1C`, and relative-offset type data.
- Only MapPiece has type data in pinned upstream: 3 booleans, one zero byte, six `float` values (`UnkT04`..`UnkT18`), and trailing zero `int32`. Object, Enemy, Player, and Collision have no subtype payload.
- MSBS param entry offsets and param name offsets are absolute file offsets; per-entry name/SIB/type-data offsets are relative to the model entry start.

### [2026-05-11] T10 ESD reader
- ESD short magic is `fSSL`; long format uses `fsSL`. The first fixed header ends at absolute offset `0x6c`, then the relative data block begins.
- The ESD top-level dictionary uses 4 varints per state group: group ID, state table relative offset, state count, and a repeated state table offset assertion.
- Multi-state groups include one duplicated dummy state immediately after the group states. Header `stateCount` includes those dummies; public group state counts should not.
- `Condition.TargetState` is stored on disk as a relative state offset, not the state ID; resolve it within the owning group after grouping states.
- T10 intentionally keeps evaluator and command argument bytecode as owned raw byte blobs; decoding remains T11.

## MSBE root dispatcher (T14b) — 2026-05-11

### MSBE wire-segment order (6 segments, version 73 = 0x49 everywhere):
1. `MODEL_PARAM_ST` v=73
2. `EVENT_PARAM_ST` v=73
3. `POINT_PARAM_ST` v=73 (regions)
4. `ROUTE_PARAM_ST` v=73
5. `LAYER_PARAM_ST` v=0x49 (= 73, EmptyParam — no typed entry, 0 entries always)
6. `PARTS_PARAM_ST` v=73

### Key differences vs MSBS (T14a):
- 6 segments instead of 8 (no MAPSTUDIO_PARTS_POSE_ST, no MAPSTUDIO_BONE_NAME_STRING)
- LAYER version is 0x49 (Read body in MSBE.cs replaces the constructor's 0x23 with 0x49)
- All other params are version 73 (vs MSBS's mix of 35 for params and 0x23 for layer)
- Layer is NOT exposed as a typed sub-param in the public API (sf_msbe.h has no `sf_msbe_layer_t`)
- No bone-names / parts-poses params at all

### Pattern notes:
- Direct port of msbs.c with simpler structure (no layer hook needed)
- Reservation key prefix changed from "Msbs..." to "Msbe..." to avoid collision
- Default versions for fresh-construction (matching upstream MSBE ctor defaults): all 73 except layer which is 0x23 fresh / 0x49 after Read — we use 0x49 since that's what Read() body sets and the round-trip test must match

## [2026-05-11] MSBVI Root Dispatcher (T14c)

### MSBVI vs MSBE divergence
- MSBVI has 6 segments in same order as MSBE: Models, Events, Regions, Routes, Layers, Parts
- **Critical**: All 6 params (including LayerParam) use upstream `base(52, "..._PARAM_ST")` — version 52, NOT version 73/0x49 like MSBE
- LayerParam is a typed `Param<Layer>` (AC6 ships real layer entries); MSBE's LayerParam is EmptyParam
- Dispatcher must route Layer to `msbvi_layer_param_read/write`, never expect_zero_entries

### Pattern reuse
- T14b (msbe.c) is the perfect template for T14c; only diffs are:
  - All version constants → 52 (MSBVI uses uniform 52, MSBE mixes 73 and 0x49)
  - Add typed `sf_msbvi_layer_t` opaque type + accessors
  - Layer dispatch case uses param_read instead of expect_zero
  - Reservation key prefix "Msbvi" instead of "Msbe"

### Build observation
- Pre-existing untracked task: `src/script/esd_bytecode.{c,h}` + `tests/script/test_esd_bytecode.c` are in-flight by a separate task and currently fail to build (`nodes`/`node_count` API mismatch). Not blocking MSBVI work.

## 2026-05-11 — MSBS EventParam implementation
- Implemented Sekiro EventParam with upstream EventType values 4,5,7,9,14,15,17,18,20,21,22,23,24,0xFFFFFFFF.
- Event entries follow the ModelParam relative-offset layout: name, base data, optional type data; PartsGroup and Other have no type data.
- Synthetic root round-trip uses all 14 Sekiro event subtypes and validates internal fields after sf_msbs_write_to_memory -> sf_msbs_read_from_memory.
## [2026-05-11] MSBS PointParam
- MSBS PointParam uses the same param header convention as Model/Event: second int is `entry_count + 1`, followed by name offset, entry offsets, then next-list offset.
- Region entry base layout matches upstream `PointParam.Region`: UTF-16 name, type, id, shape type, position/rotation, `Unk2C`, two counted `short` lists, activation part index, entity ID, optional shape data, optional type data.
- `SoundSpaceOverride`, `PartsGroupArea`, and `AutoDrawGroupPoint` require 8-byte padding before type data, matching upstream writer behavior.

## [2026-05-11] MSBS PartsParam
- Sekiro PartsParam has eight part types: MapPiece, Object, Enemy, Player, Collision, DummyObject, DummyEnemy, ConnectCollision.
- Part entries use relative offsets for UTF-16 name/SIB, optional Unk1/Unk2/Gparam/SceneGparam/Unk7 blocks, mandatory entity data, and mandatory type data.
- Entity data writes an extra 8-byte alignment pad after its trailing 0x10 zero pattern; preserving this is required for byte-exact round trips.
- Optional block presence by type: Unk1 on MapPiece/Object/Enemy/Collision; Unk2 on Collision/ConnectCollision; Gparam on MapPiece/Object/Enemy/Collision/DummyObject/DummyEnemy; SceneGparam only Collision; Unk7 only MapPiece.

## RouteParam (Sekiro MSBS) — 2026-05-11

- RouteParam entry layout is uniform across both subtypes (MufflingPortalLink=3, MufflingBoxLink=4):
  i64 name_offset, i32 unk08, i32 unk0c, u32 type, i32 id, 0x68 bytes of zero pattern, name UTF-16, pad 8.
- Read entry: peek type at `pos + 0x10` per upstream `RouteParam.ReadEntry`; route header is fixed so no
  type-data sub-section is needed.
- The route struct (`msbs_route_t` in msbs_internal.h) holds type+name+unk08+unk0c only; no per-type
  union members because Route subtypes carry no extra fields.
- Param header pattern used: `i32 version=35, i32 entry_count+1, i64 name_offset, N×i64 entry offsets,
  i64 next_param_offset` — same as model_param.c.
- Write path uses `sf_binary_writer_write_pattern(w, 0x68, 0x00)`; matches `assert_pattern` on read side.
- 9 map tests now pass including the new `msbs_route_param_synthetic` (3 subtests: portal link, box link, empty).

## 2026-05-12 — MSBE sub-param implementation notes
- MSBE root writer must let non-empty sub-param writers emit full Param headers because entry-offset slots sit between nameOffset and nextParamOffset.
- MSBE Model/Event/Point/Parts/Route synthetic tests now cover root write→read paths for representative ER subtypes and validate count/type/name round-trips.
- clangd diagnostics could not run in this environment because clangd is not installed; MinGW targeted builds and CTest were used for verification.

## 2026-05-12 — MSBVI sub-param implementation

- MSBVI root order is `MODEL`, `EVENT`, `POINT`, `ROUTE`, `LAYER`, `PARTS`, all version 52.
- Unlike MSBE, MSBVI `LAYER_PARAM_ST` is a typed param: entries are `{ nameOffset, unk08, 0, unk10, unk14, UTF-16 name, pad8 }` from upstream `LayerParam.cs`.
- Empty synthetic fixtures exercise all six sub-param dispatch hooks through `sf_msbvi_write_to_memory`/`sf_msbvi_read_from_memory`; AC6 real-data tests remain unavailable in this environment.

## T36-T39: ESD/MSBE/MSBS real-game e2e (2026-05-12)

- All four e2e tests use the existing helper singletons (er_test_helper,
  nightreign_test_helper, sekiro_test_helper) which auto-unwrap outer DCX.
- talkesdbnd.dcx is a BND4 archive wrapping `.esd` files. Pipeline:
  er_extract_from_data0 → sf_bnd4_read_from_memory → scan files for `.esd`
  suffix → sf_esd_read_from_memory → assert state_group_count > 0.
- In this environment, mapstudio MSBs live in Data2/data2 (not Data0).
  The er/nightreign helpers only open Data0, so the ER/NR MSBE tests
  IGNORE gracefully. This matches the probe note in
  `tests/probes/probe_nightreign_msb.c:293`.
- ESD/MSBE/MSBS test labels: `e2e_er`, `e2e_nightreign`, `e2e_sekiro`.
- `sf_free` lives in `sf_io.h`, not `sf_common.h`. The pattern `sf_free(NULL, ptr)`
  uses the default allocator implicitly.
