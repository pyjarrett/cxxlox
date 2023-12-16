#pragma once

#include "common.hpp"

namespace cxxlox {

enum class ValueType
{
	Bool,
	Nil,
	Number,

	// A heap allocated object
	Obj,
};

// Object "base class" (for lack of a better term)
struct Obj;
struct ObjClosure;
struct ObjFunction;
struct ObjNative;
struct ObjString;

// Could be done with std::variant.
struct Value {
	ValueType type;
	union
	{
		bool boolean;
		double number;
		Obj* obj;
	} as;

	// Creation helpers
	[[nodiscard]] static Value makeBool(bool b);
	[[nodiscard]] static Value makeNumber(double d);
	[[nodiscard]] static Value makeString(ObjString* str);
	[[nodiscard]] static Value makeObj(Obj* obj);
	[[nodiscard]] static Value makeFunction(ObjFunction* fn);
	[[nodiscard]] static Value makeClosure(ObjClosure* closure);
	[[nodiscard]] static Value makeNative(ObjNative* fn);
	[[nodiscard]] static Value makeNil();

	// Runtime type conversions.  These should be checked with the associated
	// `is*` function before use.
	[[nodiscard]] bool toBool() const;
	[[nodiscard]] double toNumber() const;
	[[nodiscard]] Obj* toObj() const;

	// Runtime type checking.
	[[nodiscard]] bool isNil() const;
	[[nodiscard]] bool isBool() const;
	[[nodiscard]] bool isNumber() const;
	[[nodiscard]] bool isObj() const;

	[[nodiscard]] bool operator==(const Value& rhs) const;
};

void printValue(Value v);

} // namespace cxxlox
