#pragma once
#include "OfflinePacket.hpp"
#include "../OfflineConfig.hpp"

struct OfflineConfigPacket : public OfflinePacket
{
    OfflineConfig config;

    OfflineConfigPacket(uint8_t ref);
    virtual ~OfflineConfigPacket();
    virtual bool Read(ReadableStream& stream);
    virtual bool Write(WritableStream& stream);
};
