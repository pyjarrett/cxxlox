#pragma once

#include "chunk.hpp"
#include "common.hpp"
#include "table.hpp"
#include "value.hpp"

#include <iosfwd>

namespace cxxlox {

// Deviation: was OBJ_CLOSURE, OBJ_FUNCTION, OBJ_STRING
enum class ObjType
{
	Closure,
	Class,
	Instance,
	Function,
	Native,
	String,
	Upvalue,
};

template <typename T>
constexpr ObjType typeOf()
{
	return T::type;
}

[[nodiscard]] const char* objTypeToString(ObjType type);
[[nodiscard]] bool isObjType(Value value, ObjType type);

struct ObjString;
struct ObjFunction;
struct ObjClosure;
struct ObjClass;
struct ObjInstance;
struct ObjNative;
struct ObjUpvalue;

// An opaque header applied to all object subtypes to ensure every type has a
// type and a pointer to the next type.
struct Obj {
	ObjType type;

	// Tracks whether this object is marked for the garbage collector.
	bool isMarked = false;

	// Intrusive linked list pointer.  Used for tracking objects for the
	// garbage collector.
	Obj* next = nullptr;

	// Conversion to overlying types.
	[[nodiscard]] ObjString* toString();
	[[nodiscard]] ObjFunction* toFunction();
	[[nodiscard]] ObjClosure* toClosure();
	[[nodiscard]] ObjClass* toClass();
	[[nodiscard]] ObjInstance* toInstance();
	[[nodiscard]] ObjUpvalue* toUpvalue();
	[[nodiscard]] ObjNative* toNative();
};

std::ostream& operator<<(std::ostream& out, Obj* obj);

struct ObjFunction {
	static constexpr ObjType type = ObjType::Function;

	Obj obj;
	Chunk chunk;
	ObjString* name = nullptr;
	int32_t arity = 0;
	int32_t upvalueCount = 0;

	ObjFunction() = default;
	~ObjFunction() = default;

	// Should be dealing with pointers to ObjFunction only, so prohibit moving
	// and copying.
	ObjFunction(const ObjFunction&) = delete;
	ObjFunction& operator=(const ObjFunction&) = delete;

	ObjFunction(ObjFunction&&) = delete;
	ObjFunction& operator=(ObjFunction&&) = delete;

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

using NativeFunction = Value (*)(int argCount, Value* args);

// A function for calling native code.
struct ObjNative {
	static constexpr ObjType type = ObjType::Native;

	Obj obj;
	NativeFunction function = nullptr;
};

// Wraps an ObjFunction and tracks upvalues.
struct ObjClosure {
	static constexpr ObjType type = ObjType::Closure;

	Obj obj;

	// The function underlying this function.  Multiple closures might reference
	// the same function.
	ObjFunction* function = nullptr;

	// Closed over values which might on the stack above this function, or
	// stored on the heap.
	Vector<ObjUpvalue*> upvalues;

	explicit ObjClosure(ObjFunction* fn);
	~ObjClosure() = default;

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

struct ObjClass {
	static constexpr ObjType type = ObjType::Class;

	Obj obj;

	// Name for stack traces.
	ObjString* name;

	explicit ObjClass(ObjString* name);

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

struct ObjInstance {
	static constexpr ObjType type = ObjType::Instance;

	Obj obj;

	explicit ObjInstance(ObjClass* klass);

	ObjClass* klass;
	Table fields;

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

// Every ObjString owns its own characters.
struct ObjString {
	static constexpr ObjType type = ObjType::String;

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

// Tracks the location of an upvalue.
struct ObjUpvalue {
	static constexpr ObjType type = ObjType::Upvalue;

	Obj obj;

	// Pointer to location of an upvalue.  This might be on the stack or on
	// the heap.
	Value* location = nullptr;

	// When the upvalue is removed from the stack, this is the place to where
	// the value will be moved.  The upvalue is heap allocated, so this keeps
	// the value alive.  `location` will point to this value when this
	// occurs.
	Value closed = Value::makeNil();

	// The next lower (previous) pvalue in the stack.
	ObjUpvalue* next = nullptr;

	explicit ObjUpvalue(Value* slot);
	~ObjUpvalue() = default;

	[[nodiscard]] Obj* asObj() { return reinterpret_cast<Obj*>(this); }
};

[[nodiscard]] ObjString* copyString(const char* chars);

// Copies a string into an ObjString with ownership of the copied memory.
[[nodiscard]] ObjString* copyString(const char* chars, uint32_t length);

/// Take ownership of the given string.
[[nodiscard]] ObjString* takeString(char* chars, uint32_t length);

} // namespace cxxlox
