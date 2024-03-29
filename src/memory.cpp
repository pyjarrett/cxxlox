#include "memory.hpp"

#include "gc.hpp"
#include "object.hpp"
#include "value.hpp"
#include "vm.hpp"
#include <format>
#include <iostream>

namespace cxxlox {

void* realloc(void* pointer, size_t oldSize, size_t newSize)
{
	GC::instance().addUsedMemory(int64_t(newSize) - int64_t(oldSize));

	if (newSize == 0) {
		std::free(pointer);
		return nullptr;
	}

#ifdef DEBUG_STRESS_GC
	GC::instance().garbageCollect();
#else
	if (GC::instance().wantsToGarbageCollect()) {
		GC::instance().garbageCollect();
	}
#endif

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
	std::cout << "1* Marked: " << std::hex << obj << ' ' << Value::makeObj(obj);
	std::cout << '\n';
#endif
	obj->isMarked = true;
	GC::instance().grayStack.push(obj);
}

// Trace references within the given object.
void blackenObj(Obj* obj)
{
#ifdef DEBUG_LOG_GC
	std::cout << "2* Blacken: " << std::hex << (void*)obj << ' ' << objTypeToString(obj->type) << ' ';
	std::cout << Value::makeObj(obj) << '\n';
#endif

	switch (obj->type) {
		case ObjType::BoundMethod:{
			ObjBoundMethod* method = obj->to<ObjBoundMethod>();
			markValue(&method->receiver);
			markObject(asObj(method->method));
		} break;
		case ObjType::String:
			break;
		case ObjType::Native:
			break;
		case ObjType::Upvalue:
			markValue(&obj->toUpvalue()->closed);
			break;
		case ObjType::Closure: {
			ObjClosure* closure = obj->toClosure();
			markObject(asObj(closure->function));
			for (auto i = 0; i < closure->upvalues.count(); ++i) {
				markObject(asObj(closure->upvalues[i]));
			}
		} break;
		case ObjType::Class: {
			ObjClass* klass = obj->toClass();
			markObject(asObj(klass->name));
			klass->methods.mark();
		} break;
		case ObjType::Instance: {
			ObjInstance* instance = obj->toInstance();
			markObject(asObj(instance->klass));
			instance->fields.mark();
		}break;
		case ObjType::Function: {
			ObjFunction* fn = obj->toFunction();
			markObject(asObj(fn->name));
			for (auto i = 0; i < fn->chunk.constants.count(); ++i) {
				markValue(&fn->chunk.constants[i]);
			}
		} break;
		default:
			CL_FATAL("Unknown object type.");
	}
}

} // namespace cxxlox
