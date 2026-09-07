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
