#pragma once

#include "common.hpp"

#include <algorithm>

namespace cxxlox {

enum OpCode : uint8_t
{
	OP_RETURN
};

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

	[[nodiscard]] int32_t capacity() const { return capacity_; }
	[[nodiscard]] int32_t count() const { return count_; }

	const T& operator[](int32_t) const;

private:
	/// The number of used elements.
	int32_t count_ = 0;

	/// The allocated array size.
	int32_t capacity_ = 0;

	/// Dynamically allocated array of data.
	T* data = nullptr;
};

[[nodiscard]] int32_t growCapacity(int32_t previousCapacity);

using Chunk = Array<uint8_t>;

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
		T* larger = new T[newCapacity];
		std::copy(data, data + capacity_, larger);
		delete [] data;
		data = larger;
	}
	data[count_] = value;
	++count_;
}

template <typename T>
void Array<T>::free()
{
	delete [] data;
	data = nullptr;
	count_ = 0;
	capacity_ = 0;
}

template <typename T>
const T& Array<T>::operator[](int32_t index) const
{
	CL_ASSERT(index >= 0 && index < count_);
	return data[index];
}

} // namespace cxxlox