#include "OfflineProtocol.hpp"
#include "sbem_types.h"

OfflinePacketType parseOfflinePacketType(uint8_t b)
{
    if (b > OfflinePacketTypeUnknown && b < OfflinePacketTypeCount)
        return (OfflinePacketType)b;
    return OfflinePacketTypeUnknown;
}

bool OfflineCommandPacket::decode(const wb::Array<uint8_t>& value)
{
    if (value.size() > SIZE || value.size() < 3 || value[0] != OfflinePacketTypeCommand)
        return false;

    if (value[1] == OFFLINE_PACKET_INVALID_REF)
        return false;

    if (!(value[2] > OfflineCmdUnknown && value[2] < OfflineCmdCount))
        return false;

    mCountParameters = value.size() - 3;
    memcpy(mBuffer, value.begin(), value.size());
    return true;
}

wb::Array<uint8_t> OfflineCommandPacket::encode()
{
    uint8_t params_len = getParameters().size();
    return wb::MakeArray<uint8_t>(mBuffer, 3 + params_len);
}

bool OfflineStatusPacket::decode(const wb::Array<uint8_t>& value)
{
    if (value.size() != SIZE || value[0] != OfflinePacketTypeStatus)
        return false;

    if (value[1] == OFFLINE_PACKET_INVALID_REF)
        return false;

    memcpy(mBuffer, value.begin(), value.size());
    return true;
}

wb::Array<uint8_t> OfflineStatusPacket::encode()
{
    return wb::MakeArray<uint8_t>(mBuffer, SIZE);
}

bool OfflineConfigPacket::decode(const wb::Array<uint8_t>& value)
{
    if (value.size() != SIZE || value[0] != OfflinePacketTypeConfig)
        return false;

    if (value[1] == OFFLINE_PACKET_INVALID_REF)
        return false;

    memcpy(mBuffer, value.begin(), value.size());
    return true;
}

wb::Array<uint8_t> OfflineConfigPacket::encode()
{
    return wb::MakeArray<uint8_t>(mBuffer, SIZE);
}

WB_RES::OfflineConfig OfflineConfigPacket::getConfig()
{
    return WB_RES::OfflineConfig{
        .wakeUpBehavior = (WB_RES::OfflineWakeup::Type)mBuffer[2],
        .sampleRates = wb::MakeArray<uint16_t>(
            reinterpret_cast<const uint16_t*>(mBuffer + 3),
            WB_RES::OfflineMeasurement::COUNT),
        .sleepDelay = *reinterpret_cast<const uint16_t*>(mBuffer + 15)
    };
}

bool OfflineDataPacket::decode(const wb::Array<uint8_t>& value)
{
    if (value.size() > SIZE || value.size() < 10 || value[0] != OfflinePacketTypeData)
        return false;

    if (value[1] == OFFLINE_PACKET_INVALID_REF)
        return false;

    mCountBytes = value.size() - 10;
    memcpy(mBuffer, value.begin(), value.size());
    return true;
}

wb::Array<uint8_t> OfflineDataPacket::encode()
{
    uint32_t size = WB_MIN(SIZE - 10, getBytes().size());
    return wb::MakeArray<uint8_t>(mBuffer, 10 + size);
}

bool OfflineLogListPacket::decode(const wb::Array<uint8_t>& value)
{
    if (value.size() > SIZE || value.size() < 4 || value[0] != OfflinePacketTypeLogList)
        return false;

    if (value[1] == OFFLINE_PACKET_INVALID_REF)
        return false;

    int datalen = value.size() - 4;
    if (datalen != value[2] * 16)
        return false;

    mCountItems = datalen / 16;
    memcpy(mBuffer, value.begin(), value.size());
    return true;
}

wb::Array<uint8_t> OfflineLogListPacket::encode()
{
    uint32_t size = mCountItems * 16;
    return wb::MakeArray<uint8_t>(mBuffer, 4 + size);
}

bool OfflineLogListPacket::addItem(const WB_RES::LogEntry& entry)
{
    if (isFull())
        return false;

    uint32_t size = entry.size.hasValue() ? entry.size.getValue() : 0;
    OfflineLogItem item = {
        .id = entry.id,
        .size = size,
        .modified = entry.modificationTimestamp
    };
    pushItems(&item, 1);
    setCount(getCount() + 1);
    return true;
}

void OfflineLogListPacket::resetItems()
{
    clearItems();
    setCount(0);
}

bool OfflineLogListPacket::isFull() const
{
    return getCount() == OFFLINE_LOG_LIST_PACKET_MAX_ITEMS;
}