#pragma once
#include <protocol/OfflinePacket.hpp>

struct OfflineCommandPacket : public OfflinePacket
{
    static constexpr size_t MAX_PARAM_DATA = 32;

    enum Command : uint8_t
    {
        CmdUnknown,
        CmdReadConfig,
        CmdListLogs,
        CmdReadLog,
        CmdClearLogs,
        CmdCount
    } command;

    ByteBuffer<MAX_PARAM_DATA> params;

    OfflineCommandPacket(uint8_t ref, Command cmd = CmdUnknown);
    virtual ~OfflineCommandPacket();
    virtual bool Read(ReadableStream& stream);
    virtual bool Write(WritableStream& stream);
};
