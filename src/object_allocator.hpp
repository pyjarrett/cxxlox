#pragma once

#include "gc.hpp"
#include "memory.hpp"
#include "object.hpp"
#include <iostream> // FIXME: Not great to have in this file.
#include <type_traits>

// This file splits out the allocator because it needs the VM to do the
// allocation since I'm using a template instead of a macro, it needs to be
// visible.
namespace cxxlox {

template <typename T, typename... Args>
T* allocateObj(Args&&... args)
{
	static_assert(isObjFormat<T>(), "Type does not meet requirements to be an Obj-like type.");

	void* location = nullptr;
	location = realloc(location, 0, sizeof(T));
	T* t = new (location) T(std::forward<Args>(args)...);
	t->obj.type = typeOf<T>();
	GC::instance().track(reinterpret_cast<Obj*>(t));

#ifdef DEBUG_LOG_GC
	std::cout << "Allocate " << std::hex << location << " of " << sizeof(T) << " for " << objTypeToString(typeOf<T>())
			  << '\n';
#endif

	return t;
}

} // namespace cxxlox
