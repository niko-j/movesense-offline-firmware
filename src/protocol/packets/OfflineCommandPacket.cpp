#include "OfflineCommandPacket.hpp"

OfflineCommandPacket::OfflineCommandPacket(uint8_t ref, Command cmd)
    : OfflinePacket(OfflinePacket::TypeCommand, ref)
    , command(cmd)
{
}

OfflineCommandPacket::~OfflineCommandPacket() 
{
}

bool OfflineCommandPacket::Read(ReadableStream& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&command, sizeof(command));
    result &= stream.write_to(params);
    return result;
};

bool OfflineCommandPacket::Write(WritableStream& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&command, sizeof(command));
    result &= params.write_to(stream);
    return result;
}
