#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>

#include "app-resources/resources.h"

#define PAYLOAD_SIZE 128

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
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onPutResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onPostResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

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

    bool asyncSubsribeHandleResource(int16_t charHandle, whiteboard::ResourceId& resourceOut);

    void sendOfflineData(const uint8_t* data, uint32_t size);
    void sendOfflineConfig(const WB_RES::OfflineConfig &config);
    void recvOfflineConfig(const WB_RES::OfflineConfig &config);

    int16_t offlineSvcHandle;

    int16_t commandCharHandle;
    int16_t configCharHandle;
    int16_t dataCharHandle;

    whiteboard::ResourceId commandCharResource;
    whiteboard::ResourceId configCharResource;
    whiteboard::ResourceId dataCharResource;

    // Part counter (4) + client reference (1) + payload
    uint8_t dataBuffer[4+1+PAYLOAD_SIZE];
    uint8_t dataClientReference;
};
