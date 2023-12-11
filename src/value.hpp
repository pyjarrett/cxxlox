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

	[[nodiscard]] static Value makeBool(bool b)
	{
		Value v {.type = ValueType::Bool};
		v.as.boolean = b;
		return v;
	}

	[[nodiscard]] static Value makeNumber(double d)
	{
		Value v {.type = ValueType::Number};
		v.as.number = d;
		return v;
	}

	[[nodiscard]] static Value makeString(ObjString* str)
	{
		Value v {.type = ValueType::Obj};
		v.as.obj = reinterpret_cast<Obj*>(str);
		return v;
	}

	[[nodiscard]] static Value makeObj(Obj* obj)
	{
		Value v {.type = ValueType::Obj};
		v.as.obj = obj;
		return v;
	}

	[[nodiscard]] static Value makeFunction(ObjFunction* fn)
	{
		Value v {.type = ValueType::Obj};
		v.as.obj = reinterpret_cast<Obj*>(fn);
		return v;
	}

	[[nodiscard]] static Value makeNative(ObjNative* fn)
	{
		Value v {.type = ValueType::Obj};
		v.as.obj = reinterpret_cast<Obj*>(fn);
		return v;
	}

	static Value makeNil() { return {.type = ValueType::Nil}; }

	// Trying explicit casts here.
	[[nodiscard]] bool toBool() const
	{
		CL_ASSERT(type == ValueType::Bool);
		return as.boolean;
	}

	[[nodiscard]] double toNumber() const
	{
		CL_ASSERT(type == ValueType::Number);
		return as.number;
	}

	[[nodiscard]] Obj* toObj() const {
		CL_ASSERT(type == ValueType::Obj);
		CL_ASSERT(as.obj);
		return as.obj;
	}

	[[nodiscard]] bool isNil() const {
		return type == ValueType::Nil;
	}

	[[nodiscard]] bool isBool() const {
		return type == ValueType::Bool;
	}

	[[nodiscard]] bool isNumber() const {
		return type == ValueType::Number;
	}

	[[nodiscard]] bool isObj() const {
		return type == ValueType::Obj;
	}

	[[nodiscard]] bool operator==(const Value& rhs) const;
};

void printValue(Value v);

} // namespace cxxlox
