#include "GestureService.hpp"
#include "modules-resources/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const GestureService::LAUNCHABLE_NAME = "GestureSvc";
constexpr uint16_t DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE = 104;
constexpr uint16_t DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE = 13;
constexpr uint16_t DEFAULT_ORIENTATION_ACC_SAMPLE_RATE = 13;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::GESTURE_TAP::LID,
    WB_RES::LOCAL::GESTURE_SHAKE::LID,
    WB_RES::LOCAL::GESTURE_ORIENTATION::LID
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
    case WB_RES::LOCAL::GESTURE_ORIENTATION::LID:
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
    case WB_RES::LOCAL::GESTURE_ORIENTATION::LID:
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

void GestureService::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onSubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }
}

void GestureService::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& rResultData)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onUnsubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }
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

        if (m_state.orientationSubscribers)
            orientationDetection(data);

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
    {
        m_state.tapSubscribers += 1;
        m_tap.reset();
    }
    else if (resourceId == WB_RES::LOCAL::GESTURE_SHAKE::LID)
    {
        m_state.shakeSubscribers += 1;
        m_shake.reset();
    }
    else if (resourceId == WB_RES::LOCAL::GESTURE_ORIENTATION::LID)
    {
        m_state.orientationSubscribers += 1;
        m_orientation.reset();
    }

    uint16_t requiredSampleRate = getAccSampleRate();

    if (currentSampleRate != requiredSampleRate)
    {
        if (currentSampleRate > 0)
        {
            asyncUnsubscribe(
                WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(),
                AsyncRequestOptions::Empty, currentSampleRate);
        }

        DebugLogger::info("%s: Subscribing to /Meas/Acc/%u", LAUNCHABLE_NAME, requiredSampleRate);
        asyncSubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(),
            AsyncRequestOptions::NotCriticalSubscription, requiredSampleRate);
    }

    return true;
}

void GestureService::handleUnsubscribe(wb::LocalResourceId resourceId)
{
    uint16_t currentSampleRate = getAccSampleRate();

    if (resourceId == WB_RES::LOCAL::GESTURE_TAP::LID && m_state.tapSubscribers > 0)
        m_state.tapSubscribers -= 1;
    else if (resourceId == WB_RES::LOCAL::GESTURE_SHAKE::LID && m_state.shakeSubscribers > 0)
        m_state.shakeSubscribers -= 1;
    else if (resourceId == WB_RES::LOCAL::GESTURE_ORIENTATION::LID && m_state.orientationSubscribers > 0)
        m_state.orientationSubscribers -= 1;

    uint16_t requiredSampleRate = getAccSampleRate();

    if (currentSampleRate != requiredSampleRate)
    {
        DebugLogger::info("%s: Changing acc samplerate %u -> %u",
            LAUNCHABLE_NAME, currentSampleRate, requiredSampleRate);

        if (currentSampleRate > 0)
        {
            asyncUnsubscribe(
                WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(),
                AsyncRequestOptions::Empty, currentSampleRate);
        }

        if (requiredSampleRate)
        {
            asyncSubscribe(
                WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(),
                AsyncRequestOptions::NotCriticalSubscription, requiredSampleRate);
        }
    }
}

void GestureService::tapDetection(const WB_RES::AccData& data)
{
    constexpr uint32_t THRESHOLD = 20; // ~2g threshold
    constexpr uint32_t LATENCY = 80; // ms, should work with the lowest sample rate
    constexpr uint32_t TIMEOUT = 2000;

    float interval = 1000.0f / getAccSampleRate(); // delta between samples

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        float t = data.timestamp + i * interval;
        float z = data.arrayAcc[i].z;

        if (m_tap.t_rise > 0.0f) // Threshold triggered
        {
            if (t - m_tap.t_rise < LATENCY)
            {
                float diff = abs(z - m_tap.z_base);
                if (diff > THRESHOLD)
                {
                    m_tap.t_start = data.timestamp;
                    m_tap.count += 1;
                    m_tap.t_rise = 0.0f;
                }
            }
            else // reset threshold
            {
                m_tap.t_rise = 0.0f;
            }
        }
        else // Threshold not triggered
        {
            float diff = z - m_tap.z_prev;
            if (abs(diff) > THRESHOLD)
            {
                m_tap.t_rise = t;
                m_tap.z_base = m_tap.z_prev + 0.9f * diff;
            }
        }

        m_tap.z_prev = z;
    }

    if (m_tap.t_start > 0 && data.timestamp - m_tap.t_start > TIMEOUT)
    {
        // Tap counts > 5 are probably shaking or other false readings
        if (m_tap.count > 1 && m_tap.count < 6)
        {
            WB_RES::TapGestureData tapData;
            tapData.timestamp = data.timestamp;
            tapData.count = m_tap.count;

            updateResource(
                WB_RES::LOCAL::GESTURE_TAP(),
                ResponseOptions::ForceAsync, tapData);
        }
        m_tap.count = 0;
        m_tap.t_start = 0;
    }
}

void GestureService::shakeDetection(const WB_RES::AccData& data)
{
    // Detection phases:
    // 0 - not triggered (wait positive)
    // 1 - positive threshold triggered (wait negative)
    // 2 - negative threshold triggered (wait zero)

    constexpr float THRESHOLD = (9.81f * 1.5f);
    constexpr uint32_t LATENCY = 500; // ms

    float interval = 1000.0f / getAccSampleRate();

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        auto a = m_shake.lpf.filter(data.arrayAcc[i]);
        uint32_t t = data.timestamp + i * interval;
        float len = a.length<float>();

        if (m_shake.t_cycle > 0 && t - m_shake.t_cycle > LATENCY) // Reset?
        {
            if (m_shake.cycles > 1 &&
                m_shake.cycles < 100) // More than one cycle is interpreted as shaking
            {
                WB_RES::ShakeGestureData shake;
                shake.timestamp = t;
                shake.duration = m_shake.t_cycle - m_shake.t_begin;
                updateResource(WB_RES::LOCAL::GESTURE_SHAKE(), ResponseOptions::ForceAsync, shake);
            }

            m_shake.t_cycle = 0;
            m_shake.t_begin = 0;
            m_shake.phase = 0;
        }

        if (m_shake.phase == 0 && len > THRESHOLD)
        {
            m_shake.phase = 1;
            m_shake.max = a;

            if (m_shake.t_begin == 0) // Shaking started
                m_shake.t_begin = t;
        }
        else
        {
            float dot = m_shake.max.dotProduct(a);
            if (m_shake.phase == 1 && len > THRESHOLD && dot < 0.0f)
            {
                m_shake.phase = 2;
            }
            else if (m_shake.phase == 2 && dot > 0.0f)
            {
                m_shake.phase = 0;
                m_shake.cycles += 1; // Shake cycle detected
                m_shake.t_cycle = t;
            }
        }
    }
}

void GestureService::orientationDetection(const WB_RES::AccData& data)
{
    float interval = 1000.0f / getAccSampleRate();
    constexpr uint32_t LATENCY = 1000; // Time to hold (ms) the same orientation before committing

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        auto v = m_orientation.hpf.filter(data.arrayAcc[i]);
        float x = abs(v.x), y = abs(v.y), z = abs(v.z);
        WB_RES::Orientation orientation = m_orientation.pending;

        if (x > y && x > z) // LEFT or RIGHT
        {
            if (v.x > 0.0)
                orientation = WB_RES::Orientation::RIGHT;
            else
                orientation = WB_RES::Orientation::LEFT;
        }
        else if (z > x && z > y) // UP or DOWN
        {
            if (v.z > 0.0)
                orientation = WB_RES::Orientation::UP;
            else
                orientation = WB_RES::Orientation::DOWN;
        }
        else // STANDING
        {
            if (v.y > 0.0)
                orientation = WB_RES::Orientation::UPRIGHT;
            else
                orientation = WB_RES::Orientation::UPSIDEDOWN;
        }

        uint32_t t = data.timestamp + i * interval;
        if (orientation != m_orientation.pending) // Changed, start measuring time
        {
            m_orientation.t_changed = t;
            m_orientation.pending = orientation;
        }

        if (t - m_orientation.t_changed > LATENCY && m_orientation.current != orientation)
        {
            WB_RES::OrientationData orientationData;
            orientationData.timestamp = t;
            orientationData.orientation = orientation;
            updateResource(WB_RES::LOCAL::GESTURE_ORIENTATION(), ResponseOptions::ForceAsync, orientationData);

            m_orientation.current = orientation;
        }
    }
}

uint16_t GestureService::getAccSampleRate()
{
    if (m_state.tapSubscribers > 0)
        return DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE;

    if (m_state.shakeSubscribers > 0)
        return DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE;

    if (m_state.orientationSubscribers > 0)
        return DEFAULT_ORIENTATION_ACC_SAMPLE_RATE;

    return 0;
}

void GestureService::TapDetection::reset()
{
    count = 0;
    t_start = 0;
    z_prev = 0.0f;
    z_base = 0.0f;
    t_rise = 0.0f;
}

void GestureService::ShakeDetection::reset()
{
    max = {};
    t_begin = 0;
    t_cycle = 0;
    cycles = 0;
    phase = 0;
    lpf.reset();
}

void GestureService::Orientation::reset()
{
    current = WB_RES::Orientation::UP;
    hpf.reset();
}
