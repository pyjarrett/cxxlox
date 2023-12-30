#include "test_helpers.hpp"
#include <vector.hpp>
#include <gtest/gtest.h>

// Write tests for the generic vec -- they should work both when using the VM's
// garbage collector and also when using the standard allocator.
template <typename T>
class VectorTest : public cxxlox::LoxTest
{
protected:
	using Vector = cxxlox::Vector<int, T::value>;
	Vector vec;
};
using WithAndWithoutGC = ::testing::Types<std::true_type, std::false_type>;
TYPED_TEST_SUITE(VectorTest, WithAndWithoutGC);

TYPED_TEST(VectorTest, BasicOps)
{
	// Arrays are created with no capacity, so they can be reserved once if desired.
	EXPECT_EQ(0, this->vec.capacity());
	EXPECT_EQ(0, this->vec.count());

	// Adding to an empty vec forces the first allocation.
	const int start = 100;
	this->vec.push(start);
	EXPECT_EQ(1, this->vec.count());
	EXPECT_EQ(start, this->vec[0]);

	// Write enough values in the vec to force another growth.
	const int initialCapacity = this->vec.capacity();
	EXPECT_LT(initialCapacity, 4096); // Initial capacity should be "reasonable."
	EXPECT_LT(0, initialCapacity);
	const int numFinalElements = 2 * initialCapacity;
	for (int i = 1; i < numFinalElements; ++i) {
		this->vec.push(start + i);
	}
	// Array should have grown and contain the desired total number of elements.
	EXPECT_LT(initialCapacity, this->vec.capacity());
	EXPECT_EQ(numFinalElements, this->vec.count());

	// Verified values were copied over and new values appended.
	for (int i = 0; i < this->vec.count(); ++i) {
		EXPECT_EQ(this->vec[i], start + i);
	}

	this->vec.clear();
	EXPECT_EQ(0, this->vec.count());
	EXPECT_EQ(0, this->vec.capacity());
}

TYPED_TEST(VectorTest, AddAndRemove)
{
	this->vec.push(1);
	this->vec.push(3);
	this->vec.push(5);
	this->vec.push(7);
	this->vec.push(9);
	EXPECT_EQ(5, this->vec.count());

	EXPECT_EQ(9, this->vec.pop());
	EXPECT_EQ(4, this->vec.count());

	EXPECT_EQ(7, this->vec.pop());
	EXPECT_EQ(3, this->vec.count());
}

TYPED_TEST(VectorTest, Reserve)
{
	EXPECT_EQ(0, this->vec.capacity());
	EXPECT_EQ(0, this->vec.count());

	const int numReservedItems = 21;
	const int numDesiredItems = 35;
	static_assert(numReservedItems < numDesiredItems, "Test should try to force a capacity growth.");

	// Reserving doesn't make any items.
	this->vec.reserve(numReservedItems);
	EXPECT_EQ(numReservedItems, this->vec.capacity());
	EXPECT_EQ(0, this->vec.count());

	// Fill with items.
	for (int i = 0; i < numReservedItems; ++i) {
		this->vec.push(i);
	}
	// Just filled the vec and didn't allocate any more space.
	EXPECT_EQ(numReservedItems, this->vec.capacity());
	EXPECT_EQ(numReservedItems, this->vec.count());
	for (int i = 0; i < numReservedItems; ++i) {
		EXPECT_EQ(this->vec[i], i);
	}

	// Overflow with items and force a growth.
	for (int i = 0; i < (numDesiredItems - numReservedItems); ++i) {
		this->vec.push(numReservedItems + i);
	}
	// Should have grown for more items than reserved.
	EXPECT_LT(numReservedItems, this->vec.capacity());
	EXPECT_EQ(numDesiredItems, this->vec.count());
	// Preexisting items were copied over, new items were added.
	for (int i = 0; i < numDesiredItems; ++i) {
		EXPECT_EQ(this->vec[i], i);
	}

	// It's unlikely a growth factor is EXACTLY the same (not the best assert)
	// but here to ensure there's extra space in this particular case before
	// the following reserve to prove capacity shrinkage works.
	EXPECT_LT(numDesiredItems, this->vec.capacity());
	this->vec.reserve(numDesiredItems);
	EXPECT_EQ(numDesiredItems, this->vec.capacity());

	// Trying to reserve with less than the current count has no effect.
	this->vec.reserve(numReservedItems);
	EXPECT_EQ(numDesiredItems, this->vec.capacity());
	EXPECT_EQ(numDesiredItems, this->vec.count());
}
