#include "OfflineLogPacket.hpp"

OfflineLogPacket::OfflineLogPacket(uint8_t ref)
    : OfflinePacket(OfflinePacket::TypeLogList, ref)
    , count(0)
    , complete(false)
{
}

OfflineLogPacket::~OfflineLogPacket()
{
}

bool OfflineLogPacket::Read(ReadableStream& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&count, sizeof(count));
    result &= stream.read(&complete, sizeof(complete));

    for (uint8_t i = 0; i < count; i++)
    {
        result &= stream.read(&items[i].id, sizeof(items[i].id));
        result &= stream.read(&items[i].modified, sizeof(items[i].modified));
        result &= stream.read(&items[i].size, sizeof(items[i].size));
    }
    return result;
};

bool OfflineLogPacket::Write(WritableStream& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&count, sizeof(count));
    result &= stream.write(&complete, sizeof(complete));

    for (uint8_t i = 0; i < count; i++)
    {
        result &= stream.write(&items[i].id, sizeof(items[i].id));
        result &= stream.write(&items[i].modified, sizeof(items[i].modified));
        result &= stream.write(&items[i].size, sizeof(items[i].size));
    }
    return result;
}
