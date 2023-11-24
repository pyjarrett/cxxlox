#pragma once

#include "common.hpp"
#include "value.hpp"
#include "vm.hpp"
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace cxxlox {

extern VM vm;

enum class ObjType
{
	String,
};

struct Obj {
	ObjType type;
	Obj* next = nullptr;

	[[nodiscard]] ObjString* toString()
	{
		CL_ASSERT(type == ObjType::String);
		return reinterpret_cast<ObjString*>(this);
	}
};

// TODO: Eventually this might also take an arg pack.
template <typename T>
T* allocateObj(ObjType type)
{
	static_assert(offsetof(T, obj) == 0, "Obj must be the first member of the type.");
	static_assert(std::is_standard_layout_v<T>, "Type is not standard layout.");
	T* t = new T;
	t->obj.type = type;
	vm.track(reinterpret_cast<Obj*>(t));
	return t;
}

// Every ObjString owns its own characters.
struct ObjString {
	Obj obj;
	int length;
	char* chars;

	ObjString() = default;
	~ObjString();

	// Disable copy since memory is being managed by ObjString, which should
	// only be dynamically allocated.
	ObjString(const ObjString&) = delete;
	ObjString& operator=(const ObjString&) = delete;

	// Move could be allowed, but is explicitly disabled for now.
	ObjString(ObjString&&) = delete;
	ObjString& operator=(ObjString&&) = delete;

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

void printObj(Obj* obj);

[[nodiscard]] bool isObjType(Value value, ObjType type);

// Copies a string into an ObjString with ownership of the copied memory.
[[nodiscard]] ObjString* copyString(const char* chars, int length);

/// Take ownership of the given string.
[[nodiscard]] ObjString* takeString(char* chars, int length);

// Creating a string, taking ownership of the memory referred to by the passed
// character pointer.
[[nodiscard]] ObjString* allocateString(char* chars, int length);

} // namespace cxxlox
