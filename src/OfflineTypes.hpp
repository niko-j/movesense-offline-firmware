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
