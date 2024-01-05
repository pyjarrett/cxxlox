#pragma once

#include "common.hpp"
#include "value.hpp"

namespace cxxlox {

struct ObjString;

struct Entry final {
	ObjString* key = nullptr;
	Value value = Value::makeNil();

	// An entry removed from the hash table, but remains with a sentinel value
	// to preserve chaining so that linear probing still finds the appropriate
	// values.
	[[nodiscard]] bool isTombstone() const;
	void setTombstone();
};

// A simple hash table implementation using strings as keys.
struct Table final {
public:
	Table();
	~Table();

	Table(const Table&) = delete;
	Table& operator=(const Table&) = delete;

	Table(Table&&) = delete;
	Table& operator=(Table&&) = delete;

	ObjString* findKey(const char* chars, uint32_t length, uint32_t hash) const;

	bool set(ObjString* key, Value value);
	bool get(ObjString* key, Value* outValue);

	bool remove(ObjString* key);
	void addAll(Table& other);

	void print();

	// Mark objects for garbage collector.
	void mark();
	void removeUnmarked();

private:
	void adjustCapacity();
	[[nodiscard]] static Entry* findEntry(Entry* entries, int32_t capacity, ObjString* key);

	static constexpr double kMaxLoadFactor = 0.75;

	int32_t count_ = 0;
	int32_t capacity_ = 0;
	Entry* entries_ = nullptr;
};
} // namespace cxxlox
