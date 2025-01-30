#include "OfflineInternal.hpp"

WB_RES::OfflineConfig internalToWb(const OfflineConfig& config)
{
    return WB_RES::OfflineConfig{
        .wakeUpBehavior = (WB_RES::OfflineWakeup::Type)config.wakeUpBehavior,
        .sampleRates = wb::MakeArray(config.sampleRates),
        .sleepDelay = config.sleepDelay,
        .options = config.optionsFlags
    };
}

OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config)
{
    OfflineConfig internal;
    internal.wakeUpBehavior = (OfflineConfig::WakeUpBehavior) config.wakeUpBehavior.getValue();
    for (size_t i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        internal.sampleRates[i] = i < config.sampleRates.size() ? config.sampleRates[i] : 0;
    }
    internal.sleepDelay = config.sleepDelay;
    internal.optionsFlags = config.options;
    return internal;
}

wb::Array<uint8_t> bufferToArray(const WritableStream& stream)
{
    return wb::MakeArray(stream.get_write_ptr(), stream.get_write_pos());
}

ByteBufferConstWrapper arrayToBuffer(const wb::Array<uint8_t>& array)
{
    return ByteBufferConstWrapper(array.begin(), array.size());
}
