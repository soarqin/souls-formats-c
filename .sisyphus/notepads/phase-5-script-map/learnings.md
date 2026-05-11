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
