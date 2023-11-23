#pragma once

#include <string>

namespace cxxlox {

struct Chunk;

[[nodiscard]] bool compile(const std::string& source, Chunk* chunk);

}