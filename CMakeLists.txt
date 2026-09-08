cmake_minimum_required(VERSION 3.25)

project(
    QuasiRegularPatternsWithWangTiles
    VERSION 0.1.0
    DESCRIPTION "Seamless procedural patterns with Wang tiles and domain-invariant warping"
    LANGUAGES C CXX
)

option(QRP_BUILD_TESTS "Build the CPU reference tests" ON)
option(QRP_BUILD_REALTIME "Build the GLFW/OpenGL realtime application" ON)

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

if(QRP_BUILD_REALTIME)
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

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG 01380c579715e62fb9a8d6ec0502c4ea83bfde6e # 1.92.9
    )
    FetchContent_MakeAvailable(imgui)

    add_library(qrp_glad STATIC
        third_party/glad/src/gl.c
    )
    target_include_directories(qrp_glad
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include"
    )

    add_library(qrp_imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    target_include_directories(qrp_imgui
        PUBLIC
            "${imgui_SOURCE_DIR}"
            "${imgui_SOURCE_DIR}/backends"
    )
    target_link_libraries(qrp_imgui PUBLIC glfw)
    target_compile_features(qrp_imgui PUBLIC cxx_std_20)
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

add_library(qrp_project STATIC
    src/project/PatternProject.cpp
)

target_link_libraries(qrp_project
    PUBLIC
        qrp_model
        qrp_presets
)
target_compile_features(qrp_project PUBLIC cxx_std_20)

add_library(qrp_render STATIC
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

add_library(qrp_export STATIC
    src/export/PngWriter.cpp
    src/export/PpmWriter.cpp
    src/export/ProjectMetadataWriter.cpp
)

target_link_libraries(qrp_export
    PUBLIC
        qrp_lodepng
        qrp_project
        qrp_render
)
target_compile_features(qrp_export PUBLIC cxx_std_20)

add_executable(qrp_cpu_reference
    apps/cpu_reference_main.cpp
)

target_link_libraries(qrp_cpu_reference PRIVATE qrp_export qrp_presets qrp_render)
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

    add_library(qrp_ui STATIC
        src/ui/PatternEditor.cpp
    )
    target_link_libraries(qrp_ui
        PUBLIC
            qrp_imgui
            qrp_opengl
            qrp_project
    )
    target_compile_features(qrp_ui PUBLIC cxx_std_20)

    add_executable(qrp_realtime
        apps/realtime_main.cpp
    )

    target_link_libraries(qrp_realtime
        PRIVATE
            glfw
            qrp_glad
            qrp_export
            qrp_opengl
            qrp_project
            qrp_presets
            qrp_render
            qrp_ui
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
    qrp_project
    qrp_render
    qrp_export
    qrp_cpu_reference
)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

if(QRP_BUILD_REALTIME)
    foreach(target qrp_imgui qrp_opengl qrp_ui qrp_realtime)
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
