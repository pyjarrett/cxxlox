#pragma once

#include "common.hpp"
#include "token.hpp"

#include <string>

namespace cxxlox {

struct Scanner {
	explicit Scanner(const std::string& source);

	Token scanToken();

private:
	[[nodiscard]] bool isAtEnd();
	[[nodiscard]] char peek();
	[[nodiscard]] char peekNext();
	char advance();
	[[nodiscard]] bool match(char ch);
	[[nodiscard]] int64_t currentLexemeSize();
	void skipWhitespace();
	[[nodiscard]] Token makeToken(TokenType type);
	[[nodiscard]] Token errorToken(const char* message);
	[[nodiscard]] Token makeString();
	[[nodiscard]] Token number();
	[[nodiscard]] TokenType checkKeyword(int charsAlreadyMatched, std::string_view remaining, TokenType keyword);
	[[nodiscard]] TokenType identifierType();
	[[nodiscard]] Token identifier();

	std::string source;

	/// Start of the next token
	const char* start = nullptr;

	/// Current cursor position (either at `start`, or after `start`)
	const char* current = nullptr;

	uint32_t line = 1;
};

} // namespace cxxlox