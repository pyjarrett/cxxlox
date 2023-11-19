#include "compiler.hpp"

#include "scanner.hpp"

#include <iostream>

namespace cxxlox {

void compile(const std::string& source)
{
	// FIXME: This is very unsafe.
	// Maybe something with iterators over a string might be better?
	initScanner(source.data());

	// TODO: This is temporary code to drive the scanner.

	int previousLine = 0;
	while (true)
	{
		// Get next token
		Token token = scanToken();

		// Print next token with line number or a | for a continuation.
		if (token.line == previousLine) {
			std::cout << "  | ";
		}
		else {
			std::cout << token.line;
			previousLine = token.line;
		}

		// Print the token type and lexeme.

		// Stop if we reached an end of file token.
	}
}

}