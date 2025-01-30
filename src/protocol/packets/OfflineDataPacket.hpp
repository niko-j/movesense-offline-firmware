#pragma once
#include "OfflinePacket.hpp"

/// @brief Data packet header, read/write actual data after calling Read/Write
struct OfflineDataPacket : public OfflinePacket
{
    static constexpr size_t MAX_PAYLOAD = MAX_PACKET_SIZE - 8;

    uint32_t offset;
    uint32_t totalBytes;
    ReadableBuffer data;
    
    OfflineDataPacket(uint8_t ref);
    virtual ~OfflineDataPacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
