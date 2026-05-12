cmake_minimum_required(VERSION 3.24)

project(souls_formats_c
    VERSION 0.4.0
    DESCRIPTION "Pure C library for FromSoftware game file formats"
    HOMEPAGE_URL "https://github.com/soar/souls-formats-c"
    LANGUAGES C)

#-----------------------------------------------------------------------------
# Project options
#-----------------------------------------------------------------------------
# BUILD_TESTING is the canonical CTest variable. It defaults to whether this
# project is the top-level project (i.e. standalone dev build), so that
# consumers who pull us in via add_subdirectory() / FetchContent do not
# implicitly build our tests.
#
# SF_BUILD_TESTS is retained as a deprecated backwards-compat alias.
if(DEFINED SF_BUILD_TESTS)
    message(DEPRECATION
        "SF_BUILD_TESTS is deprecated; use -DBUILD_TESTING=ON/OFF instead.")
    set(BUILD_TESTING ${SF_BUILD_TESTS} CACHE BOOL "Build tests" FORCE)
endif()
if(NOT DEFINED BUILD_TESTING)
    set(BUILD_TESTING ${PROJECT_IS_TOP_LEVEL} CACHE BOOL "Build tests")
endif()
include(CTest)

option(SF_BUILD_EXAMPLES    "Build the examples/ tools"           OFF)
option(SF_ENABLE_SANITIZERS "Build with ASan/UBSan (Clang/GCC)"   OFF)

#-----------------------------------------------------------------------------
# Module path & helpers
#-----------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(compiler_warnings)
include(sanitizers)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/CPM.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/deps/zlib-ng.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/deps/zstd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/deps/mxml.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/deps/klib.cmake)

#-----------------------------------------------------------------------------
# Platform guard — Windows only (per project charter)
#-----------------------------------------------------------------------------
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(FATAL_ERROR
        "souls-formats-c is Windows-only (target system is ${CMAKE_SYSTEM_NAME}). "
        "Use cmake/toolchain-mingw-w64.cmake to cross-compile from WSL2 / Linux.")
endif()

#-----------------------------------------------------------------------------
# Default build type
#-----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

#-----------------------------------------------------------------------------
# Source set (shared by static + shared targets)
#-----------------------------------------------------------------------------
set(SF_PUBLIC_HEADERS
    include/souls_formats/souls_formats.h
    include/souls_formats/sf_common.h
    include/souls_formats/sf_math.h
    include/souls_formats/sf_io.h
    include/souls_formats/sf_encoding.h
    include/souls_formats/sf_hash.h
    include/souls_formats/sf_msb.h
    include/souls_formats/sf_msbs.h
    include/souls_formats/sf_msbe.h
    include/souls_formats/sf_msbvi.h
    include/souls_formats/sf_path.h
    include/souls_formats/sf_dcx.h
    include/souls_formats/sf_oodle.h
    include/souls_formats/sf_regulation.h
    include/souls_formats/sf_param.h
    include/souls_formats/sf_paramdef.h
    include/souls_formats/sf_paramtdf.h
    include/souls_formats/sf_fmg.h
    include/souls_formats/sf_emevd.h
    include/souls_formats/sf_esd.h
    include/souls_formats/sf_bhd5.h
    include/souls_formats/sf_binder.h
    include/souls_formats/sf_bnd3.h
    include/souls_formats/sf_bnd4.h
    include/souls_formats/sf_bxf3.h
    include/souls_formats/sf_bxf4.h
    include/souls_formats/sf_enfl.h
    include/souls_formats/sf_tpf.h
    include/souls_formats/sf_flver.h
    include/souls_formats/sf_flver2.h
    include/souls_formats/sf_mtd.h
    include/souls_formats/sf_matbin.h
)

list(APPEND SF_PUBLIC_HEADERS
    include/souls_formats/sf_tae.h
    include/souls_formats/sf_fxr3.h
)

set(SF_SOURCES
    src/core/error.c
    src/core/encoding_win32.c
    src/core/filename_hash.c
    src/core/path.c
    src/core/stream.c
    src/core/binary_reader.c
    src/core/binary_writer.c
    src/core/sf_util.c
    src/compression/dcx.c
    src/compression/deflate_zlibng.c
    src/compression/deflate_chunked.c
    src/compression/zstd_wrap.c
    src/compression/oodle/oodle_loader.c
    src/compression/oodle/oodle_v6.c
    src/compression/oodle/oodle_v8.c
    src/compression/oodle/oodle_v9.c
    src/crypto/aes_cng.c
    src/crypto/md5_cng.c
    src/crypto/regulation.c
    src/crypto/regulation_keys.c
    src/crypto/rsa_cng.c
    src/crypto/sl2.c
    src/archive/bhd5.c
    src/archive/bhd5_keys.c
    src/archive/binder_common.c
    src/archive/sf_binder.c
    src/archive/bnd3.c
    src/archive/bnd4.c
    src/archive/bxf3.c
    src/archive/bxf4.c
    src/archive/enfl.c
    src/archive/tpf.c
    src/archive/tpf_headerizer.c
    src/internal/dds_header.c
    src/param/param.c
    src/param/paramdef.c
    src/param/paramdef_apply.c
    src/param/paramdef_xml_read.c
    src/param/paramtdf.c
    src/text/fmg.c
    src/script/emevd.c
    src/script/emevd_event.c
    src/script/emevd_instruction.c
    src/script/emevd_layer.c
    src/script/emevd_parameter.c
    src/script/esd.c
    src/script/esd_write.c
    src/script/esd_bytecode.c
    src/map/msb_common.c
    src/map/msbs/msbs.c
    src/map/msbs/event_param.c
    src/map/msbs/model_param.c
    src/map/msbs/parts_param.c
    src/map/msbs/point_param.c
    src/map/msbs/route_param.c
    src/map/msbe/msbe.c
    src/map/msbe/event_param.c
    src/map/msbe/model_param.c
    src/map/msbe/parts_param.c
    src/map/msbe/point_param.c
    src/map/msbe/route_param.c
    src/map/msbvi/msbvi.c
    src/map/msbvi/event_param.c
    src/map/msbvi/layer_param.c
    src/map/msbvi/model_param.c
    src/map/msbvi/parts_param.c
    src/map/msbvi/point_param.c
    src/map/msbvi/route_param.c
    src/geom/flver_common.c
    src/geom/flver2_vertex.c
    src/geom/flver2_decode.c
    src/geom/flver2.c
    src/geom/flver2_faceset.c
    src/geom/flver2_material.c
    src/geom/flver2_mesh.c
    src/geom/flver2_skeleton.c
    src/geom/flver2_vertex_buffer.c
    src/geom/mtd.c
    src/geom/matbin.c
)

list(APPEND SF_SOURCES
    src/effects/tae.c
    src/effects/fxr3.c
    src/effects/fxr3_xml_read.c
    src/effects/fxr3_xml_write.c
)

if(TARGET libzstd_static)
    set(SF_ZSTD_TARGET libzstd_static)
elseif(TARGET zstd::libzstd_static)
    set(SF_ZSTD_TARGET zstd::libzstd_static)
else()
    message(FATAL_ERROR "Could not find libzstd_static target")
endif()

#-----------------------------------------------------------------------------
# Helper: configure a souls_formats target with shared settings
#-----------------------------------------------------------------------------
function(_sf_configure_library target)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/compression
            ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
            ${CMAKE_CURRENT_SOURCE_DIR}/src/archive
    )
    target_compile_features(${target} PUBLIC c_std_11)
    target_link_libraries(${target} PRIVATE bcrypt crypt32 zlib ${SF_ZSTD_TARGET} mxml_static)
    sf_apply_warnings(${target})
    sf_apply_sanitizers(${target})
endfunction()

#-----------------------------------------------------------------------------
# souls_formats_static — the canonical / link-everywhere target
#-----------------------------------------------------------------------------
add_library(souls_formats_static STATIC ${SF_SOURCES})
add_library(souls_formats::static ALIAS souls_formats_static)
add_library(souls_formats ALIAS souls_formats_static)
_sf_configure_library(souls_formats_static)
set_target_properties(souls_formats_static PROPERTIES OUTPUT_NAME souls_formats)

#-----------------------------------------------------------------------------
# souls_formats_shared — DLL build with __declspec(dllexport)
#-----------------------------------------------------------------------------
add_library(souls_formats_shared SHARED ${SF_SOURCES})
add_library(souls_formats::shared ALIAS souls_formats_shared)
_sf_configure_library(souls_formats_shared)
set_target_properties(souls_formats_shared PROPERTIES
    OUTPUT_NAME              souls_formats
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(souls_formats_shared
    PUBLIC  SF_BUILD_SHARED
    PRIVATE SF_BUILD_DLL
)

#-----------------------------------------------------------------------------
# Tests (only when standalone / top-level)
#-----------------------------------------------------------------------------
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()

#-----------------------------------------------------------------------------
# Examples (only when standalone / top-level)
#-----------------------------------------------------------------------------
if(SF_BUILD_EXAMPLES AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(examples)
endif()

#-----------------------------------------------------------------------------
# Configuration summary
#-----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "souls-formats-c v${PROJECT_VERSION}")
message(STATUS "  C compiler        : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Target system     : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Build type        : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Top-level project : ${PROJECT_IS_TOP_LEVEL}")
message(STATUS "  Build tests       : ${BUILD_TESTING}")
message(STATUS "  Build examples    : ${SF_BUILD_EXAMPLES}")
message(STATUS "  Sanitizers        : ${SF_ENABLE_SANITIZERS}")
message(STATUS "")
