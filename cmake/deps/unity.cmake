# Unity test framework (ThrowTheSwitch). Active in Phase 0.
include_guard(GLOBAL)

CPMAddPackage(
    NAME Unity
    VERSION 2.6.1
    GITHUB_REPOSITORY ThrowTheSwitch/Unity
    OPTIONS
        "UNITY_EXTENSION_FIXTURE OFF"
        "UNITY_EXTENSION_MEMORY OFF"
        "BUILD_FIXTURE OFF"
)

# Unity exposes the `unity` target. Sanity check.
if(NOT TARGET unity)
    message(FATAL_ERROR "Unity package fetched but `unity` target missing")
endif()

# We need IEEE 754 double assertion support (TEST_ASSERT_EQUAL_DOUBLE).
# Both the unity library and its consumers must see UNITY_INCLUDE_DOUBLE,
# so propagate it via the target's PUBLIC interface.
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
