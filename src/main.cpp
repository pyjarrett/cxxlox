#include "chunk.hpp"
#include "common.hpp"
#include "debug.hpp"

int main(int argc, char** argv)
{
	using namespace cxxlox;

	Chunk chunk;
	const auto constant = chunk.addConstant(1.2);
	chunk.write(OP_CONSTANT, 123);
	chunk.write(constant, 123);
	chunk.write(OP_RETURN, 123);

	disassembleChunk(chunk, "test chunk");
	return 0;
}
