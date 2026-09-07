cmake_minimum_required(VERSION 3.25)

project(
    QuasiRegularPatternsWithWangTiles
    VERSION 0.1.0
    DESCRIPTION "Seamless procedural patterns with Wang tiles and domain-invariant warping"
    LANGUAGES C CXX
)

option(QRP_BUILD_TESTS "Build the CPU reference tests" ON)
option(QRP_BUILD_REALTIME "Build the GLFW/OpenGL realtime application" ON)

if(QRP_BUILD_REALTIME)
    include(FetchContent)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 # 3.4
    )
    FetchContent_MakeAvailable(glfw)

    add_library(qrp_glad STATIC
        third_party/glad/src/gl.c
    )
    target_include_directories(qrp_glad
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include"
    )
endif()

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

add_library(qrp_presets STATIC
    src/presets/PatternPreset.cpp
)

target_link_libraries(qrp_presets PUBLIC qrp_color qrp_generators)
target_compile_features(qrp_presets PUBLIC cxx_std_20)

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

target_link_libraries(qrp_cpu_reference PRIVATE qrp_presets qrp_render)
target_compile_features(qrp_cpu_reference PRIVATE cxx_std_20)

if(QRP_BUILD_REALTIME)
    add_library(qrp_opengl STATIC
        src/render/opengl/GpuPatternRenderer.cpp
        src/render/opengl/ShaderProgram.cpp
    )

    target_link_libraries(qrp_opengl
        PUBLIC
            qrp_color
            qrp_generators
            qrp_model
            qrp_glad
    )
    target_compile_features(qrp_opengl PUBLIC cxx_std_20)

    add_executable(qrp_realtime
        apps/realtime_main.cpp
    )

    target_link_libraries(qrp_realtime
        PRIVATE
            glfw
            qrp_glad
            qrp_opengl
            qrp_presets
            qrp_render
    )
    target_compile_features(qrp_realtime PRIVATE cxx_std_20)
    target_compile_definitions(
        qrp_realtime
        PRIVATE
            QRP_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    )

    add_custom_target(qrp_gpu_validate
        COMMAND
            $<TARGET_FILE:qrp_realtime>
            --preset 3
            --capture "${CMAKE_CURRENT_BINARY_DIR}/gpu_validation.ppm"
            --validate
        DEPENDS qrp_realtime
        USES_TERMINAL
    )
endif()

foreach(target
    qrp_math
    qrp_model
    qrp_generators
    qrp_color
    qrp_presets
    qrp_render
    qrp_cpu_reference
)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

if(QRP_BUILD_REALTIME)
    foreach(target qrp_opengl qrp_realtime)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()
endif()

if(QRP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
