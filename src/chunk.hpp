#pragma once

#include "common.hpp"
#include "value.hpp"
#include "vector.hpp"

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
	OP_JUMP,
	OP_JUMP_IF_FALSE,
	OP_LOOP,
	OP_CALL,

	// Optimized function for method invocations.
	OP_INVOKE,

	// Optimized invocation of super method.
	OP_SUPER_INVOKE,

	OP_CLOSURE,
	OP_CLOSE_UPVALUE,
	OP_POP,
	OP_GET_LOCAL,
	OP_GET_GLOBAL,
	OP_DEFINE_GLOBAL,
	OP_GET_UPVALUE,
	OP_SET_UPVALUE,
	OP_SET_LOCAL,
	OP_SET_GLOBAL,

	// Really this is SET_FIELD, but named to be consistent with OP_GET_PROPERTY
	OP_SET_PROPERTY,
	OP_GET_PROPERTY,
	OP_RETURN,
	OP_CLASS,
	OP_GET_SUPER,

	// Applies inheritance to a class.
	OP_INHERIT,

	// Creates a method on the class at the top of the stack.
	OP_METHOD,

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

	Vector<uint8_t> code;
	Vector<Value> constants;
	Vector<uint32_t> lines;

	Chunk() = default;
	~Chunk() = default;
	CL_PROHIBIT_MOVE_AND_COPY(Chunk);
};
static_assert(sizeof(Chunk) == 3 * sizeof(Vector<char>));

} // namespace cxxlox