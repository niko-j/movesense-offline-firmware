#include "OfflinePacket.hpp"

OfflinePacket::OfflinePacket(Type packetType, uint8_t ref)
    : type(packetType)
    , reference(ref)
{
}

OfflinePacket::~OfflinePacket()
{
}

bool OfflinePacket::Read(ReadableBuffer& stream)
{
    bool result = true;
    result &= stream.read(&type, 1);
    result &= stream.read(&reference, 1);
    return result;
};

bool OfflinePacket::Write(WritableBuffer& stream)
{
    bool result = true;
    result &= stream.write(&type, 1);
    result &= stream.write(&reference, 1);
    return result;
}
