#include "OfflineCommandPacket.hpp"

OfflineCommandPacket::OfflineCommandPacket(uint8_t ref, Command cmd)
    : OfflinePacket(OfflinePacket::TypeCommand, ref)
    , command(cmd)
{
}

OfflineCommandPacket::~OfflineCommandPacket() 
{
}

bool OfflineCommandPacket::Read(ReadableBuffer& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&command, sizeof(command));
    result &= stream.write_to(params);
    return result;
};

bool OfflineCommandPacket::Write(WritableBuffer& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&command, sizeof(command));

    if(params.get_write_pos() > 0)
        result &= params.write_to(stream, params.get_write_pos());

    return result;
}
