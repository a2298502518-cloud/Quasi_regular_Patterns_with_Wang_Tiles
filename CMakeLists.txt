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
    src/model/ParametricQrpField.cpp
    src/model/PhaseCompatibleQrpTiles.cpp
    src/model/QrpChannelComposition.cpp
)
target_include_directories(qrp_wang_qrp_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(qrp_wang_qrp_core PUBLIC cxx_std_20)
qrp_enable_warnings(qrp_wang_qrp_core)

add_library(qrp_raster STATIC
    src/color/InkCoverage.cpp
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

add_library(qrp_source_export STATIC apps/QrpSourceExport.cpp)
target_include_directories(qrp_source_export PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/apps")
target_link_libraries(qrp_source_export PUBLIC qrp_wang_qrp_core)
qrp_enable_warnings(qrp_source_export)

add_executable(qrp_export_sources apps/export_sources_main.cpp)
target_link_libraries(qrp_export_sources PRIVATE qrp_source_export)
qrp_enable_warnings(qrp_export_sources)

add_library(qrp_tile_study STATIC
    apps/TileStudyRecipes.cpp
    apps/TileStudyExport.cpp
)
target_include_directories(qrp_tile_study PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/apps")
target_link_libraries(qrp_tile_study
    PUBLIC
        qrp_source_export
        qrp_png
        qrp_raster
        qrp_wang_qrp_core
)
qrp_enable_warnings(qrp_tile_study)

add_executable(qrp_wang_qrp_experiment
    apps/wang_qrp_experiment_main.cpp
    apps/ReusableTileStudy.cpp
)
target_link_libraries(qrp_wang_qrp_experiment PRIVATE qrp_tile_study)
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
