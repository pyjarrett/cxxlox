#include "parser.hpp"

#include <format>
#include <iostream>

namespace cxxlox {

///////////////////////////////////////////////////////////////////////////////
// Error handling
///////////////////////////////////////////////////////////////////////////////
void Parser::errorAt(const Token& token, const char* message)
{
	// Don't emit more errors if the parser is already in a panic state.
	if (panicMode) {
		return;
	}
	panicMode = true;
	std::cerr << std::format("[line {}] Error", token.line);

	if (token.type == TokenType::Eof) {
		std::cerr << " at the end.\n";
	} else if (token.type == TokenType::Error) {
		// some sort of error token...
	} else {
		std::cerr << " at " << token.view();
	}

	std::cerr << ": " << message << '\n';

	// Deviation from Lox to provide more in-depth error analysis.
	constexpr auto kMaxContextLength = 80;
	const char* cursor = token.start + token.length;
	uint32_t contextLength = token.length;
	for (int i = 0; i < kMaxContextLength && *cursor != '\0'; ++i) {
		++cursor;
		++contextLength;
	}
	std::cerr << "Context following error:\n"
			  << "    " << std::string_view(token.start, contextLength) << '\n';

	hadError = true;
}

void Parser::errorAtCurrent(const char* message)
{
	errorAt(current, message);
}

void Parser::error(const char* message)
{
	errorAt(previous, message);
}

} // namespace cxxlox