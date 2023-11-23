#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#define CL_ASSERT(expr) assert(expr)
#define CL_UNUSED(expr) (void)(expr)

#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
