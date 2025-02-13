#include "movesense.h"
#include "OfflineMeasurements.hpp"
#include "compression/BitPack.hpp"
#include "compression/DeltaCompression.hpp"
#include "compression/FixedPoint.hpp"
#include "utils/Filters.hpp"

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

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include <functional>

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

const char* const OfflineMeasurements::LAUNCHABLE_NAME = "OfflineMeas";
constexpr uint16_t DEFAULT_ACC_SAMPLE_RATE = 13;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_MEAS_ECG_SAMPLERATE::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_HR::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_RR::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_GYRO_SAMPLERATE::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_MAGN_SAMPLERATE::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_TEMP::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID,
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
    return true;
}

void OfflineMeasurements::stopModule()
{
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

    wb::Result result = wb::HTTP_CODE_NOT_FOUND;

    switch (lid)
    {
    case WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::LID:
    {
        const auto& params = WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::SUBSCRIBE::ParameterListRef(parameters);
        if (subscribeAcc(lid, params.getSampleRate()))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID:
    {
        if (subscribeAcc(lid, DEFAULT_ACC_SAMPLE_RATE))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_GYRO_SAMPLERATE::LID:
    {
        const auto& params = WB_RES::LOCAL::OFFLINE_MEAS_GYRO_SAMPLERATE::SUBSCRIBE::ParameterListRef(parameters);
        if (subscribeGyro(lid, params.getSampleRate()))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_MAGN_SAMPLERATE::LID:
    {
        const auto& params = WB_RES::LOCAL::OFFLINE_MEAS_MAGN_SAMPLERATE::SUBSCRIBE::ParameterListRef(parameters);
        if (subscribeMagn(lid, params.getSampleRate()))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_HR::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_RR::LID:
    {
        subscribeHR(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_SAMPLERATE::LID:
    {
        const auto& params = WB_RES::LOCAL::OFFLINE_MEAS_ECG_SAMPLERATE::SUBSCRIBE::ParameterListRef(parameters);
        if (subscribeECG(lid, params.getSampleRate()))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE::LID:
    {
        const auto& params = WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE::SUBSCRIBE::ParameterListRef(parameters);
        if (subscribeECG(lid, params.getSampleRate()))
            result = wb::HTTP_CODE_OK;
        else
            result = wb::HTTP_CODE_FORBIDDEN;
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

    returnResult(request, result);
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
    case WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID:
    {
        dropAccSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_GYRO_SAMPLERATE::LID:
    {
        dropGyroSubscription(lid);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_MEAS_MAGN_SAMPLERATE::LID:
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
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_SAMPLERATE::LID:
    case WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE::LID:
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
    DebugLogger::info("%s: onGetResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineMeasurements::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPostResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineMeasurements::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPutResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineMeasurements::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::info("%s: onSubscribeResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_HR::LID:
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_TEMP::LID:
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
        if (m_state.subscriberCount[WB_RES::OfflineMeasurement::HR])
            recordHRAverages(data);
        if (m_state.subscriberCount[WB_RES::OfflineMeasurement::RR])
            recordRRIntervals(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::AccData&>();

        if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC])
            recordAccelerationSamples(data);

        if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY])
            recordActivity(data);

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

bool OfflineMeasurements::subscribeAcc(wb::LocalResourceId resourceId, int32_t sampleRate)
{
    uint16_t currentSampleRate = getAccSampleRate();

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::LID)
    {
        if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC] > 0)
            return false; // Only one subscriber allowed at a time            

        m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC] += 1;
        m_state.sampleRates[WB_RES::OfflineMeasurement::ACC] = sampleRate;
    }
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID)
        m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY] += 1;

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

bool OfflineMeasurements::subscribeGyro(wb::LocalResourceId resourceId, int32_t sampleRate)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::GYRO];
    if (subscribers > 0)
        return false; // Only one subscriber allowed at a time

    subscribers += 1;
    m_state.sampleRates[WB_RES::OfflineMeasurement::MAGN] = sampleRate;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Gyro/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }

    return true;
}

bool OfflineMeasurements::subscribeMagn(wb::LocalResourceId resourceId, int32_t sampleRate)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::MAGN];
    if (subscribers > 0)
        return false; // Only one subscriber allowed at a time

    subscribers += 1;
    m_state.sampleRates[WB_RES::OfflineMeasurement::MAGN] = sampleRate;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Magn/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }

    return true;
}

bool OfflineMeasurements::subscribeHR(wb::LocalResourceId resourceId)
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

    return true;
}

bool OfflineMeasurements::subscribeECG(wb::LocalResourceId resourceId, int32_t sampleRate)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::ECG];
    if (subscribers > 0)
        return false; // Allow only one subscriber

    subscribers += 1;
    m_state.sampleRates[WB_RES::OfflineMeasurement::ECG] = sampleRate;
    m_options.useEcgCompression = (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE::LID);

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/ECG/%u", LAUNCHABLE_NAME, sampleRate);
        asyncSubscribe(
            WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE(), AsyncRequestOptions::Empty, sampleRate);
    }
    return true;
}

bool OfflineMeasurements::subscribeTemp(wb::LocalResourceId resourceId)
{
    auto& subscribers = m_state.subscriberCount[WB_RES::OfflineMeasurement::TEMP];
    subscribers += 1;

    if (subscribers == 1)
    {
        DebugLogger::info("%s: Subscribing to /Meas/Temp", LAUNCHABLE_NAME);
        asyncSubscribe(WB_RES::LOCAL::MEAS_TEMP(), AsyncRequestOptions::Empty);
    }

    return true;
}

void OfflineMeasurements::dropAccSubscription(wb::LocalResourceId resourceId)
{
    auto& accSubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC];
    auto& activitySubs = m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY];

    uint16_t currentSampleRate = getAccSampleRate();

    if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE::LID && accSubs > 0)
        accSubs -= 1;
    else if (resourceId == WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY::LID && activitySubs > 0)
        activitySubs -= 1;

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

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG_SAMPLERATE(), ResponseOptions::ForceAsync, ecg);
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
        uint16_t sampleRate = m_state.sampleRates[WB_RES::OfflineMeasurement::ECG];
        int32_t offset = int32_t((1000.0f / sampleRate) * sample_offset);

        WB_RES::OfflineECGCompressedData ecg;
        ecg.timestamp = data.timestamp + offset;
        ecg.bytes = wb::MakeArray(block, BLOCK_SIZE);
        updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED_SAMPLERATE(), ResponseOptions::ForceAsync, ecg);

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

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ACC_SAMPLERATE(), ResponseOptions::ForceAsync, acc);
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

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_GYRO_SAMPLERATE(), ResponseOptions::ForceAsync, gyro);
}

void OfflineMeasurements::recordMagnetometerSamples(const WB_RES::MagnData& data)
{
    static WB_RES::Vec3_Q10_6 buffer[8]; // max 8 x (3 x 16-bit) samples
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

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_MAGN_SAMPLERATE(), ResponseOptions::ForceAsync, magn);
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

    // Simple low-pass filter to mostly eliminate 
    // the gravity from acceleration readings
    static LowPassFilter lpf;

    float total_len = 0.0f;
    size_t count = data.arrayAcc.size();
    for (size_t i = 0; i < count; i++)
    {
        const auto& s = data.arrayAcc[i];
        wb::FloatVector3D v = lpf.filter(s);
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
        activityData.activity = static_cast<uint16_t>(accumulated_average * (100.0f / acc_count));

        updateResource(
            WB_RES::LOCAL::OFFLINE_MEAS_ACTIVITY(),
            ResponseOptions::ForceAsync, activityData);

        accumulated_average = 0;
        acc_count = 0;
        activity_start = data.timestamp;
    }
}

uint16_t OfflineMeasurements::getAccSampleRate()
{
    uint16_t acc = m_state.sampleRates[WB_RES::OfflineMeasurement::ACC];

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACC] > 0)
        return acc > 0 ? acc : DEFAULT_ACC_SAMPLE_RATE;

    if (m_state.subscriberCount[WB_RES::OfflineMeasurement::ACTIVITY] > 0)
        return DEFAULT_ACC_SAMPLE_RATE;

    return 0;
}

