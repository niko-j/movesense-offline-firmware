#include "OfflineInternalTypes.hpp"
#include "common/core/dbgassert.h"
#include "sbem_types.h"

OfflineCommandRequest::OfflineCommandRequest(const WB_RES::Characteristic& characteristic)
    : value(characteristic.bytes)
{}

OfflineCommand OfflineCommandRequest::getCommand() const
{
    uint8_t cmd = value.size() > 0 ? value[0] : 0;
    if(cmd < (uint8_t) OfflineCommand::COUNT)
        return (OfflineCommand) cmd;
    return OfflineCommand::UNKNOWN;
}

uint8_t OfflineCommandRequest::getRequestRef() const
{
    return value.size() > 1 ? value[1] : 0;
}

const uint8_t* OfflineCommandRequest::getData() const
{
    return value.size() > 2 ? (&value[2]) : nullptr;
}

size_t OfflineCommandRequest::getDataLen() const
{
    size_t len = value.size();
    return len > 2 ? (len - 2) : 0;
}

size_t OfflineDataPacket::writeRawData(const uint8_t* data, uint32_t size, uint32_t offset)
{
    ASSERT(offset < size);

    uint32_t left = size - offset;
    uint32_t payload_len = WB_MIN(MAX_PAYLOAD_SIZE - 8, left);

    memcpy(&bytes[HEADER_SIZE], &offset, sizeof(offset));
    memcpy(&bytes[HEADER_SIZE+4], &size, sizeof(size));
    memcpy(&bytes[HEADER_SIZE+8], data + offset, payload_len);

    packet_size = HEADER_SIZE + 8 + payload_len;
    return payload_len;
}

size_t OfflineDataPacket::writeRawDataPartial(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset)
{
    uint32_t payload_len = WB_MIN(MAX_PAYLOAD_SIZE - 8, partSize);

    memcpy(&bytes[HEADER_SIZE], &offset, 4);
    memcpy(&bytes[HEADER_SIZE+4], &totalSize, 4);
    memcpy(&bytes[HEADER_SIZE+8], data, payload_len);

    packet_size = HEADER_SIZE + 8 + payload_len;
    return payload_len;
}

size_t OfflineDataPacket::writeSbem(wb::LocalResourceId resourceId, const wb::Value& data, uint32_t offset)
{
    size_t sbemSize = getSbemLength(resourceId, data);

    memcpy(&bytes[HEADER_SIZE], &offset, 4);
    memcpy(&bytes[HEADER_SIZE+4], &sbemSize, 4);

    size_t written = writeToSbemBuffer(
        &bytes[HEADER_SIZE+8], MAX_PAYLOAD_SIZE - 8, offset,
        resourceId, data);

    packet_size = HEADER_SIZE + 8 + written;
    return written;
}

size_t OfflineStatusPacket::writeStatus(uint16_t status)
{
    memcpy(&bytes[HEADER_SIZE], &status, sizeof(status));
    packet_size = HEADER_SIZE + sizeof(status);
    return sizeof(status);
}
