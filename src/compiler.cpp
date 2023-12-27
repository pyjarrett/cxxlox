#include "compiler.hpp"

#include "chunk.hpp"
#include "object.hpp"
#include "object_allocator.hpp"
#include "parser.hpp"
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

// A local variable.
struct Local {
	Token name;

	// The scope depth of this variable.  A depth of -1 means that it is
	// declared, but not yet initialized (defined).
	int32_t depth = -1;

	// Is this local captured as an upvalue?
	bool captured = false;

	static constexpr int32_t kUninitialized = -1;
};
static_assert(sizeof(Local) == 32);

// A tracked upvalue, which is a variable in an outer scope which has been
// captured by a closure.
struct Upvalue {
	uint8_t index = 0;
	bool isLocal = true;
};

// Indicate whether the compiler is currently in a top level scope, or in a
// function's scope.
enum class FunctionType
{
	Function,

	// Top level.
	Script
};

struct Compiler;

// Global compilation state.  Not preferred, but how the book
// handles it.
static Parser parser;

namespace clox {
static Compiler* current = nullptr;
}

// Tracks compilation of the top level and each Lox function.
struct Compiler {
	// Compilers form a stack, with each compiler taking the previously open
	// one as its enclosing scope.
	explicit Compiler(Compiler* enclosing, FunctionType type) : enclosing(enclosing), type(type)
	{
		function = allocateObj<ObjFunction>();
		if (type != FunctionType::Script) {
			function->name = copyString(parser.previous.start, parser.previous.length);
		}

		// Allocate a local for use by the compiler.
		Local* local = &locals[localCount++];
		local->depth = 0;
		local->name.start = ""; // So the user can't refer to it.
		local->name.length = 0;
	}

	// Deviation: was renamed endCompiler()
	[[nodiscard]] ObjFunction* end();

	// Looks in the current and enclosing scopes for a local with the given name,
	// returning the index of a local with the given name, or return -1 if not found.
	[[nodiscard]] int resolveLocal(Token* name);
	[[nodiscard]] int32_t addUpvalue(uint8_t index, bool isLocal);
	[[nodiscard]] int32_t resolveUpvalue(Token* name);

	void beginScope();
	void endScope();

	[[nodiscard]] Chunk* chunk() { return &function->chunk; }

	[[nodiscard]] uint8_t makeConstant(Value value);
	[[nodiscard]] uint8_t identifierConstant(Token* name);
	void addLocal(Token name);
	void declareVariable();

	void markInitialized();
	void defineVariable(uint8_t global);

	[[nodiscard]] uint8_t parseVariable(const char* errorMessage);

	////////////////////////////////////////////////////////////////////////////
	// Bytecode emission
	////////////////////////////////////////////////////////////////////////////
	void emitByte(uint8_t byte);
	void emitBytes(uint8_t byte1, uint8_t byte2);
	void emitLoop(int loopStart);
	[[nodiscard]] int emitJump(uint8_t byte);
	void emitConstant(Value value);
	void emitReturn();
	void patchJump(int offset);

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

	Upvalue upvalues[kUInt8Count];
};

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////
static void declaration();
static void statement();
static void expression();
static const ParseRule* getRule(TokenType type);

///////////////////////////////////////////////////////////////////////////////
// State management
///////////////////////////////////////////////////////////////////////////////
uint8_t Compiler::makeConstant(Value value)
{
	const int constant = chunk()->addConstant(value);
	if (constant > std::numeric_limits<uint8_t>::max()) {
		parser.error("Too many constants in one chunk.");
	}
	return static_cast<uint8_t>(constant);
}

[[nodiscard]] uint8_t Compiler::identifierConstant(Token* name)
{
	return makeConstant(Value::makeObj(copyString(name->start, name->length)->asObj()));
}

///////////////////////////////////////////////////////////////////////////////
// Bytecode emission
///////////////////////////////////////////////////////////////////////////////
void Compiler::emitByte(uint8_t byte)
{
	chunk()->write(byte, parser.previous.line);
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

// A backwards jump back to the offset of a loop start.
void Compiler::emitLoop(int loopStart)
{
	emitByte(OP_LOOP);

	// 2 to skip over the two offset bytes of the OP_LOOP instruction
	const int offset = chunk()->code.count() - loopStart + 2;
	if (offset > std::numeric_limits<uint16_t>::max()) {
		parser.error("Loop body is too large.");
	}

	emitByte((offset >> 8) & 0xFF);
	emitByte(offset & 0xFF);
}

// Emit a jump instruction and then return the offset of the address to jump to
// so that it can be patched once that location is known.  "Jumps" are only
// forwards.
int Compiler::emitJump(uint8_t byte)
{
	emitByte(byte);

	// Jump addresses are 2 bytes (16 bits).
	emitByte(0xFF);
	emitByte(0xFF);

	return chunk()->code.count() - 2;
}

void Compiler::emitConstant(Value value)
{
	emitBytes(OP_CONSTANT, makeConstant(value));
}

// Patch a jump from the given offset to the next instruction to be emitted.
void Compiler::patchJump(int offset)
{
	// The location being jumped from is the
	const int target = chunk()->code.count() - offset - 2;

	CL_ASSERT(target > 0);

	// TODO: This feels weird putting the larger byte in first on little-endian
	// I'd expect these to be swapped.
	chunk()->code[offset] = (target >> 8) & 0xFF;
	chunk()->code[offset + 1] = target & 0xFF;
}

void Compiler::emitReturn()
{
	// Implicitly return nil if no return value is provided.
	emitByte(OP_NIL);
	emitByte(OP_RETURN);
}

ObjFunction* Compiler::end()
{
	// TODO: Prevent other member functions from being called after this is called.
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(*chunk(), function->name != nullptr ? function->name->chars : "<script>");
	}
#endif

	return function;
}

///////////////////////////////////////////////////////////////////////////////
// Parsing
///////////////////////////////////////////////////////////////////////////////
static void parsePrecedence(Precedence precedence)
{
	// Read the token for the rule we want to act on.
	parser.advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (!prefixRule) {
		parser.error("Expected an expression.");
		return;
	}

	const bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		parser.advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && parser.match(TokenType::Equal)) {
		parser.error("Invalid assignment target.");
	}
}

// Looks in the current and enclosing scopes for a local with the given name,
// returning the index of a local with the given name, or return -1 if not found.
int Compiler::resolveLocal(Token* name)
{
	// Look at the current and upwards scopes for a variable of this name.
	for (int i = localCount - 1; i >= 0; --i) {
		Local* local = &locals[i];
		if (identifiersEqual(&local->name, name)) {
			if (local->depth == Local::kUninitialized) {
				parser.error("Cannot reference a local variable in its own initializer.");
			}
			return i;
		}
	}

	return -1;
}

// Track an upvalue in the given compiler.
int32_t Compiler::addUpvalue(uint8_t index, bool isLocal)
{
	// The compiler needs to track all the locals used as upvalues to extract
	// them when the function returns or a block ends.
	const int upvalueCount = function->upvalueCount;

	// Check to see that this upvalue hasn't already been captured by this closure.
	for (int32_t i = 0; i < upvalueCount; ++i) {
		if (upvalues[i].index == index && upvalues[i].isLocal == isLocal) {
			return upvalues[i].index;
		}
	}

	// We ran out of space to store upvalues.
	if (upvalueCount == kUInt8Count) {
		parser.error("Too many closure variables in function.");
		return 0;
	}

	upvalues[upvalueCount].index = index;
	upvalues[upvalueCount].isLocal = isLocal;
	return function->upvalueCount++;
}

// Looks for an upvalue in the enclosing function compiler scope.
int32_t Compiler::resolveUpvalue(Token* name)
{
	CL_ASSERT(name);

	// There are no more compilers to check.
	if (!enclosing) {
		return -1;
	}

	// Look for a local matching the upvalue.
	if (int local = enclosing->resolveLocal(name); local != -1) {
		// This value now requires it to stay alive, since it's an upvalue.
		enclosing->locals[local].captured = true;

		// Found, so mark it as an upvalue.
		return addUpvalue(static_cast<uint8_t>(local), true);
	}

	// This upvalue might already be an existing upvalue higher up in the
	// compiler chain.
	if (int upvalue = enclosing->resolveUpvalue(name); upvalue != -1) {
		return addUpvalue(static_cast<uint8_t>(upvalue), false);
	}

	// Not found
	return -1;
}

void Compiler::beginScope()
{
	++scopeDepth;
}

void Compiler::endScope()
{
	--scopeDepth;

	// Remove variables from the current scope from the top of the stack.
	// Variables which are captured as upvalues must be saved, but all others
	// can just be popped.
	while (localCount > 0 && locals[localCount - 1].depth > scopeDepth) {
		if (locals[localCount - 1].captured) {
			emitByte(OP_CLOSE_UPVALUE);
		} else {
			emitByte(OP_POP);
		}
		--localCount;
	}
}

void Compiler::addLocal(Token name)
{
	// The VM only supports a limited number of locals.
	if (localCount == kUInt8Count) {
		parser.error("Too many local variables in function.");
		return;
	}

	Local* local = &locals[localCount++];
	local->name = name;
	local->depth = Local::kUninitialized;
}

void Compiler::declareVariable()
{
	// Variables declared without a scope are global.
	if (scopeDepth == 0) {
		return;
	}

	Token* name = &parser.previous;

	// Look for variables with similar names, starting with the current scope.
	for (int i = localCount - 1; i >= 0; --i) {

		Local* local = &locals[i];
		if (local->depth != Local::kUninitialized && local->depth < scopeDepth) {
			// We've proceeded above the new variable's scope.
			break;
		}

		if (identifiersEqual(&local->name, name)) {
			// If the names match, then it's an error.
			parser.error("Variable with duplicate name");
		}
	}
	addLocal(*name);
}

static uint8_t argumentList()
{
	uint8_t argCount = 0;
	if (!parser.check(TokenType::RightParen)) {
		do {
			expression();
			if (argCount == 255) {
				parser.error("Can't have more than 255 arguments.");
			}
			++argCount;
		} while (parser.match(TokenType::Comma));
	}
	parser.consume(TokenType::RightParen, "Expected ')' after argument list.");
	return argCount;
}

// Look for a variable, returning the index in the constants map.
// If the variable is a local variable, then return 0.
uint8_t Compiler::parseVariable(const char* errorMessage)
{
	parser.consume(TokenType::Identifier, errorMessage);
	declareVariable();

	// The variable is local.
	if (scopeDepth > 0) {
		return 0;
	}

	return identifierConstant(&parser.previous);
}

// Marks the top variable as initialized.
void Compiler::markInitialized()
{
	// Global functions aren't in a scope to be marked as initialized.
	if (scopeDepth == 0) {
		return;
	}

	locals[localCount - 1].depth = scopeDepth;
}

void Compiler::defineVariable(uint8_t global)
{
	// The variable is local.
	if (scopeDepth > 0) {
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
	const int endJump = clox::current->emitJump(OP_JUMP_IF_FALSE);

	// Remove left-hand side from the stack.
	clox::current->emitByte(OP_POP);
	parsePrecedence(PREC_AND);

	// Evaluating the right-hand will leave the appropriate value on the top of
	// the stack.
	clox::current->patchJump(endJump);
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
	const int elseJump = clox::current->emitJump(OP_JUMP_IF_FALSE);

	// lhs was true, so bypass right-hand evaluation.
	const int endJump = clox::current->emitJump(OP_JUMP);

	// rhs evaluation
	clox::current->patchJump(elseJump);
	clox::current->emitByte(OP_POP); // pop lhs off the stack
	parsePrecedence(PREC_OR);

	clox::current->patchJump(endJump);

	// Leave either lhs or rhs on stack
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
			clox::current->emitByte(OP_ADD);
			break;
		case TokenType::Minus:
			clox::current->emitByte(OP_SUBTRACT);
			break;
		case TokenType::Star:
			clox::current->emitByte(OP_MULTIPLY);
			break;
		case TokenType::Slash:
			clox::current->emitByte(OP_DIVIDE);
			break;
		case TokenType::EqualEqual:
			clox::current->emitByte(OP_EQUAL);
			break;
		case TokenType::BangEqual:
			clox::current->emitBytes(OP_EQUAL, OP_NOT);
			break;
		case TokenType::Less:
			clox::current->emitByte(OP_LESS);
			break;
		case TokenType::LessEqual:
			clox::current->emitBytes(OP_GREATER, OP_NOT);
			break;
		case TokenType::Greater:
			clox::current->emitByte(OP_GREATER);
			break;
		case TokenType::GreaterEqual:
			clox::current->emitBytes(OP_LESS, OP_NOT);
			break;
		default:
			CL_ASSERT(false); // Unknown operator type.
	}
}

static void call(bool canAssign)
{
	const uint8_t argCount = argumentList();
	clox::current->emitBytes(OP_CALL, argCount);
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
	while (!parser.check(TokenType::Eof) && !parser.check(TokenType::RightBrace)) {
		declaration();
	}
	parser.consume(TokenType::RightBrace, "Expected '}' to terminate block.");
}

// Compile a function.
static void function(FunctionType type)
{
	// Maximum number of function parameters.
	constexpr int32_t kMaxFunctionArity = 255;

	// Each function gets compiled by a separate compiler.
	Compiler compiler(clox::current, type);
	clox::current = &compiler;

	clox::current->beginScope();

	// Parameter parsing
	parser.consume(TokenType::LeftParen, "Expected `(` after function name.");
	if (!parser.check(TokenType::RightParen)) {
		do {
			++clox::current->function->arity;
			if (clox::current->function->arity > kMaxFunctionArity) {
				parser.errorAtCurrent("Can't have more than 255 parameters.");
			}

			const uint8_t constant = clox::current->parseVariable("Expected parameter name.");
			clox::current->defineVariable(constant);
		} while (parser.match(TokenType::Comma));
	}
	parser.consume(TokenType::RightParen, "Expected `)` after function parameters.");
	parser.consume(TokenType::LeftBrace, "Expected `{` after function parameter list.");

	// Function body
	block();

	// Close up the function
	ObjFunction* fn = clox::current->end();

	// FIXME: Still have functions depending on clox::current
	clox::current = clox::current->enclosing;

	Compiler* enclosing = clox::current;
	enclosing->emitBytes(OP_CLOSURE, enclosing->makeConstant(Value::makeFunction(fn)));

	// OP_CLOSURE is a variable sized instruction which includes bytes following
	// it describing upvalues, written as (local, index) pairs.
	for (int32_t i = 0; i < fn->upvalueCount; ++i) {
		enclosing->emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
		enclosing->emitByte(compiler.upvalues[i].index);
	}

	// No `endScope()` here because there's no need to close the outermost scope.
	// The call frame is going to get popped if it's an inner function, and the
	// program is terminating if it's the outermost script level.
}

static void functionDeclaration()
{
	const uint8_t global = clox::current->parseVariable("Expected a function name.");
	clox::current->markInitialized();

	// This isn't part of a top level script.
	function(FunctionType::Function);
	clox::current->defineVariable(global);
}

static void varDeclaration()
{
	const uint8_t global = clox::current->parseVariable("Expected a variable name.");
	if (parser.match(TokenType::Equal)) {
		expression();
	} else {
		clox::current->emitByte(OP_NIL);
	}
	parser.consume(TokenType::Semicolon, "Expected a ';' after a variable declaration.");
	clox::current->defineVariable(global);
}

// Parse a print statement, assuming the previous token is "print".
static void printStatement()
{
	expression();
	parser.consume(TokenType::Semicolon, "Expected a ';' after print statement.");
	clox::current->emitByte(OP_PRINT);
}

static void expressionStatement()
{
	expression();
	parser.consume(TokenType::Semicolon, "Expected a ';' after expression.");
	clox::current->emitByte(OP_POP);
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
	clox::current->beginScope();
	parser.consume(TokenType::LeftParen, "Expected '(' after `for`.");

	// Variable declaration and/or initializer
	if (parser.match(TokenType::Var)) {
		varDeclaration();
	} else if (parser.match(TokenType::Semicolon)) {
		// No initializer.
	} else {
		expressionStatement();
	}

	// Condition
	int loopStart = clox::current->chunk()->code.count();
	int exitJump = -1;
	if (!parser.match(TokenType::Semicolon)) {
		expression();
		parser.consume(TokenType::Semicolon, "Expected ';' after condition.");
		exitJump = clox::current->emitJump(OP_JUMP_IF_FALSE);
		clox::current->emitByte(OP_POP);
	}

	// Increment
	if (!parser.match(TokenType::RightParen)) {
		// Skip increment on initial loop pass.
		const int bodyJump = clox::current->emitJump(OP_JUMP);
		const int increment = clox::current->chunk()->code.count();
		expression();
		clox::current->emitByte(OP_POP);
		parser.consume(TokenType::RightParen, "Expected ')' after `for` clauses.");

		// Start the next loop.
		clox::current->emitLoop(loopStart);

		// The body should jump to the increment only if there is one.
		loopStart = increment;
		clox::current->patchJump(bodyJump);
	}

	statement();
	clox::current->emitLoop(loopStart);

	if (exitJump != -1) {
		clox::current->patchJump(exitJump);

		// Pop the condition.
		clox::current->emitByte(OP_POP);
	}
	clox::current->endScope();
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
	parser.consume(TokenType::LeftParen, "Expected a '(' after `if`.");
	expression();
	parser.consume(TokenType::RightParen, "Expected a ')' after `if` condition.");

	const int thenJump = clox::current->emitJump(OP_JUMP_IF_FALSE);
	clox::current->emitByte(OP_POP);
	statement();

	const int elseJump = clox::current->emitJump(OP_JUMP);
	clox::current->patchJump(thenJump);
	clox::current->emitByte(OP_POP);

	if (parser.match(TokenType::Else)) {
		statement();
	}
	clox::current->patchJump(elseJump);
}

static void returnStatement(Compiler* compiler)
{
	if (compiler->type == FunctionType::Script) {
		parser.error("Cannot return from top-level code.");
	}

	if (parser.match(TokenType::Semicolon)) {
		clox::current->emitReturn();
	} else {
		expression();
		parser.consume(TokenType::Semicolon, "Expected ';' after return expression.");
		clox::current->emitByte(OP_RETURN);
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
static void whileStatement(Compiler* compiler)
{
	const int loopStart = clox::current->chunk()->code.count();

	parser.consume(TokenType::LeftParen, "Expected '(' after while.");
	expression();
	parser.consume(TokenType::RightParen, "Expected ')' after while condition.");

	const int exitJump = clox::current->emitJump(OP_JUMP_IF_FALSE);
	clox::current->emitByte(OP_POP);
	statement();
	clox::current->emitLoop(loopStart);

	clox::current->patchJump(exitJump);
	clox::current->emitByte(OP_POP);
}

static void declaration()
{
	if (parser.match(TokenType::Fun)) {
		functionDeclaration();
	} else if (parser.match(TokenType::Var)) {
		varDeclaration();
	} else {
		statement();
	}

	// The parser could be in an error state, if so, then synchronize to a reasonable
	// "known good" point.
	if (parser.panicMode) {
		parser.synchronize();
	}
}

static void statement()
{
	if (parser.match(TokenType::Print)) {
		printStatement();
	} else if (parser.match(TokenType::For)) {
		forStatement();
	} else if (parser.match(TokenType::If)) {
		ifStatement();
	} else if (parser.match(TokenType::Return)) {
		returnStatement(clox::current);
	} else if (parser.match(TokenType::While)) {
		whileStatement(clox::current);
	} else if (parser.match(TokenType::LeftBrace)) {
		clox::current->beginScope();
		block();
		clox::current->endScope();
	} else {
		expressionStatement();
	}
}

// Grouping expression like "(expr)".  Assumes the leading "(" has already been
// encountered.
static void grouping([[maybe_unused]] bool canAssign)
{
	expression();
	parser.consume(TokenType::RightParen, "Expected ')' after expression.");
}

static void unary([[maybe_unused]] bool canAssign)
{
	const TokenType operatorType = parser.previous.type;

	// Compile the expression the unary applies to.
	parsePrecedence(PREC_UNARY);

	// current->emit the operation AFTER the expression, since the expression should be
	// below this operand to have it applied to that expression's result.
	switch (operatorType) {
		case TokenType::Minus:
			clox::current->emitByte(OP_NEGATE);
			break;
		case TokenType::Bang:
			clox::current->emitByte(OP_NOT);
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
	clox::current->emitConstant(Value::makeNumber(value));
}

static void string([[maybe_unused]] bool canAssign)
{
	const std::string_view previous = parser.previous.view();
	const std::string_view withoutQuotes = previous.substr(1, previous.length() - 2);
	clox::current->emitConstant(Value::makeString(copyString(withoutQuotes.data(), withoutQuotes.length())));
}

// Emit the variable and appropriate op code to get or set a variable,
// depending on if this is an assignment.
static void namedVariable(Token name, bool canAssign)
{
	// Decide if this is an operation on a global or a local.
	uint8_t getOp, setOp;
	int arg = clox::current->resolveLocal(&name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = clox::current->resolveUpvalue(&name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		// Global
		arg = clox::current->identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	CL_ASSERT(arg >= 0 && arg <= std::numeric_limits<uint8_t>::max());

	if (canAssign && parser.match(TokenType::Equal)) {
		expression();
		clox::current->emitBytes(setOp, static_cast<uint8_t>(arg));
	} else {
		clox::current->emitBytes(getOp, static_cast<uint8_t>(arg));
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
			clox::current->emitByte(OP_NIL);
			break;
		case TokenType::True:
			clox::current->emitByte(OP_TRUE);
			break;
		case TokenType::False:
			clox::current->emitByte(OP_FALSE);
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

///////////////////////////////////////////////////////////////////////////////
// External interface.
///////////////////////////////////////////////////////////////////////////////
ObjFunction* compile(const std::string& source)
{
	// FIXME: This is very unsafe.
	// Maybe something with iterators over a string might be better?
	initScanner(source.data());

	// FIXME: This is a horribly bad idea.
	static Compiler compiler(nullptr, FunctionType::Script);
	clox::current = &compiler;

	// Reset the parser.
	parser = {};

	parser.advance();

	while (!parser.match(TokenType::Eof)) {
		declaration();
	}
	parser.consume(TokenType::Eof, "Expected end of expression.");
	ObjFunction* function = clox::current->end();
	clox::current = clox::current->enclosing;

	return parser.hadError ? nullptr : function;
}

} // namespace cxxlox