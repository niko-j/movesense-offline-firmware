#pragma once
#include <cstdint>
#include <vector>
#include "sbem.hpp"

// ======================
// Whiteboard definitions
// ======================
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

//! ========================================
//! Copy generated data structures into here
//! and implement ISbemSerialized for them!
//! ========================================

typedef uint32 OfflineTimestamp;

template<typename T, uint8_t Q>
float fixed_point_to_float(T value)
{
	return ((double)value / (double)(1 << Q));
}

struct WB_ALIGN(1) Q16_8 : ISbemSerialized
{
	WB_ALIGN(1) uint8 b0;
	WB_ALIGN(1) uint8 b1;
	WB_ALIGN(1) int8 b2;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);

	inline float toFloat() const
	{
		int32_t fixed = b0 | (b1 << 8) | (b2 << 16);
		return fixed_point_to_float<int32_t, 8>(fixed);
	}
};

struct WB_ALIGN(2) Vec3_Q16_8 : ISbemSerialized
{
	WB_ALIGN(2) Q16_8 x;
	WB_ALIGN(2) Q16_8 y;
	WB_ALIGN(2) Q16_8 z;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(1) Q10_6 : ISbemSerialized
{
	WB_ALIGN(1) uint8 b0;
	WB_ALIGN(1) int8 b1;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);

	inline float toFloat() const
	{
		int16_t fixed = b0 | (b1 << 8);
		return fixed_point_to_float<int16_t, 6>(fixed);
	}
};

struct WB_ALIGN(2) Vec3_Q10_6 : ISbemSerialized
{
	WB_ALIGN(2) Q10_6 x;
	WB_ALIGN(2) Q10_6 y;
	WB_ALIGN(2) Q10_6 z;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(1) Q12_12 : ISbemSerialized
{
	WB_ALIGN(1) uint8 b0;
	WB_ALIGN(1) uint8 b1;
	WB_ALIGN(1) int8 b2;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);

	inline float toFloat() const
	{
		int32_t fixed = b0 | (b1 << 8) | (b2 << 16);
		return fixed_point_to_float<int32_t, 12>(fixed);
	}
};

struct WB_ALIGN(1) Vec3_Q12_12 : ISbemSerialized
{
	WB_ALIGN(1) Q12_12 x;
	WB_ALIGN(1) Q12_12 y;
	WB_ALIGN(1) Q12_12 z;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineECGData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< int16 > sampleData;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineECGCompressedData : OfflineECGData
{
	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineHRData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(1) uint8 average;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineRRData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< uint8 > intervalData;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
	std::vector<uint16_t> unpack() const;
};

struct WB_ALIGN(4) OfflineAccData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< Vec3_Q12_12 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineGyroData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< Vec3_Q12_12 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineMagnData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< Vec3_Q10_6 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineTempData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(1) int8 measurement;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineActivityData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) Q10_6 activity;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineTapData : ISbemSerialized
{
	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(1) uint8 count;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};
