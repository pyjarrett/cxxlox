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
	BoundMethod,
	Class,
	Closure,
	Function,
	Instance,
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

// struct Obj;
struct ObjClass;
struct ObjClosure;
struct ObjFunction;
struct ObjInstance;
struct ObjNative;
struct ObjString;
struct ObjUpvalue;

// Obj-like types are heap allocated and stored inside a `Obj*` within Value,
// so reinterpret_cast to the real type must work correctly.
//
// This relies on them being "standard layout", having a leading `obj` header to
// track the type and garbage collection utilities, and having an associate
// `static ObjType type` static member.
template <typename T>
[[nodiscard]] constexpr bool isObjFormat()
{
	// clang-format off
	return (offsetof(T, obj) == 0)
		&& std::is_standard_layout_v<T>
		&& std::is_same_v<decltype(typeOf<T>()), ObjType>;
	// clang-format on
}

// C is less strict about pointer conversions, so use a template in C++ here,
// to also get the format check before reinterpret_cast.
template <typename T>
[[nodiscard]] Obj* asObj(T* t)
{
	static_assert(isObjFormat<T>());
	return reinterpret_cast<Obj*>(t);
}

// An opaque header applied to all object subtypes to ensure every type has a
// type and a pointer to the next type.
struct Obj {
	ObjType type;

	// Tracks whether this object is marked for the garbage collector.
	bool isMarked = false;

	// Intrusive linked list pointer.  Used for tracking objects for the
	// garbage collector.
	Obj* next = nullptr;

	template <typename T>
	[[nodiscard]] bool is() const
	{
		static_assert(isObjFormat<T>());
		return type == typeOf<T>();
	}

	template <typename T>
	[[nodiscard]] T* to()
	{
		static_assert(isObjFormat<T>());

		CL_ASSERT(type == typeOf<T>());
		return reinterpret_cast<T*>(this);
	}

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
};

using NativeFunction = Value (*)(int argCount, Value* args);

// A function for calling native code.
struct ObjNative {
	static constexpr ObjType type = ObjType::Native;

	Obj obj;
	NativeFunction function = nullptr;
};

struct ObjBoundMethod {
	static constexpr ObjType type = ObjType::BoundMethod;

	Obj obj;

	Value receiver;
	ObjClosure* method = nullptr;

	ObjBoundMethod(Value receiver, ObjClosure* method);
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
};

struct ObjClass {
	static constexpr ObjType type = ObjType::Class;

	Obj obj;

	// Name for stack traces.
	ObjString* name;

	Table methods;

	explicit ObjClass(ObjString* name);
};

struct ObjInstance {
	static constexpr ObjType type = ObjType::Instance;

	Obj obj;

	ObjClass* klass;
	Table fields;

	explicit ObjInstance(ObjClass* klass);
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
};

[[nodiscard]] ObjString* copyString(const char* chars);

// Copies a string into an ObjString with ownership of the copied memory.
[[nodiscard]] ObjString* copyString(const char* chars, uint32_t length);

/// Take ownership of the given string.
[[nodiscard]] ObjString* takeString(char* chars, uint32_t length);

} // namespace cxxlox
