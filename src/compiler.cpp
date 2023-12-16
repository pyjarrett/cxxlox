#include "compiler.hpp"

#include "chunk.hpp"
#include "object.hpp"
#include "pratt.hpp"
#include "scanner.hpp"

#ifdef DEBUG_PRINT_CODE
	#include "debug.hpp"
#endif

#include <cassert>
#include <cstring>
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

// A local variable.
struct Local {
	Token name;

	// The scope depth of this variable.  A depth of -1 means that it is
	// declared, but not yet initialized (defined).
	int32_t depth = -1;

	static constexpr int32_t kUninitialized = -1;
};
static_assert(sizeof(Local) == 32);

// Indicate whether the compiler is currently in a top level scope, or in a
// function's scope.
enum class FunctionType
{
	Function,

	// Top level.
	Script
};

struct Compiler;

static Parser parser;
static Compiler* current = nullptr;

// Tracks compilation of the top level and each Lox function.
struct Compiler {
	// Compilers form a stack, with each compiler taking the previously open
	// one as its enclosing scope.
	explicit Compiler(FunctionType type) : type(type)
	{
		// Track the enclosing function compiler and mark this one as current.
		enclosing = current;
		current = this;

		function = makeFunction();
		if (type != FunctionType::Script) {
			function->name = copyString(parser.previous.start, parser.previous.length);
		}

		// Allocate a local for use by the compiler.
		Local* local = &locals[localCount++];
		local->depth = 0;
		local->name.start = ""; // So the user can't refer to it.
		local->name.length = 0;
	}

	// The parent function in which this compilation instance is occurring.
	Compiler* enclosing = nullptr;

	// The related code this compiler is building.
	ObjFunction* function = nullptr;
	FunctionType type = FunctionType::Script;

	// A stack containing all of our current locals.  The most recently declared
	// locals will be later in this array.
	Local locals[kUInt8Count] {};

	// The total number of locals which have been declared.
	int localCount = 0;
	int scopeDepth = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////
static void declaration();
static void statement();
static void expression();
static const ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static bool check(TokenType type);
static bool match(TokenType type);
static void consume(TokenType type, const char* errorMessage);
static uint8_t makeConstant(Value value);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static int emitJump(uint8_t byte);
static void patchJump(int offset);

///////////////////////////////////////////////////////////////////////////////
// Error handling
///////////////////////////////////////////////////////////////////////////////
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

	const bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TokenType::Equal)) {
		error("Invalid assignment target.");
	}
}

[[nodiscard]] static uint8_t identifierConstant(Token* name)
{
	return makeConstant(Value::makeObj(copyString(name->start, name->length)->asObj()));
}

[[nodiscard]] static bool identifiersEqual(Token* a, Token* b)
{
	if (a->length != b->length) {
		return false;
	}

	return std::memcmp(a->start, b->start, a->length) == 0;
}

// Looks in the current and enclosing scopes for a local with the given name,
// returning the index of a local with the given name, or return -1 if not found.
[[nodiscard]] static int resolveLocal(Compiler* compiler, Token* name)
{
	// Look at the current and upwards scopes for a variable of this name.
	for (int i = compiler->localCount - 1; i >= 0; --i) {
		Local* local = &compiler->locals[i];
		if (identifiersEqual(&local->name, name)) {
			if (local->depth == Local::kUninitialized) {
				error("Cannot reference a local variable in its own initializer.");
			}
			return i;
		}
	}

	return -1;
}

static void addLocal(Token name)
{
	// The VM only supports a limited number of locals.
	if (current->localCount == kUInt8Count) {
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = Local::kUninitialized;
}

static void declareVariable()
{
	// Variables declared without a scope are global.
	if (current->scopeDepth == 0) {
		return;
	}

	Token* name = &parser.previous;

	// Look for variables with similar names, starting with the current scope.
	for (int i = current->localCount - 1; i >= 0; --i) {

		Local* local = &current->locals[i];
		if (local->depth != Local::kUninitialized && local->depth < current->scopeDepth) {
			// We've proceeded above the new variable's scope.
			break;
		}

		if (identifiersEqual(&local->name, name)) {
			// If the names match, then it's an error.
			error("Variable with duplicate name");
		}
	}
	addLocal(*name);
}

static uint8_t argumentList()
{
	uint8_t argCount = 0;
	if (!check(TokenType::RightParen)) {
		do {
			expression();
			if (argCount == 255) {
				error("Can't have more than 255 arguments.");
			}
			++argCount;
		} while (match(TokenType::Comma));
	}
	consume(TokenType::RightParen, "Expected ')' after argument list.");
	return argCount;
}

// Look for a variable, returning the index in the constants map.
// If the variable is a local variable, then return 0.
[[nodiscard]] static uint8_t parseVariable(const char* errorMessage)
{
	consume(TokenType::Identifier, errorMessage);
	declareVariable();

	// The variable is local.
	if (current->scopeDepth > 0) {
		return 0;
	}

	return identifierConstant(&parser.previous);
}

// Marks the top variable as initialized.
static void markInitialized()
{
	// Global functions aren't in a scope to be marked as initialized.
	if (current->scopeDepth == 0) {
		return;
	}

	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
	// The variable is local.
	if (current->scopeDepth > 0) {
		markInitialized();
		return;
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

// Short-circuiting `and` operator.
static void andOperator(bool canAssign)
{
	CL_UNUSED(canAssign);

	// The left hand side should on the top of the stack.
	// Skip the right hand evaluation if it is false.
	const int endJump = emitJump(OP_JUMP_IF_FALSE);

	// Remove left-hand side from the stack.
	emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	// Evaluating the right-hand will leave the appropriate value on the top of
	// the stack.
	patchJump(endJump);
}

// Short-circuiting `or` operator
//
// lhs || right
//
// lhs on stack
// if false, go to rhs ----------->+
// was true, so go to end ---->+   |
// pop       <---------------- |  --+
// evaluate right-hand side    |
// <---------------------------+
static void orOperator(bool canAssign)
{
	CL_UNUSED(canAssign);

	// The left side should be on the stack.
	const int elseJump = emitJump(OP_JUMP_IF_FALSE);

	// lhs was true, so bypass right-hand evaluation.
	const int endJump = emitJump(OP_JUMP);

	// rhs evaluation
	patchJump(elseJump);
	emitByte(OP_POP); // pop lhs off the stack
	parsePrecedence(PREC_OR);

	patchJump(endJump);

	// Leave either lhs or rhs on stack
}

///////////////////////////////////////////////////////////////////////////////
// Token operations
///////////////////////////////////////////////////////////////////////////////

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

[[nodiscard]] CL_FORCE_INLINE Chunk* currentChunk()
{
	return &current->function->chunk;
}

///////////////////////////////////////////////////////////////////////////////
// Bytecode emission
///////////////////////////////////////////////////////////////////////////////
static void emitByte(uint8_t byte)
{
	currentChunk()->write(byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

// A backwards jump back to the offset of a loop start.
static void emitLoop(int loopStart)
{
	emitByte(OP_LOOP);

	// 2 to skip over the two offset bytes of the OP_LOOP instruction
	const int offset = currentChunk()->code.count() - loopStart + 2;
	if (offset > std::numeric_limits<uint16_t>::max()) {
		error("Loop body is too large.");
	}

	emitByte((offset >> 8) & 0xFF);
	emitByte(offset & 0xFF);
}

// Emit a jump instruction and then return the offset of the address to jump to
// so that it can be patched once that location is known.  "Jumps" are only
// forwards.
static int emitJump(uint8_t byte)
{
	emitByte(byte);

	// Jump addresses are 2 bytes (16 bits).
	emitByte(0xFF);
	emitByte(0xFF);

	return currentChunk()->code.count() - 2;
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

// Patch a jump from the given offset to the next instruction to be emitted.
static void patchJump(int offset)
{
	// The location being jumped from is the
	const int target = currentChunk()->code.count() - offset - 2;

	CL_ASSERT(target > 0);

	// TODO: This feels weird putting the larger byte in first on little-endian
	// I'd expect these to be swapped.
	currentChunk()->code[offset] = (target >> 8) & 0xFF;
	currentChunk()->code[offset + 1] = target & 0xFF;
}

static void emitReturn()
{
	// Implicitly return nil if no return value is provided.
	emitByte(OP_NIL);
	emitByte(OP_RETURN);
}

///////////////////////////////////////////////////////////////////////////////
// Compiler helpers
///////////////////////////////////////////////////////////////////////////////
static ObjFunction* endCompiler()
{
	emitReturn();
	ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(*currentChunk(), function->name != nullptr ? function->name->chars : "<script>");
	}
#endif

	current = current->enclosing;
	return function;
}

static void beginScope()
{
	++current->scopeDepth;
}

static void endScope()
{
	--current->scopeDepth;

	// Pop all variables at the current scope off the stack.
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		emitByte(OP_POP);
		--current->localCount;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Syntactical structures
///////////////////////////////////////////////////////////////////////////////
static void binary([[maybe_unused]] bool canAssign)
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
			emitBytes(OP_GREATER, OP_NOT);
			break;
		case TokenType::Greater:
			emitByte(OP_GREATER);
			break;
		case TokenType::GreaterEqual:
			emitBytes(OP_LESS, OP_NOT);
			break;
		default:
			CL_ASSERT(false); // Unknown operator type.
	}
}

static void call(bool canAssign)
{
	const uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
	while (!check(TokenType::Eof) && !check(TokenType::RightBrace)) {
		declaration();
	}
	consume(TokenType::RightBrace, "Expected '}' to terminate block.");
}

// Compile a function.
static void function(FunctionType type)
{
	// Maximum number of function parameters.
	constexpr int32_t kMaxFunctionArity = 255;

	// Each function gets compiled by a separate compiler.
	Compiler compiler(type);
	current = &compiler;

	beginScope();

	// Parameter parsing
	consume(TokenType::LeftParen, "Expected `(` after function name.");
	if (!check(TokenType::RightParen)) {
		do {
			++current->function->arity;
			if (current->function->arity > kMaxFunctionArity) {
				errorAtCurrent("Can't have more than 255 parameters.");
			}

			const uint8_t constant = parseVariable("Expected parameter name.");
			defineVariable(constant);
		} while (match(TokenType::Comma));
	}
	consume(TokenType::RightParen, "Expected `)` after function parameters.");
	consume(TokenType::LeftBrace, "Expected `{` after function parameter list.");

	// Function body
	block();

	// Close up the function
	ObjFunction* fn = endCompiler();

	// Store the new function in the enclosing function's scope.
	emitBytes(OP_CLOSURE, makeConstant(Value::makeFunction(fn)));

	// No `endScope()` here because there's no need to close the outermost scope.
	// The call frame is going to get popped if it's an inner function, and the
	// program is terminating if it's the outermost script level.
}

static void functionDeclaration()
{
	const uint8_t global = parseVariable("Expected a function name.");
	markInitialized();

	// This isn't part of a top level script.
	function(FunctionType::Function);
	defineVariable(global);
}

static void varDeclaration()
{
	const uint8_t global = parseVariable("Expected a variable name.");
	if (match(TokenType::Equal)) {
		expression();
	} else {
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

// Outputs the increment step before the loop body due to limitations of being
// as single-pass compiler -- it can't hold onto the increment step and output
// it later, like it could if it could keep it in an AST.
//
// `for` `(` initializer? `;` condition? `;` increment `)`
//     statement
//
// Begin scope (if initializer present)
// Create variable from declaration (if present)
// Run initializer (if present)
// Push condition <---------------------+
// Exit loop if false ----------------- | ----->+
// Pop condition                        |       |
//                                      |       |
// Body jump (if increment)-->+         |       |
//                            |         |       |
// Increment (if present)<--- | ----+   |       |
// Pop increment result       |     |   |       |
// Jump to condition check -------- | --+       |
//                            |     |           |
// Statement <----------------+     |           |
// Jump to increment -------------->+           |
// Pop condition <------------------------------+
// End scope (if initializer present)
//
static void forStatement()
{
	beginScope();
	consume(TokenType::LeftParen, "Expected '(' after `for`.");

	// Variable declaration and/or initializer
	if (match(TokenType::Var)) {
		varDeclaration();
	} else if (match(TokenType::Semicolon)) {
		// No initializer.
	} else {
		expressionStatement();
	}

	// Condition
	int loopStart = currentChunk()->code.count();
	int exitJump = -1;
	if (!match(TokenType::Semicolon)) {
		expression();
		consume(TokenType::Semicolon, "Expected ';' after condition.");
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);
	}

	// Increment
	if (!match(TokenType::RightParen)) {
		// Skip increment on initial loop pass.
		const int bodyJump = emitJump(OP_JUMP);
		const int increment = currentChunk()->code.count();
		expression();
		emitByte(OP_POP);
		consume(TokenType::RightParen, "Expected ')' after `for` clauses.");

		// Start the next loop.
		emitLoop(loopStart);

		// The body should jump to the increment only if there is one.
		loopStart = increment;
		patchJump(bodyJump);
	}

	statement();
	emitLoop(loopStart);

	if (exitJump != -1) {
		patchJump(exitJump);

		// Pop the condition.
		emitByte(OP_POP);
	}
	endScope();
}

// `if` <*> `(` condition `)`
//     statement
// `else`
//     statement
//
// Push condition onto stack
// Conditional jump if false ------->+
// Pop condition                     |
// Then statement                    |
// Jump over else branch ---->+      |
// <------------------------- | -----+
// Pop condition              |
// Else statements            |
// <--------------------------+
//
static void ifStatement()
{
	// Starts immediately after `if`.
	consume(TokenType::LeftParen, "Expected a '(' after `if`.");
	expression();
	consume(TokenType::RightParen, "Expected a ')' after `if` condition.");

	const int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	const int elseJump = emitJump(OP_JUMP);
	patchJump(thenJump);
	emitByte(OP_POP);

	if (match(TokenType::Else)) {
		statement();
	}
	patchJump(elseJump);
}

static void returnStatement()
{
	if (current->type == FunctionType::Script) {
		error("Cannot return from top-level code.");
	}

	if (match(TokenType::Semicolon)) {
		emitReturn();
	} else {
		expression();
		consume(TokenType::Semicolon, "Expected ';' after return expression.");
		emitByte(OP_RETURN);
	}
}

// `while` `(` expression `)`
//    statement
//
// Evaluate condition  <-----------------+
// Skip loop if condition not met--->+   |
// Pop condition                     |   |
// Execute the loop                  |   |
// Jump to condition evaluation ---> | ->+
// Pop condition <-------------------+
//
static void whileStatement()
{
	const int loopStart = currentChunk()->code.count();

	consume(TokenType::LeftParen, "Expected '(' after while.");
	expression();
	consume(TokenType::RightParen, "Expected ')' after while condition.");

	const int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);
}

static void declaration()
{
	if (match(TokenType::Fun)) {
		functionDeclaration();
	} else if (match(TokenType::Var)) {
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
	} else if (match(TokenType::For)) {
		forStatement();
	} else if (match(TokenType::If)) {
		ifStatement();
	} else if (match(TokenType::Return)) {
		returnStatement();
	} else if (match(TokenType::While)) {
		whileStatement();
	} else if (match(TokenType::LeftBrace)) {
		beginScope();
		block();
		endScope();
	} else {
		expressionStatement();
	}
}

// Grouping expression like "(expr)".  Assumes the leading "(" has already been
// encountered.
static void grouping([[maybe_unused]] bool canAssign)
{
	expression();
	consume(TokenType::RightParen, "Expected ')' after expression.");
}

static void unary([[maybe_unused]] bool canAssign)
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

static void number([[maybe_unused]] bool canAssign)
{
	double value;
	const auto result = std::from_chars(parser.previous.start, parser.previous.start + parser.previous.length, value);

	if (result.ec != std::errc {}) {
		CL_ASSERT(false); // Invalid conversion.
		value = 0.0;
	}
	emitConstant(Value::makeNumber(value));
}

static void string([[maybe_unused]] bool canAssign)
{
	const std::string_view previous = parser.previous.view();
	const std::string_view withoutQuotes = previous.substr(1, previous.length() - 2);
	emitConstant(Value::makeString(copyString(withoutQuotes.data(), withoutQuotes.length())));
}

// Emit the variable and appropriate op code to get or set a variable,
// depending on if this is an assignment.
static void namedVariable(Token name, bool canAssign)
{
	// Decide if this is an operation on a global or a local.
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg == -1) {
		// Global
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	} else {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}

	CL_ASSERT(arg >= 0 && arg <= std::numeric_limits<uint8_t>::max());

	if (canAssign && match(TokenType::Equal)) {
		expression();
		emitBytes(setOp, static_cast<uint8_t>(arg));
	} else {
		emitBytes(getOp, static_cast<uint8_t>(arg));
	}
}

static void variable(bool canAssign)
{
	namedVariable(parser.previous, canAssign);
}

static void literal([[maybe_unused]] bool canAssign)
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

static PrattRuleMap rules = {
	{TokenType::LeftParen, {grouping, call, PREC_CALL}},
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

	{TokenType::Identifier, {variable, nullptr, PREC_NONE}},
	{TokenType::String, {string, nullptr, PREC_NONE}},
	{TokenType::Number, {number, nullptr, PREC_NONE}},

	{TokenType::And, {nullptr, andOperator, PREC_AND}},
	{TokenType::Or, {nullptr, orOperator, PREC_OR}},

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

static const ParseRule* getRule(TokenType type)
{
	return rules[type];
}

ObjFunction* compile(const std::string& source)
{
	// FIXME: This is very unsafe.
	// Maybe something with iterators over a string might be better?
	initScanner(source.data());

	// FIXME: This is a horribly bad idea.
	static Compiler compiler(FunctionType::Script);
	current = &compiler;

	// Reset the parser.
	parser = {};

	advance();

	while (!match(TokenType::Eof)) {
		declaration();
	}
	consume(TokenType::Eof, "Expected end of expression.");
	ObjFunction* function = endCompiler();

	return parser.hadError ? nullptr : function;
}

} // namespace cxxlox