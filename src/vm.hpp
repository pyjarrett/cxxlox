#pragma once

#include "common.hpp"
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

	VM();
	~VM();

	[[nodiscard]] uint8_t readByte();
	[[nodiscard]] Value readConstant();

	void push(Value value);
	[[nodiscard]] Value pop();

	[[nodiscard]] Value peek(int distance) const;

	void runtimeError(const std::string& message);

	[[nodiscard]] InterpretResult interpret(const std::string& source);

	void track(Obj* obj);

private:
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
};

InterpretResult interpret(const std::string& source);

} // namespace cxxlox