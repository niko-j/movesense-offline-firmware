#pragma once
#include "app-resources/resources.h"

template<typename WhiteboardType>
struct OfflineInternalType
{
    virtual WhiteboardType convert() = 0;
    virtual void assign(const WhiteboardType& value) = 0;
};

struct OfflineConfig : OfflineInternalType<WB_RES::OfflineConfig>
{
    uint16_t sampleRates[WB_RES::MeasurementSensors::COUNT];
    uint8_t wakeUpBehavior;
    uint16_t sleepDelay;

    WB_RES::OfflineConfig convert();
    void assign(const WB_RES::OfflineConfig& value);
};
