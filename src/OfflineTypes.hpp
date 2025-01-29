#pragma once
#include "app-resources/resources.h"

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

struct OfflineConfig
{
    uint8_t wakeUpBehavior = WB_RES::OfflineWakeup::DOUBLETAP;
    uint16_t sampleRates[WB_RES::OfflineMeasurement::COUNT] = {};
    uint16_t sleepDelay = 0;
    uint8_t optionsFlags = 0;
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

inline WB_RES::Q16_8 float_to_fixed_point_Q16_8(float value)
{
    int32_t fixed = float_to_fixed_point<int32_t, 8>(value);
    WB_RES::Q16_8 out = {};
    out.b0 = (fixed & 0xFF);
    out.b1 = ((fixed >> 8) & 0xFF);
    out.b2 = ((fixed >> 16) & 0xFF);
    return out;
}

inline float fixed_point_Q16_8_to_float(WB_RES::Q16_8 value)
{
    int32_t fixed = value.b0 | (value.b1 << 8) | (value.b2 << 16);
    return fixed_point_to_float<int32_t, 8>(fixed);
}

inline WB_RES::Q12_12 float_to_fixed_point_Q12_12(float value)
{
    int32_t fixed = float_to_fixed_point<int32_t, 12>(value);
    WB_RES::Q12_12 out = {};
    out.b0 = (fixed & 0xFF);
    out.b1 = ((fixed >> 8) & 0xFF);
    out.b2 = ((fixed >> 16) & 0xFF);
    return out;
}

inline float fixed_point_Q12_12_to_float(WB_RES::Q12_12 value)
{
    int32_t fixed = value.b0 | (value.b1 << 8) | (value.b2 << 16);
    return fixed_point_to_float<int32_t, 12>(fixed);
}

inline WB_RES::Q10_6 float_to_fixed_point_Q10_6(float value)
{
    int16_t fixed = float_to_fixed_point<int16_t, 6>(value);
    WB_RES::Q10_6 out = {};
    out.b0 = (fixed & 0xFF);
    out.b1 = ((fixed >> 8) & 0xFF);
    return out;
}

inline float fixed_point_Q10_6_to_float(WB_RES::Q10_6 value)
{
    int16_t fixed = value.b0 | (value.b1 << 8);
    return fixed_point_to_float<int16_t, 6>(fixed);
}

