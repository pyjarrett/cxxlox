#pragma once

#include <string_view>

namespace cxxlox {

struct Scanner {
	/// Start of the next token
	const char* start = nullptr;

	/// Current cursor position (either at `start`, or after `start`)
	const char* current = nullptr;

	int line = 1;
};

enum class TokenType
{
	LeftParen,
	RightParen,
	LeftBrace,
	RightBrace,

	Comma,
	Dot,
	Semicolon,

	Plus,
	Minus,
	Star,
	Slash,

	// One or two character
	Bang,
	BangEqual,
	Equal,
	EqualEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,

	// Literals
	Identifier,
	String,
	Number,

	// Keywords
	And,
	Or,

	If,
	Else,
	While,
	For,
	Return,

	Class,
	Fun,
	Print,

	Super,
	This,
	Nil,
	True,
	False,

	Error,
	Eof,
};

/// A small, value-based type to pass around which can be consumed by the parse
/// as an atomic element within the parsing process.
struct Token {
	TokenType type = TokenType::Eof;

	// Lexeme
	// TODO: This looks exactly like a `string_view`.
	const char* start = nullptr;
	size_t length = 0;

	// Line number where this appears.
	int line = 0;

	[[nodiscard]] std::string_view view() const {
		return std::string_view(start, length);
	}
};

void initScanner(const char* source);
[[nodiscard]] Token scanToken();

} // namespace cxxlox