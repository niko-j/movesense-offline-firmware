#include "OfflineCommandPacket.hpp"

OfflineCommandPacket::OfflineCommandPacket(uint8_t ref, Command cmd, CommandParams args)
    : OfflinePacket(OfflinePacket::TypeCommand, ref)
    , command(cmd)
    , params(args)
{
}

OfflineCommandPacket::~OfflineCommandPacket()
{
}

bool OfflineCommandPacket::Read(ReadableBuffer& stream)
{
    bool result = OfflinePacket::Read(stream);
    result &= stream.read(&command, sizeof(command));

    switch (command)
    {
    case CmdReadLog:
    {
        result &= stream.read(
            &params.ReadLogParams.logIndex,
            sizeof(params.ReadLogParams.logIndex));
        break;
    }
    default: break;
    }

    return result;
};

bool OfflineCommandPacket::Write(WritableBuffer& stream)
{
    bool result = OfflinePacket::Write(stream);
    result &= stream.write(&command, sizeof(command));

    switch (command)
    {
    case CmdReadLog:
    {
        result &= stream.write(
            &params.ReadLogParams.logIndex,
            sizeof(params.ReadLogParams.logIndex));
        break;
    }
    default: break;
    }

    return result;
}
