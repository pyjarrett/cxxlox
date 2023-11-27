#pragma once

#include "array.hpp"
#include "common.hpp"
#include "value.hpp"

namespace cxxlox {

/// Plain enum instead of enum class to allow easier conversion to uint8_t
enum OpCode : uint8_t
{
	OP_CONSTANT,
	OP_ADD,
	OP_SUBTRACT,
	OP_MULTIPLY,
	OP_DIVIDE,
	OP_NOT,
	OP_NEGATE,
	OP_PRINT,
	OP_RETURN,

	// Comparisons
	// Note that >= and <= are implemented as !< and !> respectively.
	OP_EQUAL,
	OP_GREATER,
	OP_LESS,

	// Literals
	OP_NIL,
	OP_TRUE,
	OP_FALSE,
};

/// I'm not sure how to structure this yet since I'm porting this from examples
/// in plain C, and I'm not sure where to draw lines for system and type
/// boundaries yet.
struct Chunk {
	void write(uint8_t byte, int32_t line);
	[[nodiscard]] int32_t addConstant(Value value);

	Array<uint8_t> code;
	Array<Value> constants;
	Array<int32_t> lines;
};

} // namespace cxxlox