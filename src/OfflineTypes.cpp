#include "OfflineTypes.hpp"

WB_RES::OfflineConfig OfflineConfig::convert()
{
    return WB_RES::OfflineConfig {
        .wakeUpBehavior = (WB_RES::WakeUpBehavior::Type) wakeUpBehavior,
        .sampleRates = wb::MakeArray(sampleRates),
        .sleepDelay = sleepDelay
    };
}

void OfflineConfig::assign(const WB_RES::OfflineConfig& value)
{
    wakeUpBehavior = value.wakeUpBehavior;
    for(size_t i = 0; i < WB_RES::MeasurementSensors::COUNT; i++)
    {
        sampleRates[i] = i < value.sampleRates.size() ? value.sampleRates[i] : 0;
    }
    sleepDelay = value.sleepDelay;
}
