#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define CL_ASSERT(expr) assert(expr)
#define CL_UNUSED(expr) (void)(expr)
#define CL_FATAL(message) do { std::abort(); } while(0)

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION

namespace cxxlox {

[[nodiscard]] int32_t growCapacity(int32_t previousCapacity);

} // namespace cxxlox
