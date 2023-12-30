#include "memory.hpp"

#include "object.hpp"
#include "value.hpp"
#include "vm.hpp"

#include <format>
#include <iostream>

namespace cxxlox {

void* realloc(void* pointer, size_t oldSize, size_t newSize)
{
	if (newSize == 0) {
		std::free(pointer);
		return nullptr;
	}

	// Realloc
	void* result = std::realloc(pointer, newSize);
	if (result == nullptr) {
		// Failed allocation.
		CL_FATAL("Unable to allocate additional memory.");
	}

	return result;
}

void markValue(Value* value)
{
	CL_ASSERT(value);
	if (value && value->isObj()) {
		markObject(value->toObj());
	}
}

void markObject(Obj* obj)
{
	// Ensure obj is valid and not repeating a cycle.
	if (obj == nullptr || obj->isMarked) {
		return;
	}
#ifdef DEBUG_LOG_GC
	std::cout << std::hex << obj << ' ';
	printValue(Value::makeObj(obj));
	std::cout << '\n';
#endif
	obj->isMarked = true;
	VM::instance().grayStack.push(obj);
}

// Trace references within the given object.
void blackenObj(Obj* obj)
{
#ifdef DEBUG_LOG_GC
	std::cout << std::format("{} blacken", (void*)obj);
	printValue(Value::makeObj(obj));
	std::cout << '\n';
#endif

	switch (obj->type) {
		case ObjType::String:
			break;
		case ObjType::Native:
			break;
		case ObjType::Upvalue:
			markValue(&obj->toUpvalue()->closed);
			break;
		case ObjType::Closure: {
			ObjClosure* closure = obj->toClosure();
			markObject(closure->function->asObj());
			for (auto i = 0; i < closure->upvalues.count(); ++i) {
				markObject(closure->upvalues[i]->asObj());
			}
		} break;
		case ObjType::Function: {
			ObjFunction* fn = obj->toFunction();
			markObject(fn->name->asObj());
			for (auto i = 0; i < fn->chunk.constants.count(); ++i) {
				markValue(&fn->chunk.constants[i]);
			}
		} break;
		default:
			CL_FATAL("Unknown object type.");
	}
}

} // namespace cxxlox
