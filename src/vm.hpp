#pragma once

#include "common.hpp"
#include "value.hpp"

#include <string> qwq

namespace cxxlox {

struct Chunk;

struct VM {
	static constexpr int32_t kStackMax = 256;

	Chunk* chunk = nullptr;

	/// The next instruction to be executed.
	/// "Instruction pointer" ("program counter").
	const uint8_t* ip = nullptr;

	/// Stack used to store values used as intermediate results and operands.
	Value stack[kStackMax] = {};

	/// A pointer to the element just past the current element.  This is where
	/// the next element will be pushed.
	Value* stackTop = &stack[0];

	VM();

	void resetStack();

	[[nodiscard]] uint8_t readByte();
	[[nodiscard]] Value readConstant();

	void push(Value value);
	[[nodiscard]] Value pop();
};

void initVM();
void freeVM();

enum class InterpretResult
{
	Ok,
	CompileError,
	RuntimeError,
};

[[nodiscard]] InterpretResult interpret(const std::string& source);

} // namespace cxxlox