#pragma once
#include <protocol/OfflinePacket.hpp>
#include <protocol/OfflineLogItem.hpp>

struct OfflineLogPacket : public OfflinePacket
{
    static constexpr size_t MAX_ITEMS = 6;

    uint8_t count;
    bool complete;
    OfflineLogItem items[MAX_ITEMS];

    OfflineLogPacket(uint8_t ref);
    virtual ~OfflineLogPacket();
    virtual bool Read(ReadableStream& stream);
    virtual bool Write(WritableStream& stream);
};
