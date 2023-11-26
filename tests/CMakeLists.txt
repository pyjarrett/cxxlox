# GoogleTest help: https://google.github.io/googletest/quickstart-cmake.html

include(GoogleTest)

add_executable(object_tests
        object_tests.cpp
)
target_link_libraries(object_tests cxxlox_vm GTest::gtest_main)
target_include_directories(object_tests SYSTEM PRIVATE "../src")
gtest_discover_tests(object_tests)

add_executable(table_tests
        table_tests.cpp
)
target_link_libraries(table_tests cxxlox_vm GTest::gtest_main)
target_include_directories(table_tests SYSTEM PRIVATE "../src")
gtest_discover_tests(table_tests)

add_executable(vm_tests
        vm_tests.cpp
)
target_link_libraries(vm_tests cxxlox_vm GTest::gtest_main)
target_include_directories(vm_tests SYSTEM PRIVATE "../src")
gtest_discover_tests(vm_tests)