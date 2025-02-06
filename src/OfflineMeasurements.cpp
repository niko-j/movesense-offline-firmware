#include "movesense.h"
#include "OfflineMeasurements.hpp"
#include "compression/BitPack.hpp"
#include "compression/DeltaCompression.hpp"
#include "compression/FixedPoint.hpp"

#include "app-resources/resources.h"
#include "system_debug/resources.h"
#include "system_settings/resources.h"
#include "ui_ind/resources.h"
#include "meas_ecg/resources.h"
#include "meas_hr/resources.h"
#include "meas_acc/resources.h"
#include "meas_gyro/resources.h"
#include "meas_magn/resources.h"
#include "meas_temp/resources.h"
#include "mem_datalogger/resources.h"
#include "mem_logbook/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include <functional>

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

const char* const OfflineMeasurements::LAUNCHABLE_NAME = "OfflineMeas";
constexpr uint16_t DEFAULT_ACC_SAMPLE_RATE = 13;
constexpr uint16_t DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE = 104;
constexpr uint16_t DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE = 13;
constexpr uint16_t DEFAULT_ACTIGRAPHY_ACC_SAMPLE_RATE = 13;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_MEAS_ECG::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_HR::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_RR::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_GYRO::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_MAGN::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_TEMP::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_TAP::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_SHAKE::LID,
};

OfflineMeasurements::OfflineMeasurements()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , m_state({})
    , m_options({})
{

}

OfflineMeasurements::~OfflineMeasurements()
{
}

bool OfflineMeasurements::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK) {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineMeasurements::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineMeasurements::startModule()
{
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    asyncSubscribe(WB_RES::LOCAL::OFFLINE_STATE());
    return true;
}

void OfflineMeasurements::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::OFFLINE_STATE());
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineMeasurements::onGetRequest(
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

void OfflineMeasurements::onPostRequest(
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

void OfflineMeasurements::onSubscribe(
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

    switch (lid)
    {
    case WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_TAP::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_SHAKE::LID:
    {
        subscribeAcc(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_GYRO::LID:
    {
        subscribeGyro(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_MAGN::LID:
    {
        subscribeMagn(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_HR::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_RR::LID:
    {
        subscribeHR(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED::LID:
    {
        subscribeECG(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_TEMP::LID:
    {
        subscribeTemp(lid);
        break;
    }
    default:
    {
        DebugLogger::warning("%s: Unimplemented SUBSCRIBE for resource %d", LAUNCHABLE_NAME, lid);
        break;
    }
    }

    returnResult(request, wb::HTTP_CODE_OK);
}

void OfflineMeasurements::onUnsubscribe(
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
    case WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_TAP::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_SHAKE::LID:
    {
        dropAccSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_GYRO::LID:
    {
        dropGyroSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_MAGN::LID:
    {
        dropMagnSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_HR::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_RR::LID:
    {
        dropHRSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED::LID:
    {
        dropECGSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_TEMP::LID:
    {
        dropTempSubscription(lid);
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

void OfflineMeasurements::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onGetResult - res: %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        auto config = result.convertTo<WB_RES::OfflineConfig>();
        configureLogger(config);
        break;
    }
    default:
        break;
    }
}

void OfflineMeasurements::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPostResult - res: %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineMeasurements::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPutResult - res: %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_DATALOGGER_STATE::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            // Start new log after stopping data logger
            if (m_state.configured && !m_state.logging)
                asyncPost(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::Empty);
        }
        else
        {
            switch (resultCode)
            {
            case wb::HTTP_CODE_INSUFFICIENT_STORAGE:
            {
                asyncPut(
                    WB_RES::LOCAL::OFFLINE_STATE(),
                    AsyncRequestOptions::Empty,
                    WB_RES::OfflineState::ERROR_STORAGE_FULL);
                break;
            }
            default:
            {
                asyncPut(
                    WB_RES::LOCAL::OFFLINE_STATE(),
                    AsyncRequestOptions::Empty,
                    WB_RES::OfflineState::ERROR_SYSTEM_FAILURE);
                break;
            }
            }
            stopLogging();
        }
        break;
    }
    case WB_RES::LOCAL::MEM_DATALOGGER_CONFIG::LID:
    {
        switch (resultCode)
        {
        case wb::HTTP_CODE_BAD_REQUEST:
        {
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(),
                AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_INVALID_CONFIG);
            m_state.configured = false;
            break;
        }
        case wb::HTTP_CODE_OK:
        {
            m_state.configured = true;
            startLogging();
            break;
        }
        default:
        {
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(),
                AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_SYSTEM_FAILURE);
            stopLogging();
            break;
        }
        }
        break;
    }
    default:
        break;
    }
}

void OfflineMeasurements::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::info("%s: onSubscribeResult - res: %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
    {
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        break;
    }
    case WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_HR::LID:
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_TEMP::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            stopLogging();
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(),
                AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        }
        break;
    }
    default:
        break;
    }

}

void OfflineMeasurements::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& rResultData)
{
    DebugLogger::info("%s: onUnsubscribeResult (res: %d), status %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineMeasurements::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
    {
        auto state = value.convertTo<WB_RES::OfflineState>();

        stopLogging();

        if (state == WB_RES::OfflineState::RUNNING)
        {
            asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        }

        break;
    }
    case WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::ECGData&>();
        if (m_options.useEcgCompression)
            compressECGSamples(data);
        else
            recordECGSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_HR::LID:
    {
        auto data = value.convertTo<const WB_RES::HRData&>();
        if (m_state.measurements[WB_RES::OfflineMeasurement::HR])
            recordHRAverages(data);
        if (m_state.measurements[WB_RES::OfflineMeasurement::RR])
            recordRRIntervals(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::AccData&>();

        if (m_state.measurements[WB_RES::OfflineMeasurement::ACC])
            recordAccelerationSamples(data);

        if (m_state.measurements[WB_RES::OfflineMeasurement::ACTIVITY])
            recordActivity(data);

        if (m_state.measurements[WB_RES::OfflineMeasurement::TAP])
            tapDetection(data);

        break;
    }
    case WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::GyroData&>();
        recordGyroscopeSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::MagnData&>();
        recordMagnetometerSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_TEMP::LID:
    {
        auto data = value.convertTo<const WB_RES::TemperatureValue&>();
        recordTemperatureSamples(data);
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
    }
}

uint8_t OfflineMeasurements::configureLogger(const WB_RES::OfflineConfig& config)
{
    ASSERT(!m_state.logging);
    m_options = {
        .useEcgCompression = !!(config.options & WB_RES::OfflineOptionsFlags::COMPRESSECGSAMPLES)
    };

    uint8_t count = 0;
    static const char* paths[WB_RES::OfflineMeasurement::COUNT] = {
        "/Offline/Meas/ECG",
        "/Offline/Meas/HR",
        "/Offline/Meas/RR",
        "/Offline/Meas/Acc",
        "/Offline/Meas/Gyro",
        "/Offline/Meas/Magn",
        "/Offline/Meas/Temp",
        "/Offline/Meas/Activity",
        "/Offline/Meas/Tap",
        "/Offline/Meas/Shake",
    };

    WB_RES::DataEntry entries[WB_RES::OfflineMeasurement::COUNT] = {};
    for (auto i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        m_state.measurements[i] = config.sampleRates[i];
        if (config.sampleRates[i])
        {
            if (i == WB_RES::OfflineMeasurement::ECG && m_options.useEcgCompression)
                entries[count].path = "/Offline/Meas/ECG/Compressed";
            else
                entries[count].path = paths[i];

            count++;
        }
    }

    WB_RES::DataLoggerConfig logConfig = {};
    logConfig.dataEntries.dataEntry = wb::MakeArray(entries, count);
    asyncPut(WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(), AsyncRequestOptions::Empty, logConfig);

    return count;
}

bool OfflineMeasurements::startLogging()
{
    if (!m_state.logging)
    {
        DebugLogger::info("%s: Starting Data Logger...", LAUNCHABLE_NAME);
        m_state.logging = true;

        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::ForceAsync,
            WB_RES::DataLoggerState::DATALOGGER_LOGGING);

        return true;
    }
    return false;
}

void OfflineMeasurements::stopLogging()
{
    if (m_state.logging)
    {
        DebugLogger::info("%s: Stopping Data Logger...", LAUNCHABLE_NAME);
        m_state.logging = false;

        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::Empty,
            WB_RES::DataLoggerState::DATALOGGER_READY);
    }
}

void OfflineMeasurements::subscribeAcc(wb::LocalResourceId resourceId)
{
    uint16_t currentSampleRate = getSubbedAccSampleRate();

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID)
        m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC] += 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_TAP::LID)
        m_state.subscriberCount[WB_RES::OfflineMeasurement::TAP] += 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID)
        m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY] += 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_SHAKE::LID)
        m_state.subscriberCount[WB_RES::OfflineMeasurement::SHAKE] += 1;

    uint16_t requiredSampleRate = getSubbedAccSampleRate();

    if (currentSampleRate != requiredSampleRate)
    {
        if (currentSampleRate > 0)
            asyncUnsubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID);

        DebugLogger::info("%s: Subscribing to /Meas/Acc/%u", LAUNCHABLE_NAME, requiredSampleRate);
        asyncSubscribe(WB_RES::LOCAL::MEAS_ACC_SAMPLERATE(), AsyncRequestOptions::Empty, requiredSampleRate);
    }
}

void OfflineMeasurements::subscribeGyro(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::GYRO];
    subscribers += 1;

    uint16_t sampleRate = m_state.measurements[WB_RES::OfflineMeasurement::GYRO];
    constexpr uint16_t DEFAULT_SAMPLE_RATE = 13;
    if (sampleRate == 0)
        sampleRate = DEFAULT_SAMPLE_RATE;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Gyro/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }
}

void OfflineMeasurements::subscribeMagn(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::MAGN];
    subscribers += 1;

    uint16_t sampleRate = m_state.measurements[WB_RES::OfflineMeasurement::MAGN];
    constexpr uint16_t DEFAULT_SAMPLE_RATE = 13;
    if (sampleRate == 0)
        sampleRate = DEFAULT_SAMPLE_RATE;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Magn/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }
}

void OfflineMeasurements::subscribeHR(wb::LocalResourceId resourceId)
{
    auto& hrSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::HR];
    auto& rrSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::RR];

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_HR::LID)
        hrSubs += 1;

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_RR::LID)
        rrSubs += 1;

    if (hrSubs + rrSubs == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/HR", LAUNCHABLE_NAME);
        asyncSubscribe(WB_RES::LOCAL::MEAS_HR(), AsyncRequestOptions::Empty);
    }
}

void OfflineMeasurements::subscribeECG(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::ECG];
    subscribers += 1;

    uint16_t sampleRate = m_state.measurements[WB_RES::OfflineMeasurement::ECG];
    constexpr uint16_t DEFAULT_SAMPLE_RATE = 125;
    if (sampleRate == 0)
        sampleRate = DEFAULT_SAMPLE_RATE;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/ECG/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }
}

void OfflineMeasurements::subscribeTemp(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::TEMP];
    subscribers += 1;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Temp", LAUNCHABLE_NAME);
        asyncSubscribe(WB_RES::LOCAL::MEAS_TEMP(), AsyncRequestOptions::Empty);
    }
}

void OfflineMeasurements::dropAccSubscription(wb::LocalResourceId resourceId)
{
    auto& accSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC];
    auto& activitySubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY];
    auto& tapSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::TAP];
    auto& shakeSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::SHAKE];

    uint16_t currentSampleRate = getSubbedAccSampleRate();

    // Subscription priority: ACC (* Hz) > TAP (104 Hz) > ACTIVITY (13 Hz) > SHAKE (13 Hz)
    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID && accSubs > 0)
        accSubs -= 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_TAP::LID && tapSubs > 0)
        tapSubs -= 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID && activitySubs > 0)
        activitySubs -= 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_SHAKE::LID && shakeSubs > 0)
        shakeSubs -= 1;

    uint16_t requiredSampleRate = getSubbedAccSampleRate();

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

void OfflineMeasurements::dropGyroSubscription(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::GYRO];
    subscribers -= 1;

    if (subscribers == 0)
    {
        DebugLogger::info("%s: Unsubscribing from gyro", LAUNCHABLE_NAME);
        asyncUnsubscribe(WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::ID);
    }
}

void OfflineMeasurements::dropMagnSubscription(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::MAGN];
    subscribers -= 1;

    if (subscribers == 0)
    {
        DebugLogger::info("%s: Unsubscribing from magn", LAUNCHABLE_NAME);
        asyncUnsubscribe(WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::ID);
    }
}

void OfflineMeasurements::dropHRSubscription(wb::LocalResourceId resourceId)
{
    bool unsub = false;
    auto& hrSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::HR];
    auto& rrSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::RR];

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_HR::LID && hrSubs > 0)
        hrSubs -= 1;

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_RR::LID && rrSubs > 0)
        rrSubs -= 1;

    if (hrSubs == 0 && rrSubs == 0)
    {
        DebugLogger::info("%s: Unsubscribing from HR", LAUNCHABLE_NAME);
        asyncUnsubscribe(WB_RES::LOCAL::MEAS_HR::ID);
    }
}

void OfflineMeasurements::dropECGSubscription(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::ECG];
    subscribers -= 1;

    if (subscribers == 0)
    {
        DebugLogger::info("%s: Unsubscribing from ECG", LAUNCHABLE_NAME);
        asyncUnsubscribe(WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::ID);
    }
}

void OfflineMeasurements::dropTempSubscription(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::TEMP];
    subscribers -= 1;

    if (subscribers == 0)
    {
        DebugLogger::info("%s: Unsubscribing from temperature", LAUNCHABLE_NAME);
        asyncSubscribe(WB_RES::LOCAL::MEAS_TEMP::ID);
    }
}

void OfflineMeasurements::recordECGSamples(const WB_RES::ECGData& data)
{
    // ECG Samples: 18 bits in registers
    // ENOB is around 15.5, so we can probably discard first two bits by shifting to right

    static int16_t buffer[16];
    size_t samples = data.samples.size();
    ASSERT(samples <= 16);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i] = (data.samples[i] >> 2);
    }

    WB_RES::OfflineECGData ecg;
    ecg.timestamp = data.timestamp;
    ecg.sampleData = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG(), ResponseOptions::ForceAsync, ecg);
}

void OfflineMeasurements::compressECGSamples(const WB_RES::ECGData& data)
{
    // EXPERIMENTAL
    // Calculates deltas and encodes them using Elias Gamma coding with bijection
    // This has potential compression ratio of around 60%, but it might also
    // consume a lot more data if the signal is noisy.

    constexpr uint8_t BLOCK_SIZE = 32;
    static DeltaCompression<int16_t, BLOCK_SIZE> compressor;
    static int32_t sample_offset = 0;

    static int16_t buffer[16];
    size_t samples = data.samples.size();
    ASSERT(samples <= 16);
    for (size_t i = 0; i < samples; i++)
    {
        buffer[i] = (data.samples[i] >> 2); // Discard 2 LSBs
    }

    // Callback to write blocks as they get completed
    static auto onWrite = [&](uint8_t block[BLOCK_SIZE]) {
        uint8_t blockSamples = block[0];
        uint16_t sampleRate = m_state.measurements[WB_RES::OfflineMeasurement::ECG];
        int32_t offset = int32_t((1000.0f / sampleRate) * sample_offset);

        WB_RES::OfflineECGCompressedData ecg;
        ecg.timestamp = data.timestamp + offset;
        ecg.bytes = wb::MakeArray(block, BLOCK_SIZE);
        updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED(), ResponseOptions::ForceAsync, ecg);

        sample_offset += blockSamples;
        };

    size_t compressed = compressor.pack_continuous(wb::MakeArray(buffer), onWrite);
    ASSERT(compressed == samples);

    sample_offset -= samples;
}

void OfflineMeasurements::recordHRAverages(const WB_RES::HRData& data)
{
    static uint8_t last = 0;
    uint8_t average = static_cast<uint8_t>(roundf(data.average));

    if (last != average)
        return;
    last = average;

    WB_RES::OfflineHRData hr;
    hr.timestamp = WbTimestampGet();
    hr.average = average;

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_HR(), ResponseOptions::ForceAsync, hr);
}

void OfflineMeasurements::recordRRIntervals(const WB_RES::HRData& data)
{
    // RTOR samples: Bit-packed chunk of 12-bit values
    // 8 x 12 bits = 96 bits = 8 bytes
    constexpr uint8_t sampleBits = 12;
    constexpr uint8_t chunkSamples = 8;
    constexpr uint8_t bufferSize = sampleBits * chunkSamples / 8;

    static uint8_t buffer[bufferSize] = {};
    static uint8_t index = 0;
    static uint32_t timestamp = 0;

    for (size_t i = 0; i < data.rrData.size(); i++)
    {
        if (index == 0) // Update timestamp on first sample
            timestamp = WbTimestampGet();

        uint16_t sample = data.rrData[i];
        bit_pack::write<uint16_t, sampleBits, chunkSamples>(sample, buffer, index);
        index += 1;

        if (index == chunkSamples)
        {
            WB_RES::OfflineRRData rr;
            rr.timestamp = timestamp;
            rr.intervalData = wb::MakeArray(buffer, bufferSize);

            updateResource(WB_RES::LOCAL::OFFLINE_MEAS_RR(), ResponseOptions::ForceAsync, rr);
            index = 0;
        }
    }
}

void OfflineMeasurements::recordAccelerationSamples(const WB_RES::AccData& data)
{
    static WB_RES::Vec3_Q12_12 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayAcc.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_Q12_12(data.arrayAcc[i].x);
        buffer[i].y = float_to_fixed_point_Q12_12(data.arrayAcc[i].y);
        buffer[i].z = float_to_fixed_point_Q12_12(data.arrayAcc[i].z);
    }

    WB_RES::OfflineAccData acc;
    acc.timestamp = data.timestamp;
    acc.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ACC(), ResponseOptions::ForceAsync, acc);
}

void OfflineMeasurements::recordGyroscopeSamples(const WB_RES::GyroData& data)
{
    static WB_RES::Vec3_Q12_12 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayGyro.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_Q12_12(data.arrayGyro[i].x);
        buffer[i].y = float_to_fixed_point_Q12_12(data.arrayGyro[i].y);
        buffer[i].z = float_to_fixed_point_Q12_12(data.arrayGyro[i].z);
    }

    WB_RES::OfflineGyroData gyro;
    gyro.timestamp = data.timestamp;
    gyro.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_GYRO(), ResponseOptions::ForceAsync, gyro);
}

void OfflineMeasurements::recordMagnetometerSamples(const WB_RES::MagnData& data)
{
    static WB_RES::Vec3_Q10_6 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayMagn.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_Q10_6(data.arrayMagn[i].x);
        buffer[i].y = float_to_fixed_point_Q10_6(data.arrayMagn[i].y);
        buffer[i].z = float_to_fixed_point_Q10_6(data.arrayMagn[i].z);
    }

    WB_RES::OfflineMagnData magn;
    magn.timestamp = data.timestamp;
    magn.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_MAGN(), ResponseOptions::ForceAsync, magn);
}

void OfflineMeasurements::recordTemperatureSamples(const WB_RES::TemperatureValue& data)
{
    static int8_t last = 0;
    int8_t as_c = (int8_t)CLAMP(data.measurement - 273.15f, INT8_MIN, INT8_MAX);

    if (as_c == last) // Temperature has not changed enough. Ignore.
        return;
    last = as_c;

    WB_RES::OfflineTempData temp;
    temp.timestamp = data.timestamp;
    temp.measurement = as_c;

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_TEMP(), ResponseOptions::ForceAsync, temp);
}

void OfflineMeasurements::recordActivity(const WB_RES::AccData& data)
{
    constexpr uint32_t ACTIVITY_INTERVAL = 60000;

    static uint32_t activity_start = data.timestamp;
    static uint32_t acc_count = 0;
    static float accumulated_average = 0;

    float total_len = 0.0f;
    size_t count = data.arrayAcc.size();
    for (size_t i = 0; i < count; i++)
    {
        const auto& s = data.arrayAcc[i];

        // Remove gravity, we do not care about the orientation, 
        // only the lengths of components and total acceleration
        wb::FloatVector3D v(s.x, s.y, fabs(s.z) - 9.81);
        total_len += v.length<float>();
    }
    float avg_len = total_len / count;

    accumulated_average += avg_len;
    acc_count += 1;

    uint32_t timediff = data.timestamp - activity_start;
    if (timediff >= ACTIVITY_INTERVAL)
    {
        WB_RES::OfflineActivityData activityData;
        activityData.timestamp = data.timestamp;
        activityData.activity = float_to_fixed_point_Q10_6(accumulated_average * (1.0f / acc_count));

        updateResource(
            WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY(),
            ResponseOptions::ForceAsync, activityData);

        accumulated_average = 0;
        acc_count = 0;
        activity_start = data.timestamp;
    }
}

void OfflineMeasurements::tapDetection(const WB_RES::AccData& data)
{
    constexpr uint32_t THRESHOLD = 20; // ~2g threshold
    constexpr uint32_t LATENCY = 80; // ms, should work with the lowest sample rate
    constexpr uint32_t TIMEOUT = 2000;

    static uint8_t tapCount = 0;
    static uint32_t tapStart = 0;
    static uint32_t lastTimestamp = data.timestamp;
    static uint32_t sampleCount = data.arrayAcc.size();
    uint32_t diffTime = data.timestamp - lastTimestamp;

    if (diffTime == 0)
        return;

    float dt = diffTime / (float)sampleCount; // delta between samples

    lastTimestamp = data.timestamp;
    sampleCount = data.arrayAcc.size();

    static float z_prev = data.arrayAcc[0].z;
    static float z_base = 0.0f; // Baseline before trigger
    static float t_rise = 0.0f; // t when triggered

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        float t = data.timestamp + i * dt;
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
            WB_RES::OfflineTapData tapData;
            tapData.timestamp = data.timestamp;
            tapData.count = tapCount;

            updateResource(
                WB_RES::LOCAL::OFFLINE_MEAS_TAP(),
                ResponseOptions::ForceAsync, tapData);
        }
        tapCount = 0;
        tapStart = 0;
    }
}

void OfflineMeasurements::shakeDetection(const WB_RES::AccData& data)
{
    // TODO
}

uint16_t OfflineMeasurements::getSubbedAccSampleRate()
{
    uint16_t acc = m_state.measurements[WB_RES::OfflineMeasurement::ACC];

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC] > 0)
        return acc > 0 ? acc : DEFAULT_ACC_SAMPLE_RATE;

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::TAP] > 0)
        return DEFAULT_TAP_DETECTION_ACC_SAMPLE_RATE;

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY] > 0)
        return DEFAULT_ACTIGRAPHY_ACC_SAMPLE_RATE;

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::SHAKE] > 0)
        return DEFAULT_SHAKE_DETECTION_ACC_SAMPLE_RATE;

    return 0;
}

