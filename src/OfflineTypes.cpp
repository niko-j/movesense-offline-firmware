#include "OfflineTypes.hpp"

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config)
{
    return WB_RES::OfflineConfig{
        .wakeUpBehavior = (WB_RES::WakeUpBehavior::Type) config.wakeUpBehavior,
        .sampleRates = wb::MakeArray(config.sampleRates),
        .sleepDelay = config.sleepDelay
    };
}

OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config)
{
    OfflineConfig internal;
    internal.wakeUpBehavior = config.wakeUpBehavior;
    for (size_t i = 0; i < WB_RES::MeasurementSensors::COUNT; i++)
    {
        internal.sampleRates[i] = i < config.sampleRates.size() ? config.sampleRates[i] : 0;
    }
    internal.sleepDelay = config.sleepDelay;
    return internal;
}
