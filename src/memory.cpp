#include "memory.hpp"

#include "object.hpp"
#include "value.hpp"
#include <iostream>

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

void markValue(Value* value)
{
	CL_ASSERT(value);
	if (value && value->isObj()) {
		markObject(value->toObj());
	}
}

void markObject(Obj* obj)
{
	if (obj == nullptr) {
		return;
	}
#ifdef DEBUG_LOG_GC
	std::cout << std::hex << obj << ' ';
	printValue(Value::makeObj(obj));
	std::cout << '\n';
#endif
	obj->marked = true;
}

} // namespace cxxlox
