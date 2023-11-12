# GoogleTest help: https://google.github.io/googletest/quickstart-cmake.html

include(GoogleTest)

add_executable(example_tests
    example_tests.cpp
)
target_link_libraries(example_tests GTest::gtest_main)
gtest_discover_tests(example_tests)
