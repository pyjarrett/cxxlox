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

}