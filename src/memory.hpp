#pragma once

#include "common.hpp"

namespace cxxlox {

struct Obj;
struct Value;

// There is limited usage of std::string which means I don't really want to do
// a full global replacement of "new"/"delete", since that means the garbage
// collector would be affected by these calls as well.
//
// This is a single point of entry for all allocation, and handles news as well
// as deletes.
[[nodiscard]] void* realloc(void* pointer, size_t oldSize, size_t newSize);

void markValue(Value* value);
void markObject(Obj* obj);

void blackenObj(Obj* obj);

} // namespace cxxlox
