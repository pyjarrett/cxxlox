#pragma once

#include "common.hpp"
#include "value.hpp"

namespace cxxlox {

struct Chunk;

struct VM {
	Chunk* chunk = nullptr;

	/// The next instruction to be executed.
	/// "Instruction pointer" ("program counter").
	const uint8_t* ip = nullptr;

	[[nodiscard]] uint8_t readByte();
	[[nodiscard]] Value readConstant();
};

void initVM();
void freeVM();

enum class InterpretResult {
	Ok,
	CompileError,
	RuntimeError,
};

InterpretResult interpret(Chunk* chunk);

} // namespace cxxlox