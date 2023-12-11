#pragma once

#include "object.hpp"
#include "vm.hpp"
#include <type_traits>

// This file splits out the allocator because it needs the VM to do the
// allocation since I'm using a template instead of a macro, it needs to be
// visible.

namespace cxxlox {
// TODO: Eventually this might also take an arg pack.
template <typename T>
T* allocateObj(ObjType type)
{
	// TODO: Formalize this extern in a better way.
	static_assert(offsetof(T, obj) == 0, "Obj must be the first member of the type.");
	static_assert(std::is_standard_layout_v<T>, "Type is not standard layout.");
	T* t = new T;
	t->obj.type = type;
	VM::instance().track(reinterpret_cast<Obj*>(t));
	return t;
}
} // namespace cxxlox
