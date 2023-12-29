# GoogleTest help: https://google.github.io/googletest/quickstart-cmake.html
include(GoogleTest)

add_executable(cxxlox_unit_tests
    object_tests.cpp
    table_tests.cpp
    vm_tests.cpp
)
target_link_libraries(cxxlox_unit_tests cxxlox_vm GTest::gtest_main)
target_include_directories(cxxlox_unit_tests SYSTEM PRIVATE "../src")
target_compile_definitions(cxxlox_unit_tests PRIVATE ${CL_COMMON_DEFNS})
gtest_discover_tests(cxxlox_unit_tests)