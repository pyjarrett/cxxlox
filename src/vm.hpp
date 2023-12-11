#pragma once

#include "common.hpp"
#include "object.hpp"
#include "table.hpp"
#include "value.hpp"
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
	ObjFunction* function;

	// The **next** instruction to be executed.
	// "Instruction pointer" ("program counter").
	uint8_t* ip;

	// Points to the base argument of the called function.  This will be a slice
	// of the VM value stack.
	Value* slots;
};
static_assert(sizeof(CallFrame) == 24);

struct VM {
	static constexpr int32_t kFramesMax = 64;
	static constexpr int32_t kStackMax = kFramesMax * kUInt8Count;

	static VM& instance();
	static void reset();

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

	[[nodiscard]] bool call(ObjFunction* fn, int argCount);
	[[nodiscard]] bool callValue(Value callee, int argCount);

	void runtimeError(const std::string& message);

	void defineNative(const char* name, NativeFunction fn);

	[[nodiscard]] InterpretResult interpret(const std::string& source);

	void track(Obj* obj);
	void intern(ObjString* str);
	ObjString* lookup(const char* chars, uint32_t length, uint32_t hash) const;

private:
	VM();
	~VM();

	[[nodiscard]] InterpretResult run();

	void resetStack();
	void freeObjects();

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

	static VM* s_instance;
};

InterpretResult interpret(const std::string& source);

} // namespace cxxlox