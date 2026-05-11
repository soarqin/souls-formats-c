# AGENTS.md — souls-formats-c project context

> Read this before doing **anything** in this repo. It tells you what is real,
> what is in flight, and where to look next.

---

## 1. What this project is

Pure C library that reads and writes the binary file formats used by
**FromSoftware** game engines (Sekiro, Elden Ring, Nightreign, Armored Core
VI in v1; older titles deferred to v2+). Hand-port of the C# library
[SoulsFormatsNEXT](https://github.com/soulsmods/SoulsFormatsNEXT). Not a
literal translation — the API is C-idiomatic with explicit allocators,
result codes, and opaque pointer types.

* **License:** GPL-3.0 (inherited from upstream — non-negotiable).
* **Platform:** Windows-only, x86_64. Built via MinGW-w64 cross from WSL2 or
  natively via MSVC on Windows.
* **Why Windows-only:** DCX_KRAK compression requires the official
  `oo2core_{6,8,9}_win64.dll` shipped with FromSoft games; we cannot
  redistribute it and the upstream community has no portable replacement.

---

## 2. Current status

| Phase | Title | State | Tests |
|---|---|---|---|
| 0 | Project scaffolding (CMake, CPM, CI, smoke) | ✅ done | 4/4 PASS |
| 1 | Runtime (IO, encoding, math, hash) | ✅ done | 5/5 PASS across 5 binaries (verified 2026-05-10) |
| 2 | Compression + crypto (DCX, AES, Oodle) | ✅ done | 13/13 PASS across 13 binaries (verified 2026-05-10) |
| 3 | Archive containers (BND/BXF/BHD5/TPF/ENFL) | ✅ done | 32/32 PASS across 12 binaries (verified 2026-05-10) |
| 4 | Param + text (PARAM/PARAMDEF/PARAMTDF/FMG) | ✅ done | 20/20 PASS across 20 test binaries |
| 5 | Script + map (EMEVD/ESD/MSB*) | ✅ done | 5/5 PASS across 32 test binaries (verified 2026-05-12) |
| 6 | Geometry + material (FLVER2/MTD/MATBIN) | 🚧 in progress | — |
| 7 | Animation + effects (TAE/FXR3) | ⏳ optional / v1.1 | — |
| v2 | Legacy games (DS1/DS2/DS3/BB/DeS) | ⏳ post v1 GA | — |

**Current artifacts** (after `cmake --build build-mingw`):
* `libsouls_formats.a` — static lib
* `libsouls_formats.dll` + `libsouls_formats.dll.a` — shared lib + import lib (`<DLL exports — verify with: x86_64-w64-mingw32-objdump -p libsouls_formats.dll | grep -c 'sf_'>` `sf_*` symbols exported)
* `tests/souls_formats_test_*.exe` — 13 unit test runners

Run all tests: `ctest --test-dir build-mingw --output-on-failure`.

---

## 3. How to build

### Daily dev loop (WSL2 + MinGW-w64 cross)
```bash
cmake -B build-mingw -G Ninja \
    --toolchain cmake/toolchain-mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```
The produced `.exe` files run directly via WSL interop (binfmt_misc); no
emulator needed. Real Windows kernel + Win32 APIs.

### Canonical / release build (Windows host + MSVC)
```powershell
cmake -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

### Sanitizer build (clang-cl or MinGW)
```bash
cmake -B build-asan -G Ninja \
    --toolchain cmake/toolchain-mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Debug -DSF_ENABLE_SANITIZERS=ON
```

---

## 4. Where things live

```
souls-formats-c/
├── AGENTS.md                       ← you are here
├── README.md                       ← user-facing summary
├── LICENSE                         ← GPL-3.0
├── CMakeLists.txt                  ← top-level build
├── .clangd                         ← clangd config (cross-compile aware)
├── .clang-format                   ← LLVM base + 4 spaces + col 100
├── .sisyphus/plans/
│   └── PLAN.md                     ← canonical Momus-audited project plan
├── docs/
│   └── roadmap/                    ← per-phase implementation guides
│       ├── README.md               ← phase index
│       ├── phase-2-compression-crypto.md
│       ├── phase-3-archive-containers.md
│       ├── phase-4-param-text.md
│       ├── phase-5-script-map.md
│       ├── phase-6-geometry-material.md
│       ├── phase-7-animation-effects.md
│       └── post-v1.md
├── cmake/
│   ├── CPM.cmake                   ← v0.42.0 vendored
│   ├── toolchain-mingw-w64.cmake   ← WSL2 cross toolchain
│   ├── compiler_warnings.cmake     ← /W4 /WX  + -Wall -Wextra -Werror …
│   ├── sanitizers.cmake            ← ASan/UBSan opt-in
│   └── deps/                       ← CPM package recipes per third-party lib
├── include/souls_formats/
│   ├── souls_formats.h             ← umbrella
│   ├── sf_common.h                 ← errors, allocator, SF_API
│   ├── sf_math.h                   ← POD vec/quat/mat
│   ├── sf_io.h                     ← stream + binary reader/writer
│   ├── sf_encoding.h               ← Shift-JIS / UTF-16 / UTF-8
│   └── sf_hash.h                   ← FromPath hash
├── src/
│   ├── core/                       ← Phase 1 deliverables (DONE)
│   │   ├── error.c
│   │   ├── stream.c
│   │   ├── binary_reader.c
│   │   ├── binary_writer.c
│   │   ├── encoding_win32.c
│   │   └── filename_hash.c
│   └── internal/sf_internal.h      ← internal-only helpers
├── tests/
│   ├── CMakeLists.txt              ← `sf_add_test()` helper
│   ├── unity_runner.c              ← Phase 0 smoke + BCrypt sanity
│   └── core/
│       ├── test_filename_hash.c
│       ├── test_encoding.c
│       ├── test_binary_reader.c
│       └── test_binary_writer.c
└── .github/workflows/ci.yml        ← MSVC + clang-cl + MinGW (msys2) + Ubuntu cross
```

Upstream reference checkout: `/home/soar/src/SoulsFormatsNEXT` (read-only).
Test data: `/mnt/c/Games/ELDEN RING`, `~/dev/oodle/`, `~/dev/paramdex/` —
see [`PLAN.md` §8.4](.sisyphus/plans/PLAN.md) for the exact contract.

---

## 5. API conventions (DO NOT DRIFT)

Every public function follows these rules. Drift = breakage of consumer
ABI and downstream binding generators.

* **Prefix:** every public symbol starts with `sf_`. Every public type ends
  with `_t`. Every constant is `SF_<CATEGORY>_<NAME>`.
* **Error path:** all fallible APIs return `sf_result_t`. Output via
  pointer parameters. NEVER use return-by-value to signal failure.
* **Memory:** every "create" API takes `const sf_allocator_t *alloc` (pass
  `NULL` for default malloc/free). The created object remembers its
  allocator and uses it for all internal allocations and the eventual
  `_destroy`. Strings produced by reader APIs are heap-owned by the
  caller and freed via `sf_free(allocator, ptr)`.
* **Strings:** UTF-8 everywhere on the boundary. Internal Win32 calls use
  `MultiByteToWideChar` / `WideCharToMultiByte` to bridge. Shift-JIS is
  CP 932; UTF-16 is LE on Win32 by default; BE is byte-swap-on-the-fly.
* **Endianness:** every binary reader/writer instance carries its own
  mutable `big_endian` flag. Public types are always already host-endian
  by the time the caller sees them.
* **Opacity:** all public types are forward-declared opaque pointers.
  Layouts can change between releases; users may not assume sizeof.
* **`SF_API`:** every public symbol declared in a public header must be
  decorated with `SF_API`. The DLL build defines `SF_BUILD_DLL` so the
  same headers do the right thing for static + shared consumers.
* **`_Static_assert`:** add one after every enum table to catch drift.
  See `src/core/error.c` for the canonical pattern.

Refer to [`PLAN.md` §5](.sisyphus/plans/PLAN.md) for the canonical
description; this file is just a quick reminder.

### 5.x STRICT UPSTREAM REFERENCE / API MIRRORS UPSTREAM

Two mandatory rules that apply to **every** code change in this repo:

1. **STRICT UPSTREAM REFERENCE**: Every code implementation must strictly
   reference upstream code at the pinned commit (see
   [`docs/api-mapping/UPSTREAM.md`](docs/api-mapping/UPSTREAM.md)).
   Guessing at semantics, signatures, or wire formats is FORBIDDEN.
   When in doubt, read the `.cs` file.

2. **API MIRRORS UPSTREAM**: Public C API design must mirror upstream as
   closely as possible. Minor C-style adjustments (out-param error returns,
   pointer-based ownership, snake_case) are explicitly allowed. Functional
   differences are FORBIDDEN. Every divergence must be either (a) a
   documented C-style adaptation in
   [`docs/api-mapping/POLICY.md`](docs/api-mapping/POLICY.md), or (b) a
   tracked extension in
   [`docs/api-mapping/extensions.md`](docs/api-mapping/extensions.md).

See also: [`docs/api-mapping/README.md`](docs/api-mapping/README.md) for
the full upstream→ours mapping table index.

---

## 6. Workflow for the next phase

1. **Open the matching phase doc** in [`docs/roadmap/`](docs/roadmap/). It
   contains the exact deliverables, file paths, upstream references, and
   QA expectations for that phase.
2. **Re-read the relevant upstream `.cs` files** under
   `/home/soar/src/SoulsFormatsNEXT/SoulsFormats/`. The phase doc lists
   which ones.
3. **Write public headers first** (`include/souls_formats/sf_*.h`), then
   sources, then tests. Keep the API conventions above religiously.
4. **Update `CMakeLists.txt`** `SF_PUBLIC_HEADERS` and `SF_SOURCES` lists.
5. **Add tests under `tests/<area>/`**, register via `sf_add_test()` in
   `tests/CMakeLists.txt`. Pick a label matching the phase: `compression`,
   `crypto`, `archive`, `param`, `script`, `map`, `geom`, `anim`.
6. **Run `cmake --build build-mingw && ctest -L <label>`** until green.
7. **Mark checkboxes complete** in `PLAN.md` Phase section, with timestamps
   and concrete test counts.
8. **Run Momus** if the plan was modified materially:
   `task(subagent_type="momus", prompt=".sisyphus/plans/PLAN.md", ...)`.

---

## 7. Hard constraints (NEVER violate)

* **Do not** copy / vendor / commit any Oodle DLL or any FromSoftware game
  byte from `/mnt/c/Games/`. Test data is referenced by hardcoded paths,
  never embedded.
* **Do not** suppress type errors with `-Wno-...` per-file. Fix at the
  source. The whole project is `-Werror`.
* **v0.x ABI breaks ARE permitted** but require, in the same commit (or
  contiguous PR): (a) every removed or renamed symbol recorded in
  `CHANGELOG.md`, (b) every test that referenced the symbol updated, (c)
  version bump 0.x.y → 0.(x+1).0 (minor, not patch).
* **Do not** start a new format module before its dependencies are
  green. Phase 3 cannot begin before Phase 2 lands; Phase 4-6 e2e tests
  cannot run before Phase 3's `er_extract_from_data0` helper exists.
* **Do not** leave reservations open in `sf_binary_writer_t`. Always pair
  every `Reserve_*` with exactly one `Fill_*` of matching kind, and call
  `sf_binary_writer_finish()` to verify before destroy.
* **Do not** read or write paths via stdio (`fopen`). Use `sf_istream_*` /
  `sf_ostream_*`, which take wide / UTF-8 paths and call Win32 directly.

---

## 8. Pointers

* Strategic plan: [`.sisyphus/plans/PLAN.md`](.sisyphus/plans/PLAN.md)
* Phase index: [`docs/roadmap/README.md`](docs/roadmap/README.md)
* Upstream source (read-only): `/home/soar/src/SoulsFormatsNEXT`
* Format inventory: [`SoulsFormatsNEXT/FORMATS.md`](file:///home/soar/src/SoulsFormatsNEXT/FORMATS.md)
* Upstream README: [`SoulsFormatsNEXT/README.md`](file:///home/soar/src/SoulsFormatsNEXT/README.md)
