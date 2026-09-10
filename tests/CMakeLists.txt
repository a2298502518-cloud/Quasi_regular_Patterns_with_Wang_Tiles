add_executable(qrp_math_tests
    math_tests.cpp
)

target_link_libraries(qrp_math_tests PRIVATE qrp_math qrp_model)
target_compile_features(qrp_math_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(qrp_math_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(qrp_math_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME qrp_math_tests COMMAND qrp_math_tests)

add_executable(qrp_pipeline_tests
    pipeline_tests.cpp
)

target_link_libraries(qrp_pipeline_tests PRIVATE qrp_export qrp_project qrp_render)
target_compile_features(qrp_pipeline_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(qrp_pipeline_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(qrp_pipeline_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME qrp_pipeline_tests COMMAND qrp_pipeline_tests)

add_executable(qrp_atlas_tests
    atlas_tests.cpp
)

target_link_libraries(qrp_atlas_tests PRIVATE qrp_atlas)
target_compile_features(qrp_atlas_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(qrp_atlas_tests PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(qrp_atlas_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME qrp_atlas_tests COMMAND qrp_atlas_tests)

add_test(
    NAME qrp_classic_wang_reference_smoke
    COMMAND
        qrp_classic_wang_reference
        "${CMAKE_CURRENT_BINARY_DIR}/classic-wang-reference"
)

add_test(
    NAME qrp_paper_core_audit_smoke
    COMMAND
        qrp_paper_core_audit
        "${CMAKE_CURRENT_BINARY_DIR}/paper-core-audit"
)
