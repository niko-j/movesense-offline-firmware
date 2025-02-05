#include "OfflineTimePacket.hpp"

OfflineTimePacket::OfflineTimePacket(uint8_t ref, int64_t t)
    : OfflinePacket(OfflinePacket::TypeTime, ref)
    , time(t)
{
}

OfflineTimePacket::~OfflineTimePacket() 
{
}

bool OfflineTimePacket::Read(ReadableBuffer& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&time, sizeof(time));
    return result;
};

bool OfflineTimePacket::Write(WritableBuffer& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&time, sizeof(time));
    return result;
}
