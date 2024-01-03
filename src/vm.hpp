#pragma once

#include "common.hpp"
#include "object.hpp"
#include "table.hpp"
#include "value.hpp"
#include "vector.hpp"

#include <string>

namespace cxxlox {

struct Chunk;
struct Obj;
struct ObjFunction;

enum class InterpretResult
{
	Ok,
	CompileError,
	RuntimeError,
};

// An interpreted function call frame.  Native functions called from Lox do
// not use this.
struct CallFrame {
	ObjClosure* closure = nullptr;

	// The **next** instruction to be executed.
	// "Instruction pointer" ("program counter").
	uint8_t* ip = nullptr;

	// Points to the base argument of the called function.  This will be a slice
	// of the VM value stack.
	Value* slots = nullptr;
};
static_assert(sizeof(CallFrame) == 24);

struct VM {
	static constexpr int32_t kFramesMax = 64;
	static constexpr int32_t kStackMax = kFramesMax * kUInt8Count;

	static VM& instance();

	// Function for resetting the VM during testing.
	void reset();

	// I tried using CL_FORCE_INLINE on this, but it optimizes down to 3
	// instructions at -O2 on GCC, force inlining takes 6 in debug and pollutes
	// the assembly, making it harder to read.
	//
	// On Windows RelWithDebInfo, it's a bit faster (>25%).
	[[nodiscard]] CallFrame* currentFrame();

	[[nodiscard]] uint8_t readByte();
	[[nodiscard]] uint16_t readShort();
	[[nodiscard]] Value readConstant();
	[[nodiscard]] ObjString* readString();

	void push(Value value);
	[[nodiscard]] Value pop();

	[[nodiscard]] Value peek(int distance) const;

	[[nodiscard]] bool call(ObjClosure* closure, int argCount);
	[[nodiscard]] bool callValue(Value callee, int argCount);

	[[nodiscard]] ObjUpvalue* captureUpvalue(Value* local);
	void closeUpvalues(Value* last);

	void defineMethod(ObjString* methodName);

	void runtimeError(const std::string& message);

	void defineNative(const char* name, NativeFunction fn);

	[[nodiscard]] InterpretResult interpret(const std::string& source);

	void garbageCollect();

	[[nodiscard]] bool wantsToGarbageCollect() const;
	void addUsedMemory(int64_t bytes);
	void track(Obj* obj);
	void intern(ObjString* str);
	ObjString* lookup(const char* chars, uint32_t length, uint32_t hash) const;

	Vector<Obj*, false> grayStack;

private:
	VM();
	~VM();

	[[nodiscard]] InterpretResult run();

	void resetStack();
	void freeObjects();

	// Garbage collection.
	void markRoots();
	void traceReferences();
	void sweep();

	void loadNativeFunctions();

	/// The current state of the active chain of program function calls.
	CallFrame frames[kFramesMax];
	int32_t frameCount = 0;

	/// Stack used to store values used as intermediate results and operands.
	Value stack[kStackMax] = {};

	/// A pointer to the element just past the current element.  This is where
	/// the next element will be pushed.
	Value* stackTop = &stack[0];

	/// Objects tracked for garbage collection.
	Obj* objects = nullptr;

	/// Interned strings.
	Table strings;

	/// Global variables.
	Table globals;

	// A list of live upvalues.
	ObjUpvalue* openUpvalues = nullptr;

	bool loadedNativeFunctions = false;

	// Garbage collector tracking and tuning.
	int64_t bytesAllocated = 0;
	int64_t nextGC = 128;

	static inline constexpr int64_t kGCHeapGrowFactor = 2;
};

InterpretResult interpret(const std::string& source);

} // namespace cxxlox