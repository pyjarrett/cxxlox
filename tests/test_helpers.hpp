#pragma once

#include "vm.hpp"
#include <gtest/gtest.h>

namespace cxxlox {

// There is common state due to how the Lox bytecode VM is designed.  Reset this
// state between test runs.
class LoxTest : public testing::Test
{
protected:
	void SetUp() override
	{
		// Interned strings rely on the VM.
		VM::reset();
	}
};

} // namespace cxxlox
