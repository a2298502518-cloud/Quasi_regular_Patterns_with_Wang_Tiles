cmake_minimum_required(VERSION 3.25)

project(
    QuasiRegularPatternsWithWangTiles
    VERSION 0.1.0
    DESCRIPTION "Content-constrained quasi-regular patterns with Wang edge labels"
    LANGUAGES CXX
)

option(QRP_BUILD_TESTS "Build the Wang-QRP numerical tests" ON)

include(FetchContent)

FetchContent_Declare(
    lodepng
    GIT_REPOSITORY https://github.com/lvandeve/lodepng.git
    GIT_TAG ed6fe5825c6a4fbb7f58ab35a4231c7543cd452a
)
FetchContent_MakeAvailable(lodepng)

add_library(qrp_lodepng STATIC
    "${lodepng_SOURCE_DIR}/lodepng.cpp"
)
target_include_directories(qrp_lodepng SYSTEM PUBLIC "${lodepng_SOURCE_DIR}")

function(qrp_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(qrp_wang_qrp_core STATIC
    src/model/HierarchicalQrpComposition.cpp
    src/model/ParametricWangQrpField.cpp
    src/model/WangContentWeight.cpp
    src/model/WangGrid.cpp
)
target_include_directories(qrp_wang_qrp_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(qrp_wang_qrp_core PUBLIC cxx_std_20)
qrp_enable_warnings(qrp_wang_qrp_core)

add_library(qrp_raster STATIC
    src/color/GradientPalette.cpp
    src/render/Image.cpp
)
target_include_directories(qrp_raster
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(qrp_raster PUBLIC cxx_std_20)
qrp_enable_warnings(qrp_raster)

add_library(qrp_png STATIC
    src/export/PngWriter.cpp
)
target_include_directories(qrp_png
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(qrp_png
    PUBLIC
        qrp_raster
    PRIVATE
        qrp_lodepng
)
target_compile_features(qrp_png PUBLIC cxx_std_20)
qrp_enable_warnings(qrp_png)

add_executable(qrp_wang_qrp_experiment
    apps/wang_qrp_experiment_main.cpp
)
target_link_libraries(qrp_wang_qrp_experiment
    PRIVATE
        qrp_png
        qrp_raster
        qrp_wang_qrp_core
)
target_compile_features(qrp_wang_qrp_experiment PRIVATE cxx_std_20)
qrp_enable_warnings(qrp_wang_qrp_experiment)

if(QRP_BUILD_TESTS)
    enable_testing()

    # Canonical QRP is a test oracle, not part of the production dependency graph.
    add_library(qrp_qrp_reference STATIC
        src/generators/CanonicalQrpField.cpp
    )
    target_include_directories(qrp_qrp_reference
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_compile_features(qrp_qrp_reference PUBLIC cxx_std_20)
    qrp_enable_warnings(qrp_qrp_reference)

    add_subdirectory(tests)
endif()
