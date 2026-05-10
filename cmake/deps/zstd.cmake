# Zstandard (Meta). Activated in Phase 2.
include_guard(GLOBAL)

CPMAddPackage(
    NAME zstd
    VERSION 1.5.7
    GITHUB_REPOSITORY facebook/zstd
    SOURCE_SUBDIR build/cmake
    OPTIONS
        "ZSTD_BUILD_PROGRAMS OFF"
        "ZSTD_BUILD_TESTS OFF"
        "ZSTD_BUILD_STATIC ON"
        "ZSTD_BUILD_SHARED OFF"
        "ZSTD_LEGACY_SUPPORT OFF"
)
