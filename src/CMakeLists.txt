set(CXXLOX_SRCS
    main.cpp
    chunk.cpp
    debug.cpp
)
set(CXXLOX_HEADERS
    array.hpp
    chunk.hpp
    common.hpp
    debug.hpp
    value.hpp
)
add_executable(cxxlox_vm ${CXXLOX_SRCS} ${CXXLOX_HEADERS})
target_include_directories(cxxlox_vm SYSTEM PUBLIC ${PROJECT_ROOT}/src)
