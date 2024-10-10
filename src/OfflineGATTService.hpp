#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>

#include "app-resources/resources.h"

// Offset (4) + Total (4) + Reference (1) + payload
#define HEADER_SIZE 9

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

    void resetBuffer();
    size_t writeHeaderToBuffer(uint32_t offset, uint32_t total_bytes);
    size_t writeRawDataToBuffer(const uint8_t* data, uint8_t length);
    size_t writeSbemToBuffer(wb::LocalResourceId resourceId, const wb::Value& data, uint32_t offset = 0);
    void sendBuffer(whiteboard::ResourceId characteristic, uint32_t len);

    void sendData(wb::ResourceId characteristic, const uint8_t* data, uint32_t size);
    void sendSbemValue(wb::ResourceId characteristic, wb::LocalResourceId resourceId, const wb::Value &value);

    int16_t offlineSvcHandle;

    int16_t commandCharHandle;
    int16_t configCharHandle;
    int16_t dataCharHandle;

    whiteboard::ResourceId commandCharResource;
    whiteboard::ResourceId configCharResource;
    whiteboard::ResourceId dataCharResource;

    uint8_t dataBuffer[HEADER_SIZE + PAYLOAD_SIZE];
    uint8_t dataClientReference;
};
