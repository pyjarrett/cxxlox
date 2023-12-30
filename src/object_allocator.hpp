#pragma once

#include "memory.hpp"
#include "object.hpp"
#include "vm.hpp"
#include <iostream> // FIXME: Not great to have in this file.
#include <type_traits>

// This file splits out the allocator because it needs the VM to do the
// allocation since I'm using a template instead of a macro, it needs to be
// visible.
namespace cxxlox {

template <typename T>
[[nodiscard]] constexpr bool isObjFormat()
{
	// A leading `obj` member variable is used for garbage collection.
	return (offsetof(T, obj) == 0) && std::is_standard_layout_v<T>;
}

template <typename T, typename... Args>
T* allocateObj(Args&&... args)
{
	static_assert(isObjFormat<T>(), "Type does not meet requirements to be an Obj-like type.");

	void* location = nullptr;
	location = realloc(location, 0, sizeof(T));
	T* t = new (location) T(std::forward<Args>(args)...);
	t->obj.type = typeOf<T>();
	VM::instance().track(reinterpret_cast<Obj*>(t));

#ifdef DEBUG_LOG_GC
	std::cout << "Allocate " << std::hex << location << " of " << sizeof(T) << " for " << static_cast<int>(typeOf<T>())
			  << '\n';
#endif

	return t;
}

template <typename T>
void freeObj(Obj* obj)
{
	T* t = reinterpret_cast<T*>(obj);
	t->~T();
	CL_UNUSED(realloc((void*)obj, sizeof(T), 0));
}

} // namespace cxxlox
