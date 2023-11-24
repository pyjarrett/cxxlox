#include "object.hpp"

#include <iostream>

namespace cxxlox {

ObjString::~ObjString()
{
	delete [] chars;
}

void printObj(Obj* obj)
{
	switch (obj->type) {
		case ObjType::String: {
			ObjString* str = reinterpret_cast<ObjString*>(obj);
			std::cout << str->chars;
		}
	}
}

bool isObjType(Value value, ObjType type)
{
	return value.isObj() && value.toObj()->type == type;
}

ObjString* copyString(const char* chars, int length)
{
	// Also allocate space for a null terminator.
	char* copiedChars = new char[length + 1];
	memcpy(copiedChars, chars, length);
	copiedChars[length] = '\0';
	return allocateString(copiedChars, length);
}

ObjString* takeString(char* chars, int length)
{
	return allocateString(chars, length);
}

ObjString* allocateString(char* chars, int length)
{
	ObjString* str = allocateObj<ObjString>(ObjType::String);
	str->chars = chars;
	str->length = length;
	return str;
}

} // namespace cxxlox
