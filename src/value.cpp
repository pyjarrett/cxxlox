#include "value.hpp"

#include "object.hpp"
#include <cstring>
#include <iostream>

namespace cxxlox {

Value Value::makeBool(bool b)
{
	Value v {.type = ValueType::Bool};
	v.as.boolean = b;
	return v;
}

Value Value::makeNumber(double d)
{
	Value v {.type = ValueType::Number};
	v.as.number = d;
	return v;
}

Value Value::makeString(ObjString* str)
{
	Value v {.type = ValueType::Obj};
	v.as.obj = reinterpret_cast<Obj*>(str);
	return v;
}

Value Value::makeObj(Obj* obj)
{
	Value v {.type = ValueType::Obj};
	v.as.obj = obj;
	return v;
}

Value Value::makeFunction(ObjFunction* fn)
{
	Value v {.type = ValueType::Obj};
	v.as.obj = reinterpret_cast<Obj*>(fn);
	return v;
}

Value Value::makeClosure(ObjClosure* fn)
{
	Value v {.type = ValueType::Obj};
	v.as.obj = reinterpret_cast<Obj*>(fn);
	return v;
}

Value Value::makeNative(ObjNative* fn)
{
	Value v {.type = ValueType::Obj};
	v.as.obj = reinterpret_cast<Obj*>(fn);
	return v;
}

Value Value::makeNil()
{
	return {.type = ValueType::Nil};
}

bool Value::toBool() const
{
	CL_ASSERT(type == ValueType::Bool);
	return as.boolean;
}

double Value::toNumber() const
{
	CL_ASSERT(type == ValueType::Number);
	return as.number;
}

Obj* Value::toObj() const
{
	CL_ASSERT(type == ValueType::Obj);
	CL_ASSERT(as.obj);
	return as.obj;
}

bool Value::isNil() const
{
	return type == ValueType::Nil;
}

bool Value::isBool() const
{
	return type == ValueType::Bool;
}

bool Value::isNumber() const
{
	return type == ValueType::Number;
}

bool Value::isObj() const
{
	return type == ValueType::Obj;
}

std::ostream& operator<<(std::ostream& out, Value v) {
	switch (v.type) {
		case ValueType::Bool:
			out << ((v.as.boolean) ? "true" : "false");
			break;
		case ValueType::Nil:
			out << "nil";
			break;
		case ValueType::Number:
			out << v.as.number;
			break;
		case ValueType::Obj:
			out << v.toObj();
			break;
		default:
			CL_FATAL("Unhandled enum type.");
	}
	return out;
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