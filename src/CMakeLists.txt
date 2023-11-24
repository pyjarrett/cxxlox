set(CXXLOX_SRCS
    main.cpp
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
add_executable(cxxlox_vm ${CXXLOX_SRCS} ${CXXLOX_HEADERS})
target_include_directories(cxxlox_vm SYSTEM PUBLIC ${PROJECT_ROOT}/src)
