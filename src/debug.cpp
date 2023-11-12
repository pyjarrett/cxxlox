#include "debug.hpp"

#include "chunk.hpp"
#include <format>
#include <iostream>

namespace cxxlox {

void disassembleChunk(const Chunk& chunk, const char* name)
{
	std::cout << std::format("== {} ==\n", name);

	int32_t offset = 0;
	while (offset < chunk.count()) {
		offset = disassembleInstruction(chunk, offset);
	}
}

/// Print a simple instruction and move to the next byte offset.
[[nodiscard]] static int32_t simpleInstruction(const char* name, int32_t offset)
{
	std::cout << name << '\n';
	return offset + 1;
}

int32_t disassembleInstruction(const Chunk& chunk, int32_t offset)
{
	std::cout << std::format("{:04} ", offset);
	const uint8_t instruction = chunk[offset];
	switch (instruction) {
		case OP_RETURN:
			return simpleInstruction("OP_RETURN", offset);
		default:
			std::cout << "Unknown opcode: " << instruction << '\n';
			return offset + 1;
	}
}

} // namespace cxxlox