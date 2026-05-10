# 2026-05-11
- Real ER `ItemName.fmg` can be reached through multiple localized `item.msgbnd.dcx` paths; trying a small path list makes the test resilient.
- `sf_fmg_read_from_memory` takes `(out, data, size, alloc)`; the e2e test must pass raw bytes from `er_load_msgbnd_entry` directly.
- The e2e param test needs the shared ER helper sources/include path in CMake, plus a convenience alias target when callers expect `souls_formats_test_*` naming.
