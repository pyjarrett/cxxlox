#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include <object.hpp>
#include <value.hpp>
#include <vm.hpp>

using cxxlox::VM;
using cxxlox::Value;

class VMTest : public cxxlox::LoxTest {};

TEST_F(VMTest, ValueStack)
{
	VM& vm = VM::instance();

	constexpr double phi = 1.6180339;

	vm.push(Value::makeBool(true));
	vm.push(Value::makeNil());
	vm.push(Value::makeNumber(phi));
	vm.push(makeValue(cxxlox::copyString("a string", 8)));

	ASSERT_EQ(vm.peek(0), makeValue(cxxlox::copyString("a string", 8)));
	ASSERT_EQ(vm.peek(1), Value::makeNumber(phi));
	ASSERT_EQ(vm.peek(2), Value::makeNil());
	ASSERT_EQ(vm.peek(3), Value::makeBool(true));

	ASSERT_EQ(vm.pop(), makeValue(cxxlox::copyString("a string", 8)));
	ASSERT_EQ(vm.pop(), Value::makeNumber(phi));
	ASSERT_EQ(vm.pop(), Value::makeNil());
	ASSERT_EQ(vm.pop(), Value::makeBool(true));
}
