set(CXXLOX_SRCS
    main.cpp
    cli.cpp
    chunk.cpp
    debug.cpp
    value.cpp
    vm.cpp
)
set(CXXLOX_HEADERS
    array.hpp
    cli.hpp
    chunk.hpp
    common.hpp
    debug.hpp
    value.hpp
    vm.hpp
)
add_executable(cxxlox_vm ${CXXLOX_SRCS} ${CXXLOX_HEADERS})
target_include_directories(cxxlox_vm SYSTEM PUBLIC ${PROJECT_ROOT}/src)
