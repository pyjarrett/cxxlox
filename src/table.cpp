#include "table.hpp"

#include "memory.hpp"
#include "object.hpp"

#include <cstring>
#include <format>
#include <iostream>

namespace cxxlox {

bool Entry::isTombstone() const
{
	return key == nullptr && !value.isNil();
}

void Entry::setTombstone()
{
	key = nullptr;
	value = Value::makeBool(true);
}

Table::Table()
{
	count_ = 0;
	capacity_ = 0;
	entries_ = nullptr;
}

Table::~Table()
{
	delete[] entries_;
}

ObjString* Table::findKey(const char* chars, uint32_t length, uint32_t hash) const
{
	if (count_ == 0) {
		return nullptr;
	}

	// Find where the entry **should** go.
	uint32_t index = hash % capacity_;

	while (true) {
		Entry* entry = &entries_[index];

		// A null key can indicate an empty slot, or a tombstone in a chain
		// of entries.
		if (entry->key == nullptr) {
			if (!entry->isTombstone()) {
				// Don't care about tombstones this time around.
				return entry->key;
			}
		} else if (entry->key->length == length && entry->key->hash == hash &&
				   std::memcmp(entry->key->chars, chars, length) == 0)
			{
				// Found the target.
				return entry->key;
			}

		// If the correct key wasn't found, use linear probing to find other
		// locations where it should be.
		index = (index + 1) % capacity_;
	}

	CL_FATAL("Couldn't find a possible entry in the hash table for a key.");
	return nullptr;
}

bool Table::set(ObjString* key, Value value)
{
	CL_ASSERT(key);

	if (count_ + 1 >= capacity_ * kMaxLoadFactor) {
		adjustCapacity();
	}

	Entry* entry = findEntry(entries_, capacity_, key);

	// The entry might have been a tombstone.
	const bool isNewKey = entry->key == nullptr && !entry->isTombstone();
	if (isNewKey) {
		count_++;
	}
	entry->key = key;
	entry->value = value;

	return isNewKey;
}

bool Table::get(ObjString* key, Value* outValue)
{
	if (count_ == 0) {
		return false;
	}

	Entry* entry = findEntry(entries_, capacity_, key);
	if (entry->key == nullptr) {
		return false;
	}

	*outValue = entry->value;
	return true;
}

bool Table::remove(ObjString* key)
{
	if (count_ == 0) {
		return false;
	}

	Entry* entry = findEntry(entries_, capacity_, key);
	if (entry->key == nullptr) {
		return false;
	}

	entry->setTombstone();
	return true;
}

void Table::addAll(Table& other)
{
	for (auto i = 0; i < other.count_; ++i) {
		Entry* entry = &other.entries_[i];
		if (entry->key != nullptr) {
			set(entry->key, entry->value);
		}
	}
}

void Table::print()
{
	int gap = 0;
	for (int i = 0; i < capacity_; ++i) {
		Entry* entry = &entries_[i];
		if (entry->key == nullptr && !entry->isTombstone()) {
			++gap;
			if (gap == 1) {
				std::cout << std::format("{:40} {}", ' ', i);
			}
		}
		else {
			if (gap > 0) {
				if (gap > 1) {
					std::cout << std::format("...{} ({} empty)\n", i - 1, gap);
				}
				else {
					std::cout << " (empty)\n";
				}
			}
			gap = 0;

			if (entry->isTombstone()) {
				std::cout << std::format("[{:6}] ", i);
				std::cout << "<<>>\n";
			}
			else {
				std::cout << std::format("[{:6}] ", i) << asObj(entry->key);
				std::cout << " " << entry->value << '\n';
			}
		}
	}
	if (gap > 0) {
		if (gap > 1) {
			std::cout << std::format("...{} ({} empty)\n", capacity_, gap);
		}
		else {
			std::cout << " (empty)\n";
		}
	}
	std::cout << "^--- Contains " << count_ << " of " << capacity_ << " with max: " << capacity_ * kMaxLoadFactor << '\n';
}

void Table::mark()
{
	for (int i = 0; i < capacity_; ++i) {
		Entry& entry = entries_[i];
		markObject(asObj(entry.key));
		markValue(&entry.value);
	}
}


void Table::removeUnmarked()
{
	for (int i = 0; i < capacity_; ++i) {
		Entry* entry = &entries_[i];
		if (entry->key && !asObj(entry->key)->isMarked) {
			remove(entry->key);
		}
	}
}


void Table::adjustCapacity()
{
	// Create a new array for storing the contents.
	const auto newCapacity = growCapacity(capacity_);

	// `new` default constructrs elements, so we don't need to set them manually.
	Entry* newEntries = new Entry[newCapacity];

	// Copy the contents, except tombstones over to the new array, into the
	// appropriate new locations, since the capacity changed.  Track the new
	// count, since tombstones won't be copied over.
	int32_t newCount = 0;
	for (int i = 0; i < capacity_; ++i) {
		Entry* src = &entries_[i];
		if (src->key == nullptr) {
			continue;
		}

		Entry* dest = findEntry(newEntries, newCapacity, src->key);
		dest->key = src->key;
		dest->value = src->value;
		++newCount;
	}

	delete[] entries_;
	entries_ = newEntries;
	count_ = newCount;
	capacity_ = newCapacity;
}

Entry* Table::findEntry(Entry* entries, int32_t capacity, ObjString* key)
{
	// Find where the entry **should** go.
	uint32_t index = key->hash % capacity;

	// The first encountered tombstone;
	Entry* tombstone = nullptr;

	while (true) {
		Entry* entry = &entries[index];

		// A null key can indicate an empty slot, or a tombstone in a chain
		// of entries.
		if (entry->key == nullptr) {
			if (!entry->isTombstone()) {
				// Found an empty slot, so reuse an earlier tombstone if
				// possible, otherwise, this is the best target location.
				return tombstone ? tombstone : entry;
			} else if (!tombstone) {
				// This tombstone is the earliest possible insertion point.
				tombstone = entry;
			}
		} else if (entry->key == key) {
			// Found the target.
			return entry;
		}

		// If the correct key wasn't found, use linear probing to find other
		// locations where it should be.
		index = (index + 1) % capacity;
	}

	CL_FATAL("Couldn't find a possible entry in the hash table for a key.");
	return nullptr;
}

} // namespace cxxlox
