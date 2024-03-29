#include "vm.hpp"

#include "chunk.hpp"
#include "common.hpp"
#include "compiler.hpp"
#include "gc.hpp"
#include "memory.hpp"
#include "object_allocator.hpp"

#ifdef DEBUG_TRACE_EXECUTION
	#include "debug.hpp"
#endif

#include "object.hpp"
#include <chrono>
#include <format>
#include <iostream>

namespace cxxlox {

using ClockType = std::chrono::high_resolution_clock;
static auto programStart = ClockType::now();

static Value clockNative(int argCount, Value* args)
{
	CL_UNUSED(argCount);
	CL_UNUSED(args);

	const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(ClockType::now() - programStart);
	return Value::makeNumber(dt.count() / 1'000.0);
}

/// Global VM, to prevent from needing to pass one to every function call.
/* static */ VM& VM::instance()
{
	static VM instance;
	return instance;
}

VM::VM()
{
	resetStack();
}

VM::~VM()
{
	initString = nullptr;
}

void VM::reset()
{
	// In-place destroy and construct.
	// Ugly, but I don't really want to expose a copy or move constructor, and
	// there's a bunch of arrays.
	this->~VM();
	GC::instance().freeObjects();
	new (this) VM();
}

void VM::resetStack()
{
	stackTop = &stack[0];
}

void VM::runtimeError(const std::string& message)
{
	std::cerr << message << '\n';

	// Print a stack trace
	for (int i = frameCount - 1; i >= 0; --i) {
		CallFrame* frame = &frames[i];
		ObjFunction* fn = frame->closure->function;
		// -1 since the previous instruction failed
		const int32_t instruction = std::distance(frame->ip, &fn->chunk.code[0]) - 1;
		std::cout << "[line " << (fn->chunk.lines[instruction]) << "] in ";
		if (frame->closure->function->name) {
			std::cerr << frame->closure->function->name->chars << '\n';
		} else {
			std::cerr << "<script>\n";
		}
	}

	resetStack();
}

void VM::defineNative(const char* name, NativeFunction fn)
{
	push(makeValue(copyString(name)));

	ObjNative* native = allocateObj<ObjNative>();
	push(makeValue(native));
	native->function = fn;

	// TODO: Why 0 and 1 here and not stackTop[-1] and stackTop [-2]?
	globals.set(stack[0].as.obj->toString(), stack[1]);
}

CallFrame* VM::currentFrame()
{
	return &frames[frameCount - 1];
}

uint8_t VM::readByte()
{
	return *currentFrame()->ip++;
}

uint16_t VM::readShort()
{
	currentFrame()->ip += 2;
	return static_cast<uint16_t>(currentFrame()->ip[-1] | (currentFrame()->ip[-2] << 8));
}

Value VM::readConstant()
{
	CL_ASSERT(currentFrame()->closure->function);
	return currentFrame()->closure->function->chunk.constants[readByte()];
}

ObjString* VM::readString()
{
	return readConstant().toObj()->toString();
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

bool VM::call(ObjClosure* closure, int argCount)
{
	if (argCount != closure->function->arity) {
		runtimeError(std::format("Expected {} arguments but got {}.", closure->function->arity, argCount));
		return false;
	}

	if (frameCount == kFramesMax) {
		runtimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &frames[frameCount++];
	frame->closure = closure;
	frame->ip = &closure->function->chunk.code[0];
	frame->slots = stackTop - argCount - 1;
	return true;
}

// Tries to call a value, such as a function.
// If the call succeeded, return true.
bool VM::callValue(Value callee, int argCount)
{
	if (callee.isObj()) {
		switch (callee.toObj()->type) {
			case ObjType::BoundMethod: {
				ObjBoundMethod* bound = callee.toObj()->to<ObjBoundMethod>();
				// Place the `this` pointer for the object being called below
				// the arguments in the stack.
				stackTop[-argCount - 1] = bound->receiver;
				return call(bound->method, argCount);
			}
			case ObjType::Class: {
				ObjClass* klass = callee.toObj()->toClass();

				// Place the instance at the top of the stack after arg count is removed.
				stackTop[-argCount - 1] = makeValue(allocateObj<ObjInstance>(klass));

				// Call class initializer on the new object.
				Value initMethod;
				if (klass->methods.get(initString, &initMethod)) {
					return call(initMethod.toObj()->to<ObjClosure>(), argCount);
				} else if (argCount != 0) {
					runtimeError(std::format("Expected 0 parameters but found {} parameters", argCount));
					return false;
				}

				return true;
			}
			case ObjType::Closure:
				return call(callee.toObj()->toClosure(), argCount);
			case ObjType::Function:
				CL_ASSERT(false);
				break;
			case ObjType::Native: {
				ObjNative* native = callee.toObj()->toNative();
				Value result = native->function(argCount, stackTop - argCount);
				stackTop -= argCount + 1; // Remove native args AND function from the stack.
				push(result);
				return true;
			}
			default:
				// uncallable function
				CL_ASSERT(false);
				break;
		}
	}

	runtimeError("Can only call closures and classes.");
	return false;
}

bool VM::invoke(cxxlox::ObjString* method, int argCount)
{
	// The object to invoke the method on will be below the argCount on the stack.
	Value receiver = peek(argCount);
	ObjInstance* instance = receiver.toObj()->to<ObjInstance>();

	// Catch when a field is actually a function being called.
	Value value;
	if (instance->fields.get(method, &value)) {
		stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}

	return invokeMethod(instance->klass, method, argCount);
}

// Deviation: invokeFromClass
bool VM::invokeMethod(cxxlox::ObjClass* klass, cxxlox::ObjString* name, int argCount)
{
	Value method;
	if (!klass->methods.get(name, &method)) {
		runtimeError(std::format("Method {} not found on {}", name->chars, klass->name->chars));
		return false;
	}

	return call(method.toObj()->to<ObjClosure>(), argCount);
}

ObjUpvalue* VM::captureUpvalue(Value* local)
{
	// The value pointed to by local might already be capture as an upvalue.
	// Look for it, so it isn't captured twice.
	ObjUpvalue* previous = nullptr;
	ObjUpvalue* current = openUpvalues;
	while (current != nullptr &&
		   reinterpret_cast<std::uintptr_t>(current->location) > reinterpret_cast<std::uintptr_t>(local)) {
		previous = current;
		current = current->next;
	}
	// Found a previously existing upvalue.
	if (current && current->location == local) {
		return current;
	}

	// Couldn't find a preexisting form of this upvalue, so create a new one.
	ObjUpvalue* createdUpvalue = allocateObj<ObjUpvalue>(local);

	// Stitch the new value into our linked list.
	createdUpvalue->next = current;
	if (previous) {
		previous->next = createdUpvalue;
	} else {
		openUpvalues = createdUpvalue;
	}

	return createdUpvalue;
}

void VM::closeUpvalues(Value* last)
{
	// Pop all open upvalues from the top of the upvalues list, to and
	// including the given value.
	while (openUpvalues && openUpvalues->location >= last) {
		ObjUpvalue* current = openUpvalues;
		current->closed = *current->location;
		current->location = &current->closed;
		openUpvalues = openUpvalues->next;
	}
}

void VM::defineMethod(cxxlox::ObjString* methodName)
{
	CL_ASSERT(methodName);
	Value method = peek(0);
	ObjClass* klass = peek(1).toObj()->toClass();
	klass->methods.set(methodName, method);

	// Remove method name.
	CL_UNUSED(pop());
}

bool VM::bindMethod(ObjClass* klass, ObjString* name)
{
	CL_ASSERT(klass);
	CL_ASSERT(name);

	Value method;
	if (!klass->methods.get(name, &method)) {
		runtimeError(std::format("Unknown method {} on class {}", name->chars, klass->name->chars));
		return false;
	}

	ObjBoundMethod* boundMethod = allocateObj<ObjBoundMethod>(peek(0), method.toObj()->to<ObjClosure>());

	// Pop instance and push the new method.
	CL_UNUSED(pop());
	push(makeValue(boundMethod));
	return true;
}

void VM::loadNativeFunctions()
{
	defineNative("clock", clockNative);
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
	vm.push(op(a, b));
	return true;
}

[[nodiscard]] static bool isFalsey(Value v)
{
	return v.isNil() || (v.isBool() && !v.toBool());
}

static void concatenate()
{
	VM& vm = VM::instance();

	// Peek to keep strings alive until GC.
	const auto* b = vm.peek(0).toObj()->toString();
	const auto* a = vm.peek(0).toObj()->toString();

	const auto length = a->length + b->length;
	char* chars = new char[length + 1];
	chars[length] = '\0';
	memcpy(&chars[0], &a->chars[0], a->length);
	memcpy(&chars[a->length], &b->chars[0], b->length);
	CL_UNUSED(vm.pop());
	CL_UNUSED(vm.pop());
	vm.push(makeValue(takeString(chars, length)));
}

InterpretResult VM::run()
{
#ifdef DEBUG_TRACE_EXECUTION
	std::cout << "== execution ==\n";
#endif
	while (true) {
#ifdef DEBUG_TRACE_EXECUTION
		{
			std::cout << "             [*]";
			for (Value* slot = stack; slot != stackTop; ++slot) {
				std::cout << " : " << (*slot);
			}
			std::cout << " [top]\n";
			Chunk& chunk = currentFrame()->closure->function->chunk;
			const auto offset = int32_t(std::distance((uint8_t*)&chunk.code[0], currentFrame()->ip));
			CL_UNUSED(disassembleInstruction(chunk, offset));
		}
#endif

		const uint8_t instruction = readByte();
		// Dispatch (decoding) the instruction.
		switch (instruction) {
			case OP_JUMP: {
				const uint16_t offset = readShort();
				currentFrame()->ip += offset;
			} break;
			case OP_JUMP_IF_FALSE: {
				const uint16_t offset = readShort();
				if (isFalsey(peek(0))) {
					currentFrame()->ip += offset;
				}
			} break;
			case OP_LOOP: {
				const uint16_t offset = readShort();
				currentFrame()->ip -= offset;
			} break;
			case OP_CALL: {
				const uint8_t numArgs = readByte();
				if (!callValue(peek(numArgs), numArgs)) {
					return InterpretResult::RuntimeError;
				}
				// Deviation: no cached `frame` to update
			} break;
			case OP_INVOKE: {
				ObjString* method = readString();
				const uint8_t argCount = readByte();
				if (!invoke(method, argCount)) {
					return InterpretResult::RuntimeError;
				}
				// Deviation: no cached `frame` to update
			} break;
			case OP_SUPER_INVOKE: {
				ObjString* method = readString();
				const uint8_t argCount = readByte();
				ObjClass* superclass = pop().toObj()->to<ObjClass>();
				if (!invokeMethod(superclass, method, argCount)) {
					return InterpretResult::RuntimeError;
				}
				// Deviation: no cached `frame` to update
			}break;
			case OP_CLOSURE: {
				ObjFunction* fn = readConstant().toObj()->toFunction();

				// Wrap the function in a closure.
				ObjClosure* closure = allocateObj<ObjClosure>(fn);
				push(makeValue(closure));
				// Read all of the upvalue (local?, index) pairs and set up the
				// environment for the closure.
				for (int32_t i = 0; i < closure->function->upvalueCount; ++i) {
					// If it's a local, just pull it from the stack.
					bool isLocal = (readByte() != 0);
					int32_t index = readByte();
					if (isLocal) {
						closure->upvalues[i] = captureUpvalue(currentFrame()->slots + index);
					} else {
						// The current frame is the surrounding frame of the closure
						// being loaded here.
						closure->upvalues[i] = currentFrame()->closure->upvalues[index];
					}
				}
			} break;
			case OP_CLOSE_UPVALUE:
				closeUpvalues(stackTop - 1);
				CL_UNUSED(pop());
				break;
			case OP_RETURN: {
				Value result = pop();
				// Deviation: no need to update cached `frame` since hidden by function,
				// but need to capture it here to prevent using the parent frame after
				// frame count change.
				CallFrame* lastFrame = currentFrame();
				closeUpvalues(lastFrame->slots);
				--frameCount;
				if (frameCount == 0) {
					CL_UNUSED(pop());
					return InterpretResult::Ok;
				}
				// Remove the last stack frame.
				stackTop = lastFrame->slots;
				push(result);
			} break;
			case OP_CLASS: {
				push(makeValue(allocateObj<ObjClass>(readString())));
			} break;
			case OP_METHOD: {
				defineMethod(readString());
			} break;
			case OP_INHERIT: {
				// Copy-down inheritance.
				// Copy all methods down from the superclass to the subclass.
				Value superclass = peek(1);

				if (!superclass.toObj()->is<ObjClass>()) {
					runtimeError("Can only inherit from a class.");
					return InterpretResult::RuntimeError;
				}

				ObjClass* parent = superclass.toObj()->to<ObjClass>();
				ObjClass* child = peek(0).toObj()->to<ObjClass>();

				child->methods.addAll(superclass.toObj()->to<ObjClass>()->methods);
				CL_UNUSED(pop()); // superclass
			} break;
			case OP_GET_SUPER: {
				ObjString* methodName = readString();
				ObjClass* superclass = pop().toObj()->to<ObjClass>();

				// Instead of using the instance's class like normal, bind the
				// method to the superclass.
				if (!bindMethod(superclass, methodName)) {
					return InterpretResult::RuntimeError;
				}
			} break;
			case OP_ADD:
				if (isObjType(peek(0), ObjType::String) && isObjType(peek(1), ObjType::String)) {
					concatenate();
				} else if (!binaryOp([](auto a, auto b) { return Value::makeNumber(a + b); })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_SUBTRACT:
				if (!binaryOp([](auto a, auto b) { return Value::makeNumber(a - b); })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_MULTIPLY:
				if (!binaryOp([](auto a, auto b) { return Value::makeNumber(a * b); })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_DIVIDE:
				if (!binaryOp([](auto a, auto b) { return Value::makeNumber(a / b); })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_NOT:
				push(Value::makeBool(isFalsey(pop())));
				break;
			case OP_NEGATE:
				push(Value::makeNumber(-pop().toNumber()));
				break;
			case OP_PRINT:
				std::cout << pop() << '\n';
				break;
			case OP_POP:
				CL_UNUSED(pop());
				break;
			case OP_GET_LOCAL: {
				const uint8_t slot = readByte();
				push(currentFrame()->slots[slot]);
			} break;
			case OP_GET_GLOBAL: {
				ObjString* name = readString();
				Value value {};
				if (!globals.get(name, &value)) {
					runtimeError(std::format("Unknown variable '{}'.", name->chars));
					return InterpretResult::RuntimeError;
				}
				push(value);
			} break;
			case OP_DEFINE_GLOBAL: {
				ObjString* name = readString();
				globals.set(name, peek(0));
				CL_UNUSED(pop());
			} break;
			case OP_GET_UPVALUE: {
				const int32_t slot = readByte();
				push(*currentFrame()->closure->upvalues[slot]->location);
			} break;
			case OP_SET_UPVALUE: {
				const int32_t slot = readByte();
				*currentFrame()->closure->upvalues[slot]->location = peek(0);
			} break;
			case OP_SET_LOCAL: {
				const uint8_t slot = readByte();
				// Assignment is an expression, so leave the assigned value on
				// the stack.
				currentFrame()->slots[slot] = peek(0);
			} break;
			case OP_SET_GLOBAL: {
				ObjString* name = readString();
				if (globals.set(name, peek(0))) {
					// If a new variable is created by trying to set this, remove the
					// new variable, since this means the global wasn't declared
					// previously.
					globals.remove(name);
					runtimeError(std::format("Unknown variable '{}'.", name->chars));
					return InterpretResult::RuntimeError;
				}
			} break;
			case OP_SET_PROPERTY: {
				// Stack: <object> <expression_value>
				//        1        0 (top)
				// Instr: OP_SET_PROPERTY <property_name>
				if (peek(1).toObj()->toInstance()->type != ObjType::Instance) {
					runtimeError("Object instances have fields.");
					return InterpretResult::RuntimeError;
				}

				ObjInstance* instance = peek(1).toObj()->toInstance();
				ObjString* property = readString();
				Value value = pop();
				instance->fields.set(property, value);
				CL_UNUSED(pop());
				push(value);
			} break;
			case OP_GET_PROPERTY: {
				if (peek(0).toObj()->type != ObjType::Instance) {
					runtimeError("Object instances have properties.");
					return InterpretResult::RuntimeError;
				}

				// Property name is in the instruction.
				ObjString* property = readString();
				Value value;

				// and the object is on the stack.
				ObjInstance* instance = peek(0).toObj()->toInstance();
				if (instance->fields.get(property, &value)) {
					CL_UNUSED(pop()); // Remove the instance.
					push(value);
					break;
				}

				if (!bindMethod(instance->klass, property)) {
					runtimeError(std::format("Unable to bind method: {} to class {}",
						property->chars,
						instance->klass->name->chars));
					return InterpretResult::RuntimeError;
				}
			} break;
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
				if (!binaryOp([](auto a, auto b) { return Value::makeBool(a > b); })) {
					return InterpretResult::RuntimeError;
				}
				break;
			case OP_LESS:
				if (!binaryOp([](auto a, auto b) { return Value::makeBool(a < b); })) {
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
#ifdef CL_DEBUG
	// Eagerly flush every print.
	std::cout.setf(std::ios::unitbuf);
#endif

	// Deviation: Cannot create Obj-like objects in c'tor since allocateObj needs
	// to ask the VM to track the object.
	if (!loadedNativeFunctions) {
		loadNativeFunctions();
		loadedNativeFunctions = true;

		initString = copyString("init", 4);
	}

	ObjFunction* function = compile(source);
	if (!function) {
		return InterpretResult::CompileError;
	}

	// Deviation: Must push compiler function here since the handling of the
	// compiler stack is slightly different.  Otherwise, this function might
	// get reaped if GC happens when the closure is allocated.
	VM::instance().push(Value::makeObj((Obj*)function));
	ObjClosure* closure = allocateObj<ObjClosure>(function);
	CL_UNUSED(pop());
	push(makeValue(closure));
	CL_UNUSED(call(closure, 0));

	return run();
}

void VM::markRoots()
{
	// Everything in the stack.
	for (Value* value = &stack[0]; value < stackTop; value++) {
		markValue(value);
	}

	// Mark the closures stored in the stack frames.
	for (int i = 0; i < this->frameCount; ++i) {
		markObject(asObj(frames[i].closure));
	}

	// Upvalues need to be tracked too.
	for (ObjUpvalue* upvalue = openUpvalues; upvalue; upvalue = upvalue->next) {
		markObject(asObj(upvalue));
	}

	globals.mark();
	markActiveCompilers();
	markObject(asObj(initString));
}

InterpretResult interpret(const std::string& source)
{
	return VM::instance().interpret(source);
}

} // namespace cxxlox