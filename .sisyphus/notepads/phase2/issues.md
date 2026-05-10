# Phase 2 Issues

## [2026-05-10] Atlas initialization

### Known risks:
1. BCryptDecrypt requires writable IV buffer — always copy IV to stack scratch before passing
2. Oodle compress is non-deterministic at level 9 — tests must compare decompress(compress(x))==x, NOT compress(x)==fixed_bytes
3. LoadLibraryExW security — use LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR flags
4. Oodle DLL ABI: all 3 versions (v6/v8/v9) have same ABI for our 5 functions — no per-version vtable needed
5. zlib-ng in ZLIB_COMPAT mode: header is <zlib.h> — BUT when using CPM, the header might be <zlib-ng.h> if compat mode has issues. Check CPM options carefully.
6. ZSTD streaming vs block: use block API (ZSTD_decompress / ZSTD_compress) NOT streaming — DCX always knows uncompressed size
7. DCP_EDGE chunk table: CompressedSize field in "DCS\0" section includes padding between chunks; individual chunk sizes do not include padding
8. DCX_EDGE CompressedSize fill: upstream fills with sum of chunk sizes WITHOUT padding (not consistent with DCP_EDGE)
9. regulation.bin decryption: NoPadding mode — raw CBC, upstream manually pads input if not block-aligned (adds blockSize - remainder zeros)

## [2026-05-10] Sisyphus-Junior implementation pass

1. clangd reports a MinGW/GCC intrinsic false-positive (`definition of builtin function '__rdtsc'`) when parsing files that include `<windows.h>`; the same diagnostic appears in existing Phase 1 Win32 files, while the MinGW build is clean under `-Werror`.
2. The Oodle loader currently uses manual Win32 declarations to avoid pulling `<windows.h>` into that file; keep declarations minimal if more loader APIs are added.

## [2026-05-10] Regulation test correction

1. The external ER regulation fixture should only ignore when the file is absent. If AES decrypt or DCX_ZSTD unwrap fails, the test must fail because the fixture path exists and the code path is fully supported without Oodle.

## [2026-05-10] Unified DCX compression params

1. Header-only LSP diagnostics in `include/` currently miss the project include path and report false `sf_common.h`/`SF_API` errors for multiple public headers. Source/test diagnostics that compile through `compile_commands.json` are clean aside from the known `<windows.h>`/`__rdtsc` false-positive.
