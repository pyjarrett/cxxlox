cmake_minimum_required(VERSION 3.26)
project(cxxlox)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;RelWithDebInfo;Release" CACHE STRING "" FORCE)
endif (CMAKE_CONFIGURATION_TYPES)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

###############################################################################
# External libraries
###############################################################################

# Use a vendored version of GoogleTest to provide everything out of the box.
add_subdirectory(vendor/googletest-1.14.0)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

include(CTest)

###############################################################################
# Configuration
###############################################################################
if (MSVC)
    message(STATUS "Using visual studio")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")      # Enable parallel build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4820")  # Disable implicit padding warning.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4061")  # Disable explicit enum switch handling warning.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd5045")  # Disable explicit enum switch handling warning.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4710")  # Disable warnings about function not being inlined
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4711")  # Disable warnings about automatic inline expansion
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4514")  # Disable warnings about unreferenced inline functions being removed.
endif ()

set(CL_COMMON_DEFNS "")
if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    list(APPEND CL_COMMON_DEFNS CL_DEBUG)
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND CL_COMMON_DEFNS CL_RELEASE)
endif ()

# Export a compilation database to most easily check commands being run.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

###############################################################################
# Libraries and Executables
###############################################################################

add_subdirectory(src)
add_subdirectory(tests)

