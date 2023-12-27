#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib> // for std::abort()
#include <limits>

// Compiler identification
#define CL_COMPILER_CLANG 0
#define CL_COMPILER_GCC 0
#define CL_COMPILER_MSVC 0
#if defined(__clang__)
	#undef CL_COMPILER_CLANG
	#define CL_COMPILER_CLANG 1
#elif defined(__GNUG__)
	#undef CL_COMPILER_GCC
	#define CL_COMPILER_GCC 1
#elif defined(_MSC_VER)
	#undef CL_COMPILER_MSVC
	#define CL_COMPILER_MSVC 1
#else
	#error "Unknown compiler."
#endif

#if !defined(CL_DEBUG) && !defined(CL_RELEASE)
	#error Either CL_DEBUG or CL_RELEASE must be defined.  Check the build setup.
#endif

#ifdef CL_DEBUG
	#ifdef _WIN32
		#include <intrin.h>
		#define CL_BREAKPOINT() __debugbreak()
	#endif
#endif

#ifndef CL_BREAKPOINT
#define CL_BREAKPOINT()
#endif

#ifdef CL_DEBUG
	#define CL_ASSERT(expr)      \
		do {                     \
			if (!(expr)) {       \
				CL_BREAKPOINT(); \
				std::abort();    \
			}                    \
		} while (0)
#else
	#define CL_ASSERT(expr)
#endif

#define CL_UNUSED(expr) (void)(expr)
#define CL_FATAL(message) \
	do {                  \
		CL_BREAKPOINT();  \
		std::abort();     \
	} while (0)

// A macro to be more forceful with the compiler on inlining, since
// I wrapped some things like the current chunk and current frame in functions.
// I didn't use it every place I thought I would, but it is a nice feature to have.
//
// https://clang.llvm.org/docs/AttributeReference.html#always-inline-force-inline
#if CL_COMPILER_MSVC
	#define CL_FORCE_INLINE __forceinline
#elif CL_COMPILER_GCC
	#define CL_FORCE_INLINE [[gnu::always_inline]] inline
#elif CL_COMPILER_CLANG
	#define CL_FORCE_INLINE [[clang::always_inline]]
#endif

static constexpr int kUInt8Count = std::numeric_limits<uint8_t>::max() + 1;

// Enable to print code once it is compiled.
//#define DEBUG_PRINT_CODE
// Enable to trace command execution.
//#define DEBUG_TRACE_EXECUTION

namespace cxxlox {

[[nodiscard]] int32_t growCapacity(int32_t previousCapacity) noexcept;

} // namespace cxxlox
