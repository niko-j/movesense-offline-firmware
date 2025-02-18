#pragma once
#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "meas_acc/resources.h"
#include "internal/LowPassFilter.hpp"

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
        const wb::Request& request,
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

    struct State
    {
        uint8_t tapSubscribers = 0;
        uint8_t shakeSubscribers = 0;
    } m_state;

    struct TapDetection
    {
        uint8_t count = 0;
        uint32_t t_start = 0;
        float z_prev = 0.0f;
        float z_base = 0.0f; // Baseline before trigger
        float t_rise = 0.0f; // t when triggered
        void reset();
    } m_tap;

    struct ShakeDetection
    {
        gesture_svc::LowPassFilter lpf;
        wb::FloatVector3D max;
        uint32_t t_begin = 0;
        uint32_t t_cycle = 0;
        uint8_t cycles = 0;
        uint8_t phase = 0;
        void reset();
    } m_shake;
};
