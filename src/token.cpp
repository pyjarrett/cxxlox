#include "token.hpp"

#include <cstring>

namespace cxxlox {

// Deviation: was in compiler.cpp
[[nodiscard]] bool identifiersEqual(Token* a, Token* b)
{
	if (a->length != b->length) {
		return false;
	}

	return std::memcmp(a->start, b->start, a->length) == 0;
}

}
