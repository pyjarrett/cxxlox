#include "vm.hpp"

#include "chunk.hpp"
#include "common.hpp"
#include "compiler.hpp"

#ifdef DEBUG_TRACE_EXECUTION
	#include "debug.hpp"
#endif

#include "object.hpp"
#include <format>
#include <iostream>

namespace cxxlox {

VM* VM::s_instance = nullptr;

static void freeObj(Obj* obj)
{
	switch (obj->type) {
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			delete str;
		} break;
		default:
			CL_FATAL("Unknown object type.");
	}
}

/// Global VM, to prevent from needing to pass one to every function call.
/* static */ VM& VM::instance()
{
	// I want to be able to reset the instance for testing.  This isn't
	// threadsafe, but this is also a toy implementation.
	if (!s_instance) {
		s_instance = new VM;
	}
	return *s_instance;
}

/* static */ void VM::reset()
{
	delete s_instance;
	s_instance = new VM;
}

VM::VM()
{
	resetStack();
}

VM::~VM()
{
	freeObjects();
}

void VM::resetStack()
{
	stackTop = &stack[0];
}

void VM::runtimeError(const std::string& message)
{
	std::cerr << message << '\n';

	// ip points to the NEXT instruction to subtract an extra 1.
	const uintptr_t instruction = reinterpret_cast<uintptr_t>(ip) - reinterpret_cast<uintptr_t>(&chunk->code[0]) - 1;
	const int line = chunk->lines[int(instruction)];

	std::cerr << std::format("[line {}] in script\n", line);
}

uint8_t VM::readByte()
{
	return *ip++;
}

Value VM::readConstant()
{
	CL_ASSERT(chunk);
	return chunk->constants[readByte()];
}

void VM::push(const Value value)
{
	// Check for stack overflow.
	// https://devblogs.microsoft.com/oldnewthing/20170927-00/?p=97095
	CL_ASSERT(reinterpret_cast<uintptr_t>(stackTop) < reinterpret_cast<uintptr_t>(stack + kStackMax));

	*stackTop = value;
	++stackTop;
}

Value VM::pop()
{
	// Check for stack underflow.
	CL_ASSERT(stackTop != stack);
	--stackTop;
	return *stackTop;
}

Value VM::peek(int distance) const
{
	return stackTop[-1 - distance];
}

void VM::freeObjects()
{
	Obj* obj = objects;
	while (obj) {
		Obj* next = obj->next;
		freeObj(obj);
		obj = next;
	}
}

// I don't like the lambda here, but I'm just trying to do this without macros.
// I will probably regret this decision later, due to the likely overhead of the
// lambda call (if they're not inlined out... is that possible?).
template <typename Op>
[[nodiscard]] inline bool binaryOp(Op&& op)
{
	VM& vm = VM::instance();
	if (!vm.peek(0).isNumber() || !vm.peek(1).isNumber()) {
		// TODO: update error message
		vm.runtimeError("Operands must be numbers.");
		return false;
	}
	const auto b = vm.pop().toNumber();
	const auto a = vm.pop().toNumber();
	vm.push(Value::makeNumber(op(a, b)));
	return true;
}

[[nodiscard]] static bool isFalsey(Value v)
{
	return v.isNil() || (v.isBool() && !v.toBool());
}

static void concatenate()
{
	VM& vm = VM::instance();
	const auto* b = vm.pop().toObj()->toString();
	const auto* a = vm.pop().toObj()->toString();

	const auto length = a->length + b->length;
	char* chars = new char[length + 1];
	chars[length] = '\0';
	memcpy(&chars[0], &a->chars[0], a->length);
	memcpy(&chars[a->length], &b->chars[0], b->length);
	vm.push(Value::makeObj(takeString(chars, length)->asObj()));
}

InterpretResult VM::run()
{
	while (true) {
#ifdef DEBUG_TRACE_EXECUTION
		{
			std::cout << "        ";
			for (Value* slot = stack; slot != stackTop; ++slot) {
				std::cout << '[';
				printValue(*slot);
				std::cout << ']';
			}
			std::cout << '\n';
			const auto offset = int32_t(std::distance(&chunk->code[0], ip));
			CL_UNUSED(disassembleInstruction(*chunk, offset));
		}
#endif

		const uint8_t instruction = readByte();
		// Dispatch (decoding) the instruction.
		switch (instruction) {
			case OP_RETURN:
				// for now, end execution -- this will be replaced with returning
				// a value from a function.
				printValue(pop());
				std::cout << '\n';
				return InterpretResult::Ok;
			case OP_ADD:
				if (isObjType(peek(0), ObjType::String) && isObjType(peek(1), ObjType::String)) {
					concatenate();
				} else if (!binaryOp([](auto a, auto b) { return a + b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_SUBTRACT:
				if (!binaryOp([](auto a, auto b) { return a - b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_MULTIPLY:
				if (!binaryOp([](auto a, auto b) { return a * b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_DIVIDE:
				if (!binaryOp([](auto a, auto b) { return a / b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_NOT:
				push(Value::makeBool(isFalsey(pop())));
				break;
			case OP_NEGATE:
				push(Value::makeNumber(-pop().toNumber()));
				break;
			case OP_CONSTANT:
				push(readConstant());
				break;
			case OP_NIL:
				push(Value::makeNil());
				break;
			case OP_TRUE:
				push(Value::makeBool(true));
				break;
			case OP_FALSE:
				push(Value::makeBool(false));
				break;
			case OP_EQUAL: {
				const auto a = pop();
				const auto b = pop();
				push(Value::makeBool(a == b));
				break;
			}
			case OP_GREATER:
				if (!binaryOp([](auto a, auto b) { return a > b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_LESS:
				if (!binaryOp([](auto a, auto b) { return a < b; })) {
					return InterpretResult::RuntimeError;
				}
				break;
			default:
				// This shouldn't be reachable.
				CL_ASSERT(false);
				return InterpretResult::RuntimeError;
		}
	}
}

InterpretResult VM::interpret(const std::string& source)
{
	Chunk compiledChunk;

	if (!compile(source, &compiledChunk)) {
		return InterpretResult::CompileError;
	}

	chunk = &compiledChunk;
	ip = &chunk->code[0];

	return run();
}

void VM::track(Obj* obj)
{
	obj->next = objects;
	objects = obj;
}

void VM::intern(ObjString* obj)
{
	strings.set(obj, Value::makeNil());
}

ObjString* VM::lookup(const char* chars, int32_t length, uint32_t hash) const
{
	return strings.findKey(chars, length, hash);
}

InterpretResult interpret(const std::string& source)
{
	return VM::instance().interpret(source);
}

} // namespace cxxlox