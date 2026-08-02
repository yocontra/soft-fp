set(SOFTFLOAT_ROOT "${PROJECT_SOURCE_DIR}/tests/testfloat/vendor/berkeley-softfloat-3")
set(SOFTFLOAT_SRC "${SOFTFLOAT_ROOT}/source")
if(NOT EXISTS "${SOFTFLOAT_SRC}/include/softfloat.h")
    message(FATAL_ERROR "SOFT_FP_BUILD_FP128 requires the vendored Berkeley SoftFloat 3e sources")
endif()

# Audited dependency closure for the public binary128 surface. Keep this an
# explicit manifest: adding an API must add its backend object deliberately,
# and unrelated f16/f32/f64/extF80 implementations never enter production.
set(SOFTFLOAT_CORE_NAMES
    f128_add f128_div f128_eq f128_le f128_lt f128_mul f128_mulAdd f128_rem
    f128_roundToInt f128_sqrt f128_sub f128_to_f64 f128_to_i32 f128_to_i64
    f128_to_ui32 f128_to_ui64 f64_to_f128 i32_to_f128 i64_to_f128
    ui32_to_f128 ui64_to_f128 s_add256M s_addMagsF128
    s_approxRecipSqrt_1Ks s_approxRecipSqrt32_1 s_countLeadingZeros64
    s_countLeadingZeros8 s_mul128To256M s_mul64To128 s_mulAddF128
    s_normRoundPackToF128 s_normSubnormalF128Sig s_normSubnormalF64Sig
    s_roundPackToF128 s_roundPackToF64 s_roundToI32 s_roundToI64
    s_roundToUI32 s_roundToUI64 s_shiftRightJam128 s_shiftRightJam128Extra
    s_shiftRightJam256M s_sub256M s_subMagsF128 softfloat_state)
set(SOFTFLOAT_SPECIALIZE_NAMES
    s_commonNaNToF128UI s_commonNaNToF64UI s_f128UIToCommonNaN
    s_f64UIToCommonNaN s_propagateNaNF128UI softfloat_raiseFlags)

# Berkeley SoftFloat's public entry points and state use process-global C
# names. Prefix that externally visible interface so applications can embed a
# separate SoftFloat build without interposing our backend. Internal helpers
# retain SoftFloat's `softfloat_*` names but have hidden visibility and bind
# within this backend.
set(SOFTFLOAT_BACKEND_SYMBOLS
    f128_add f128_div f128_eq f128_le f128_lt f128_mul f128_mulAdd f128_rem
    f128_roundToInt f128_sqrt f128_sub f128_to_f64 f128_to_i32 f128_to_i64
    f128_to_ui32 f128_to_ui64 f64_to_f128 i32_to_f128 i64_to_f128
    ui32_to_f128 ui64_to_f128 softfloat_detectTininess
    softfloat_exceptionFlags softfloat_roundingMode extF80_roundingPrecision)
set(SOFTFLOAT_BACKEND_DEFINITIONS)
foreach(_symbol IN LISTS SOFTFLOAT_BACKEND_SYMBOLS)
    list(APPEND SOFTFLOAT_BACKEND_DEFINITIONS
        "${_symbol}=sf128_backend_${_symbol}")
endforeach()
set(SOFTFLOAT_TOP)
foreach(_name IN LISTS SOFTFLOAT_CORE_NAMES)
    list(APPEND SOFTFLOAT_TOP "${SOFTFLOAT_SRC}/${_name}.c")
endforeach()
set(SOFTFLOAT_SPECIALIZE)
foreach(_name IN LISTS SOFTFLOAT_SPECIALIZE_NAMES)
    list(APPEND SOFTFLOAT_SPECIALIZE "${SOFTFLOAT_SRC}/8086-SSE/${_name}.c")
endforeach()

add_library(softfloat_core OBJECT ${SOFTFLOAT_TOP} ${SOFTFLOAT_SPECIALIZE})
target_include_directories(softfloat_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SOFTFLOAT_SRC}/8086-SSE" "${SOFTFLOAT_SRC}/include")
target_compile_definitions(softfloat_core PUBLIC
    SOFTFLOAT_FAST_INT64 SOFTFLOAT_ROUND_ODD INLINE_LEVEL=5
    SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32
    ${SOFTFLOAT_BACKEND_DEFINITIONS}
)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(softfloat_core PRIVATE -Wno-unused-but-set-variable
        -Wno-parentheses -Wno-empty-body -Wno-pointer-sign
        -Wno-tautological-constant-out-of-range-compare)
endif()
set_target_properties(softfloat_core PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

add_library(soft_fp128 STATIC soft_f128.cpp backend_state.c $<TARGET_OBJECTS:softfloat_core>)
add_library(soft_fp::fp128 ALIAS soft_fp128)
target_include_directories(soft_fp128 PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>)
target_include_directories(soft_fp128 PRIVATE "${SOFTFLOAT_SRC}/include")
target_compile_definitions(soft_fp128 PRIVATE SOFTFLOAT_FAST_INT64 SOFTFLOAT_ROUND_ODD
    ${SOFTFLOAT_BACKEND_DEFINITIONS})
target_link_libraries(soft_fp128 PUBLIC soft_fp64)
set_target_properties(soft_fp128 PROPERTIES POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_compile_features(soft_fp128 PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(soft_fp128 PRIVATE /W4 "$<$<BOOL:${SOFT_FP64_WERROR}>:/WX>")
else()
    target_compile_options(soft_fp128 PRIVATE -Wall -Wextra -Wpedantic
        "$<$<BOOL:${SOFT_FP64_WERROR}>:-Werror>")
endif()
