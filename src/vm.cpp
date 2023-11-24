#include "vm.hpp"

#include "chunk.hpp"
#include "common.hpp"
#include "compiler.hpp"

#ifdef DEBUG_TRACE_EXECUTION
	#include "debug.hpp"
#endif

#include "chunk.hpp"
#include "object.hpp"
#include <format>
#include <iostream>

namespace cxxlox {

/// Global VM, to prevent from needing to pass one to every function call.
VM vm;

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
	const uintptr_t instruction = reinterpret_cast<uintptr_t>(ip) - reinterpret_cast<uintptr_t>(&vm.chunk->code[0]) - 1;
	const int line = vm.chunk->lines[int(instruction)];

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
		obj = obj->next;
	}
}

void VM::freeObj(Obj* obj)
{
	switch (obj->type) {
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			delete str;
		}
			break;
		default:
			CL_FATAL("Unknown object type.");
	}
}

// I don't like the lambda here, but I'm just trying to do this without macros.
// I will probably regret this decision later, due to the likely overhead of the
// lambda call (if they're not inlined out... is that possible?).
template <typename Op>
[[nodiscard]] inline bool binaryOp(Op&& op)
{
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
	const auto* b = vm.pop().toObj()->toString();
	const auto* a = vm.pop().toObj()->toString();

	const auto length = a->length + b->length;
	char* chars = new char[length + 1];
	chars[length] = '\0';
	memcpy(&chars[0], &a->chars[0], a->length);
	memcpy(&chars[a->length], &b->chars[0], b->length);
	vm.push(Value::makeObj(takeString(chars, length)->asObj()));
}

static InterpretResult run()
{
	while (true) {
#ifdef DEBUG_TRACE_EXECUTION
		{
			std::cout << "        ";
			for (Value* slot = vm.stack; slot != vm.stackTop; ++slot) {
				std::cout << '[';
				printValue(*slot);
				std::cout << ']';
			}
			std::cout << '\n';
			const auto offset = int32_t(std::distance(&vm.chunk->code[0], vm.ip));
			CL_UNUSED(disassembleInstruction(*vm.chunk, offset));
		}
#endif

		const uint8_t instruction = vm.readByte();
		// Dispatch (decoding) the instruction.
		switch (instruction) {
			case OP_RETURN:
				// for now, end execution -- this will be replaced with returning
				// a value from a function.
				printValue(vm.pop());
				std::cout << '\n';
				return InterpretResult::Ok;
			case OP_ADD:
				if (isObjType(vm.peek(0), ObjType::String) && isObjType(vm.peek(1), ObjType::String)) {
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
				vm.push(Value::makeBool(isFalsey(vm.pop())));
				break;
			case OP_NEGATE:
				vm.push(Value::makeNumber(-vm.pop().toNumber()));
				break;
			case OP_CONSTANT:
				vm.push(vm.readConstant());
				break;
			case OP_NIL:
				vm.push(Value::makeNil());
				break;
			case OP_TRUE:
				vm.push(Value::makeBool(true));
				break;
			case OP_FALSE:
				vm.push(Value::makeBool(false));
				break;
			case OP_EQUAL: {
				const auto a = vm.pop();
				const auto b = vm.pop();
				vm.push(Value::makeBool(a == b));
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

InterpretResult interpret(const std::string& source)
{
	Chunk chunk;

	if (!compile(source, &chunk)) {
		return InterpretResult::CompileError;
	}

	vm.chunk = &chunk;
	vm.ip = &vm.chunk->code[0];

	return run();
}

} // namespace cxxlox