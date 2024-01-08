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
	Method,

	// "Constructor" for instances of class types.
	Initializer,

	// Top level.
	Script
};

// Tracker for the current class compiler to know if `this` is in a valid
// context or not.
struct ClassCompiler {
	ClassCompiler* enclosing = nullptr;

	ClassCompiler() = default;
	CL_PROHIBIT_MOVE_AND_COPY(ClassCompiler);
};

// Tracks compilation of the top level and each Lox function.
struct Compiler {
	// Compilers form a stack, with each compiler taking the previously open
	// one as its enclosing scope.
	explicit Compiler(Compiler* enclosing, FunctionType type, Parser& parser)
		: enclosing(enclosing), type(type), parser(parser)
	{
		// Must set s_active here since copyString might trigger a GC and
		// collect `function`.
		CL_ASSERT(s_active == enclosing);
		s_active = this;

		function = allocateObj<ObjFunction>();
		if (type != FunctionType::Script) {
			function->name = copyString(parser.previous.start, parser.previous.length);
		}

		// Allocate a local for use by the compiler.
		Local* local = &locals[localCount++];
		local->depth = 0;
		if (type != FunctionType::Function) {
			local->name.start = "this";
			local->name.length = 4;

		} else {
			local->name.start = ""; // So the user can't refer to it.
			local->name.length = 0;
		}
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
	void namedVariable(Token name, bool canAssign);

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

	////////////////////////////////////////////////////////////////////////////
	// Syntactical functions
	////////////////////////////////////////////////////////////////////////////
	void declaration();
	void classDeclaration();
	void method();
	void functionDeclaration();
	void varDeclaration();

	void defineFunction(FunctionType type);

	void statement();
	void printStatement();
	void forStatement();
	void ifStatement();
	void returnStatement();
	void whileStatement();
	void expressionStatement();

	void block();

	void expression();

	// The parent function in which this compilation instance is occurring.
	Compiler* enclosing = nullptr;

	Parser& parser;

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

	// The currently active compiler.  Keep this inside the Compiler class and
	// don't have it affect the compilation itself, it's only used for tracking
	// active compilers for garbage collection.  Only one active chain of
	// compilers is supported.
	static Compiler* s_active;

	// The currently active class compiler.
	static ClassCompiler* s_classCompiler;

	CL_PROHIBIT_MOVE_AND_COPY(Compiler);
};

Compiler* Compiler::s_active = nullptr;
ClassCompiler* Compiler::s_classCompiler = nullptr;

///////////////////////////////////////////////////////////////////////////////
// Garbage collection
///////////////////////////////////////////////////////////////////////////////
void markActiveCompilers()
{
	for (Compiler* compiler = Compiler::s_active; compiler; compiler = compiler->enclosing) {
		markObject(asObj(compiler->function));
	}
}

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////
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
	return makeConstant(makeValue(copyString(name->start, name->length)));
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
	if (type == FunctionType::Initializer) {
		// The "this" pointer is stored at location 0.
		emitBytes(OP_GET_LOCAL, 0u);
	} else {
		// Implicitly return nil if no return value is provided.
		emitByte(OP_NIL);
	}

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

	s_active = enclosing;

	return function;
}

///////////////////////////////////////////////////////////////////////////////
// Parsing
///////////////////////////////////////////////////////////////////////////////
static void parsePrecedence(Compiler* compiler, Precedence precedence)
{
	Parser& parser = compiler->parser;

	// Read the token for the rule we want to act on.
	parser.advance();
	ParseFn prefixRule = getRule(parser.previous.type)->prefix;
	if (!prefixRule) {
		parser.error("Expected an expression.");
		return;
	}

	const bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(compiler, canAssign);

	while (precedence <= getRule(parser.current.type)->precedence) {
		parser.advance();
		ParseFn infixRule = getRule(parser.previous.type)->infix;
		infixRule(compiler, canAssign);
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

static uint8_t argumentList(Compiler* compiler)
{
	Parser& parser = compiler->parser;

	uint8_t argCount = 0;
	if (!parser.check(TokenType::RightParen)) {
		do {
			compiler->expression();
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

// Emit the variable and appropriate op code to get or set a variable,
// depending on if this is an assignment.
void Compiler::namedVariable(Token name, bool canAssign)
{
	// Decide if this is an operation on a global or a local.
	uint8_t getOp, setOp;
	int arg = resolveLocal(&name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	} else if ((arg = resolveUpvalue(&name)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	} else {
		// Global
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	CL_ASSERT(arg >= 0 && arg <= std::numeric_limits<uint8_t>::max());

	if (canAssign && parser.match(TokenType::Equal)) {
		expression();
		emitBytes(setOp, static_cast<uint8_t>(arg));
	} else {
		emitBytes(getOp, static_cast<uint8_t>(arg));
	}
}

////////////////////////////////////////////////////////////////////////////////
// Syntactical functions
////////////////////////////////////////////////////////////////////////////////
void Compiler::declaration()
{
	if (parser.match(TokenType::Class)) {
		classDeclaration();
	} else if (parser.match(TokenType::Fun)) {
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

// Starts after `class` token
void Compiler::classDeclaration()
{
	parser.consume(TokenType::Identifier, "Expected a class name.");
	const uint8_t nameConstant = identifierConstant(&parser.previous);
	const Token className = parser.previous;
	declareVariable();
	emitBytes(OP_CLASS, nameConstant);

	// Define the class name, so that class internals to reference the class
	defineVariable(nameConstant);

	// Tracks whether or not we are parsing inside a class.
	ClassCompiler classCompiler;
	if (s_classCompiler) {
		s_classCompiler->enclosing = &classCompiler;
	} else {
		s_classCompiler = &classCompiler;
	}

	// Look for inheritance.
	if (parser.match(TokenType::Less)) {
		parser.consume(TokenType::Identifier, "Expected a superclass name.");
		// Deviation: was variable(false)
		// Load superclass variable onto the stack.
		namedVariable(parser.previous, false);

		if (identifiersEqual(&parser.previous, &className)) {
			parser.error("A class cannot inherit from itself {} < {}");
		}

		// Load the subclass name onto the stack.
		namedVariable(className, false);
		emitByte(OP_INHERIT);
	}

	// Put class name back on the stack.
	namedVariable(className, false);

	parser.consume(TokenType::LeftBrace, "Expected an opening brace.");

	while (!parser.check(TokenType::RightBrace) && !parser.check(TokenType::Eof)) {
		method();
	}

	parser.consume(TokenType::RightBrace, "Expected a closing brace.");

	// Pop class name.
	emitByte(OP_POP);

	s_classCompiler = s_classCompiler->enclosing;
}

void Compiler::method()
{
	// Get method name.
	parser.consume(TokenType::Identifier, "Expected a method name.");
	const uint8_t methodName = identifierConstant(&parser.previous);

	FunctionType fnType = FunctionType::Method;
	if (parser.previous.length == 4 && parser.previous.view() == "init") {
		fnType = FunctionType::Initializer;
	}
	defineFunction(fnType);
	emitBytes(OP_METHOD, methodName);
}

void Compiler::functionDeclaration()
{
	const uint8_t global = parseVariable("Expected a function name.");
	markInitialized();

	// This isn't part of a top level script.
	defineFunction(FunctionType::Function);
	defineVariable(global);
}

void Compiler::varDeclaration()
{
	const uint8_t global = parseVariable("Expected a variable name.");
	if (parser.match(TokenType::Equal)) {
		expression();
	} else {
		emitByte(OP_NIL);
	}
	parser.consume(TokenType::Semicolon, "Expected a ';' after a variable declaration.");
	defineVariable(global);
}

// Deviation: was function()
void Compiler::defineFunction(FunctionType fnType)
{
	// Maximum number of function parameters.
	constexpr int32_t kMaxFunctionArity = 255;

	// Each function gets compiled by a separate compiler.
	Compiler compiler(this, fnType, this->parser);

	compiler.beginScope();

	// Parameter parsing
	parser.consume(TokenType::LeftParen, "Expected `(` after function name.");
	if (!parser.check(TokenType::RightParen)) {
		do {
			++compiler.function->arity;
			if (compiler.function->arity > kMaxFunctionArity) {
				parser.errorAtCurrent("Can't have more than 255 parameters.");
			}

			const uint8_t constant = compiler.parseVariable("Expected parameter name.");
			compiler.defineVariable(constant);
		} while (parser.match(TokenType::Comma));
	}
	parser.consume(TokenType::RightParen, "Expected `)` after function parameters.");
	parser.consume(TokenType::LeftBrace, "Expected `{` after function parameter list.");

	// Function body
	compiler.block();

	// Close up the function
	ObjFunction* fn = compiler.end();

	emitBytes(OP_CLOSURE, makeConstant(makeValue(fn)));

	// OP_CLOSURE is a variable sized instruction which includes bytes following
	// it describing upvalues, written as (local, index) pairs.
	for (int32_t i = 0; i < fn->upvalueCount; ++i) {
		emitByte(compiler.upvalues[i].isLocal ? 1u : 0u);
		emitByte(compiler.upvalues[i].index);
	}

	// No `endScope()` here because there's no need to close the outermost scope.
	// The call frame is going to get popped if it's an inner function, and the
	// program is terminating if it's the outermost script level.
}

void Compiler::statement()
{
	if (parser.match(TokenType::Print)) {
		printStatement();
	} else if (parser.match(TokenType::For)) {
		forStatement();
	} else if (parser.match(TokenType::If)) {
		ifStatement();
	} else if (parser.match(TokenType::Return)) {
		returnStatement();
	} else if (parser.match(TokenType::While)) {
		whileStatement();
	} else if (parser.match(TokenType::LeftBrace)) {
		beginScope();
		block();
		endScope();
	} else {
		expressionStatement();
	}
}

// Parse a print statement, assuming the previous token is "print".
void Compiler::printStatement()
{
	expression();
	parser.consume(TokenType::Semicolon, "Expected a ';' after print statement.");
	emitByte(OP_PRINT);
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
void Compiler::forStatement()
{
	beginScope();
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
	int loopStart = chunk()->code.count();
	int exitJump = -1;
	if (!parser.match(TokenType::Semicolon)) {
		expression();
		parser.consume(TokenType::Semicolon, "Expected ';' after condition.");
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);
	}

	// Increment
	if (!parser.match(TokenType::RightParen)) {
		// Skip increment on initial loop pass.
		const int bodyJump = emitJump(OP_JUMP);
		const int increment = chunk()->code.count();
		expression();
		emitByte(OP_POP);
		parser.consume(TokenType::RightParen, "Expected ')' after `for` clauses.");

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
void Compiler::ifStatement()
{
	// Starts immediately after `if`.
	parser.consume(TokenType::LeftParen, "Expected a '(' after `if`.");
	expression();
	parser.consume(TokenType::RightParen, "Expected a ')' after `if` condition.");

	const int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	const int elseJump = emitJump(OP_JUMP);
	patchJump(thenJump);
	emitByte(OP_POP);

	if (parser.match(TokenType::Else)) {
		statement();
	}
	patchJump(elseJump);
}

void Compiler::returnStatement()
{
	if (type == FunctionType::Script) {
		parser.error("Cannot return from top-level code.");
	} else if (type == FunctionType::Initializer) {
		parser.error("Cannot return from an initializer.");
	}

	if (parser.match(TokenType::Semicolon)) {
		emitReturn();
	} else {
		expression();
		parser.consume(TokenType::Semicolon, "Expected ';' after return expression.");
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
void Compiler::whileStatement()
{
	const int loopStart = chunk()->code.count();

	parser.consume(TokenType::LeftParen, "Expected '(' after while.");
	expression();
	parser.consume(TokenType::RightParen, "Expected ')' after while condition.");

	const int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);
}

void Compiler::expressionStatement()
{
	expression();
	parser.consume(TokenType::Semicolon, "Expected a ';' after expression.");
	emitByte(OP_POP);
}

void Compiler::block()
{
	while (!parser.check(TokenType::Eof) && !parser.check(TokenType::RightBrace)) {
		declaration();
	}
	parser.consume(TokenType::RightBrace, "Expected '}' to terminate block.");
}

void Compiler::expression()
{
	parsePrecedence(this, PREC_ASSIGNMENT);
}

///////////////////////////////////////////////////////////////////////////////
// Pratt rule functions
///////////////////////////////////////////////////////////////////////////////

// Short-circuiting `and` operator.
static void andOperator(Compiler* compiler, bool canAssign)
{
	CL_UNUSED(canAssign);

	// The left hand side should on the top of the stack.
	// Skip the right hand evaluation if it is false.
	const int endJump = compiler->emitJump(OP_JUMP_IF_FALSE);

	// Remove left-hand side from the stack.
	compiler->emitByte(OP_POP);
	parsePrecedence(compiler, PREC_AND);

	// Evaluating the right-hand will leave the appropriate value on the top of
	// the stack.
	compiler->patchJump(endJump);
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
static void orOperator(Compiler* compiler, bool canAssign)
{
	CL_UNUSED(canAssign);

	// The left side should be on the stack.
	const int elseJump = compiler->emitJump(OP_JUMP_IF_FALSE);

	// lhs was true, so bypass right-hand evaluation.
	const int endJump = compiler->emitJump(OP_JUMP);

	// rhs evaluation
	compiler->patchJump(elseJump);
	compiler->emitByte(OP_POP); // pop lhs off the stack
	parsePrecedence(compiler, PREC_OR);

	compiler->patchJump(endJump);

	// Leave either lhs or rhs on stack
}

// Looks for `obj . identifier` for assignment and property access.
static void dot(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	Parser& parser = compiler->parser;

	// The expression for the left-hand side should be on the top of the stack.
	parser.consume(TokenType::Identifier, "Expected an identifier after '.'");
	const auto name = compiler->identifierConstant(&parser.previous);

	if (canAssign && parser.match(TokenType::Equal)) {
		// Put the right-hand-side on the stack.
		compiler->expression();
		compiler->emitBytes(OP_SET_PROPERTY, name);
	} else if (parser.match(TokenType::LeftParen)) {
		const uint8_t argCount = argumentList(compiler);
		compiler->emitBytes(OP_INVOKE, name);
		compiler->emitByte(argCount);
	} else {
		compiler->emitBytes(OP_GET_PROPERTY, name);
	}
}

static void binary(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	Parser& parser = compiler->parser;

	const TokenType operatorType = parser.previous.type;
	const ParseRule* rule = getRule(operatorType);
	// Parse the next level up, since all our binary rules are left associative.
	// This would be different if right-associative operators were handled, and
	// we would use the same level of precedence again here.
	parsePrecedence(compiler, Precedence(rule->precedence + 1));
	switch (operatorType) {
		case TokenType::Plus:
			compiler->emitByte(OP_ADD);
			break;
		case TokenType::Minus:
			compiler->emitByte(OP_SUBTRACT);
			break;
		case TokenType::Star:
			compiler->emitByte(OP_MULTIPLY);
			break;
		case TokenType::Slash:
			compiler->emitByte(OP_DIVIDE);
			break;
		case TokenType::EqualEqual:
			compiler->emitByte(OP_EQUAL);
			break;
		case TokenType::BangEqual:
			compiler->emitBytes(OP_EQUAL, OP_NOT);
			break;
		case TokenType::Less:
			compiler->emitByte(OP_LESS);
			break;
		case TokenType::LessEqual:
			compiler->emitBytes(OP_GREATER, OP_NOT);
			break;
		case TokenType::Greater:
			compiler->emitByte(OP_GREATER);
			break;
		case TokenType::GreaterEqual:
			compiler->emitBytes(OP_LESS, OP_NOT);
			break;
		default:
			CL_ASSERT(false); // Unknown operator type.
	}
}

static void call(Compiler* compiler, bool canAssign)
{
	CL_UNUSED(canAssign);
	const uint8_t argCount = argumentList(compiler);
	compiler->emitBytes(OP_CALL, argCount);
}

// Grouping expression like "(expr)".  Assumes the leading "(" has already been
// encountered.
static void grouping(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	compiler->expression();
	compiler->parser.consume(TokenType::RightParen, "Expected ')' after expression.");
}

static void unary(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	const TokenType operatorType = compiler->parser.previous.type;

	// Compile the expression the unary applies to.
	parsePrecedence(compiler, PREC_UNARY);

	// current->emit the operation AFTER the expression, since the expression should be
	// below this operand to have it applied to that expression's result.
	switch (operatorType) {
		case TokenType::Minus:
			compiler->emitByte(OP_NEGATE);
			break;
		case TokenType::Bang:
			compiler->emitByte(OP_NOT);
			break;
		default:
			CL_FATAL("Unexpected unary operation token.");
	}
}

static void number(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	Parser& parser = compiler->parser;
	double value;
	const auto result = std::from_chars(parser.previous.start, parser.previous.start + parser.previous.length, value);

	if (result.ec != std::errc {}) {
		CL_ASSERT(false); // Invalid conversion.
		value = 0.0;
	}
	compiler->emitConstant(Value::makeNumber(value));
}

static void string(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	const std::string_view previous = compiler->parser.previous.view();
	const std::string_view withoutQuotes = previous.substr(1, previous.length() - 2);

	if (withoutQuotes.length() > ObjString::kMaxStringSize) {
		compiler->parser.error("String exceeds length limits.");
	} else {
		compiler->emitConstant(
			makeValue(copyString(withoutQuotes.data(), static_cast<uint32_t>(withoutQuotes.length()))));
	}
}

static void variable(Compiler* compiler, bool canAssign)
{
	compiler->namedVariable(compiler->parser.previous, canAssign);
}

static void this_(Compiler* compiler, bool canAssign)
{
	if (!Compiler::s_classCompiler) {
		compiler->parser.error("Can't use 'this' outside of a class.");
		return;
	}

	variable(compiler, canAssign);
}

static void literal(Compiler* compiler, [[maybe_unused]] bool canAssign)
{
	switch (compiler->parser.previous.type) {
		case TokenType::Nil:
			compiler->emitByte(OP_NIL);
			break;
		case TokenType::True:
			compiler->emitByte(OP_TRUE);
			break;
		case TokenType::False:
			compiler->emitByte(OP_FALSE);
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
	{TokenType::Dot, {nullptr, dot, PREC_CALL}},
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
	{TokenType::This, {this_, nullptr, PREC_NONE}},
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
	Parser parser(source);
	Compiler compiler(nullptr, FunctionType::Script, parser);

	// Prime the pump.
	parser.advance();
	while (!parser.match(TokenType::Eof)) {
		compiler.declaration();
	}
	parser.consume(TokenType::Eof, "Expected end of expression.");
	ObjFunction* function = compiler.end();

	return parser.hadError ? nullptr : function;
}

} // namespace cxxlox