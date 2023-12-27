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

// Finds the next "known good" point if the parser is in a bad state.  This
// reduces cascading errors.
void Parser::synchronize()
{
	CL_ASSERT(panicMode);
	
	panicMode = false;
	while (current.type != TokenType::Eof) {
		// Semicolons terminate statements, so that means this might be a good spot.
		if (previous.type == TokenType::Semicolon) {
			return;
		}

		// Control flow and declarations are another good place to try to parse again.
		switch (current.type) {
			case TokenType::Class:
				[[fallthrough]];
			case TokenType::Fun:
				[[fallthrough]];
			case TokenType::Var:
				[[fallthrough]];
			case TokenType::For:
				[[fallthrough]];
			case TokenType::If:
				[[fallthrough]];
			case TokenType::While:
				[[fallthrough]];
			case TokenType::Print:
				[[fallthrough]];
			case TokenType::Return:
				return;
			default:
				break;
		}

		// Move forward and look again for a good point to try parsing again.
		advance();
	}
}

///////////////////////////////////////////////////////////////////////////////
// Token operations
///////////////////////////////////////////////////////////////////////////////
void Parser::advance()
{
	previous = current;

	// Skip through error tokens until we arrive at a good point.
	while (true) {
		current = scanToken();
		if (current.type != TokenType::Error) {
			break;
		}

		errorAtCurrent(current.start);
	}
}

// Expect the next token to be a given type, move along if it is, otherwise
// emit an error message.
void Parser::consume(TokenType type, const char* message)
{
	if (current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

// See if the current token is the given type.
bool Parser::check(TokenType type) const
{
	return current.type == type;
}

// Advance and return true if the current token type is found, false otherwise
bool Parser::match(TokenType type)
{
	if (!check(type)) {
		return false;
	}

	advance();
	return true;
}

} // namespace cxxlox
