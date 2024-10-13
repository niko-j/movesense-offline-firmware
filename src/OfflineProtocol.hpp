#pragma once
#include "app-resources/resources.h"
#include "wb-resources/resources.h"
#include "comm_ble_gattsvc/resources.h"
#include "mem_logbook/resources.h"
#include "sbem_types.h"

enum OfflinePacketType : uint8_t
{
    OfflinePacketTypeUnknown = 0x00,
    OfflinePacketTypeCommand = 0x01,
    OfflinePacketTypeStatus = 0x02,
    OfflinePacketTypeData = 0x03,
    OfflinePacketTypeConfig = 0x04,
    OfflinePacketTypeLogList = 0x05,

    OfflinePacketTypeCount
};

enum OfflineCmd : uint8_t
{
    OfflineCmdUnknown,
    OfflineCmdReadConfig,
    OfflineCmdListLogs,
    OfflineCmdReadLog,
    OfflineCmdClearLogs,
    OfflineCmdCount
};

constexpr uint32_t OFFLINE_PACKET_MAX_PAYLOAD = 128;
constexpr uint32_t OFFLINE_PACKET_HEADER_SIZE = 2;
constexpr uint32_t OFFLINE_PACKET_BUFFER_SIZE = (
    OFFLINE_PACKET_MAX_PAYLOAD + OFFLINE_PACKET_HEADER_SIZE
    );

#define OFFLINE_PACKET_PROPERTY(Type, __PropName__, Size, Offset) \
    static_assert(Offset + Size <= SIZE \
        && "Property does not fit into the buffer"); \
    const Type& get##__PropName__() const \
        { return *reinterpret_cast<const Type*>(mBuffer + Offset); } \
    void set##__PropName__(const Type& value) \
        { *reinterpret_cast<Type*>(mBuffer + Offset) = value; }

#define OFFLINE_PACKET_PROPERTY_ARRAY(Type, __PropName__, Size, Length, Offset) \
protected: \
    uint8_t mCount##__PropName__ = 0; \
public: \
    static_assert(Offset + Size * Length <= SIZE \
        && "Property array does not fit into the buffer"); \
    wb::Array<Type> get##__PropName__() const \
    { \
        return wb::MakeArray(\
            reinterpret_cast<const Type*>(mBuffer + Offset), mCount##__PropName__); \
    } \
    bool push##__PropName__(const Type* value, size_t len) \
    { \
        if(mCount##__PropName__ + len <= Length) { \
            memcpy(mBuffer + Offset + Size * mCount##__PropName__, value, len * Size); \
            mCount##__PropName__ += len; \
            return true; \
        } return false; \
    } \
    bool pop##__PropName__() \
    { \
        if(mCount##__PropName__ > 0) {\
            mCount##__PropName__ -= 1; \
            memset(mBuffer + Offset + Size * mCount##__PropName__, 0, Size); \
            return true; \
        } return false; \
    } \
    void clear##__PropName__() { \
        memset(mBuffer + Offset, 0, Size * Length); \
        mCount##__PropName__ = 0; \
    } \
    inline uint8_t* getRaw##__PropName__() { return (mBuffer + Offset); }

#define OFFLINE_PACKET_SBEM_PROPERTY(Type, LocalResourceId, __PropName__, Size, Offset) \
    static_assert(Offset + Size <= SIZE \
        && "Property does not fit into the buffer"); \
    Type get##__PropName__(); \
    bool write##__PropName__(const Type& value) \
    { \
        size_t len = getSbemLength(LocalResourceId, wb::Value(value)); \
        if (!len || len > SIZE) return false; \
        writeToSbemBuffer(mBuffer + Offset, Size, 0, LocalResourceId, wb::Value(value)); \
        return true; \
    }


#define OFFLINE_PACKET(PacketName, PacketType, ByteLength) \
protected: \
    static_assert(ByteLength > 2 && "Too small packet size"); \
    uint8_t* mBuffer = nullptr; \
public: \
    const static OfflinePacketType TYPE = PacketType; \
    const static uint32_t SIZE = ByteLength; \
    PacketName(uint8_t* buffer) : mBuffer(buffer) { memset(buffer, 0, ByteLength); *buffer = TYPE; } \
    uint8_t getReference() const { return mBuffer[1]; } \
    void setReference(uint8_t value) { mBuffer[1] = value; } \
    bool decode(const wb::Array<uint8_t>& value); \
    wb::Array<uint8_t> encode();

constexpr uint8_t OFFLINE_PACKET_INVALID_REF = 0;
struct OfflinePacket
{
    virtual uint8_t getReference() const = 0;
    virtual bool decode(const wb::Array<uint8_t>& value) = 0;
    virtual wb::Array<uint8_t> encode() = 0;
};

OfflinePacketType parseOfflinePacketType(uint8_t b);

struct OfflineCommandPacket : public OfflinePacket
{
    OFFLINE_PACKET(OfflineCommandPacket, OfflinePacketTypeCommand, 2 + 1 + 32);
    OFFLINE_PACKET_PROPERTY(OfflineCmd, Command, 1, 2);
    OFFLINE_PACKET_PROPERTY_ARRAY(uint8_t, Parameters, 1, 32, 3);
};

struct OfflineStatusPacket : public OfflinePacket
{
    OFFLINE_PACKET(OfflineStatusPacket, OfflinePacketTypeStatus, 2 + 2);
    OFFLINE_PACKET_PROPERTY(uint16_t, Status, 2, 2);
};

struct OfflineConfigPacket : public OfflinePacket
{
    OFFLINE_PACKET(OfflineConfigPacket, OfflinePacketTypeConfig, 2 + 15);
    OFFLINE_PACKET_SBEM_PROPERTY(
        WB_RES::OfflineConfig, WB_RES::LOCAL::OFFLINE_CONFIG::LID,
        Config, 15, 2);
};

struct OfflineDataPacket : public OfflinePacket
{
    static const uint32_t MAX_PAYLOAD = OFFLINE_PACKET_MAX_PAYLOAD - 8;
    OFFLINE_PACKET(OfflineDataPacket, OfflinePacketTypeData, 2 + OFFLINE_PACKET_MAX_PAYLOAD);
    OFFLINE_PACKET_PROPERTY(uint32_t, Offset, 4, 2);
    OFFLINE_PACKET_PROPERTY(uint32_t, TotalBytes, 4, 6);
    OFFLINE_PACKET_PROPERTY_ARRAY(uint8_t, Bytes, 1, MAX_PAYLOAD, 10);
};

constexpr uint32_t OFFLINE_LOG_LIST_PACKET_MAX_ITEMS = 6;
struct OfflineLogItem
{
    uint32_t id;
    uint32_t size;
    uint64_t modified;
};

struct OfflineLogListPacket : public OfflinePacket
{
    OFFLINE_PACKET(OfflineLogListPacket, OfflinePacketTypeLogList, 2 + 2 + 96);
    OFFLINE_PACKET_PROPERTY(uint8_t, Count, 1, 2);
    OFFLINE_PACKET_PROPERTY(bool, Complete, 1, 3);
    OFFLINE_PACKET_PROPERTY_ARRAY(OfflineLogItem, Items, 16, OFFLINE_LOG_LIST_PACKET_MAX_ITEMS, 4);

    bool addItem(const WB_RES::LogEntry& entry);
    void resetItems();
    bool isFull() const;
};
