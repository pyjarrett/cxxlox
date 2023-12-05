#pragma once

#include "common.hpp"
#include "table.hpp"
#include "value.hpp"

#include <string>

namespace cxxlox {

struct Chunk;
struct Obj;

enum class InterpretResult
{
	Ok,
	CompileError,
	RuntimeError,
};

struct VM {
	static constexpr int32_t kStackMax = 256;

	static VM& instance();
	static void reset();

	[[nodiscard]] uint8_t readByte();
	[[nodiscard]] uint16_t readShort();
	[[nodiscard]] Value readConstant();
	[[nodiscard]] ObjString* readString();

	void push(Value value);
	[[nodiscard]] Value pop();

	[[nodiscard]] Value peek(int distance) const;

	void runtimeError(const std::string& message);

	[[nodiscard]] InterpretResult interpret(const std::string& source);

	void track(Obj* obj);
	void intern(ObjString* str);
	ObjString* lookup(const char* chars, uint32_t length, uint32_t hash) const;

private:
	VM();
	~VM();

	[[nodiscard]] InterpretResult run();

	void resetStack();
	void freeObjects();

	Chunk* chunk = nullptr;

	/// The next instruction to be executed.
	/// "Instruction pointer" ("program counter").
	const uint8_t* ip = nullptr;

	/// Stack used to store values used as intermediate results and operands.
	Value stack[kStackMax] = {};

	/// A pointer to the element just past the current element.  This is where
	/// the next element will be pushed.
	Value* stackTop = &stack[0];

	/// Objects tracked for garbage collection.
	Obj* objects = nullptr;

	/// Interned strings.
	Table strings;

	/// Global variables.
	Table globals;

	static VM* s_instance;
};

InterpretResult interpret(const std::string& source);

} // namespace cxxlox