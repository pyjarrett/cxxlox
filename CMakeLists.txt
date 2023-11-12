cmake_minimum_required(VERSION 3.26)
project(cxxlox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)
endif (CMAKE_CONFIGURATION_TYPES)

# Use a vendored version of GoogleTest to provide everything out of the box.
add_subdirectory(vendor/googletest-1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(tests)

add_executable(cxxlox main.cpp)

# Provides a command to run all tests, since CLion can't do that on it's own.
# https://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests#736838
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} -V -C $<CONFIG>)

# check-iterate is the target to use when iterating when using tests,
# such as TDD's red->green pattern.
add_custom_target(check-iterate COMMAND ${CMAKE_CTEST_COMMAND} -V --rerun-failed -C $<CONFIG>)

enable_testing()
