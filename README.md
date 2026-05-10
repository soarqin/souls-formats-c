# souls-formats-c

A pure C library for reading and writing FromSoftware game file formats. A C
port of [SoulsFormatsNEXT](https://github.com/soulsmods/SoulsFormatsNEXT).

> ⚠️ **Status: pre-alpha (v0.2.0)** — Phase 0/1/2 complete. See [CHANGELOG.md](CHANGELOG.md)
> for details and [`.sisyphus/plans/PLAN.md`](.sisyphus/plans/PLAN.md) for the full roadmap.

## Platform

**Windows-only**, x86_64. The library loads the official Oodle DLL
(`oo2core_{6,8,9}_win64.dll`) shipped with FromSoftware games to handle
DCX_KRAK compression, so cross-platform support is not feasible without
those DLLs.

Local development on **WSL2 + Ubuntu 24.04** is fully supported via
`x86_64-w64-mingw32-gcc`; produced PE binaries run natively on the Windows
host through WSL interop.

## Target games (v1)

* Sekiro
* Elden Ring
* Elden Ring Nightreign
* Armored Core VI

Older titles (Demon's Souls, Dark Souls 1/2/3, Bloodborne, AC4/ACFA/ACV/ACVD,
King's Field, etc.) are scheduled for v2+.

## Building

### WSL2 + MinGW-w64 (daily dev loop)

```bash
sudo apt install -y ninja-build mingw-w64 mingw-w64-tools \
    g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix mingw-w64-x86-64-dev
cmake -B build-mingw -G Ninja \
    --toolchain cmake/toolchain-mingw-w64.cmake \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build-mingw
ctest --test-dir build-mingw --output-on-failure
```

### Windows + MSVC (release / canonical)

```powershell
# In a VS 2022 Developer PowerShell:
cmake -B build-msvc -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-msvc
ctest --test-dir build-msvc --output-on-failure
```

## License

[GPL-3.0](LICENSE), inherited from upstream SoulsFormatsNEXT. Tools that
statically link this library must also be GPL-3.0.

**Oodle DLLs are never redistributed**; copy them from your own legally
purchased FromSoftware game install.

## Credits

* TKGP — original SoulsFormats author
* The soulsmods community — SoulsFormatsNEXT maintainers
* The Souls modding community — format reverse engineering
