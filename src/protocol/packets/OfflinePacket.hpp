#pragma once
#include "../OfflineTypes.hpp"

#ifndef OFFLINE_BLE_MTU
#define OFFLINE_BLE_MTU 161
#endif

struct OfflinePacket
{
    static constexpr uint8_t INVALID_REF = 0;
    static constexpr uint32_t MAX_PACKET_SIZE = OFFLINE_BLE_MTU - 3;

    enum Type : uint8_t
    {
        TypeUnknown = 0x00,
        TypeCommand = 0x01,
        TypeStatus = 0x02,
        TypeData = 0x03,
        TypeConfig = 0x04,
        TypeLogList = 0x05
    } type;
    uint8_t reference;

    OfflinePacket(Type packetType, uint8_t ref);
    virtual ~OfflinePacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
