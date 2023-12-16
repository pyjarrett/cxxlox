#include "object.hpp"

#include "allocator.hpp"
#include "vm.hpp"

#include <cstring>  // for strnlen
#include <iostream>

namespace cxxlox {

ObjString* Obj::toString()
{
	CL_ASSERT(type == ObjType::String);
	return reinterpret_cast<ObjString*>(this);
}

ObjFunction* Obj::toFunction()
{
	CL_ASSERT(type == ObjType::Function);
	return reinterpret_cast<ObjFunction*>(this);
}

ObjClosure* Obj::toClosure()
{
	CL_ASSERT(type == ObjType::Closure);
	return reinterpret_cast<ObjClosure*>(this);
}

ObjNative* Obj::toNative()
{
	CL_ASSERT(type == ObjType::Native);
	return reinterpret_cast<ObjNative*>(this);
}

// Deviation: using destructor instead of `freeObject`
ObjString::~ObjString()
{
	delete[] chars;
}

static void printFunction(ObjFunction* fn) {
	if (fn->name == nullptr) {
		// Top level function
		std::cout << "<script>";
	}
	else {
		std::cout << "<fn " << fn->name->chars << '>';
	}
}

void printObj(Obj* obj)
{
	switch (obj->type) {
		case ObjType::Function: {
			printFunction(obj->toFunction());
		} break;
		case ObjType::Closure: {
			printFunction(obj->toClosure()->function);
		} break;
		case ObjType::Native: {
			std::cout << "<native fn>";
		} break;
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			std::cout << str->chars;
		} break;
	}
}

bool isObjType(Value value, ObjType type)
{
	return value.isObj() && value.toObj()->type == type;
}

// Deviation: was `newClosure`
ObjClosure* makeClosure()
{
	ObjClosure* closure = allocateObj<ObjClosure>(ObjType::Closure);
	return closure;
}

// Deviation: was `newFunction`
ObjFunction* makeFunction()
{
	ObjFunction* function = allocateObj<ObjFunction>(ObjType::Function);
	function->arity = 0;
	function->name = nullptr;
	return function;
}

// FNV-1 hash
[[nodiscard]] static uint32_t hashString(const char* chars, uint32_t length)
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
[[nodiscard]] ObjString* allocateString(char* chars, uint32_t length, uint32_t hash)
{
	ObjString* str = allocateObj<ObjString>(ObjType::String);
	str->chars = chars;
	str->length = length;
	str->hash = hash;
	VM::instance().intern(str);
	return str;
}

ObjString* copyString(const char* chars)
{
	constexpr size_t kMaxStringLength = 4096;

	if (chars == nullptr) {
		chars = "";
	}
	return copyString(chars, strnlen(chars, kMaxStringLength));
}

ObjString* copyString(const char* chars, uint32_t length)
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

ObjString* takeString(char* chars, uint32_t length)
{
	const uint32_t hash = hashString(chars, length);
	if (ObjString* interned = VM::instance().lookup(chars, length, hash)) {
		delete[] chars;
		return interned;
	}
	return allocateString(chars, length, hash);
}

} // namespace cxxlox
