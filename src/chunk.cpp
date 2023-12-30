#include "chunk.hpp"

namespace cxxlox {

void Chunk::write(uint8_t byte, int32_t line)
{
	this->code.push(byte);
	this->lines.push(line);
}

int32_t Chunk::addConstant(Value value)
{
	this->constants.push(value);
	return this->constants.count() - 1;
}

} // namespace cxxlox