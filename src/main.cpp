#include "chunk.hpp"
#include "common.hpp"
#include "debug.hpp"

int main(int argc, char** argv)
{
	using namespace cxxlox;

	Chunk chunk;
	chunk.write(OP_RETURN);

	disassembleChunk(chunk, "test chunk");
	return 0;
}
