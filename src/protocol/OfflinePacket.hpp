#pragma once
#include <wb-resources/resources.h>
#include <utils/ByteBuffer.hpp>

constexpr uint32_t OFFLINE_BLE_MTU = 161;
constexpr uint32_t OFFLINE_PACKET_SIZE = OFFLINE_BLE_MTU - 3;
constexpr uint8_t OFFLINE_PACKET_INVALID_REF = 0;

struct OfflinePacket
{
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
    virtual bool Read(ReadableStream& stream);
    virtual bool Write(WritableStream& stream);
};
