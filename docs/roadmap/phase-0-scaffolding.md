# Phase 0 — Project Scaffolding

> **Status**: ✅ done (2026-05-10) · **Depends on**: None

## Completion Retrospective

This phase established the build system, CI pipeline, and core project structure. We successfully integrated the CPM dependency manager and configured a cross-compilation toolchain for MinGW-w64.

### Deliverables
* **Build System**: CMake 3.24+ with Ninja generator.
* **Dependency Management**: CPM.cmake for zlib-ng, zstd, mxml, klib, and Unity.
* **CI/CD**: GitHub Actions matrix covering MSVC, clang-cl, and MinGW-w64.
* **Tooling**: Clang-format and EditorConfig for style consistency.
* **Smoke Tests**: Unity-based runner verifying BCrypt availability and basic allocator behavior.

## Alignment Status

Phase 0 focused on infrastructure rather than format implementation. Upstream alignment is not applicable to this phase.

## Lessons Learned

* **Cross-compilation complexity**: Setting up the MinGW-w64 toolchain for WSL2 required careful handling of POSIX thread fallbacks and static linking.
* **CI Matrix efficiency**: Parallelizing builds across different compilers early on helped catch platform-specific warnings before they accumulated.
* **Dependency locking**: Using commit hashes in CPM ensures reproducible builds across different environments.
* **LSP configuration**: Providing a `.clangd` file with cross-compile flags significantly improved the developer experience in WSL2.
