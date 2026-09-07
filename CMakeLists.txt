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

add_library(qrp_generators STATIC
    src/generators/HybridTorusGenerator.cpp
    src/generators/PeriodicGradientNoise.cpp
    src/generators/TorusFourier.cpp
)

target_link_libraries(qrp_generators PUBLIC qrp_math)
target_compile_features(qrp_generators PUBLIC cxx_std_20)

add_library(qrp_color STATIC
    src/color/GradientPalette.cpp
)

target_include_directories(qrp_color
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(qrp_color PUBLIC cxx_std_20)

add_library(qrp_render STATIC
    src/export/PpmWriter.cpp
    src/render/CpuReferenceRenderer.cpp
    src/render/Image.cpp
)

target_link_libraries(qrp_render
    PUBLIC
        qrp_color
        qrp_generators
        qrp_math
        qrp_model
)
target_compile_features(qrp_render PUBLIC cxx_std_20)

add_executable(qrp_cpu_reference
    apps/cpu_reference_main.cpp
)

target_link_libraries(qrp_cpu_reference PRIVATE qrp_render)
target_compile_features(qrp_cpu_reference PRIVATE cxx_std_20)

foreach(target
    qrp_math
    qrp_model
    qrp_generators
    qrp_color
    qrp_render
    qrp_cpu_reference
)
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
