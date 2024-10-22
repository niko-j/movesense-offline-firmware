#pragma once
#include "app-resources/resources.h"

struct OfflineConfig
{
    uint8_t wakeUpBehavior;
    uint16_t sampleRates[WB_RES::OfflineMeasurement::COUNT];
    uint16_t sleepDelay;
};

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config);
OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config);

template<typename T_out, uint8_t Q_bits, uint8_t F_bits>
inline T_out float_to_fixed_point(float value)
{
    return (T_out)(round(value * (1 << F_bits)));
}

template<typename T_in, uint8_t Q_bits, uint8_t F_bits>
inline float fixed_point_to_float(T_in value)
{
    return ((float) value / (float)(1 << F_bits));
}

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : 0))
