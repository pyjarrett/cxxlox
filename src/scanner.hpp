#pragma once

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
	TokenType type;

	// Lexeme
	// TODO: This looks exactly like a `string_view`.
	const char* start;
	size_t length;

	// Line number where this appears.
	int line;
};

void initScanner(const char* source);
[[nodiscard]] Token scanToken();

} // namespace cxxlox