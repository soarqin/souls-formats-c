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
