#pragma once

#include <string>

namespace cxxlox {

struct ObjFunction;

[[nodiscard]] ObjFunction* compile(const std::string& source);

void markActiveCompilers();

}