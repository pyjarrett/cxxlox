#include "test_helpers.hpp"
#include <array.hpp>
#include <gtest/gtest.h>

// Write tests for the generic array -- they should work both when using the VM's
// garbage collector and also when using the standard allocator.
template <typename T>
class ArrayTest : public cxxlox::LoxTest
{
protected:
	using Array = cxxlox::Array<int, T::value>;
	Array array;
};
using WithAndWithoutGC = ::testing::Types<std::true_type, std::false_type>;
TYPED_TEST_SUITE(ArrayTest, WithAndWithoutGC);

TYPED_TEST(ArrayTest, BasicOps)
{
	// Arrays are created with no capacity, so they can be reserved once if desired.
	EXPECT_EQ(0, this->array.capacity());
	EXPECT_EQ(0, this->array.count());

	// Adding to an empty array forces the first allocation.
	const int start = 100;
	this->array.write(start);
	EXPECT_EQ(1, this->array.count());
	EXPECT_EQ(start, this->array[0]);

	// Write enough values in the array to force another growth.
	const int initialCapacity = this->array.capacity();
	EXPECT_LT(initialCapacity, 4096); // Initial capacity should be "reasonable."
	EXPECT_LT(0, initialCapacity);
	const int numFinalElements = 2 * initialCapacity;
	for (int i = 1; i < numFinalElements; ++i) {
		this->array.write(start + i);
	}
	// Array should have grown and contain the desired total number of elements.
	EXPECT_LT(initialCapacity, this->array.capacity());
	EXPECT_EQ(numFinalElements, this->array.count());

	// Verified values were copied over and new values appended.
	for (int i = 0; i < this->array.count(); ++i) {
		EXPECT_EQ(this->array[i], start + i);
	}

	this->array.free();
	EXPECT_EQ(0, this->array.count());
	EXPECT_EQ(0, this->array.capacity());
}

TYPED_TEST(ArrayTest, Reserve)
{
	EXPECT_EQ(0, this->array.capacity());
	EXPECT_EQ(0, this->array.count());

	const int numReservedItems = 21;
	const int numDesiredItems = 35;
	static_assert(numReservedItems < numDesiredItems, "Test should try to force a capacity growth.");

	// Reserving doesn't make any items.
	this->array.reserve(numReservedItems);
	EXPECT_EQ(numReservedItems, this->array.capacity());
	EXPECT_EQ(0, this->array.count());

	// Fill with items.
	for (int i = 0; i < numReservedItems; ++i) {
		this->array.write(i);
	}
	// Just filled the array and didn't allocate any more space.
	EXPECT_EQ(numReservedItems, this->array.capacity());
	EXPECT_EQ(numReservedItems, this->array.count());
	for (int i = 0; i < numReservedItems; ++i) {
		EXPECT_EQ(this->array[i], i);
	}

	// Overflow with items and force a growth.
	for (int i = 0; i < (numDesiredItems - numReservedItems); ++i) {
		this->array.write(numReservedItems + i);
	}
	// Should have grown for more items than reserved.
	EXPECT_LT(numReservedItems, this->array.capacity());
	EXPECT_EQ(numDesiredItems, this->array.count());
	// Preexisting items were copied over, new items were added.
	for (int i = 0; i < numDesiredItems; ++i) {
		EXPECT_EQ(this->array[i], i);
	}

	// It's unlikely a growth factor is EXACTLY the same (not the best assert)
	// but here to ensure there's extra space in this particular case before
	// the following reserve to prove capacity shrinkage works.
	EXPECT_LT(numDesiredItems, this->array.capacity());
	this->array.reserve(numDesiredItems);
	EXPECT_EQ(numDesiredItems, this->array.capacity());

	// Trying to reserve with less than the current count has no effect.
	this->array.reserve(numReservedItems);
	EXPECT_EQ(numDesiredItems, this->array.capacity());
	EXPECT_EQ(numDesiredItems, this->array.count());
}
