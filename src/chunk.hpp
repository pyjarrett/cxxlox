#pragma once

#include "array.hpp"
#include "common.hpp"
#include "value.hpp"

namespace cxxlox {

enum OpCode : uint8_t
{
	OP_CONSTANT,
	OP_RETURN
};

struct Chunk {
	void write(uint8_t byte, int32_t line);
	[[nodiscard]] int32_t addConstant(Value value);

	Array<uint8_t> code;
	Array<Value> constants;
	Array<int32_t> lines;
};

} // namespace cxxlox