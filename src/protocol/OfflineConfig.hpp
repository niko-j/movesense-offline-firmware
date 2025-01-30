#pragma once
#include "OfflineTypes.hpp"

struct OfflineConfig
{
    enum WakeUpBehavior : uint8_t
    {
        WakeUpAlwaysOn = 0U,
		WakeUpConnector = 1U,
		WakeUpMovement = 2U,
		WakeUpSingleTap = 3U,
		WakeUpDoubleTap = 4U
    };
    
    enum Measurement
    {
        MeasECG = 0U,
		MeasHR = 1U,
		MeasRR = 2U,
		MeasAcc = 3U,
		MeasGyro = 4U,
		MeasMagn = 5U,
		MeasTemp = 6U,
		MeasActivity = 7U,
		MeasTapDetect = 8U,
		MeasCount = 9U
    };

    enum class OptionsFlags : uint8_t
    {
        COMPRESS_ECG = (1 << 0)
    };

    uint16_t sleepDelay = 0;
    uint8_t optionsFlags = 0;
    WakeUpBehavior wakeUpBehavior = WakeUpConnector;
    uint16_t sampleRates[MeasCount] = {};
};
