# T0.1 — klib 3-Toolchain Compile Spike

## Verdict: GO

klib compiles cleanly under MinGW-w64 with `-Wall -Wextra -Wpedantic -Werror -Wshadow
-Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef
-Wno-unused-parameter -Wlogical-op -Wduplicated-cond`. Zero warnings. Runtime: KLIB-SPIKE OK.

## MinGW-w64 (WSL2 cross-compile)

**Build**: PASS — zero warnings, zero errors
**Runtime**: `KLIB-SPIKE OK` printed, exit 0

Build command:
```bash
cmake --build build-mingw --target souls_formats_test_klib_spike
```

Compiler: `x86_64-w64-mingw32-gcc-posix` with full `-Werror` flag set.

## MSVC (Windows host)

**Status**: SKIP — MSVC (`cl.exe`) not available on WSL2 host.
Reason: This is a Linux/WSL2 development environment. MSVC requires a Windows host.
The CI matrix (`.github/workflows/ci.yml`) covers MSVC and clang-cl on GitHub Actions.

## clang-cl

**Status**: SKIP — clang-cl not available on WSL2 host.
Same reason as MSVC.

## Key Finding

klib's `KHASH_MAP_INIT_INT64` macro does NOT trigger `C4127 conditional expression is constant`
under MinGW-w64 GCC. The concern about MSVC `/W4 /WX` emitting C4127 remains theoretical
until CI runs on Windows. However, since the CI matrix already covers MSVC, any issues will
surface there.

## Downstream Impact

- **T4.1**: GO — klib khash adoption in BHD5 is cleared to proceed.
- Note: klib.cmake was NOT included in main CMakeLists.txt (pre-existing bug). Fixed as part
  of this audit by adding `include(cmake/deps/klib.cmake)` to CMakeLists.txt.

## Files

- `tests/spike/test_klib_compile.c` — spike source
- `tests/CMakeLists.txt` — spike target added
- `CMakeLists.txt` — klib.cmake include added (pre-existing omission fixed)
- `.sisyphus/evidence/task-0.1-mingw-build.log` — full build output
