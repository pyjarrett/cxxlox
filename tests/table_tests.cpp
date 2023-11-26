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
	table.set(makeString("truth"), Value::makeBool(true));

	Value value {};
	EXPECT_TRUE(table.get(makeString("truth"), &value));
	EXPECT_TRUE(value.isBool() && value.as.boolean == true);

	table.set(makeString("thirty"), Value::makeNumber(30));
	EXPECT_TRUE(table.get(makeString("thirty"), &value));
	EXPECT_TRUE(value.isNumber() && value.as.number == 30);
}

TEST_F(TableTest, SetWithResize)
{
	Table table;
	for (int i = 0; i < 200; ++i) {
		table.set(makeString(std::to_string(i)), Value::makeNumber(i));
	}

	table.print();
	for (int i = 0; i < 200; ++i) {
		Value value = Value::makeNil();
		EXPECT_TRUE(table.get(makeString(std::to_string(i)), &value));
		EXPECT_TRUE(value.isNumber());
		EXPECT_EQ(value.toNumber(), i);
	}
}
