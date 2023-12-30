#include <gtest/gtest.h>

#include "test_helpers.hpp"
#include <object.hpp>
#include <value.hpp>
#include <vm.hpp>

using cxxlox::Table;
using cxxlox::Value;
using cxxlox::VM;

class TableTest : public cxxlox::LoxTest {};

static cxxlox::ObjString* makeString(std::string_view view)
{
	return cxxlox::copyString(view.data(), view.length());
}

TEST_F(TableTest, BasicSet)
{
	Table table;
	EXPECT_TRUE(table.set(makeString("truth"), Value::makeBool(true)));
	// Reusing variable name, shouldn't create a new variable.
	EXPECT_FALSE(table.set(makeString("truth"), Value::makeBool(false)));
	EXPECT_FALSE(table.set(makeString("truth"), Value::makeBool(true)));

	Value value {};
	EXPECT_TRUE(table.get(makeString("truth"), &value));
	EXPECT_TRUE(value.isBool() && value.as.boolean == true);

	EXPECT_TRUE(table.set(makeString("thirty"), Value::makeNumber(30)));
	EXPECT_TRUE(table.get(makeString("thirty"), &value));
	EXPECT_TRUE(value.isNumber() && value.as.number == 30);
}

TEST_F(TableTest, SetWithResize)
{
	Table table;
	for (int i = 0; i < 200; ++i) {
		// Need to track the strings on the stack to prevent them from being GC'd
		auto key = makeString(std::to_string(i));
		VM::instance().push(Value::makeString(key));
		table.set(makeString(std::to_string(i)), Value::makeNumber(i));
	}

	table.print();
	for (int i = 0; i < 200; ++i) {
		auto key = makeString(std::to_string(i));
		Value value = Value::makeNil();
		EXPECT_TRUE(table.get(key, &value));
		EXPECT_TRUE(value.isNumber());
		EXPECT_EQ(value.toNumber(), i);
		CL_UNUSED(VM::instance().pop());
	}
}
