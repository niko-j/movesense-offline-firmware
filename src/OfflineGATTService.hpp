#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>

#include "app-resources/resources.h"
#include "mem_logbook/resources.h"
#include "OfflineInternalTypes.hpp"

class OfflineGATTService FINAL : private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineGATTService();
    ~OfflineGATTService();

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

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& rValue,
        const wb::ParameterList& rParameters) OVERRIDE;

private:
    void configGattSvc();

    void handleCommand(const OfflineCommandRequest& cmd);

    bool asyncSubsribeHandleResource(int16_t charHandle, wb::ResourceId& resourceOut);

    void sendData(wb::ResourceId characteristic, const uint8_t* data, uint32_t size);
    void sendSbem(wb::ResourceId characteristic, wb::LocalResourceId resource, const wb::Value& value);
    
    void sendStatusResponse(wb::ResourceId characteristic, uint8_t requestRef, uint16_t status);
    void sendPendingDataPacket(wb::ResourceId characteristic);

    bool asyncSendLog(uint32_t id);
    void asyncReadLogData(const WB_RES::LogEntry& entry);
    void asyncClearLogs();

    struct LogDataTransmission
    {
        int32_t logIndex = -1;
        uint32_t logSize = 0;
        uint32_t sentBytes = 0;
    } logDataTransmission;

    struct Characteristic
    {
        wb::ResourceId resourceId = wb::ID_INVALID_RESOURCE;
        int16_t handle = 0;
    };

    int16_t serviceHandle;

    Characteristic commandChar;
    Characteristic configChar;
    Characteristic dataChar;

    OfflineDataPacket pendingDataPacket;
    uint8_t pendingRequest;
};
