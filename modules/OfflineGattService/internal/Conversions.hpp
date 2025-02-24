#include "../types/OfflineConfig.hpp"
#include "../utils/Buffers.hpp"
#include <app-resources/resources.h>

namespace gatt_svc
{
#ifndef OFFLINE_GATT_CONVERSIONS_HPP
#define OFFLINE_GATT_CONVERSIONS_HPP
    WB_RES::OfflineConfig internalToWb(const OfflineConfig& config);
    OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config);

    wb::Array<uint8_t> bufferToArray(const WritableBuffer& stream);
    ReadableBuffer arrayToBuffer(const wb::Array<uint8_t>& array);
#endif // OFFLINE_GATT_CONVERSIONS_HPP

#ifdef OFFLINE_GATT_CONVERSIONS_IMPL
#undef OFFLINE_GATT_CONVERSIONS_IMPL
    WB_RES::OfflineConfig internalToWb(const OfflineConfig& config)
    {
        return WB_RES::OfflineConfig{
            .wakeUpBehavior = (WB_RES::OfflineWakeup::Type)config.wakeUpBehavior,
            .measurementParams = wb::MakeArray(config.measurementParams.array),
            .sleepDelay = config.sleepDelay,
            .options = config.optionsFlags
        };
    }

    OfflineConfig wbToInternal(const WB_RES::OfflineConfig& config)
    {
        OfflineConfig internal;
        internal.wakeUpBehavior = (OfflineConfig::WakeUpBehavior)config.wakeUpBehavior.getValue();
        for (size_t i = 0; i < OfflineConfig::MeasCount; i++)
        {
            internal.measurementParams.array[i] = i < config.measurementParams.size() ? config.measurementParams[i] : 0;
        }
        internal.sleepDelay = config.sleepDelay;
        internal.optionsFlags = config.options;
        return internal;
    }

    wb::Array<uint8_t> bufferToArray(const WritableBuffer& stream)
    {
        return wb::MakeArray(stream.get_write_ptr(), stream.get_write_pos());
    }

    ReadableBuffer arrayToBuffer(const wb::Array<uint8_t>& array)
    {
        return ReadableBuffer(array.begin(), array.size());
    }

#endif // OFFLINE_GATT_CONVERSIONS_IMPL
} // namespace offline_gatt
