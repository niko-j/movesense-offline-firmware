#pragma once
#include "app-resources/resources.h"

struct OfflineConfig
{
    uint8_t wakeUpBehavior = WB_RES::OfflineWakeup::DOUBLETAP;
    uint16_t sleepDelay = 0;
    uint8_t optionsFlags = 0;
    uint16_t sampleRates[WB_RES::OfflineMeasurement::COUNT] = {};
};

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config);
OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config);

