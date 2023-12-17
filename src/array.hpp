#pragma once

#include "common.hpp"

#include <algorithm>

namespace cxxlox {

template <typename T>
class Array
{
public:
	Array() = default;
	~Array();

	Array(const Array&) = delete;
	Array& operator=(const Array&) = delete;

	Array(Array&&) = delete;
	Array& operator=(Array&&) = delete;

	void write(T value);
	void free();

	// Reserve more space in the array.  Does nothing if there are more elements
	// than the desired new capacity.
	void reserve(int32_t newCapacity);

	[[nodiscard]] int32_t capacity() const { return capacity_; }
	[[nodiscard]] int32_t count() const { return count_; }

	const T& operator[](int32_t) const;
	T& operator[](int32_t);

private:
	/// The number of used elements.
	int32_t count_ = 0;

	/// The allocated array size.
	int32_t capacity_ = 0;

	/// Dynamically allocated array of data.
	T* data = nullptr;
};
static_assert(sizeof(Array<char>) == 16);

template <typename T>
Array<T>::~Array()
{
	free();
}

template <typename T>
void Array<T>::write(T value)
{
	if (capacity_ < count_ + 1) {
		const auto newCapacity = growCapacity(capacity_);
		reserve(newCapacity);
	}
	CL_ASSERT(count_ <= capacity_);
	data[count_] = value;
	++count_;
}

template <typename T>
void Array<T>::reserve(int32_t newCapacity)
{
	if (newCapacity < count_) {
		CL_FATAL("Trying to reserve array with smaller capacity than the current number of elements.");
		return;
	}

	T* larger = new T[newCapacity];
	std::copy(data, data + capacity_, larger);
	delete[] data;
	data = larger;
	capacity_ = newCapacity;

	CL_ASSERT(count_ <= newCapacity);
}

template <typename T>
void Array<T>::free()
{
	delete[] data;
	data = nullptr;
	count_ = 0;
	capacity_ = 0;
}

template <typename T>
const T& Array<T>::operator[](int32_t index) const
{
	CL_ASSERT(count_ <= capacity_);
	CL_ASSERT(index >= 0 && index < count_);
	return data[index];
}

template <typename T>
T& Array<T>::operator[](int32_t index)
{
	CL_ASSERT(count_ <= capacity_);
	CL_ASSERT(index >= 0 && index < count_);
	return data[index];
}

} // namespace cxxlox