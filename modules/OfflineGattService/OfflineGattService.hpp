#pragma once
#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>

#include "app-resources/resources.h"
#include "mem_logbook/resources.h"
#include "protocol/Protocol.hpp"

struct CommandPacket;

class OfflineGattService FINAL : private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineGattService();
    ~OfflineGattService();

private: /* wb::LaunchableModule*/
    virtual bool initModule() OVERRIDE;
    virtual void deinitModule() OVERRIDE;
    virtual bool startModule() OVERRIDE;
    virtual void stopModule() OVERRIDE;

private: /* wb::ResourceClient */
    virtual void onGetResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onPutResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onPostResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onDeleteResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onSubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onUnsubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& rValue,
        const wb::ParameterList& rParameters) OVERRIDE;

private:
    void handleCommand(const CommandPacket& packet);
    bool asyncSubsribeHandleResource(int16_t charHandle, wb::ResourceId& resourceOut);

    void sendData(const uint8_t* data, uint32_t size);
    void sendPartialData(const uint8_t* data, uint32_t partSize, uint32_t totalSize, uint32_t offset);
    void sendStatusResponse(uint8_t requestRef, uint16_t status);
    void sendPacket(Packet& packet);
    bool sendLog(uint32_t id);

    struct LogEntry
    {
        uint32_t index = 0;
        uint32_t size = 0;
        uint32_t progress = 0;
    } m_download;

    struct DebugLogStream
    {
        uint8_t packetRef = Packet::INVALID_REF;
        uint8_t logLevel = 0;
    } m_debugLogStream;

    struct Characteristic
    {
        wb::ResourceId resourceId = wb::ID_INVALID_RESOURCE;
        int16_t handle = 0;
    };

    int16_t serviceHandle;
    Characteristic txChar;
    Characteristic rxChar;
    uint8_t pendingRequestId;
    uint8_t buffer[Packet::MAX_PACKET_SIZE];
};
