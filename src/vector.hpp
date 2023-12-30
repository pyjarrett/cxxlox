#pragma once

#include "common.hpp"
#include "memory.hpp"
#include <algorithm>

namespace cxxlox {

// A very simple resizable array for trivially copyable types.
// Goes along with the "write the project with the bare minimum" theme.
//
// Going with the bool for TrackWithGC for now.  In the debugger it might be
// better to see an allocator selection tag like "AllocatorTag::TrackWithGC"
// instead of "true" in the typename, but I'm going with the simpler solution
// for now.
template <typename T, bool TrackWithGC = true>
class Vector
{
	static_assert(std::is_trivially_copyable_v<T>, "Array only works with trivial types.");

public:
	Vector() = default;
	~Vector();

	Vector(const Vector&) = delete;
	Vector& operator=(const Vector&) = delete;

	Vector(Vector&&) = delete;
	Vector& operator=(Vector&&) = delete;

	void clear();
	void push(T value);

	// Reserve more space in the array.  Does nothing if there are more elements
	// than the desired new capacity.
	void reserve(int32_t newCapacity);

	[[nodiscard]] int32_t capacity() const { return capacity_; }
	[[nodiscard]] int32_t count() const { return count_; }

	[[nodiscard]] const T& operator[](int32_t) const;
	[[nodiscard]] T& operator[](int32_t);

private:
	/// The number of used elements.
	int32_t count_ = 0;

	/// The allocated array size.
	int32_t capacity_ = 0;

	/// Dynamically allocated array of data.
	T* data = nullptr;
};
static_assert(sizeof(Vector<char>) == 16);

template <typename T, bool TrackWithGC>
Vector<T, TrackWithGC>::~Vector()
{
	clear();
}

template <typename T, bool TrackWithGC>
void Vector<T, TrackWithGC>::push(T value)
{
	if (capacity_ < count_ + 1) {
		const auto newCapacity = growCapacity(capacity_);
		reserve(newCapacity);
	}
	CL_ASSERT(count_ <= capacity_);
	data[count_] = value;
	++count_;
}

template <typename T, bool TrackWithGC>
void Vector<T, TrackWithGC>::reserve(int32_t newCapacity)
{
	if (newCapacity < count_) {
		return;
	}

	T* larger = nullptr;
	if constexpr (TrackWithGC) {
		// Use the local Lox realloc function.
		// Realloc copies and frees on its own.
		larger = reinterpret_cast<T*>(cxxlox::realloc(data, sizeof(T) * capacity_, sizeof(T) * newCapacity));
	} else {
		larger = new T[newCapacity];
		std::copy(data, data + count_, larger);
		delete[] data;
	}

	data = larger;
	capacity_ = newCapacity;

	CL_ASSERT(count_ <= newCapacity);
}

template <typename T, bool TrackWithGC>
void Vector<T, TrackWithGC>::clear()
{
	if constexpr (TrackWithGC) {
		std::free(reinterpret_cast<void*>(data));
	} else {
		delete[] data;
	}
	data = nullptr;
	count_ = 0;
	capacity_ = 0;
}

template <typename T, bool TrackWithGC>
const T& Vector<T, TrackWithGC>::operator[](int32_t index) const
{
	CL_ASSERT(count_ <= capacity_);
	CL_ASSERT(index >= 0 && index < count_);
	return data[index];
}

template <typename T, bool TrackWithGC>
T& Vector<T, TrackWithGC>::operator[](int32_t index)
{
	CL_ASSERT(count_ <= capacity_);
	CL_ASSERT(index >= 0 && index < count_);
	return data[index];
}

} // namespace cxxlox