# zlib-ng (compat mode). Activated in Phase 2.
include_guard(GLOBAL)

CPMAddPackage(
    NAME zlib-ng
    VERSION 2.2.4
    GITHUB_REPOSITORY zlib-ng/zlib-ng
    GIT_TAG 2.2.4
    OPTIONS
        "ZLIB_COMPAT ON"
        "ZLIB_ENABLE_TESTS OFF"
        "ZLIBNG_ENABLE_TESTS OFF"
        "WITH_GTEST OFF"
        "BUILD_SHARED_LIBS OFF"
)
