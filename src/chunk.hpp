#pragma once

#include "array.hpp"
#include "common.hpp"

namespace cxxlox {

enum OpCode : uint8_t
{
	OP_RETURN
};

using Chunk = Array<uint8_t>;

} // namespace cxxlox