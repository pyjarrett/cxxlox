#include "chunk.hpp"

namespace cxxlox {

int32_t growCapacity(const int32_t previousCapacity)
{
	constexpr auto kGrowthFactor = 2;
	constexpr auto kMinCapacity = 8;

	if (previousCapacity < kMinCapacity) {
		return kMinCapacity;
	}
	return previousCapacity * kGrowthFactor;
}

void Chunk::write(uint8_t byte, int32_t line)
{
	this->code.write(byte);
	this->lines.write(line);
}

int32_t Chunk::addConstant(Value value)
{
	this->constants.write(value);
	return this->constants.count() - 1;
}

}