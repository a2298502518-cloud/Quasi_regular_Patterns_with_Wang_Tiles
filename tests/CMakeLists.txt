function(qrp_add_unit_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${ARGN})
    target_compile_features(${target} PRIVATE cxx_std_20)
    qrp_enable_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

qrp_add_unit_test(
    qrp_wang_grid_tests
    wang_grid_tests.cpp
    qrp_wang_qrp_core
)

qrp_add_unit_test(
    qrp_canonical_qrp_tests
    canonical_qrp_tests.cpp
    qrp_qrp_reference
)

qrp_add_unit_test(
    qrp_wang_content_weight_tests
    wang_content_weight_tests.cpp
    qrp_wang_qrp_core
)

qrp_add_unit_test(
    qrp_hierarchical_qrp_composition_tests
    hierarchical_qrp_composition_tests.cpp
    qrp_wang_qrp_core
)

qrp_add_unit_test(
    qrp_parametric_wang_qrp_field_tests
    parametric_wang_qrp_field_tests.cpp
    qrp_qrp_reference
    qrp_wang_qrp_core
)
