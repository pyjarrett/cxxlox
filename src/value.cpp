#include "value.hpp"

#include "object.hpp"
#include <cstring>
#include <iostream>

namespace cxxlox {

void printValue(Value v)
{
	switch (v.type) {
		case ValueType::Bool:
			std::cout << ((v.as.boolean) ? "true" : "false");
			break;
		case ValueType::Nil:
			std::cout << "nil";
			break;
		case ValueType::Number:
			std::cout << v.as.number;
			break;
		case ValueType::Obj:
			printObj(v.toObj());
			break;
		default:
			CL_FATAL("Unhandled enum type.");
	}
}

bool Value::operator==(const Value& rhs) const
{
	if (type != rhs.type) {
		return false;
	}
	switch (type) {
		case ValueType::Bool:
			return as.boolean == rhs.as.boolean;
		case ValueType::Number:
			return as.number == rhs.as.number;
		case ValueType::Nil:
			return true;
		case ValueType::Obj: {
			if (!this->isObj() || !rhs.isObj()) {
				return false;
			}
			if (this->toObj()->type != ObjType::String || rhs.toObj()->type != ObjType::String) {
				return false;
			}
			ObjString* left = this->toObj()->toString();
			ObjString* right = rhs.toObj()->toString();
			return (left->length == right->length && std::memcmp(left->chars, right->chars, left->length) == 0);
		}
		default:
			CL_FATAL("Unknown value type.");
	}
}

} // namespace cxxlox