# 2026-05-11
- Real ER `ItemName.fmg` can be reached through multiple localized `item.msgbnd.dcx` paths; trying a small path list makes the test resilient.
- `sf_fmg_read_from_memory` takes `(out, data, size, alloc)`; the e2e test must pass raw bytes from `er_load_msgbnd_entry` directly.
- The e2e param test needs the shared ER helper sources/include path in CMake, plus a convenience alias target when callers expect `souls_formats_test_*` naming.

## T4.6 — EMEVD e2e against ER (2026-05-11)

- **`-Wlogical-op` catches alias-OR comparisons**: `fmt == A || fmt == B`
  when `A == B` (compile-time alias) triggers `-Werror=logical-op`. The
  `_Static_assert` at `sf_emevd.h:76` already guarantees
  `SF_EMEVD_FORMAT_ELDEN_RING == SF_EMEVD_FORMAT_SEKIRO`, so a single
  equality check suffices for ER/AC6/Nightreign EMEVD files. Document the
  alias relationship in the test comment to head off "why aren't you
  checking the ER constant?" review feedback.
- **`er_extract_from_data0` signature is 3-arg**: `(path, &out, &out_size)`
  — no `sf_allocator_t *` slot. Caller frees via `sf_free(NULL, *out)`.
- **`sf_emevd_read_from_memory` parameter order is `(out, data, size, alloc)`**,
  not the `(data, size, out, alloc)` shape that some task templates show.
  Always cross-check the actual header before transcribing from spec.
- **ER e2e tests SKIP gracefully in WSL mingw when Data0 unreadable**:
  `er_helper_is_available()` returns false if `GetFileAttributesW` on the
  configured `SF_E2E_ELDEN_RING_DIR L"/Game/Data0.bhd"` path returns
  `INVALID_FILE_ATTRIBUTES`. In this dev environment all five existing
  e2e tests (bhd5/bnd4/bxf4/tpf/er_helper_smoke) currently SKIP — my
  test correctly matches this pattern via `TEST_IGNORE_MESSAGE`.
- **CMake e2e wiring requires four pieces**: `target_include_directories`
  for `tests/e2e`, `target_sources` for `er_test_helper.c`,
  `target_compile_definitions` for the three `SF_E2E_*` path macros, and
  (if the spec lists an alias build target) an `add_custom_target` that
  forwards to the `sf_add_test`-created target. Pattern lifted from the
  `param_apply_paramdef_e2e` / `fmg_e2e_er` entries.
- **Always write evidence in both PASS and SKIP branches** when the spec
  lists an evidence file as a deliverable. Use sf_ostream (Win32-backed)
  rather than `fopen` per `AGENTS.md` §7. The SKIP-branch evidence still
  documents the test attempt and propagates the underlying sf_result_t
  for forensic value.
- For example binaries launched from WSL, `/mnt/c/...` and `/home/...`
  argv paths may need normalization before `MultiByteToWideChar` → Win32
  file APIs; a small `/mnt/<drive>/...` and `\wsl$\<distro>\...` mapper
  keeps the CLI usable with both Windows-style and WSL-style paths.
- The current ER `SpEffectParam.param` parses successfully even when
  `sf_param_apply_paramdef` is too strict; TSV dumping can fall back to raw
  cell-by-index rendering while still using Paramdex field names.
