#include "common.hpp"

namespace cxxlox {

int32_t growCapacity(const int32_t previousCapacity) noexcept
{
	constexpr auto kGrowthFactor = 2;
	constexpr auto kMinCapacity = 8;

	if (previousCapacity < kMinCapacity) {
		return kMinCapacity;
	}
	return previousCapacity * kGrowthFactor;
}

} // namespace cxxlox
