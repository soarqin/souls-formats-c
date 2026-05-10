# klib — Attractive Chaos's header-only collection (kvec, khash, ...).
# Activated in Phase 1 (containers used internally everywhere).
include_guard(GLOBAL)

CPMAddPackage(
    NAME klib
    GIT_TAG master
    GITHUB_REPOSITORY attractivechaos/klib
    DOWNLOAD_ONLY YES
)

# Expose klib include path as an INTERFACE target so internal sources can
# `#include "khash.h"` etc. without polluting the public include path.
if(klib_ADDED)
    add_library(klib INTERFACE)
    target_include_directories(klib INTERFACE ${klib_SOURCE_DIR})
endif()
