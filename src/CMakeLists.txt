set(CXXLOX_SRCS
    chunk.cpp
    cli.cpp
    compiler.cpp
    debug.cpp
    object.cpp
    scanner.cpp
    value.cpp
    vm.cpp
)

# Track headers and add them to the source list so that CLion can look things
# up appropriately.
set(CXXLOX_HEADERS
    array.hpp
    chunk.hpp
    cli.hpp
    compiler.hpp
    common.hpp
    debug.hpp
    object.hpp
    scanner.hpp
    value.hpp
    vm.hpp
)
add_library(cxxlox_vm STATIC ${CXXLOX_SRCS} ${CXXLOX_HEADERS})
add_executable(cxxlox_vm_cli main.cpp)
target_link_libraries(cxxlox_vm_cli cxxlox_vm)
target_include_directories(cxxlox_vm_cli SYSTEM PUBLIC ${PROJECT_ROOT}/src)

