#pragma once
#include "OfflinePacket.hpp"
#include "../OfflineConfig.hpp"

struct OfflineConfigPacket : public OfflinePacket
{
    OfflineConfig config;

    OfflineConfigPacket(uint8_t ref, OfflineConfig conf = {});
    virtual ~OfflineConfigPacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
