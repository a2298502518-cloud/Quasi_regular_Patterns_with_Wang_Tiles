cmake_minimum_required(VERSION 3.25)

project(
    QuasiRegularPatternsWithWangTiles
    VERSION 0.1.0
    DESCRIPTION "Seamless procedural patterns with Wang tiles and domain-invariant warping"
    LANGUAGES CXX
)

option(QRP_BUILD_TESTS "Build the CPU reference tests" ON)

add_library(qrp_math STATIC
    src/math/CoonsWarp.cpp
    src/math/EdgeFunction.cpp
)

target_include_directories(qrp_math
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(qrp_math PUBLIC cxx_std_20)

add_library(qrp_model STATIC
    src/model/EdgePalette.cpp
    src/model/WangGrid.cpp
)

target_link_libraries(qrp_model PUBLIC qrp_math)
target_compile_features(qrp_model PUBLIC cxx_std_20)

foreach(target qrp_math qrp_model)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

if(QRP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
