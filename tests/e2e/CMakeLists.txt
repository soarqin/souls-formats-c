# Phase 3 e2e tests — registered in tests/CMakeLists.txt.
#
# E2E test binaries link er_test_helper.c directly (not via a library)
# and receive two preprocessor path macros via target_compile_definitions
# in the parent CMakeLists.txt:
#
#   SF_E2E_ELDEN_RING_DIR  → L"C:/Games/ELDEN RING"
#   SF_E2E_OODLE_DIR       → L"\\\\wsl.localhost\\Ubuntu\\home\\soar\\dev\\oodle"
#
# er_test_helper.c also defines #ifndef fallbacks for both macros so the
# file remains compilable in isolation (clangd / IDE indexing).
