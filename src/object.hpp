#pragma once

#include "chunk.hpp"
#include "common.hpp"
#include "value.hpp"
#include "vm.hpp"

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace cxxlox {

// Deviation: was OBJ_FUNCTION, OBJ_STRING
enum class ObjType
{
	Function,
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

struct ObjFunction {
	Obj obj;
	int32_t arity = 0;
	Chunk chunk;
	ObjString* name;

	ObjFunction() = default;
	~ObjFunction() = default;

	// Should be dealing with pointers to ObjFunction only, so prohibit moving
	// and copying.
	ObjFunction(const ObjFunction&) = delete;
	ObjFunction& operator=(const ObjFunction&) = delete;

	ObjFunction(ObjFunction&&) = delete;
	ObjFunction& operator=(ObjFunction&&) = delete;
};

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

// Every ObjString owns its own characters.
struct ObjString {
	Obj obj;
	int length = 0;
	char* chars = nullptr;
	uint32_t hash = 0;

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

[[nodiscard]] ObjFunction* makeFunction();

// Copies a string into an ObjString with ownership of the copied memory.
[[nodiscard]] ObjString* copyString(const char* chars, uint32_t length);

/// Take ownership of the given string.
[[nodiscard]] ObjString* takeString(char* chars, uint32_t length);

} // namespace cxxlox
