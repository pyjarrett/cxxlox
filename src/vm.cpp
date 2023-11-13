#include "vm.hpp"

#include "chunk.hpp"
#include "common.hpp"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.hpp"
#endif

#include <iostream>

namespace cxxlox {

/// Global VM, to prevent from needing to pass one to every function call.
VM vm;

uint8_t VM::readByte()
{
	return *vm.ip++;
}

Value VM::readConstant()
{
	CL_ASSERT(vm.chunk);
	return vm.chunk->constants[readByte()];
}

void initVM() {}
void freeVM() {}

static InterpretResult run() {
	while(true) {
#ifdef DEBUG_TRACE_EXECUTION
		{
			const auto offset = std::distance(&vm.chunk->code[0], vm.ip);
			CL_UNUSED(disassembleInstruction(*vm.chunk, offset));
		}
#endif

		const uint8_t instruction = vm.readByte();
		// Dispatch (decoding) the instruction.
		switch (instruction) {
			case OP_RETURN:
				// for now, end execution -- this will be replaced with returning
				// a value from a function.
				return InterpretResult::Ok;
			case OP_CONSTANT:
				// TODO: Temp code for printing.
				printValue(vm.readConstant());
				std::cout << '\n';
				break;
			default:
				// This shouldn't be reachable.
				CL_ASSERT(false);
				return InterpretResult::RuntimeError;
		}
	}
}

InterpretResult interpret(Chunk* chunk)
{
	CL_ASSERT(chunk != nullptr);
	vm.chunk = chunk;
	vm.ip = &vm.chunk->code[0];
	return run();
}

} // namespace cxxlox