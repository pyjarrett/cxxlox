#include <gtest/gtest.h>
#include <object.hpp>
#include <value.hpp>
#include <vm.hpp>

#include <cstring>
#include <string>

using cxxlox::ObjString;
using cxxlox::Table;
using cxxlox::Value;
using cxxlox::VM;

TEST(ObjString, Copy)
{
	// Interned strings rely on the VM.
	VM::reset();

	std::string aString = "this is a string";
	ObjString* first = cxxlox::copyString(aString.c_str(), aString.length());
	ObjString* second = cxxlox::copyString(aString.c_str(), aString.length());

	EXPECT_EQ(first, second);
	EXPECT_EQ(first->hash, second->hash);
	EXPECT_EQ(strncmp(first->chars, aString.c_str(), aString.length()), 0);
	EXPECT_EQ(strncmp(second->chars, aString.c_str(), aString.length()), 0);

	// Mutating the source string doesn't change the ObjStrings
	aString[2] = 'a';
	aString[3] = 't';
	EXPECT_NE(strncmp(first->chars, aString.c_str(), aString.length()), 0);

	EXPECT_EQ(first, second);
	EXPECT_EQ(strncmp(first->chars, second->chars, aString.length()), 0);
}
