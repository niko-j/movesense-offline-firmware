#pragma once
#include "app-resources/resources.h"

struct OfflineConfig
{
    uint8_t wakeUpBehavior = WB_RES::OfflineWakeup::DOUBLETAP;
    uint16_t sampleRates[WB_RES::OfflineMeasurement::COUNT] = {};
    uint16_t sleepDelay = 0;
};

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config);
OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config);

template<typename T, uint8_t F_bits>
inline T float_to_fixed_point(float value)
{
    return static_cast<T>(round(value * (1 << F_bits)));
}

template<typename T, uint8_t F_bits>
inline float fixed_point_to_float(T value)
{
    return static_cast<float>(value) / (1 << F_bits);
}

inline WB_RES::FixedPoint_S16_8 float_to_fixed_point_S16_8(float value)
{
    int32_t fixed = float_to_fixed_point<int32_t, 8>(value);
    WB_RES::FixedPoint_S16_8 out = {};
    out.integer = (fixed >> 8) & 0xFFFF;
    out.fraction = (fixed & 0xFF);
    return out;
}

inline float fixed_point_S16_8_to_float(WB_RES::FixedPoint_S16_8 value)
{
    int32_t fixed = value.fraction | (value.integer << 8);
    return fixed_point_to_float<int32_t, 8>(fixed);
}

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))
