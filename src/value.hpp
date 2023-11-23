#pragma once

#include "common.hpp"

namespace cxxlox {

enum class ValueType
{
	Bool,
	Nil,
	Number
};

// Could be done with std::variant.
struct Value {
	ValueType type;
	union
	{
		bool boolean;
		double number;
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

	[[nodiscard]] bool isNil() const {
		return type == ValueType::Nil;
	}

	[[nodiscard]] bool isBool() const {
		return type == ValueType::Bool;
	}

	[[nodiscard]] bool isNumber() const {
		return type == ValueType::Number;
	}

	[[nodiscard]] bool operator==(const Value& rhs) const {
		if (type != rhs.type) {
			return false;
		}
		switch (type) {
			case ValueType::Bool: return as.boolean == rhs.as.boolean;
			case ValueType::Number: return as.number == rhs.as.number;
			case ValueType::Nil: return true;
			default:
				CL_FATAL("Unknown value type.");
		}
		return false;
	}
};

void printValue(Value v);

} // namespace cxxlox
