# API Realignment Issues & Gotchas

## Known Gotchas
- PLAN.md §12 already has §12附录A and §13下一步 — DO NOT introduce new §12, extend in-place
- EncryptERNRRegulation uses EldenRing key instead of Nightreign — faithfully port this quirk
- FLVER2 files are under FLVER/FLVER2/ NOT at root FLVER2/
- PARAM files are under Formats/PARAM/PARAM/ NOT Formats/PARAM.cs
- BHD5.cs is at root level NOT under Binder/

## Build Notes
- Phase 2 is complete: 66 tests across 13 binaries (stale AGENTS.md says 49/49 across 5)
- DLL exports currently ~137 sf_* symbols (stale; need actual count after build)
- Regulation.bin test: conditional on /mnt/c/Games/ELDEN RING/Game/regulation.bin existing
- Oodle KRAK test: conditional on ~/dev/oodle/ containing oo2core_*_win64.dll

## Evidence Directory
- All QA evidence goes to .sisyphus/evidence/task-<N>-<slug>.txt

## Task 2 Issues (2026-05-10)
- clangd can hold stale diagnostics across `lsp_diagnostics` calls even after `touch` and a fresh object rebuild. Workaround: cross-check with `ast_grep_search` + actual `cmake --build`.
- `edit` tool occasionally produces partially-mangled output when the oldString/newString pair contains a long block of similar lines (e.g. nine 4-row hex arrays). Workaround: replace the whole file with `write` instead of incremental edits when blocks are large.
- DCX zstd hash is bound to vendored zstd version (currently 1.5.7 via CPM). Any zstd version bump WILL invalidate `GOLDEN_DCX_ZSTD_COMPRESS_64`. Same applies to zlib-ng for `GOLDEN_DCX_DFLT_COMPRESS_64`. Decompress hash and round-trip identity remain stable.

## F5 Cross-Doc Consistency Review (2026-05-10)
- Phase/test-count drift remains: AGENTS.md says Phase 2 is 13/13 across 13 binaries, roadmap README says 10/10, while `ctest --test-dir build-mingw -N` lists 17 tests.
- `docs/api-mapping/drift-checklist.md` still has 3 unchecked items: Math static asserts and two SL2Decryptor public API/key getter entries.
- Version drift remains: `0.2.0` is present in CMakeLists.txt, sf_common.h, and CHANGELOG.md but absent from AGENTS.md; `0.1.0` is absent from CMakeLists.txt and sf_common.h.

## 2026-05-10 - F5 re-run cross-doc consistency check

- F5 check mostly consistent, but sampled upstream references resolved 16/20 rather than 20/20 because `docs/api-mapping/format-flver2.md` currently has no `FLVER2.cs:<line>` entries for the requested sample command.
- Orphaned old exported names check passed: no removed symbols matched in `build-mingw/libsouls_formats.dll`; new replacement exports were present.
