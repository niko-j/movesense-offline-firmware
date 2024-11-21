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
	typedef uint16_t LocalDataTypeId;

	template<typename T>
	using Array = std::vector<T>;
}

//! ========================================
//! Copy generated data structures into here
//! ========================================

typedef uint32 OfflineTimestamp;

template<typename T, uint8_t Q>
float fixed_point_to_float(T value)
{
	return ((double) value / (double)(1 << Q));
}

struct WB_ALIGN(2) FixedPoint_S16_8 : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26112;

	WB_ALIGN(2) int16 integer;
	WB_ALIGN(1) uint8 fraction;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);

	inline float toFloat() const
	{
		int32_t fixed = fraction | (integer << 8);
		return fixed_point_to_float<int32_t, 8>(fixed);
	}
};

struct WB_ALIGN(4) OfflineECGData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26113;

	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< FixedPoint_S16_8 > sampleData;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineHRData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26114;

	WB_ALIGN(2) uint16 average;
	WB_ALIGN(4) whiteboard::Array< uint16 > rrValues;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(2) FixedPointVec3_S16_8 : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26115;

	WB_ALIGN(2) FixedPoint_S16_8 x;
	WB_ALIGN(2) FixedPoint_S16_8 y;
	WB_ALIGN(2) FixedPoint_S16_8 z;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineAccData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26116;

	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< FixedPointVec3_S16_8 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineGyroData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26117;

	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< FixedPointVec3_S16_8 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineMagnData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26118;

	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(4) whiteboard::Array< FixedPointVec3_S16_8 > measurements;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};

struct WB_ALIGN(4) OfflineTempData : ISbemSerialized
{
	// Structure type identification and serialization
	typedef int Structure;
	static const whiteboard::LocalDataTypeId DATA_TYPE_ID = 26119;

	WB_ALIGN(4) OfflineTimestamp timestamp;
	WB_ALIGN(1) int8 measurement;

	virtual bool readFrom(const std::vector<char>&data, size_t offset);
};
