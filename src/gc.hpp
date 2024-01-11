#pragma once

#include "table.hpp"
#include "vector.hpp"

namespace cxxlox {

struct Obj;

// A separate type to break the cycle that the VM would need the VM to allocate
// the objects to construct the VM.
struct GC {
	GC() = default;
	~GC();

	[[nodiscard]] static GC& instance();

	void printTracked();

	// Forcibly free all tracked objects.
	void freeObjects();

	void track(Obj* obj);
	void garbageCollect();

	[[nodiscard]] bool wantsToGarbageCollect() const;
	void addUsedMemory(int64_t bytes);

	// String interning.
	void intern(ObjString* str);
	ObjString* lookup(const char* chars, uint32_t length, uint32_t hash) const;

private:
	// Garbage collection.
	void markRoots();
	void traceReferences();
	void sweep();

	Obj* objects = nullptr;

	/// Interned strings.
	Table strings;

public:
	Vector<Obj*, false> grayStack;
	// Garbage collector tracking and tuning.
	int64_t bytesAllocated = 0;
	int64_t nextGC = 128;

	static inline constexpr int64_t kGCHeapGrowFactor = 2;
};

} // namespace cxxlox
