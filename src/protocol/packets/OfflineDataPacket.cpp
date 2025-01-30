#include "OfflineDataPacket.hpp"

OfflineDataPacket::OfflineDataPacket(uint8_t ref)
    : OfflinePacket(OfflinePacket::TypeData, ref)
    , offset(0)
    , totalBytes(0)
    , data(nullptr, 0)
{
}

OfflineDataPacket::~OfflineDataPacket()
{
}

bool OfflineDataPacket::Read(ReadableStream& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&offset, sizeof(offset));
    result &= stream.read(&totalBytes, sizeof(totalBytes));
    return result;
};

bool OfflineDataPacket::Write(WritableStream& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&offset, sizeof(offset));
    result &= stream.write(&totalBytes, sizeof(totalBytes));
    result &= data.write_to(stream);
    return result;
}
