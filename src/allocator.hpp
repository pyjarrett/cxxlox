#pragma once

#include "object.hpp"
#include "vm.hpp"
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

	T* t = new T(std::forward<Args>(args)...);
	t->obj.type = typeOf<T>();
	VM::instance().track(reinterpret_cast<Obj*>(t));
	return t;
}
} // namespace cxxlox
