#pragma once

#include "scanner.hpp"

namespace cxxlox {

struct Parser {
	///////////////////////////////////////////////////////////////////////////////
	// Error handling
	///////////////////////////////////////////////////////////////////////////////
	void errorAt(const Token& token, const char* message);
	void errorAtCurrent(const char* message);
	void error(const char* message);

	Token current;
	Token previous;

	bool hadError = false;

	// Flag to set on parser error to allow it to resync without blasting out
	// innumerable errors while finding a synchronization point.
	bool panicMode = false;
};

} // namespace cxxlox
