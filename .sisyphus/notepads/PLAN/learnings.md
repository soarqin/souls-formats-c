
## 2026-05-12 — ESD writer
- Implemented `sf_esd_write_to_memory` in a dedicated writer TU. The writer mirrors upstream ordering: header/data header, group table, state headers plus dummy states, condition headers, command headers, arg tables, condition-offset tables, evaluator bytecode, command bytecode, and final offset fills.
- `sf_esd_bytecode_encode` now emits decoded trees back to raw opcode bytes; decode keeps legacy flat `nodes`/`node_count` compatibility used by existing tests.
- Verification: `cmake --build build-mingw`, `ctest --test-dir build-mingw -R esd_write --output-on-failure`, and full `ctest --test-dir build-mingw --output-on-failure` pass. LSP diagnostics could not run because clangd is not installed in this environment.
