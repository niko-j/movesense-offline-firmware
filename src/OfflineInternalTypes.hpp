#pragma once
#include "wb-resources/resources.h"
#include "comm_ble_gattsvc/resources.h"

// Packet Header:
//  type (1) + referecne
// Status Code Payload: 
//  status code (2)
// Data Payload Packet: 
//  offset (4) + total (4) + payload (max 120)

#define DATA_PAYLOAD_SIZE 120

enum class OfflineCommand : uint8_t
{
    UNKNOWN,
    READ_CONFIG,
    REPORT_STATUS,
    GET_SESSIONS,
    GET_SESSION_LOG,
    CLEAR_SESSION_LOGS,
    COUNT
};

class OfflineCommandRequest
{
public:
    OfflineCommandRequest(const WB_RES::Characteristic& characteristic);
    
    OfflineCommand getCommand() const;
    uint8_t getRequestRef() const;
    const uint8_t* getData() const;
    size_t getDataLen() const;

    template<typename T>
    bool tryGetValue(size_t offset, T& out) const
    {
        size_t len = getDataLen();
        if(len < offset + sizeof(T)) return false;
        auto ptr = reinterpret_cast<const T*>(getData() + offset);
        if(!ptr) return false;
        out = *ptr;
        return true;
    }

private:
    const wb::Array<uint8_t>& value;
};

enum class OfflinePacketType : uint8_t
{
    Unknown = 0,
    StatusCode,
    Data,
    Count
};

template<OfflinePacketType pktType, size_t pktSize>
class OfflinePacket
{
public:
    static const size_t HEADER_SIZE = 2;
    static const size_t MAX_PAYLOAD_SIZE = pktSize;

    void init(uint8_t requestReference)
    {
        reset();
        bytes[1] = requestReference;
    }

    void reset()
    {
        memset(bytes, 0, sizeof(bytes));
        bytes[0] = (uint8_t) pktType;
        packet_size = 0;
    }

    OfflinePacketType getType() const
    {
        return pktType;
    }

    uint8_t getRequestRef() const
    {
        return bytes[1];
    }

    const uint8_t* getData() const
    {
        return bytes;
    }

    uint8_t getSize() const
    {
        return packet_size;
    }

protected:
    uint8_t bytes[HEADER_SIZE + MAX_PAYLOAD_SIZE] = {};
    uint8_t packet_size = 0;
};

class OfflineDataPacket 
    : public OfflinePacket<OfflinePacketType::Data, 8 + DATA_PAYLOAD_SIZE>
{
public:
    // Reads data from the offset and writes it to the buffer
    // Returns count of written data
    size_t writeRawData(const uint8_t* data, uint32_t size, uint32_t offset = 0);

    // Expects data pointer to be already offset
    // Returns count of written data
    size_t writeRawDataPartial(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset);

    size_t writeSbem(wb::LocalResourceId resourceId, const wb::Value& data, uint32_t offset = 0);
};

class OfflineStatusPacket
    : public OfflinePacket<OfflinePacketType::StatusCode, 2>
{
public:
    size_t writeStatus(uint16_t status);
};
