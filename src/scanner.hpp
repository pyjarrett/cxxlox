#pragma once

#include "common.hpp"
#include "token.hpp"

namespace cxxlox {

struct Scanner {
	/// Start of the next token
	const char* start = nullptr;

	/// Current cursor position (either at `start`, or after `start`)
	const char* current = nullptr;

	uint32_t line = 1;
};

void initScanner(const char* source);
[[nodiscard]] Token scanToken();

} // namespace cxxlox