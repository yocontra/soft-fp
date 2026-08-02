add_library(soft_fp256 STATIC soft_f256.cpp)
add_library(soft_fp::fp256 ALIAS soft_fp256)
target_include_directories(soft_fp256 PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(soft_fp256 PUBLIC soft_fp64)
target_compile_features(soft_fp256 PRIVATE cxx_std_17)
set_target_properties(soft_fp256 PROPERTIES POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
if(MSVC)
    target_compile_options(soft_fp256 PRIVATE /W4 "$<$<BOOL:${SOFT_FP64_WERROR}>:/WX>")
else()
    target_compile_options(soft_fp256 PRIVATE -Wall -Wextra -Wpedantic
        "$<$<BOOL:${SOFT_FP64_WERROR}>:-Werror>")
endif()
