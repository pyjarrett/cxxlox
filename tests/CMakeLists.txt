# GoogleTest help: https://google.github.io/googletest/quickstart-cmake.html

include(GoogleTest)

add_executable(vm_tests
    vm_tests.cpp
)
target_link_libraries(vm_tests cxxlox_vm GTest::gtest_main)
target_include_directories(vm_tests SYSTEM PRIVATE "../src")
gtest_discover_tests(vm_tests)
