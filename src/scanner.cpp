#include "scanner.hpp"

#include "common.hpp"

namespace cxxlox {

static Scanner scanner;

void initScanner(const char* source)
{
	scanner.start = source;
	scanner.current = source;
	scanner.line = 1;
}

[[nodiscard]] static bool isAtEnd()
{
	// Assumes input source is null terminated.
	return scanner.current == nullptr;
}

[[nodiscard]] static Token makeToken(TokenType type)
{
	return Token {
		.type = type,
		.start = scanner.start,
		.length = static_cast<size_t>(reinterpret_cast<uintptr_t>(scanner.current) - reinterpret_cast<uintptr_t>(scanner.start)),
		.line = scanner.line,
	};
}

Token scanToken()
{
	// Reset the next token to start here.
	scanner.start = scanner.current;

	// Report and end-of-file token if there's no more source to look at.
	if (isAtEnd()) {
		return makeToken(TokenType::Eof);
	}

	// The token wasn't recognized, so report an error.
	return errorToken("Unexpected character.");
}

} // namespace cxxlox