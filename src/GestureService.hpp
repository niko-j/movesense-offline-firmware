#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "app-resources/resources.h"
#include "meas_acc/resources.h"

class GestureService FINAL : private wb::ResourceProvider, private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    GestureService();
    ~GestureService();

private: /* wb::LaunchableModule*/
    virtual bool initModule() OVERRIDE;
    virtual void deinitModule() OVERRIDE;
    virtual bool startModule() OVERRIDE;
    virtual void stopModule() OVERRIDE;

private: /* wb::ResourceProvider */
    virtual void onGetRequest(
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onPostRequest(
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onSubscribe(
        const wb::Request& request, 
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onUnsubscribe(
        const wb::Request& rRequest, 
        const wb::ParameterList& parameters) OVERRIDE;

private: /* wb::ResourceClient */
    virtual void onGetResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onPostResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onPutResult(
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
        const wb::Value& rResultData) OVERRIDE;

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& value,
        const wb::ParameterList& parameters) OVERRIDE;

private:
    bool handleSubscribe(wb::LocalResourceId resourceId);
    void handleUnsubscribe(wb::LocalResourceId resourceId);
    uint16_t getAccSampleRate();

    void tapDetection(const WB_RES::AccData& data);
    void shakeDetection(const WB_RES::AccData& data);

    struct 
    {
        uint8_t tapSubscribers = 0;
        uint8_t shakeSubscribers = 0;
    } m_state;
};
