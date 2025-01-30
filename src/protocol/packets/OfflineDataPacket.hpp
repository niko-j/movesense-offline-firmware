#pragma once
#include "OfflinePacket.hpp"
#include "buffer/Stream.hpp"

/// @brief Data packet header, read/write actual data after calling Read/Write
struct OfflineDataPacket : public OfflinePacket
{
    static constexpr size_t MAX_PAYLOAD = MAX_PACKET_SIZE - 8;

    uint32_t offset;
    uint32_t totalBytes;
    ReadableStream data;
    
    OfflineDataPacket(uint8_t ref);
    virtual ~OfflineDataPacket();
    virtual bool Read(ReadableStream& stream);
    virtual bool Write(WritableStream& stream);
};
