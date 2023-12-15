#include "pratt.hpp"

namespace cxxlox {

PrattRuleMap::PrattRuleMap(std::initializer_list<PrattRuleRow> items)
{
	rules_ = new ParseRule[items.size()];

	// Require the rules to be in order.  This also ensures that every token
	// type gets covered and none forgotten.
	int nextRuleIndex = 0;
	for (const auto& row : items) {
		const int index = static_cast<int>(row.type);
		CL_ASSERT(index == nextRuleIndex);
		++nextRuleIndex;
		rules_[index] = row.rule;
	}
}

} // namespace cxxlox
