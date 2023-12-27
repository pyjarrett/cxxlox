#pragma once

#include "common.hpp"

#include <string_view>

namespace cxxlox {

struct Scanner {
	/// Start of the next token
	const char* start = nullptr;

	/// Current cursor position (either at `start`, or after `start`)
	const char* current = nullptr;

	uint32_t line = 1;
};

enum class TokenType : uint8_t
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
	Var,
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
	// Lexeme
	const char* start = nullptr;
	uint32_t length = 0;

	// Line number where this appears.
	uint32_t line = 0;

	TokenType type = TokenType::Eof;

	[[nodiscard]] std::string_view view() const {
		return std::string_view(start, length);
	}
};
// TODO: int could change in size, replace with int32_t.
static_assert(sizeof(Token) == 24);
static_assert(alignof(Token) == 8);

[[nodiscard]] bool identifiersEqual(Token* a, Token* b);

void initScanner(const char* source);
[[nodiscard]] Token scanToken();

} // namespace cxxlox