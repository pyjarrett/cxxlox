#include "compiler.hpp"

#include "chunk.hpp"
#include "object.hpp"
#include "scanner.hpp"

#ifdef DEBUG_PRINT_CODE
	#include "debug.hpp"
#endif

#include <cassert>
#include <format>
#include <iostream>

namespace cxxlox {

struct Parser {
	Token current;
	Token previous;

	bool hadError = false;

	// Flag to set on parser error to allow it to resync without blasting out
	// innumerable errors while finding a synchronization point.
	bool panicMode = false;
};

// Use enum value auto-incrementing to decide precedence levels.
enum Precedence
{
	PREC_NONE,
	PREC_ASSIGNMENT, // =
	PREC_OR,         // or
	PREC_AND,        // and
	PREC_EQUALITY,   // ==
	PREC_COMPARISON, // < > <= >=
	PREC_TERM,       // + -
	PREC_FACTOR,     // * /
	PREC_UNARY,      // ! -
	PREC_CALL,       // . ()
	PREC_PRIMARY
};

// We (sadly) maintain inner state in this module, so all parse functions are
// just plain functions with no parameters because of the global state... :(
using ParseFn = void (*)();

// Pratt parsing rule.
struct ParseRule {
	// Function to use when encountering the key's token type as a prefix.
	ParseFn prefix;

	// Function to use when encountering the key's token type as a infix operator.
	ParseFn infix;

	Precedence precedence;
};

static Parser parser;
static Chunk* compilingChunk;

// C++ doesn't have C99 array designated initializers.  Emulate this.  This also
// hides that TokenType can't be directly converted to an index without a cast.
struct PrattRuleMap {
	struct PrattRuleRow {
		TokenType type;
		ParseRule rule;
	};

	PrattRuleMap(std::initializer_list<PrattRuleRow> items)
	{
		rules_ = new ParseRule[items.size()];

		// Require the rules to be in order.  This also ensures that every token
		// type gets covered and none forgotten.
		int nextRuleIndex = 0;
		for (const auto& row : items) {
			const int index = static_cast<int>(row.type);
			CL_ASSERT(index == nextRuleIndex);
			++nextRuleIndex;
			rules_[index] = row.rule;
		}
	}

	~PrattRuleMap() { delete[] rules_; }

	PrattRuleMap(const PrattRuleMap&) = delete;
	PrattRuleMap& operator=(const PrattRuleMap&) = delete;

	PrattRuleMap(PrattRuleMap&&) = delete;
	PrattRuleMap& operator=(PrattRuleMap&&) = delete;

	const ParseRule* operator[](TokenType token) const { return &rules_[static_cast<int>(token)]; }

private:
	ParseRule* rules_ = nullptr;
};

static void declaration();
static void statement();
static void expression();
static const ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void consume(TokenType type, const char* errorMessage);
static uint8_t makeConstant(Value value);
static void emitBytes(uint8_t byte1, uint8_t byte2);

static void errorAt(const Token& token, const char* message)
{
	// Don't emit more errors if the parser is already in a panic state.
	if (parser.panicMode) {
		return;
	}
	parser.panicMode = true;
	std::cerr << std::format("[line {}] Error", token.line);

	if (token.type == TokenType::Eof) {
		std::cerr << " at the end.\n";
	} else if (token.type == TokenType::Error) {
		// some sort of error token...
	} else {
		std::cerr << " at " << token.view() << '\n';
	}

	std::cerr << ": " << message << '\n';

	parser.hadError = true;
}

static void errorAtCurrent(const char* message)
{
	errorAt(parser.current, message);
}

static void error(const char* message)
{
	errorAt(parser.previous, message);
}

static void advance()
{
	parser.previous = parser.current;

	// Skip through error tokens until we arrive at a good point.
	while (true) {
		parser.current = scanToken();
		if (parser.current.type != TokenType::Error) {
			break;
		}

		errorAtCurrent(parser.current.start);
	}
}

static void parsePrecedence(Precedence precedence)
{
	// Read the token for the rule we want to act on.
	advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (!prefixRule) {
		error("Expected an expression.");
		return;
	}
	prefixRule();

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule();
	}
}

[[nodiscard]] static uint8_t identifierConstant(Token* name)
{
	return makeConstant(Value::makeObj(copyString(name->start, name->length)->asObj()));
}

// Look for a variable, returning the index in the constants map.
[[nodiscard]] static uint8_t parseVariable(const char* errorMessage)
{
	consume(TokenType::Identifier, errorMessage);
	return identifierConstant(&parser.previous);
}

static void defineVariable(uint8_t global)
{
	emitBytes(OP_DEFINE_GLOBAL, global);
}

// Expect the next token to be a given type, move along if it is, otherwise
// emit an error message.
static void consume(TokenType type, const char* message)
{
	if (parser.current.type == type) {
		advance();
		return;
	}

	errorAtCurrent(message);
}

// See if the current token is the given type.
[[nodiscard]] static bool check(TokenType type)
{
	return parser.current.type == type;
}

// Advance and return true if the current token type is found, false otherwise
[[nodiscard]] static bool match(TokenType type)
{
	if (!check(type)) {
		return false;
	}

	advance();
	return true;
}

[[nodiscard]] Chunk* currentChunk()
{
	return compilingChunk;
}

static void emitByte(uint8_t byte)
{
	currentChunk()->write(byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

[[nodiscard]] static uint8_t makeConstant(Value value)
{
	const int constant = currentChunk()->addConstant(value);
	if (constant > std::numeric_limits<uint8_t>::max()) {
		error("Too many constants in one chunk.");
	}
	return static_cast<uint8_t>(constant);
}

static void emitConstant(Value value)
{
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void emitReturn()
{
	emitByte(OP_RETURN);
}

static void endCompiling()
{
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(*currentChunk(), "code");
	}
#endif
}

static void binary()
{
	const TokenType operatorType = parser.previous.type;
	const ParseRule* rule = getRule(operatorType);
	// Parse the next level up, since all our binary rules are left associative.
	// This would be different if right-associative operators were handled, and
	// we would use the same level of precedence again here.
	parsePrecedence(Precedence(rule->precedence + 1));
	switch (operatorType) {
		case TokenType::Plus:
			emitByte(OP_ADD);
			break;
		case TokenType::Minus:
			emitByte(OP_SUBTRACT);
			break;
		case TokenType::Star:
			emitByte(OP_MULTIPLY);
			break;
		case TokenType::Slash:
			emitByte(OP_DIVIDE);
			break;
		case TokenType::EqualEqual:
			emitByte(OP_EQUAL);
			break;
		case TokenType::BangEqual:
			emitBytes(OP_EQUAL, OP_NOT);
			break;
		case TokenType::Less:
			emitByte(OP_LESS);
			break;
		case TokenType::LessEqual:
			emitBytes(OP_EQUAL, OP_NOT);
			break;
		case TokenType::Greater:
			emitByte(OP_GREATER);
			break;
		case TokenType::GreaterEqual:
			emitBytes(OP_EQUAL, OP_NOT);
			break;
		default:
			CL_ASSERT(false); // Unknown operator type.
	}
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

static void varDeclaration()
{
	const uint8_t global = parseVariable("Expected a variable name.");
	if (match (TokenType::Equal)) {
		expression();
	}else {
		emitByte(OP_NIL);
	}
	consume(TokenType::Semicolon, "Expected a ';' after a variable declaration.");
	defineVariable(global);
}

// Parse a print statement, assuming the previous token is "print".
static void printStatement()
{
	expression();
	consume(TokenType::Semicolon, "Expected a ';' after print statement.");
	emitByte(OP_PRINT);
}

// Finds the next "known good" point if the parser is in a bad state.  This
// reduces cascading errors.
static void synchronize()
{
	CL_ASSERT(parser.panicMode);

	parser.panicMode = false;
	while (parser.current.type != TokenType::Eof) {
		// Semicolons terminate statements, so that means this might be a good spot.
		if (parser.previous.type == TokenType::Semicolon) {
			return;
		}

		// Control flow and declarations are another good place to try to parse again.
		switch (parser.current.type) {
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

static void expressionStatement()
{
	expression();
	consume(TokenType::Semicolon, "Expected a ';' after expression.");
	emitByte(OP_POP);
}

static void declaration()
{
	if (match(TokenType::Var)) {
		varDeclaration();
	} else {
		statement();
	}

	// The parser could be in an error state, if so, then synchronize to a reasonable
	// "known good" point.
	if (parser.panicMode) {
		synchronize();
	}
}

static void statement()
{
	if (match(TokenType::Print)) {
		printStatement();
	} else {
		expressionStatement();
	}
}

// Grouping expression like "(expr)".  Assumes the leading "(" has already been
// encountered.
static void grouping()
{
	expression();
	consume(TokenType::RightParen, "Expected ')' after expression.");
}

static void unary()
{
	const TokenType operatorType = parser.previous.type;

	// Compile the expression the unary applies to.
	parsePrecedence(PREC_UNARY);

	// Emit the operation AFTER the expression, since the expression should be
	// below this operand to have it applied to that expression's result.
	switch (operatorType) {
		case TokenType::Minus:
			emitByte(OP_NEGATE);
			break;
		case TokenType::Bang:
			emitByte(OP_NOT);
			break;
		default:
			CL_FATAL("Unexpected unary operation token.");
	}
}

static void number()
{
	double value;
	const auto result = std::from_chars(parser.previous.start, parser.previous.start + parser.previous.length, value);

	if (result.ec != std::errc {}) {
		CL_ASSERT(false); // Invalid conversion.
		value = 0.0;
	}
	emitConstant(Value::makeNumber(value));
}

static void string()
{
	const std::string_view previous = parser.previous.view();
	const std::string_view withoutQuotes = previous.substr(1, previous.length() - 2);
	emitConstant(Value::makeString(copyString(withoutQuotes.data(), withoutQuotes.length())));
}

static void literal()
{
	switch (parser.previous.type) {
		case TokenType::Nil:
			emitByte(OP_NIL);
			break;
		case TokenType::True:
			emitByte(OP_TRUE);
			break;
		case TokenType::False:
			emitByte(OP_FALSE);
			break;
		default:
			CL_FATAL("Unexpected literal type.");
	}
}

// clang-format off
static PrattRuleMap rules = {
	{TokenType::LeftParen, {grouping, nullptr, PREC_NONE}},
	{TokenType::RightParen, {nullptr, nullptr, PREC_NONE}},
	{TokenType::LeftBrace, {nullptr, nullptr, PREC_NONE}},
	{TokenType::RightBrace, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Comma, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Dot, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Semicolon, {nullptr, nullptr, PREC_NONE}},

	{TokenType::Plus, {nullptr, binary, PREC_TERM}},
	{TokenType::Minus, {unary, binary, PREC_TERM}},
	{TokenType::Star, {nullptr, binary, PREC_FACTOR}},
	{TokenType::Slash, {nullptr, binary, PREC_FACTOR}},

	{TokenType::Bang, {unary, nullptr, PREC_NONE}},
	{TokenType::BangEqual, {nullptr, binary, PREC_EQUALITY}},
	{TokenType::Equal, {nullptr, nullptr, PREC_NONE}},
	{TokenType::EqualEqual, {nullptr, binary, PREC_EQUALITY}},
	{TokenType::Less, {nullptr, binary, PREC_COMPARISON}},
	{TokenType::LessEqual, {nullptr, binary, PREC_COMPARISON}},
	{TokenType::Greater, {nullptr, binary, PREC_COMPARISON}},
	{TokenType::GreaterEqual, {nullptr, binary, PREC_COMPARISON}},

	{TokenType::Identifier, {nullptr, nullptr, PREC_NONE}},
	{TokenType::String, {string, nullptr, PREC_NONE}},
	{TokenType::Number, {number, nullptr, PREC_NONE}},

	{TokenType::And, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Or, {nullptr, nullptr, PREC_NONE}},

	{TokenType::If, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Else, {nullptr, nullptr, PREC_NONE}},
	{TokenType::While, {nullptr, nullptr, PREC_NONE}},
	{TokenType::For, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Return, {nullptr, nullptr, PREC_NONE}},

	{TokenType::Class, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Fun, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Var, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Print, {nullptr, nullptr, PREC_NONE}},

	{TokenType::Super, {nullptr, nullptr, PREC_NONE}},
	{TokenType::This, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Nil, {literal, nullptr, PREC_NONE}},
	{TokenType::True, {literal, nullptr, PREC_NONE}},
	{TokenType::False, {literal, nullptr, PREC_NONE}},

	{TokenType::Error, {nullptr, nullptr, PREC_NONE}},
	{TokenType::Eof, {nullptr, nullptr, PREC_NONE}},
};
// clang-format on

static const ParseRule* getRule(TokenType type)
{
	return rules[type];
}

bool compile(const std::string& source, Chunk* chunk)
{
	CL_ASSERT(chunk);

	// FIXME: This is very unsafe.
	// Maybe something with iterators over a string might be better?
	initScanner(source.data());
	compilingChunk = chunk;

	// Reset the parser.
	parser = {};

	advance();

	while (!match(TokenType::Eof)) {
		declaration();
	}
	consume(TokenType::Eof, "Expected end of expression.");
	endCompiling();
	return !parser.hadError;
}

} // namespace cxxlox