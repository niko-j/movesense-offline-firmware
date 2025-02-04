#pragma once
#include <cstdint>
#include <vector>

#define WB_ALIGN(x)
#define int32 int32_t
#define uint32 uint32_t
#define int16 int16_t
#define uint16 uint16_t
#define int8 int8_t
#define uint8 uint8_t

namespace whiteboard
{
	template<typename T>
	using Array = std::vector<T>;
}
