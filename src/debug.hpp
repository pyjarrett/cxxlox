#pragma once

#include "chunk.hpp"
#include "common.hpp"

namespace cxxlox {

void disassembleChunk(const Chunk& chunk, const char* name);
[[nodiscard]] int32_t disassembleInstruction(const Chunk& chunk, int32_t offset);

}