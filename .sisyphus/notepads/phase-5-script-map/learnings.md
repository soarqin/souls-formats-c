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
