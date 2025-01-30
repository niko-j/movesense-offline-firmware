#include "OfflineConfigPacket.hpp"

OfflineConfigPacket::OfflineConfigPacket(uint8_t ref)
    : OfflinePacket(OfflinePacket::TypeConfig, ref)
    , config({})
{
}
OfflineConfigPacket::~OfflineConfigPacket()
{
}

bool OfflineConfigPacket::Read(ReadableBuffer& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&config.wakeUpBehavior, 1);
    result &= stream.read(&config.sleepDelay, 2);
    result &= stream.read(&config.optionsFlags, 1);
    result &= stream.read(&config.sampleRates, OfflineConfig::MeasCount * 2);
    return result;
};

bool OfflineConfigPacket::Write(WritableBuffer& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&config.wakeUpBehavior, 1);
    result &= stream.write(&config.sleepDelay, 2);
    result &= stream.write(&config.optionsFlags, 1);
    result &= stream.write(&config.sampleRates, OfflineConfig::MeasCount * 2);
    return result;
}
