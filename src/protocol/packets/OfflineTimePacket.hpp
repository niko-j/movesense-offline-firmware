#pragma once
#include "OfflinePacket.hpp"

struct OfflineTimePacket : public OfflinePacket
{
    int64_t time;

    OfflineTimePacket(uint8_t ref, int64_t t = 0);
    virtual ~OfflineTimePacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
