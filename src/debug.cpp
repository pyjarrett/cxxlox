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
		case OP_NIL:
			return simpleInstruction("OP_NIL", offset);
		case OP_TRUE:
			return simpleInstruction("OP_TRUE", offset);
		case OP_FALSE:
			return simpleInstruction("OP_FALSE", offset);
		case OP_ADD:
			return simpleInstruction("OP_ADD", offset);
		case OP_SUBTRACT:
			return simpleInstruction("OP_SUBTRACT", offset);
		case OP_MULTIPLY:
			return simpleInstruction("OP_MULTIPLY", offset);
		case OP_DIVIDE:
			return simpleInstruction("OP_DIVIDE", offset);
		case OP_NOT:
			return simpleInstruction("OP_NOT", offset);
		case OP_NEGATE:
			return simpleInstruction("OP_NEGATE", offset);
		case OP_PRINT:
			return simpleInstruction("OP_PRINT", offset);
		case OP_POP:
			return simpleInstruction("OP_POP", offset);
		case OP_GET_GLOBAL:
			return constantInstruction("OP_GET_GLOBAL", chunk, offset);
		case OP_DEFINE_GLOBAL:
			return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		case OP_EQUAL:
			return simpleInstruction("OP_EQUAL", offset);
		case OP_GREATER:
			return simpleInstruction("OP_GREATER", offset);
		case OP_LESS:
			return simpleInstruction("OP_LESS", offset);
		default:
			std::cout << "Unknown opcode: " << int(instruction) << '\n';
			return offset + 1;
	}
}

} // namespace cxxlox