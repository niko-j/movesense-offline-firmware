#include "OfflineStatusPacket.hpp"

OfflineStatusPacket::OfflineStatusPacket(uint8_t ref, uint16_t statusCode)
    : OfflinePacket(OfflinePacket::TypeStatus, ref)
    , status(statusCode)
{
}

OfflineStatusPacket::~OfflineStatusPacket() 
{
}

bool OfflineStatusPacket::Read(ReadableStream& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&status, sizeof(status));
    return result;
};

bool OfflineStatusPacket::Write(WritableStream& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&status, sizeof(status));
    return result;
}
