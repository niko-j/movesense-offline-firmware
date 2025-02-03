#pragma once
#include "OfflinePacket.hpp"

struct OfflineCommandPacket : public OfflinePacket
{
    static constexpr size_t MAX_PARAM_DATA = 16;

    enum Command : uint8_t
    {
        CmdUnknown,
        CmdReadConfig,
        CmdListLogs,
        CmdReadLog,
        CmdClearLogs,
        CmdCount
    } command;

    union CommandParams
    {
        struct 
        {
            uint16_t logIndex;
        } ReadLogParams;
    } params;

    OfflineCommandPacket(uint8_t ref, Command cmd = CmdUnknown, CommandParams args = {});
    virtual ~OfflineCommandPacket();
    virtual bool Read(ReadableBuffer& stream);
    virtual bool Write(WritableBuffer& stream);
};
