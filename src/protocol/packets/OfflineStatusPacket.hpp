#pragma once
#include "OfflinePacket.hpp"

struct OfflineStatusPacket : public OfflinePacket
{
    uint16_t status;

    OfflineStatusPacket(uint8_t ref, uint16_t statusCode);
    virtual ~OfflineStatusPacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
