#include "debug.hpp"

#include "chunk.hpp"
#include <format>
#include <iostream>

namespace cxxlox {

void disassembleChunk(const Chunk& chunk, const char* name)
{
	std::cout << std::format("== {} ==\n", name);

	int32_t offset = 0;
	while (offset < chunk.code.count()) {
		offset = disassembleInstruction(chunk, offset);
	}
}

static void printValue(Value v)
{
	std::cout << v;
}

/// Print a simple instruction and move to the next byte offset.
[[nodiscard]] static int32_t simpleInstruction(const char* name, int32_t offset)
{
	std::cout << name << '\n';
	return offset + 1;
}

[[nodiscard]] static int32_t constantInstruction(const char* name, const Chunk& chunk, int32_t offset)
{
	// Get the location of the constant from the bytecode.
	// [OP_CONSTANT] [constant_index]
	const auto constantIndex = chunk.code[offset + 1];
	std::cout << std::format("{:<16} {:4} '", name, constantIndex);
	printValue(chunk.constants[constantIndex]);
	std::cout << "'\n";

	// Skip the instruction (byte 1) and the constant (byte 2).
	return offset + 2;
}

int32_t disassembleInstruction(const Chunk& chunk, int32_t offset)
{
	std::cout << std::format("{:04} ", offset);

	if (offset > 0 && chunk.lines[offset - 1] == chunk.lines[offset]) {
		std::cout << "   | ";
	} else {
		std::cout << std::format("{:4} ", chunk.lines[offset]);
	}

	const uint8_t instruction = chunk.code[offset];
	switch (instruction) {
		case OP_CONSTANT:
			return constantInstruction("OP_CONSTANT", chunk, offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		default:
			std::cout << "Unknown opcode: " << int(instruction) << '\n';
			return offset + 1;
	}
}

} // namespace cxxlox