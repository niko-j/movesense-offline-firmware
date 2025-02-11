#include "GestureService.hpp"
#include "movesense.h"
#include "utils/Filters.hpp"
#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const GestureService::LAUNCHABLE_NAME = "GestureSvc";
constexpr uint16_t DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE = 104;
constexpr uint16_t DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE = 13;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::GESTURE_TAP::LID,
    WB_RES::LOCAL::GESTURE_SHAKE::LID,
};

GestureService::GestureService()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , m_state({})
{

}

GestureService::~GestureService()
{
}

bool GestureService::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK) {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void GestureService::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool GestureService::startModule()
{
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void GestureService::stopModule()
{
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void GestureService::onGetRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    wb::LocalResourceId lid = request.getResourceId().localResourceId;
    DebugLogger::verbose("%s: onGetRequest resource %d", LAUNCHABLE_NAME, lid);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (lid)
    {
    default:
        DebugLogger::warning("%s: Unimplemented GET for resource %d", LAUNCHABLE_NAME, lid);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void GestureService::onPostRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    wb::LocalResourceId lid = request.getResourceId().localResourceId;
    DebugLogger::verbose("%s: onPostRequest resource %d", LAUNCHABLE_NAME, lid);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (lid)
    {
    default:
        DebugLogger::warning("%s: Unimplemented POST for resource %d", LAUNCHABLE_NAME, lid);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void GestureService::onSubscribe(
    const whiteboard::Request& request,
    const whiteboard::ParameterList& parameters)
{
    wb::LocalResourceId lid = request.getResourceId().localResourceId;
    DebugLogger::verbose("%s: onSubscribe resource %d", LAUNCHABLE_NAME, lid);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    wb::Result result = wb::HTTP_CODE_NOT_FOUND;

    switch (lid)
    {
    case WB_RES::LOCAL::GESTURE_TAP::LID:
    case WB_RES::LOCAL::GESTURE_SHAKE::LID:
    {
        if (handleSubscribe(lid))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    default:
    {
        DebugLogger::warning("%s: Unimplemented SUBSCRIBE for resource %d", LAUNCHABLE_NAME, lid);
        break;
    }
    }

    returnResult(request, result);
}

void GestureService::onUnsubscribe(
    const whiteboard::Request& request,
    const whiteboard::ParameterList& rarameters)
{
    wb::LocalResourceId lid = request.getResourceId().localResourceId;
    DebugLogger::verbose("%s: onUnsubscribe resource %d", LAUNCHABLE_NAME, lid);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (lid)
    {
    case WB_RES::LOCAL::GESTURE_SHAKE::LID:
    case WB_RES::LOCAL::GESTURE_TAP::LID:
    {
        handleUnsubscribe(lid);
        break;
    }
    default:
    {
        DebugLogger::warning("%s: Unimplemented UNSUBSCRIBE for resource %d", LAUNCHABLE_NAME, lid);
        break;
    }
    }

    returnResult(request, wb::HTTP_CODE_OK);
}

void GestureService::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onGetResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void GestureService::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPostResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void GestureService::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPutResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void GestureService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::info("%s: onSubscribeResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(), AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        }
        break;
    }
    default:
        break;
    }

}

void GestureService::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& rResultData)
{
    DebugLogger::info("%s: onUnsubscribeResult (res: %d), status %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void GestureService::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::AccData&>();

        if (m_state.tapSubscribers)
            tapDetection(data);

        if (m_state.shakeSubscribers)
            shakeDetection(data);

        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
    }
}

bool GestureService::handleSubscribe(wb::LocalResourceId resourceId)
{
    uint16_t currentSampleRate = getAccSampleRate();

    if (resourceId == WB_RES::LOCAL::GESTURE_TAP::LID)
        m_state.tapSubscribers += 1;
    else if (resourceId == WB_RES::LOCAL::GESTURE_SHAKE::LID)
        m_state.shakeSubscribers += 1;

    uint16_t requiredSampleRate = getAccSampleRate();

    if (currentSampleRate != requiredSampleRate)
    {
        if (currentSampleRate > 0)
            asyncUnsubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID);

        DebugLogger::info("%s: Subscribing to /Meas/Acc/%u", LAUNCHABLE_NAME, requiredSampleRate);
        asyncSubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(), AsyncRequestOptions::Empty, requiredSampleRate);
    }

    return true;
}

void GestureService::handleUnsubscribe(wb::LocalResourceId resourceId)
{
    uint16_t currentSampleRate = getAccSampleRate();

    // Subscription priority: ACC (* Hz) > TAP (104 Hz) > ACTIVITY (13 Hz) > SHAKE (13 Hz)
    if (resourceId == WB_RES::LOCAL::GESTURE_TAP::LID && m_state.tapSubscribers > 0)
        m_state.tapSubscribers -= 1;
    else if (resourceId == WB_RES::LOCAL::GESTURE_SHAKE::LID && m_state.shakeSubscribers > 0)
        m_state.shakeSubscribers -= 1;

    uint16_t requiredSampleRate = getAccSampleRate();

    if (currentSampleRate != requiredSampleRate)
    {
        DebugLogger::info("%s: Changing acc samplerate %u -> %u",
            LAUNCHABLE_NAME, currentSampleRate, requiredSampleRate);

        if (currentSampleRate > 0)
            asyncUnsubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID);

        if (requiredSampleRate)
            asyncSubscribe(
                WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(), AsyncRequestOptions::Empty,
                requiredSampleRate);
    }
}

void GestureService::tapDetection(const WB_RES::AccData& data)
{
    constexpr uint32_t THRESHOLD = 20; // ~2g threshold
    constexpr uint32_t LATENCY = 80; // ms, should work with the lowest sample rate
    constexpr uint32_t TIMEOUT = 2000;

    static uint8_t tapCount = 0;
    static uint32_t tapStart = 0;
    float interval = 1000.0f / getAccSampleRate(); // delta between samples

    static float z_prev = data.arrayAcc[0].z;
    static float z_base = 0.0f; // Baseline before trigger
    static float t_rise = 0.0f; // t when triggered

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        float t = data.timestamp + i * interval;
        float z = data.arrayAcc[i].z;

        if (t_rise > 0.0f) // Threshold triggered
        {
            if (t - t_rise < LATENCY)
            {
                float diff = abs(z - z_base);
                if (diff > THRESHOLD)
                {
                    tapStart = data.timestamp;
                    tapCount += 1;
                    t_rise = 0.0f;
                }
            }
            else // reset threshold
            {
                t_rise = 0.0f;
            }
        }
        else // Threshold not triggered
        {
            float diff = z - z_prev;
            if (abs(diff) > THRESHOLD)
            {
                t_rise = t;
                z_base = z_prev + 0.9f * diff;
            }
        }

        z_prev = z;
    }

    if (tapStart > 0 && data.timestamp - tapStart > TIMEOUT)
    {
        if (tapCount > 1)
        {
            WB_RES::TapGestureData tapData;
            tapData.timestamp = data.timestamp;
            tapData.count = tapCount;

            updateResource(
                WB_RES::LOCAL::GESTURE_TAP(),
                ResponseOptions::ForceAsync, tapData);
        }
        tapCount = 0;
        tapStart = 0;
    }
}

void GestureService::shakeDetection(const WB_RES::AccData& data)
{
    constexpr float THRESHOLD = (9.81f * 1.5f);
    constexpr uint32_t LATENCY = 500; // ms

    static LowPassFilter lpf;
    static wb::FloatVector3D max;
    static uint32_t t_begin = 0;
    static uint32_t t_start_cycle = 0;
    static uint8_t cycles = 0;

    // Detection phases:
    // 0 - not triggered (wait positive)
    // 1 - positive threshold triggered (wait negative)
    // 2 - negative threshold triggered (wait zero)
    static uint8_t phase = 0;

    float interval = 1000.0f / getAccSampleRate();

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        auto a = lpf.filter(data.arrayAcc[i]);
        uint32_t t = data.timestamp + i * interval;
        float len = a.length<float>();

        if (t_start_cycle > 0 && t - t_start_cycle > LATENCY) // Reset?
        {
            if (cycles > 1) // More than one cycle is interpreted as shaking
            {
                WB_RES::ShakeGestureData shake;
                shake.timestamp = t;
                shake.duration = t - t_begin;

                updateResource(
                    WB_RES::LOCAL::GESTURE_SHAKE(),
                    ResponseOptions::ForceAsync, shake);
            }

            t_start_cycle = 0;
            t_begin = 0;
            phase = 0;
        }

        if (phase == 0 && len > THRESHOLD)
        {
            phase = 1;
            t_begin = t;
            t_start_cycle = t;
            max = a;
        }
        else
        {
            float dot = max.dotProduct(a);
            if (phase == 1 && len > THRESHOLD && dot < 0.0f)
            {
                phase = 2;
            }
            else if (phase == 2 && dot > 0.0f)
            {
                phase = 0;
                cycles += 1; // Shake cycle detected
                t_start_cycle = t;
            }
        }
    }
}

uint16_t GestureService::getAccSampleRate()
{
    if (m_state.tapSubscribers > 0)
        return DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE;

    if (m_state.shakeSubscribers > 0)
        return DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE;

    return 0;
}
