#include "scanner.hpp"

#include "common.hpp"
#include <cstring>
#include <string_view>

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
	return *scanner.current == '\0';
}

// Looks at the next character without advancing the scanner.
[[nodiscard]] static char peek()
{
	return *scanner.current;
}

// Looks ahead two characters without advancing the scanner.
[[nodiscard]] static char peekNext()
{
	if (isAtEnd()) {
		return '\0';
	}
	return scanner.current[1];
}

// Advance and return the previous character.
static char advance()
{
	++scanner.current;
	return scanner.current[-1];
}

// Advance ONLY if the current character is the given character.
[[nodiscard]] static bool match(char ch)
{
	if (*scanner.current != ch) {
		return false;
	}

	++scanner.current;
	return true;
}

[[nodiscard]] static int64_t currentLexemeSize()
{
	return std::distance(scanner.start, scanner.current);
}

static void skipWhitespace()
{
	while (true) {
		const char ch = peek();
		switch (ch) {
			case ' ':
				[[fallthrough]];
			case '\r':
				[[fallthrough]];
			case '\t':
				CL_UNUSED(advance());
				break;
			case '\n':
				++scanner.line;
				CL_UNUSED(advance());
				break;
			// Line comments aren't technically whitespace, but treat them as so.
			case '/':
				if (peekNext() == '/') {
					// Keep looking until a newline.
					while (!isAtEnd() && peek() != '\n') {
						CL_UNUSED(advance());
					}
				} else {
					// It's just a plain '/', so we found a non-whitespace character.
					return;
				}
				break;
			default:
				return;
		}
	}
}

[[nodiscard]] static Token makeToken(TokenType type)
{
	return Token {
		.type = type,
		.start = scanner.start,
		.length = static_cast<size_t>(currentLexemeSize()),
		.line = scanner.line,
	};
}

// Assumes that the lifetime of the `message` parameter does not need to
// be managed by this function.
[[nodiscard]] static Token errorToken(const char* message)
{
	return Token {
		.type = TokenType::Error,
		.start = message,
		.length = strlen(message), // FIXME: Shouldn't use this, just to get it working.
		.line = scanner.line,
	};
}

// This does not support escaped characters.
[[nodiscard]] static Token makeString()
{
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n') {
			++scanner.line;
		}
		advance();
	}

	// Advance past the last double quote.
	if (isAtEnd()) {
		return errorToken("Unterminated string.");
	}

	advance();
	return makeToken(TokenType::String);
}

[[nodiscard]] static bool isDigit(char c)
{
	return c >= '0' && c <= '9';
}

[[nodiscard]] static bool isAlpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

[[nodiscard]] static Token number()
{
	// Parse a number, which is like [:digit:]+([.][:digit:]*)?

	// Consume all leading digits.
	while (isDigit(peek())) {
		advance();
	}

	// Look for a decimal point, and then trailing digits.  If we're at the end,
	// then peek will be '\0'.
	if (peek() == '.') {
		// Consume the decimal point.
		advance();

		// Consume trailing digits.
		while (isDigit(peek())) {
			advance();
		}
	}

	// Rely on later code to parse the digit.
	return makeToken(TokenType::Number);
}

// Return the keyword type if the remaining text matches, or an identifier otherwise.
[[nodiscard]] static TokenType checkKeyword(int charsAlreadyMatched, std::string_view remaining, TokenType keyword)
{
	std::string_view inBuffer(scanner.start + charsAlreadyMatched, remaining.length());
	return (inBuffer == remaining) ? keyword : TokenType::Identifier;
}


// Determine the type of an identifier currently held by the scanner, it could
// be a keyword, or a name.
[[nodiscard]] static TokenType identifierType()
{
	// The scanner containers the current state of the next token.
	// Parse the type of the next token like it's a "trie" to look for keywords.
	switch (scanner.start[0]) {
		case 'a': return checkKeyword(1, "nd", TokenType::And);
		case 'c': return checkKeyword(1, "lass", TokenType::Class);
		case 'e': return checkKeyword(1, "lse", TokenType::Else);
			// More advanced case
		case 'f':
		{
			if (currentLexemeSize() < 2) {
				return TokenType::Identifier;
			}

			// Look at the next character.
			switch (scanner.start[1]) {
				case 'a': return checkKeyword(2, "lse", TokenType::False);
				case 'o': return checkKeyword(2, "r", TokenType::For);
				case 'u': return checkKeyword(2, "n", TokenType::Fun);
				default:
					return TokenType::Identifier;
			}
		}
		case 'i': return checkKeyword(1, "f", TokenType::If);
		case 'n': return checkKeyword(1, "il", TokenType::Nil);
		case 'o': return checkKeyword(1, "r", TokenType::Or);
		case 'p': return checkKeyword(1, "rint", TokenType::Print);
		case 'r': return checkKeyword(1, "eturn", TokenType::Return);
		case 's': return checkKeyword(1, "uper", TokenType::Super);
		case 't': {
			if (currentLexemeSize() < 2) {
				return TokenType::Identifier;
			}
			switch(scanner.start[1]) {
				case 'h': return checkKeyword(1, "his", TokenType::This);
				case 'r': return checkKeyword(1, "rue", TokenType::True);
				default:
					return TokenType::Identifier;
			}
		}
		case 'w': return checkKeyword(1, "hile", TokenType::While);
	}

	// If the identifier hasn't been matched against a keyword, then it is a name.
	return TokenType::Identifier;
}

// Parse an identifier or a keyword.  The token type will be determined by
// identifierType().
[[nodiscard]] static Token identifier()
{
	// We only end up in this function if we started with an alphabetic character
	// or a _, so all remaining characters can be alphanumeric or an underscore.
	while (isAlpha(peek()) || isDigit(peek())) {
		advance();
	}

	return makeToken(identifierType());
}

Token scanToken()
{
	// Skip possible whitespace between characters.
	skipWhitespace();

	// Reset the next token to start here.
	scanner.start = scanner.current;

	// Report and end-of-file token if there's no more source to look at.
	if (isAtEnd()) {
		return makeToken(TokenType::Eof);
	}

	// clang-format off
	// Here comes a GIGANTIC switch statement to process tokens.
	const char ch = advance();

	if (isDigit(ch)) return number();
	if (isAlpha(ch)) return identifier();

	switch (ch) {
		// Simple single character tokens.
		case '(': return makeToken(TokenType::LeftParen);
		case ')': return makeToken(TokenType::RightParen);
		case '{': return makeToken(TokenType::LeftBrace);
		case '}': return makeToken(TokenType::RightBrace);
		case ',': return makeToken(TokenType::Comma);
		case '.': return makeToken(TokenType::Dot);
		case ';': return makeToken(TokenType::Semicolon);
		case '+': return makeToken(TokenType::Plus);
		case '-': return makeToken(TokenType::Minus);
		case '*': return makeToken(TokenType::Star);
		case '/': return makeToken(TokenType::Slash);

		// Possibly two character tokens.
		case '!': return match('=') ? makeToken(TokenType::BangEqual) : makeToken(TokenType::Bang);
		case '=': return match('=') ? makeToken(TokenType::EqualEqual) : makeToken(TokenType::Equal);
		case '<': return match('=') ? makeToken(TokenType::LessEqual) : makeToken(TokenType::Less);
		case '>': return match('=') ? makeToken(TokenType::GreaterEqual) : makeToken(TokenType::Greater);

		case '"': return makeString();
		default:
			// The token wasn't recognized, so report an error.
			return errorToken("Unexpected character.");
	}
	// clang-format on

}

} // namespace cxxlox