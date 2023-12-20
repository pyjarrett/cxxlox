#include "memory.hpp"

namespace cxxlox {

void* realloc(void* pointer, size_t oldSize, size_t newSize)
{
	if (newSize == 0) {
		std::free(pointer);
		return nullptr;
	}

	// Realloc
	void* result = std::realloc(pointer, newSize);
	if (result == nullptr) {
		// Failed allocation.
		CL_FATAL("Unable to allocate additional memory.");
	}

	return result;
}

} // namespace cxxlox
