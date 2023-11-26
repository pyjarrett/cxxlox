#include "object.hpp"

#include "vm.hpp"

#include <iostream>

namespace cxxlox {

ObjString::~ObjString()
{
	delete[] chars;
}

void printObj(Obj* obj)
{
	switch (obj->type) {
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			std::cout << str->chars;
		}
	}
}

bool isObjType(Value value, ObjType type)
{
	return value.isObj() && value.toObj()->type == type;
}

// FNV-1 hash
[[nodiscard]] static uint32_t hashString(const char* chars, int length)
{
	CL_ASSERT(chars != nullptr);
	uint32_t hash = 2166136261u;
	for (int i = 0; i < length; ++i) {
		hash ^= static_cast<uint32_t>(chars[i]);
		hash *= 16777619;
	}
	return hash;
}

// Creating a string, taking ownership of the memory referred to by the passed
// character pointer.
[[nodiscard]] ObjString* allocateString(char* chars, int length, uint32_t hash)
{
	ObjString* str = allocateObj<ObjString>(ObjType::String);
	str->chars = chars;
	str->length = length;
	str->hash = hash;
	VM::instance().intern(str);
	return str;
}

ObjString* copyString(const char* chars, int length)
{
	const uint32_t hash = hashString(chars, length);
	if (ObjString* interned = VM::instance().lookup(chars, length, hash)) {
		return interned;
	}

	// Also allocate space for a null terminator.
	char* copiedChars = new char[length + 1];
	memcpy(copiedChars, chars, length);
	copiedChars[length] = '\0';

	return allocateString(copiedChars, length, hash);
}

ObjString* takeString(char* chars, int length)
{
	const uint32_t hash = hashString(chars, length);
	if (ObjString* interned = VM::instance().lookup(chars, length, hash)) {
		delete[] chars;
		return interned;
	}
	return allocateString(chars, length, hash);
}

} // namespace cxxlox
