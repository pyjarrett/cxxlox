#include "gc.hpp"

#include "object.hpp"
#include "vm.hpp"

namespace cxxlox {

template <typename T>
static void freeObj(Obj* obj)
{
	T* t = reinterpret_cast<T*>(obj);
	t->~T();
	CL_UNUSED(realloc((void*)obj, sizeof(T), 0));
}

static void freeObj(Obj* obj)
{
#ifdef DEBUG_LOG_GC
	std::cout << "Freeing " << std::hex << obj << " of type " << objTypeToString(obj->type) << '\n';
	std::cout << "    -> " << obj << '\n';
#endif

	switch (obj->type) {
		case ObjType::BoundMethod: {
			freeObj<ObjBoundMethod>(obj);
		} break;
		case ObjType::String: {
			freeObj<ObjString>(obj);
		} break;
		case ObjType::Closure: {
			freeObj<ObjClosure>(obj);
		} break;
		case ObjType::Class: {
			freeObj<ObjClass>(obj);
		} break;
		case ObjType::Instance:
			freeObj<ObjInstance>(obj);
			break;
		case ObjType::Function: {
			freeObj<ObjFunction>(obj);
		} break;
		case ObjType::Native: {
			freeObj<ObjNative>(obj);
		} break;
		case ObjType::Upvalue: {
			freeObj<ObjUpvalue>(obj);
		} break;
		default:
			CL_FATAL("Unknown object type.");
	}
}

GC::~GC()
{
	freeObjects();
}

GC& GC::instance()
{
	static GC s_instance;
	return s_instance;
}

void GC::track(Obj* obj)
{
	obj->next = objects;
	objects = obj;
}

void GC::printTracked()
{
#ifdef DEBUG_LOG_GC
	std::cout << "Tracked Objects\n";
	Obj* obj = objects;
	while (obj) {
		std::cout << " @ " << std::hex << ((void*)obj) << " OBJ: " << obj << '\n';
		obj = obj->next;
	}
#endif
}

void GC::freeObjects()
{
	printTracked();
	Obj* obj = objects;
	while (obj) {
		Obj* next = obj->next;
		freeObj(obj);
		obj = next;
	}
	objects = nullptr;
	grayStack.clear();
}

void GC::garbageCollect()
{
	printTracked();
#ifdef DEBUG_LOG_GC
	std::cout << "-- gc start\n";
	int64_t bytesBefore = int64_t(bytesAllocated);
#endif

	markRoots();
	traceReferences();
//	strings.removeUnmarked();
	sweep();

	nextGC = bytesAllocated * kGCHeapGrowFactor;

#ifdef DEBUG_LOG_GC
	std::cout << "-- gc end\n";
	std::cout << "Collected " << (bytesBefore - bytesAllocated) << " bytes " << bytesAllocated
			  << " remain, next collect is at " << nextGC << '\n';
#endif
	printTracked();
}

bool GC::wantsToGarbageCollect() const
{
	return bytesAllocated > nextGC;
}

void GC::addUsedMemory(int64_t bytes)
{
	bytesAllocated += bytes;
}

void GC::markRoots()
{
	VM::instance().markRoots();
}

// Expand outward from the roots to trace and find all referenced objects.
void GC::traceReferences()
{
	while (grayStack.count() > 0) {
		Obj* obj = grayStack.pop();
		blackenObj(obj);
	}
}

// Garbage collection pass.
void GC::sweep()
{
	Obj* previous = nullptr;
	Obj* current = objects;
	while (current != nullptr) {
		if (current->isMarked) {
			// Reset status and move along.
			current->isMarked = false;
			previous = current;
			current = current->next;
		} else {
			// Wasn't referenced (white node), so remove.
			Obj* garbage = current;
			current = current->next;

			if (previous) {
				// Interior or tail node.
				previous->next = current;
			} else {
				// Reattach list head.
				objects = current;
			}
			freeObj(garbage);
		}
	}
}

} // namespace cxxlox