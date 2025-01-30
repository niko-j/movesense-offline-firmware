#include "OfflineConfigPacket.hpp"

OfflineConfigPacket::OfflineConfigPacket(uint8_t ref)
    : OfflinePacket(OfflinePacket::TypeConfig, ref)
    , config({})
{
}
OfflineConfigPacket::~OfflineConfigPacket()
{
}

bool OfflineConfigPacket::Read(ReadableStream& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&config.wakeUpBehavior, 1);
    result &= stream.read(&config.sleepDelay, 2);
    result &= stream.read(&config.optionsFlags, 1);
    result &= stream.read(&config.sampleRates, WB_RES::OfflineMeasurement::COUNT * 2);
    return result;
};

bool OfflineConfigPacket::Write(WritableStream& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&config.wakeUpBehavior, 1);
    result &= stream.write(&config.sleepDelay, 2);
    result &= stream.write(&config.optionsFlags, 1);
    result &= stream.write(&config.sampleRates, WB_RES::OfflineMeasurement::COUNT * 2);
    return result;
}
