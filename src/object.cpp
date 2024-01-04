#include "object.hpp"

#include "object_allocator.hpp"
#include "vm.hpp"
#include <cstring> // for strnlen
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

ObjClass* Obj::toClass()
{
	CL_ASSERT(type == ObjType::Class);
	return reinterpret_cast<ObjClass*>(this);
}

ObjInstance* Obj::toInstance()
{
	CL_ASSERT(type == ObjType::Instance);
	return reinterpret_cast<ObjInstance*>(this);
}

ObjUpvalue* Obj::toUpvalue()
{
	CL_ASSERT(type == ObjType::Upvalue);
	return reinterpret_cast<ObjUpvalue*>(this);
}

ObjNative* Obj::toNative()
{
	CL_ASSERT(type == ObjType::Native);
	return reinterpret_cast<ObjNative*>(this);
}

ObjBoundMethod::ObjBoundMethod(Value receiver, ObjClosure* method)
: receiver(receiver)
, method(method)
{
	CL_ASSERT(method);
}

ObjClosure::ObjClosure(cxxlox::ObjFunction* fn)
{
	CL_ASSERT(fn);

	// Set up space for upvalues.
	upvalues.reserve(fn->upvalueCount);
	for (int32_t i = 0; i < fn->upvalueCount; ++i) {
		upvalues.push(nullptr);
	}
	function = fn;
}

ObjClass::ObjClass(ObjString* name) : name(name)
{
	CL_ASSERT(name);
}

ObjInstance::ObjInstance(cxxlox::ObjClass* klass)
: klass(klass)
{
	CL_ASSERT(klass);
}

// Deviation: using destructor instead of `freeObject`
ObjString::~ObjString()
{
	delete[] chars;
}

ObjUpvalue::ObjUpvalue(cxxlox::Value* slot)
{
	location = slot;
}

static std::ostream& operator<<(std::ostream& out, ObjFunction* fn)
{
	if (fn->name == nullptr) {
		// Top level function
		out << "<script>";
	} else {
		out << "<fn " << fn->name->chars << '>';
	}

	// Deviation: Show arity and number of upvalues.
	out << "(" << fn->arity;
	if (fn->upvalueCount > 0) {
		out << ", ^" << fn->upvalueCount;
	}
	out << ") ";
	return out;
}

const char* objTypeToString(ObjType type)
{
	switch (type) {
		case ObjType::Closure:
			return "Closure";
		case ObjType::Class:
			return "Class";
		case ObjType::Function:
			return "Function";
		case ObjType::Native:
			return "Native";
		case ObjType::String:
			return "String";
		case ObjType::Upvalue:
			return "Upvalue";
		default:
			return "Unknown type";
	}
}

std::ostream& operator<<(std::ostream& out, Obj* obj)
{
	switch (obj->type) {
		case ObjType::BoundMethod:
			out << obj->to<ObjBoundMethod>()->method->function;
			break;
		case ObjType::Function:
			out << obj->toFunction();
			break;
		case ObjType::Closure:
			out << obj->toClosure()->function;
			break;
		case ObjType::Class:
			out << obj->toClass()->name->chars;
			break;
		case ObjType::Instance:
			out << obj->toInstance()->klass->name->chars << " instance";
			break;
		case ObjType::Native:
			out << "<native fn>";
			break;
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			out << str->chars;
		} break;
		case ObjType::Upvalue:
			out << "upvalue";
			break;
	}
	return out;
}

bool isObjType(Value value, ObjType type)
{
	return value.isObj() && value.toObj()->type == type;
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
	ObjString* str = allocateObj<ObjString>();
	str->chars = chars;
	str->length = length;
	str->hash = hash;
	VM::instance().push(Value::makeString(str));
	VM::instance().intern(str);
	CL_UNUSED(VM::instance().pop());
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
