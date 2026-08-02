# Smoke test — references every ABI symbol group.
if(MSVC)
    set(SF64_TEST_FP_OPTIONS /fp:strict)
else()
    set(SF64_TEST_FP_OPTIONS -ffp-contract=off -fno-math-errno)
endif()
find_package(Threads REQUIRED)
add_executable(test_scaffold test_scaffold.cpp)
target_link_libraries(test_scaffold PRIVATE soft_fp64::soft_fp64)
target_compile_options(test_scaffold PRIVATE ${SF64_TEST_FP_OPTIONS})
add_test(NAME test_scaffold COMMAND test_scaffold)

# The public C ABI must compile as C11 and link against the C++ implementation.
add_executable(test_c_api test_c_api.c)
set_target_properties(test_c_api PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(test_c_api PRIVATE c_std_11)
target_link_libraries(test_c_api PRIVATE soft_fp::soft_fp)
add_test(NAME test_c_api COMMAND test_c_api)
add_test(NAME test_abi_manifest
    COMMAND bash "${PROJECT_SOURCE_DIR}/scripts/check_abi_manifest.sh" "${PROJECT_BINARY_DIR}")

if(TARGET soft_fp128)
    add_executable(test_fp128 test_fp128.cpp)
    target_link_libraries(test_fp128 PRIVATE soft_fp::fp128)
    add_test(NAME test_fp128 COMMAND test_fp128)
    if(SOFT_FP64_FENV STREQUAL "tls")
        add_executable(test_fp128_threads test_fp128_threads.cpp)
        target_link_libraries(test_fp128_threads PRIVATE soft_fp::fp128 Threads::Threads)
        add_test(NAME test_fp128_threads COMMAND test_fp128_threads)
    endif()
endif()
if(TARGET soft_fp256)
    add_executable(test_fp256 test_fp256.cpp)
    target_link_libraries(test_fp256 PRIVATE soft_fp::fp256)
    add_test(NAME test_fp256 COMMAND test_fp256)
endif()

# Arithmetic / compare / convert / sqrt / rounding / classify unit tests.
set(SF64_CORE_TESTS
    test_arithmetic_exact
    test_compare_all_predicates
    test_convert_subnormal
    test_convert_widths
    test_sqrt_fma_exact
    test_rounding_edges
    test_rounding_modes
    test_classify
    test_hypot_scaling
    test_reentrancy
    test_fenv
    test_fenv_threads
    test_snan_payload
)
if(SOFT_FP64_OCL STREQUAL "on")
    list(APPEND SF64_CORE_TESTS test_ocl_mode)
endif()

foreach(_test IN LISTS SF64_CORE_TESTS)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_test}.cpp")
    if(EXISTS ${_src})
        add_executable(${_test} ${_test}.cpp)
        target_link_libraries(${_test} PRIVATE soft_fp64::soft_fp64)
        target_compile_options(${_test} PRIVATE ${SF64_TEST_FP_OPTIONS})
        if(${_test} STREQUAL "test_reentrancy")
            target_link_libraries(${_test} PRIVATE Threads::Threads)
        endif()
        if(${_test} STREQUAL "test_fenv" OR ${_test} STREQUAL "test_fenv_threads")
            # test_fenv cross-thread subtest spawns std::thread workers;
            # test_fenv_threads is a multi-threaded TLS isolation stress.
            target_link_libraries(${_test} PRIVATE Threads::Threads)
            # Pipe the build-time fenv mode selection into the test so it
            # can gate flag-set assertions (under `disabled` every raise is
            # a no-op and the test becomes a surface-smoke only).
            if(SOFT_FP64_FENV STREQUAL "tls")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=1)
            elseif(SOFT_FP64_FENV STREQUAL "disabled")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=0)
            elseif(SOFT_FP64_FENV STREQUAL "explicit")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=2)
            endif()
        endif()
        if(${_test} STREQUAL "test_snan_payload")
            # Probe test asserts bit-exact NaN sign/payload behavior under
            # the build-time SOFT_FP64_SNAN policy. The lib's compile-def is
            # PRIVATE so the test reads the same value via SOFT_FP64_SNAN
            # cache var to keep the install-consumer compile line clean. The
            # fenv mode is passed separately so explicit/disabled builds skip
            # TLS flag assertions against the intentionally no-op legacy
            # surface.
            target_compile_definitions(${_test} PRIVATE
                SOFT_FP64_SNAN_PROPAGATE=${SOFT_FP64_SNAN_PROPAGATE_VALUE})
            if(SOFT_FP64_FENV STREQUAL "tls")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=1)
            elseif(SOFT_FP64_FENV STREQUAL "disabled")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=0)
            elseif(SOFT_FP64_FENV STREQUAL "explicit")
                target_compile_definitions(${_test} PRIVATE SF64_TEST_FENV_MODE=2)
            endif()
        endif()
        if(${_test} STREQUAL "test_ocl_mode")
            target_compile_definitions(${_test} PRIVATE
                SOFT_FP64_FTZ_MODE=${SOFT_FP64_FTZ_VALUE})
        endif()
        if(${_test} STREQUAL "test_rounding_modes")
            # Host FPU oracle under std::fesetround needs the compiler to
            # honour the dynamic rounding mode — otherwise O2 will hoist
            # constant arithmetic outside the fenv guard and we compare
            # against stale RNE results.
            if(NOT MSVC)
                target_compile_options(${_test} PRIVATE -frounding-math)
            endif()
        endif()
        add_test(NAME ${_test} COMMAND ${_test})
    else()
        message(STATUS "Core test pending: ${_test}.cpp")
    endif()
endforeach()

# Transcendental 1-ULP diff tests.
set(SF64_TRANSCENDENTAL_TESTS
    test_transcendental_1ulp
    test_powr_ieee754
)

foreach(_test IN LISTS SF64_TRANSCENDENTAL_TESTS)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/${_test}.cpp")
    if(EXISTS ${_src})
        add_executable(${_test} ${_test}.cpp)
        target_link_libraries(${_test} PRIVATE soft_fp64::soft_fp64)
        target_compile_options(${_test} PRIVATE ${SF64_TEST_FP_OPTIONS})
        add_test(NAME ${_test} COMMAND ${_test})
    else()
        message(STATUS "Transcendental test pending: ${_test}.cpp")
    endif()
endforeach()

# Exhaustive / oracle tests. Each lives in its own subdirectory and is
# lazily added if present (keeps the top-level configure path light when
# oracle deps are missing).
foreach(_oracle_dir testfloat mpfr)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_oracle_dir}/CMakeLists.txt")
        add_subdirectory(${_oracle_dir})
    else()
        message(STATUS "Oracle ${_oracle_dir} pending (tests/${_oracle_dir}/CMakeLists.txt missing)")
    endif()
endforeach()

# MPFR-backed coverage test — gracefully skipped if MPFR is not installed.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_coverage_mpfr.cpp")
    set(_pg_mpfr_found FALSE)
    set(_pg_mpfr_include_dirs "")
    set(_pg_mpfr_libraries "")

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PG_MPFR_PC QUIET mpfr)
        if(PG_MPFR_PC_FOUND)
            set(_pg_mpfr_found TRUE)
            set(_pg_mpfr_include_dirs ${PG_MPFR_PC_INCLUDE_DIRS})
            set(_pg_mpfr_libraries ${PG_MPFR_PC_LINK_LIBRARIES})
            if(NOT _pg_mpfr_libraries)
                set(_pg_mpfr_libraries ${PG_MPFR_PC_LIBRARIES})
            endif()
            pkg_check_modules(PG_GMP_PC QUIET gmp)
            if(PG_GMP_PC_FOUND)
                list(APPEND _pg_mpfr_include_dirs ${PG_GMP_PC_INCLUDE_DIRS})
            endif()
        endif()
    endif()

    if(NOT _pg_mpfr_found)
        find_library(PG_MPFR_LIB mpfr)
        find_path(PG_MPFR_INCLUDE mpfr.h)
        if(PG_MPFR_LIB AND PG_MPFR_INCLUDE)
            set(_pg_mpfr_found TRUE)
            set(_pg_mpfr_include_dirs ${PG_MPFR_INCLUDE})
            set(_pg_mpfr_libraries ${PG_MPFR_LIB})
            find_library(PG_GMP_LIB gmp)
            find_path(PG_GMP_INCLUDE gmp.h)
            if(PG_GMP_LIB)
                list(APPEND _pg_mpfr_libraries ${PG_GMP_LIB})
            endif()
            if(PG_GMP_INCLUDE)
                list(APPEND _pg_mpfr_include_dirs ${PG_GMP_INCLUDE})
            endif()
        endif()
    endif()

    if(_pg_mpfr_found)
        add_executable(test_coverage_mpfr test_coverage_mpfr.cpp)
        target_include_directories(test_coverage_mpfr PRIVATE ${_pg_mpfr_include_dirs})
        target_link_libraries(test_coverage_mpfr
            PRIVATE
                soft_fp64::soft_fp64
                ${_pg_mpfr_libraries}
        )
        target_compile_options(test_coverage_mpfr PRIVATE ${SF64_TEST_FP_OPTIONS})
        add_test(NAME test_coverage_mpfr COMMAND test_coverage_mpfr)
        set_tests_properties(test_coverage_mpfr PROPERTIES TIMEOUT 60)
        message(STATUS "test_coverage_mpfr registered")
        if(TARGET soft_fp256)
            add_executable(test_fp256_mpfr test_fp256_mpfr.cpp)
            target_include_directories(test_fp256_mpfr PRIVATE ${_pg_mpfr_include_dirs})
            target_link_libraries(test_fp256_mpfr PRIVATE soft_fp::fp256 ${_pg_mpfr_libraries})
            add_test(NAME test_fp256_mpfr COMMAND test_fp256_mpfr)
            set_tests_properties(test_fp256_mpfr PROPERTIES TIMEOUT 120)
        endif()
        if(TARGET soft_fp128)
            add_executable(test_fp128_mpfr test_fp128_mpfr.cpp)
            target_include_directories(test_fp128_mpfr PRIVATE ${_pg_mpfr_include_dirs})
            target_link_libraries(test_fp128_mpfr PRIVATE soft_fp::fp128 ${_pg_mpfr_libraries})
            add_test(NAME test_fp128_mpfr COMMAND test_fp128_mpfr)
            set_tests_properties(test_fp128_mpfr PROPERTIES TIMEOUT 120)
        endif()
    else()
        message(STATUS "MPFR not found — skipping test_coverage_mpfr "
                       "(install with `brew install mpfr` on macOS).")
    endif()
endif()
