cmake_minimum_required(VERSION 3.20)
project(soft_fp
    VERSION 2.0.0
    DESCRIPTION "Portable bit-exact IEEE-754 binary64, binary128, and binary256 library"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED PROJECT_IS_TOP_LEVEL)
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL TRUE)
    else()
        set(PROJECT_IS_TOP_LEVEL FALSE)
    endif()
endif()

# Off-by-default for add_subdirectory / FetchContent consumers.
option(SOFT_FP64_BUILD_TESTS "Build host unit tests"      ${PROJECT_IS_TOP_LEVEL})
option(SOFT_FP64_WERROR      "Treat warnings as errors"   ${PROJECT_IS_TOP_LEVEL})
option(SOFT_FP64_BUILD_EXHAUSTIVE "Build exhaustive binary32 round-trip test" OFF)
option(SOFT_FP_BUILD_FP128   "Build IEEE-754 binary128 support" ON)
option(SOFT_FP_BUILD_FP256   "Build IEEE-754 binary256 support" ON)

add_library(soft_fp64 STATIC)
add_library(soft_fp64::soft_fp64 ALIAS soft_fp64)
add_library(soft_fp::fp64 ALIAS soft_fp64)

target_include_directories(soft_fp64
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Arithmetic + classify + convert + rounding + sqrt/fma, then the
# SLEEF-derived transcendentals (see src/sleef/NOTICE). Partitioned by
# family: trig / exp-log / inv+hyp+pow / long-tail special functions.
target_sources(soft_fp64 PRIVATE
    src/arithmetic.cpp
    src/compare.cpp
    src/convert.cpp
    src/sqrt_fma.cpp
    src/rounding.cpp
    src/classify.cpp
    src/fenv.cpp
    src/sleef/sleef_exp_log.cpp
    src/sleef/sleef_trig.cpp
    src/sleef/sleef_inv_hyp_pow.cpp
    src/sleef/sleef_special.cpp
)

# Optional OpenCL C compatibility ABI. The strict `sf64_*` surface always
# preserves subnormals; this additive surface emits `sf64_ocl_*` wrappers and
# OpenCL-style `sf64_native_*` entry points. FTZ is scoped to that surface.
set(SOFT_FP64_OCL "off" CACHE STRING
    "Build OpenCL C compatibility ABI: off | on")
set_property(CACHE SOFT_FP64_OCL PROPERTY STRINGS off on)
set(SOFT_FP64_OCL_VALUE 0)
set(SOFT_FP64_FTZ "off" CACHE STRING
    "Flush binary64 subnormal inputs/outputs to signed zero on the OpenCL ABI: off | on")
set_property(CACHE SOFT_FP64_FTZ PROPERTY STRINGS off on)
if(SOFT_FP64_FTZ STREQUAL "off")
    set(SOFT_FP64_FTZ_VALUE 0)
elseif(SOFT_FP64_FTZ STREQUAL "on")
    set(SOFT_FP64_FTZ_VALUE 1)
else()
    message(FATAL_ERROR "SOFT_FP64_FTZ must be off | on")
endif()
if(SOFT_FP64_OCL STREQUAL "on")
    set(SOFT_FP64_OCL_VALUE 1)
    target_sources(soft_fp64 PRIVATE src/ocl.cpp)
    target_compile_definitions(soft_fp64 PRIVATE
        SOFT_FP64_OCL_ENABLED=1
        SOFT_FP64_FTZ_MODE=${SOFT_FP64_FTZ_VALUE})
elseif(NOT SOFT_FP64_OCL STREQUAL "off")
    message(FATAL_ERROR "SOFT_FP64_OCL must be off | on")
endif()

# IEEE-754 exception-flag storage. `tls` uses a thread_local accumulator;
# `disabled` compiles every raise / query to a no-op (zero-cost); `explicit`
# emits the parallel `sf64_*_ex` caller-state ABI for GPU / SIMT consumers
# (Apple Metal, WebGPU, OpenCL device code) where `thread_local` is not
# available — and the legacy TLS surface stays present as no-op stubs so
# adapters that reference both can link.
set(SOFT_FP64_FENV "tls" CACHE STRING
    "Exception-flag storage: tls | disabled | explicit")
set_property(CACHE SOFT_FP64_FENV PROPERTY STRINGS tls disabled explicit)
if(SOFT_FP64_FENV STREQUAL "tls")
    set(SOFT_FP64_FENV_MODE_VALUE 1)
    target_compile_definitions(soft_fp64 PRIVATE SOFT_FP64_FENV_MODE=1)
elseif(SOFT_FP64_FENV STREQUAL "disabled")
    set(SOFT_FP64_FENV_MODE_VALUE 0)
    target_compile_definitions(soft_fp64 PRIVATE SOFT_FP64_FENV_MODE=0)
elseif(SOFT_FP64_FENV STREQUAL "explicit")
    set(SOFT_FP64_FENV_MODE_VALUE 2)
    target_compile_definitions(soft_fp64 PRIVATE SOFT_FP64_FENV_MODE=2)
else()
    message(FATAL_ERROR "SOFT_FP64_FENV must be tls | disabled | explicit")
endif()

# sNaN policy. `quiet` (default) returns canonical-payload qNaN from the four
# sites (sub sign-flip, fmod/remainder NaN-result paths) that today drop the
# source NaN's sign / payload. `propagate` makes those sites preserve the
# source NaN's sign and the 51 payload bits, with bit 51 (quiet bit) forced —
# IEEE 754-2008 §6.2.3 strict mode. The other propagation paths
# (propagate_nan, sqrt, fma, f32 widen/narrow) already preserve payload in
# both modes; only the four bypass sites differ.
set(SOFT_FP64_SNAN "quiet" CACHE STRING
    "sNaN policy: quiet (default — canonical NaN at four bypass sites) | propagate (§6.2.3 source-NaN preservation)")
set_property(CACHE SOFT_FP64_SNAN PROPERTY STRINGS quiet propagate)
if(SOFT_FP64_SNAN STREQUAL "quiet")
    set(SOFT_FP64_SNAN_PROPAGATE_VALUE 0)
elseif(SOFT_FP64_SNAN STREQUAL "propagate")
    set(SOFT_FP64_SNAN_PROPAGATE_VALUE 1)
else()
    message(FATAL_ERROR "SOFT_FP64_SNAN must be quiet | propagate")
endif()
target_compile_definitions(soft_fp64 PRIVATE
    SOFT_FP64_SNAN_PROPAGATE=${SOFT_FP64_SNAN_PROPAGATE_VALUE})

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/soft_fp64/config.h"
    @ONLY)

# Soft-fp64 TUs must not have `a*b+c` fused into llvm.fma.f64 at -O3 —
# fma depends on add/mul and we'd create a link-time cycle. Disable
# contraction globally for this target.
if(MSVC)
    target_compile_options(soft_fp64 PRIVATE /W4 /fp:strict)
else()
    target_compile_options(soft_fp64 PRIVATE
        -ffp-contract=off
        -fno-math-errno
        -fno-trapping-math
        -Wall
        -Wextra
        -Wpedantic)
endif()

# ABI surface: hide every symbol by default; only `SF64_EXPORT`-decorated
# entry points in include/soft_fp64/soft_f64.h escape the archive. The
# install-smoke CI job runs `nm -g` and fails if non-`sf64_*` symbols
# appear in the global table.
set_target_properties(soft_fp64 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

if(SOFT_FP64_WERROR)
    if(MSVC)
        target_compile_options(soft_fp64 PRIVATE /WX)
    else()
        target_compile_options(soft_fp64 PRIVATE -Werror)
    endif()
endif()

if(SOFT_FP_BUILD_FP128)
    add_subdirectory(src/fp128)
endif()
if(SOFT_FP_BUILD_FP256)
    add_subdirectory(src/fp256)
endif()

# Unified target. The per-format targets remain available for consumers that
# only want one precision, while soft_fp::soft_fp is the complete project.
add_library(soft_fp INTERFACE)
add_library(soft_fp::soft_fp ALIAS soft_fp)
target_link_libraries(soft_fp INTERFACE soft_fp64)
if(TARGET soft_fp128)
    target_link_libraries(soft_fp INTERFACE soft_fp128)
endif()
if(TARGET soft_fp256)
    target_link_libraries(soft_fp INTERFACE soft_fp256)
endif()

if(SOFT_FP64_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SOFT_FP64_BUILD_EXHAUSTIVE)
    enable_testing()
    add_executable(exhaustive_f32_f64_roundtrip
        fuzz/exhaustive_f32_f64_roundtrip.cpp)
    target_link_libraries(exhaustive_f32_f64_roundtrip PRIVATE soft_fp64::soft_fp64)
    if(MSVC)
        target_compile_options(exhaustive_f32_f64_roundtrip PRIVATE /O2 /fp:strict)
    else()
        target_compile_options(exhaustive_f32_f64_roundtrip PRIVATE -O2
            -ffp-contract=off -fno-math-errno -fno-trapping-math)
    endif()
    add_test(NAME test_exhaustive_f32_roundtrip COMMAND exhaustive_f32_f64_roundtrip)
    set_tests_properties(test_exhaustive_f32_roundtrip PROPERTIES
        TIMEOUT 900 LABELS "long;exhaustive")
endif()

# libFuzzer targets — opt-in, requires clang.
option(SOFT_FP64_BUILD_FUZZ "Build libFuzzer targets (requires clang)" OFF)
if(SOFT_FP64_BUILD_FUZZ AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/CMakeLists.txt")
    add_subdirectory(fuzz)
endif()

# Microbenchmarks — opt-in.
option(SOFT_FP64_BUILD_BENCH "Build microbenchmark harness" OFF)
if(SOFT_FP64_BUILD_BENCH AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bench/CMakeLists.txt")
    add_subdirectory(bench)
endif()

# -----------------------------------------------------------------------------
# Install, export set, find_package + pkg-config integration.
#
# Consumers can use either of:
#   find_package(soft_fp64 CONFIG REQUIRED)          # CMake >= 3.20
#   pkg-config --cflags --libs soft_fp64             # plain Makefiles, etc.
#
# Install is skipped automatically when the project is pulled in as a
# subdirectory (add_subdirectory / FetchContent), so embedding soft_fp64 in
# another build tree doesn't pollute the host project's install tree.
# -----------------------------------------------------------------------------
option(SOFT_FP64_INSTALL "Generate install + export rules for soft_fp64"
       ${PROJECT_IS_TOP_LEVEL})

if(SOFT_FP64_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(SOFT_FP64_CMAKE_CONFIG_INSTALL_DIR
        "${CMAKE_INSTALL_LIBDIR}/cmake/soft_fp64"
        CACHE STRING "Install path for soft_fp64 CMake package files")

    install(TARGETS soft_fp64 soft_fp
        EXPORT soft_fp64Targets
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    if(TARGET soft_fp128)
        install(TARGETS soft_fp128
            EXPORT soft_fp64Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
    if(TARGET soft_fp256)
        install(TARGETS soft_fp256
            EXPORT soft_fp64Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()

    install(DIRECTORY include/soft_fp64
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
    install(DIRECTORY include/soft_fp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h")
    if(TARGET soft_fp128)
        install(DIRECTORY include/soft_fp128
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h")
    endif()
    if(TARGET soft_fp256)
        install(DIRECTORY include/soft_fp256
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h")
    endif()
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/soft_fp64/config.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/soft_fp64")

    # Ship the `sf64_*` source bodies + the SLEEF-derived transcendental
    # sources alongside the installed headers. Consumers that need to
    # compile soft-fp64 into a non-host target (the AdaptiveCpp libkernel
    # bitcode build, downstream GPU adapters) read these via the
    # `soft_fp64_SOURCE_DIR` / `soft_fp64_SLEEF_SOURCE_DIR` variables
    # exposed by `soft_fp64Config.cmake`. Hidden-visibility internal
    # headers (`internal*.h`) ride along; they are not promoted to the
    # public include set.
    install(DIRECTORY src/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/soft_fp64/src"
        FILES_MATCHING
            PATTERN "*.cpp"
            PATTERN "*.h"
            PATTERN "*.hpp"
            PATTERN "fp128" EXCLUDE
            PATTERN "fp256" EXCLUDE)

    install(EXPORT soft_fp64Targets
        FILE soft_fp64Targets.cmake
        NAMESPACE soft_fp64::
        DESTINATION ${SOFT_FP64_CMAKE_CONFIG_INSTALL_DIR})

    set(SOFT_FP_CMAKE_CONFIG_INSTALL_DIR
        "${CMAKE_INSTALL_LIBDIR}/cmake/soft_fp")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/soft_fpConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fpConfig.cmake"
        INSTALL_DESTINATION "${SOFT_FP_CMAKE_CONFIG_INSTALL_DIR}")
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fpConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fpConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fpConfigVersion.cmake"
        DESTINATION "${SOFT_FP_CMAKE_CONFIG_INSTALL_DIR}")

    # Source-tree paths surfaced to consumers that need to compile the
    # `sf64_*` bodies themselves (e.g. AdaptiveCpp's libkernel bitcode
    # build, which globs `src/*.cpp` + `src/sleef/*.cpp` directly into a
    # SYCL/SSCP target rather than linking the host static library).
    # Resolved at install time relative to the install prefix; the source
    # tree is shipped under `${CMAKE_INSTALL_DATADIR}/soft_fp64/src`.
    set(SOFT_FP64_SOURCE_RELDIR        "${CMAKE_INSTALL_DATADIR}/soft_fp64/src")
    set(SOFT_FP64_SLEEF_SOURCE_RELDIR  "${CMAKE_INSTALL_DATADIR}/soft_fp64/src/sleef")
    set(SOFT_FP64_INCLUDE_RELDIR       "${CMAKE_INSTALL_INCLUDEDIR}")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/soft_fp64Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64Config.cmake"
        INSTALL_DESTINATION "${SOFT_FP64_CMAKE_CONFIG_INSTALL_DIR}"
        PATH_VARS SOFT_FP64_SOURCE_RELDIR SOFT_FP64_SLEEF_SOURCE_RELDIR
                  SOFT_FP64_INCLUDE_RELDIR)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64ConfigVersion.cmake"
        DESTINATION "${SOFT_FP64_CMAKE_CONFIG_INSTALL_DIR}")

    # pkg-config descriptor for non-CMake consumers.
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/soft_fp64.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64.pc"
        @ONLY)

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/soft_fp64.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    # License + third-party attribution files alongside the headers.
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/sleef/NOTICE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}" RENAME SLEEF-NOTICE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/testfloat/vendor/berkeley-softfloat-3/COPYING.txt"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}" RENAME SoftFloat-LICENSE)
endif()
