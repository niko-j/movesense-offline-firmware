#include "movesense.h"
#include "OfflineLogger.hpp"
#include "OfflineTypes.hpp"

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

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const OfflineLogger::LAUNCHABLE_NAME = "OfflineLog";

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_MEAS_ECG::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_HR::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_ACC::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_GYRO::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_MAGN::LID,
    WB_RES::LOCAL::OFFLINE_MEAS_TEMP::LID,
};

OfflineLogger::OfflineLogger()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , _configured(false)
    , _logging(false)
{
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
        _measurements[i] = {};
}

OfflineLogger::~OfflineLogger()
{
}

bool OfflineLogger::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK) {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineLogger::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineLogger::startModule()
{
    asyncSubscribe(WB_RES::LOCAL::OFFLINE_STATE());

    // // Setup DataLogger to record config changes
    // WB_RES::DataEntry entry = { .path = "/Offline/Config" };
    // WB_RES::DataLoggerConfig config = {};
    // config.dataEntries.dataEntry = wb::MakeArray<WB_RES::DataEntry>(&entry, 1);

    // asyncPut(
    //     WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(),
    //     AsyncRequestOptions::Empty,
    //     config);

    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineLogger::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::OFFLINE_STATE());
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineLogger::onGetRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onGetRequest resource %d",
        LAUNCHABLE_NAME, request.getResourceId().localResourceId);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    default:
        DebugLogger::warning("%s: Unimplemented GET for resource %d",
            LAUNCHABLE_NAME, request.getResourceId().localResourceId);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void OfflineLogger::onPostRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onPostRequest resource %d",
        LAUNCHABLE_NAME, request.getResourceId().localResourceId);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    default:
        DebugLogger::warning("%s: Unimplemented POST for resource %d",
            LAUNCHABLE_NAME, request.getResourceId().localResourceId);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void OfflineLogger::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        auto config = result.convertTo<WB_RES::OfflineConfig>();
        applyConfig(config);
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled GET result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineLogger::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_DATALOGGER_STATE::LID:
    {
        DebugLogger::error("%s: /DataLogger/State result %u",
            LAUNCHABLE_NAME, resultCode);

        if (resultCode != wb::HTTP_CODE_OK)
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
        else
        {
            _logging = _configured;
        }
        break;
    }
    case WB_RES::LOCAL::MEM_DATALOGGER_CONFIG::LID:
    {
        DebugLogger::info("%s: /DataLogger/Config result %u",
            LAUNCHABLE_NAME, resultCode);

        switch (resultCode)
        {
        case wb::HTTP_CODE_BAD_REQUEST:
        {
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(),
                AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_INVALID_CONFIG);
            stopLogging();
            break;
        }
        case wb::HTTP_CODE_OK:
        {
            // Config ok, subscribe to resources
            _configured = true;
            if (!startLogging()) // Starts logging if no resources need subscribing
                subscribeResources();
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
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
        break;
    default:
        DebugLogger::verbose("%s: Unhandled PUT result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineLogger::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
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
            DebugLogger::error("%s: Failed to subscribe measurement resource %d, status %d",
                LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

            stopLogging();
            asyncPut(
                WB_RES::LOCAL::OFFLINE_STATE(),
                AsyncRequestOptions::Empty,
                WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        }
        else
        {
            auto* entry = findResourceEntry(resourceId);
            if (entry)
            {
                entry->subscribed = true;
                if (!startLogging()) // attempt to start logging
                    DebugLogger::info("%s: Still waiting all resources to be ready...", LAUNCHABLE_NAME);
            }
            else
            {
                DebugLogger::info("%s: Resource %d subscribed but no longer needed!",
                    LAUNCHABLE_NAME, resourceId.localResourceId);
                asyncUnsubscribe(resourceId); // no longer wanted
            }
        }

        break;
    }
    default:
        DebugLogger::verbose("%s: Unhandled SUBSCRIBE result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }

}

void OfflineLogger::onNotify(
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
        recordECGSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_HR::LID:
    {
        auto data = value.convertTo<const WB_RES::HRData&>();
        recordHeartRateSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::AccData&>();
        recordAccelerationSamples(data);
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

void OfflineLogger::applyConfig(const WB_RES::OfflineConfig& config)
{
    ASSERT(!_logging && !_configured);
    uint8_t i = 0;
    WB_RES::DataEntry entries[6] = {};

    if (config.sampleRates[WB_RES::OfflineMeasurement::ECG])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::ID;
        _measurements[i].sampleRate = config.sampleRates[WB_RES::OfflineMeasurement::ECG];
        entries[i].path = "/Offline/Meas/ECG";
        i++;
    }

    if (config.sampleRates[WB_RES::OfflineMeasurement::HR])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_HR::ID;
        entries[i].path = "/Offline/Meas/HR";
        i++;
    }

    if (config.sampleRates[WB_RES::OfflineMeasurement::ACC])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID;
        _measurements[i].sampleRate = config.sampleRates[WB_RES::OfflineMeasurement::ACC];
        entries[i].path = "/Offline/Meas/Acc";
        i++;
    }

    if (config.sampleRates[WB_RES::OfflineMeasurement::GYRO])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::ID;
        _measurements[i].sampleRate = config.sampleRates[WB_RES::OfflineMeasurement::GYRO];
        entries[i].path = "/Offline/Meas/Gyro";
        i++;
    }

    if (config.sampleRates[WB_RES::OfflineMeasurement::MAGN])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::ID;
        _measurements[i].sampleRate = config.sampleRates[WB_RES::OfflineMeasurement::MAGN];
        entries[i].path = "/Offline/Meas/Magn";
        i++;
    }

    if (config.sampleRates[WB_RES::OfflineMeasurement::TEMP])
    {
        _measurements[i].resourceId = WB_RES::LOCAL::MEAS_TEMP::ID;
        entries[i].path = "/Offline/Meas/Temp";
        i++;
    }

    // entries[i].path = "/Offline/Config"; // Keep recording config changes
    // i++;

    if (i > 4) // too many simulaneous measurements
    {
        asyncPut(
            WB_RES::LOCAL::OFFLINE_STATE(),
            AsyncRequestOptions::Empty,
            WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        return;
    }

    // Setup DataLogger to receive measurements
    WB_RES::DataLoggerConfig logConfig = {};
    logConfig.dataEntries.dataEntry = wb::MakeArray(entries, i);

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(),
        AsyncRequestOptions::Empty,
        logConfig);
}

bool OfflineLogger::startLogging()
{
    if (isSubscribedToResources() && _configured)
    {
        DebugLogger::info("%s: Resources subscribed, starting DataLogger", LAUNCHABLE_NAME);
        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(),
            AsyncRequestOptions::ForceAsync,
            WB_RES::DataLoggerState::DATALOGGER_LOGGING);
        return true;
    }
    return false;
}

void OfflineLogger::stopLogging()
{
    unsubscribeResources();

    if (_logging)
    {
        _logging = false;
        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(),
            AsyncRequestOptions::Empty,
            WB_RES::DataLoggerState::DATALOGGER_READY);
    }
}

void OfflineLogger::recordECGSamples(const WB_RES::ECGData& data)
{
    // ECG Samples: 18 bits in registers (pad to 3 bytes)

    static WB_RES::FixedPoint_S16_8 buffer[16]; // max 16 x 24-bit values
    size_t samples = data.samples.size();
    ASSERT(samples <= 16);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].fraction = (data.samples[i] & 0xFF);
        buffer[i].integer = (data.samples[i] >> 8) & 0xFFFF;
    }

    WB_RES::OfflineECGData ecg;
    ecg.timestamp = data.timestamp;
    ecg.sampleData = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG(), ResponseOptions::ForceAsync, ecg);
}

void OfflineLogger::recordHeartRateSamples(const WB_RES::HRData& data)
{
    // RTOR Samples: 14 bits in registers
    // TODO: RR: Maybe use delta compression?

    // Average as a 8.8 fixed-point:
    // Max: 255.99609375 (HRData.average maximum is 250.0, no need to clamp)
    // Min: 0 (same as HRData.average minimum)
    // Res: 0.00390625

    WB_RES::OfflineHRData hr;
    hr.average = float_to_fixed_point<uint16_t, 8>(data.average);
    hr.rrValues = data.rrData; // max items 60

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_HR(), ResponseOptions::ForceAsync, hr);
}

void OfflineLogger::recordAccelerationSamples(const WB_RES::AccData& data)
{
    // Vector components as signed 16.8 fixed-point values (24 bits per component)
    // TODO: The range should be enough for most cases, but I'd like to hear others' opinions
    // Max: 32767.99609375
    // Min: -32768
    // Res: 0.00390625

    static WB_RES::FixedPointVec3_S16_8 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayAcc.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_S16_8(data.arrayAcc[i].x);
        buffer[i].y = float_to_fixed_point_S16_8(data.arrayAcc[i].y);
        buffer[i].z = float_to_fixed_point_S16_8(data.arrayAcc[i].z);
    }

    WB_RES::OfflineAccData acc;
    acc.timestamp = data.timestamp;
    acc.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ACC(), ResponseOptions::ForceAsync, acc);
}

void OfflineLogger::recordGyroscopeSamples(const WB_RES::GyroData& data)
{
    // Vector components as signed 16.8 fixed-point values (24 bits per component)
    // TODO: The range should be enough for most cases, but I'd like to hear others' opinions
    // Max: 32767.99609375
    // Min: -32768
    // Res: 0.00390625

    static WB_RES::FixedPointVec3_S16_8 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayGyro.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_S16_8(data.arrayGyro[i].x);
        buffer[i].y = float_to_fixed_point_S16_8(data.arrayGyro[i].y);
        buffer[i].z = float_to_fixed_point_S16_8(data.arrayGyro[i].z);
    }

    WB_RES::OfflineGyroData gyro;
    gyro.timestamp = data.timestamp;
    gyro.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_GYRO(), ResponseOptions::ForceAsync, gyro);
}

void OfflineLogger::recordMagnetometerSamples(const WB_RES::MagnData& data)
{
    // Vector components as signed 16.8 fixed-point values (24 bits per component)
    // TODO: The range should be enough for most cases, but I'd like to hear others' opinions
    // Max: 32767.99609375
    // Min: -32768
    // Res: 0.00390625

    static WB_RES::FixedPointVec3_S16_8 buffer[8]; // max 8 x (3 x 24-bit) samples
    size_t samples = data.arrayMagn.size();
    ASSERT(samples <= 8);

    for (size_t i = 0; i < samples; i++)
    {
        buffer[i].x = float_to_fixed_point_S16_8(data.arrayMagn[i].x);
        buffer[i].y = float_to_fixed_point_S16_8(data.arrayMagn[i].y);
        buffer[i].z = float_to_fixed_point_S16_8(data.arrayMagn[i].z);
    }

    WB_RES::OfflineMagnData magn;
    magn.timestamp = data.timestamp;
    magn.measurements = wb::MakeArray(buffer, samples);

    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_MAGN(), ResponseOptions::ForceAsync, magn);
}

void OfflineLogger::recordTemperatureSamples(const WB_RES::TemperatureValue& data)
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

OfflineLogger::ResourceEntry* OfflineLogger::findResourceEntry(wb::ResourceId id)
{
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        if (_measurements[i].resourceId.localResourceId == id.localResourceId)
            return &_measurements[i];
    }
    return nullptr;
}

bool OfflineLogger::isSubscribedToResources() const
{
    bool result = false;
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        if (_measurements[i].resourceId == wb::ID_INVALID_RESOURCE)
            continue;

        result = true;

        if (!_measurements[i].subscribed)
            return false;
    }
    return result;
}

void OfflineLogger::subscribeResources()
{
    DebugLogger::info("%s: Subscribing to all measurements (if any)", LAUNCHABLE_NAME);
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        auto& entry = _measurements[i];
        if (entry.resourceId != wb::ID_INVALID_RESOURCE && !entry.subscribed)
        {
            if (entry.sampleRate)
                asyncSubscribe(entry.resourceId, AsyncRequestOptions::Empty, (int32_t)entry.sampleRate);
            else
                asyncSubscribe(entry.resourceId, AsyncRequestOptions::Empty);
        }
    }
}

void OfflineLogger::unsubscribeResources()
{
    DebugLogger::info("%s: Unsubscribing from all measurements (if any)", LAUNCHABLE_NAME);
    _configured = false;
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        if (_measurements[i].resourceId != wb::ID_INVALID_RESOURCE && _measurements[i].subscribed)
        {
            asyncUnsubscribe(_measurements[i].resourceId);
        }
        _measurements[i] = {};
    }
}