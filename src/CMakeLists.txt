set(CXXLOX_SRCS
    chunk.cpp
    cli.cpp
    common.cpp
    compiler.cpp
    debug.cpp
    memory.cpp
    object.cpp
    object_allocator.cpp
    pratt.cpp
    scanner.cpp
    table.cpp
    value.cpp
    vm.cpp
)

# Track headers and add them to the source list so that CLion can look things
# up appropriately.
set(CXXLOX_HEADERS
    array.hpp
    chunk.hpp
    cli.hpp
    common.hpp
    compiler.hpp
    debug.hpp
    memory.hpp
    object.hpp
    object_allocator.hpp
    pratt.hpp
    scanner.hpp
    table.hpp
    value.hpp
    vm.hpp
)
add_library(cxxlox_vm STATIC ${CXXLOX_SRCS} ${CXXLOX_HEADERS})
add_executable(cxxlox_vm_cli main.cpp)
target_link_libraries(cxxlox_vm_cli cxxlox_vm)
target_include_directories(cxxlox_vm_cli SYSTEM PUBLIC ${PROJECT_ROOT}/src)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    # Print code and trace commands in debug modes.
    target_compile_definitions(cxxlox_vm PRIVATE DEBUG_PRINT_CODE DEBUG_TRACE_EXECUTION)
endif()

target_compile_definitions(cxxlox_vm PRIVATE ${CL_COMMON_DEFNS})
