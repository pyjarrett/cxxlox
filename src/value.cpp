#include "value.hpp"

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
		default:
			CL_FATAL("Unhandled enum type.");
	}
}

} // namespace cxxlox