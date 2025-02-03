#include "movesense.h"
#include "OfflineLogger.hpp"
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

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include <functional>

#define CLAMP(x, min, max) (x < min ? min : (x > max ? max : x))

const char* const OfflineLogger::LAUNCHABLE_NAME = "OfflineLog";
constexpr uint16_t DEFAULT_ACC_SAMPLE_RATE = 13;
constexpr uint8_t ECG_COMPRESSION_BLOCKSIZE = 32;

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
};

OfflineLogger::OfflineLogger()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , _configured(false)
    , _logging(false)
    , _options({})
{
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        _loggedResource[i] = false;
        _measurements[i] = {};
    }
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
        if (_options.useEcgCompression)
            compressECGSamples(data);
        else
            recordECGSamples(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_HR::LID:
    {
        auto data = value.convertTo<const WB_RES::HRData&>();
        if (_loggedResource[WB_RES::OfflineMeasurement::HR])
            recordHRAverages(data);
        if (_loggedResource[WB_RES::OfflineMeasurement::RR])
            recordRRIntervals(data);
        break;
    }
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    {
        auto data = value.convertTo<const WB_RES::AccData&>();

        if (_loggedResource[WB_RES::OfflineMeasurement::ACC])
            recordAccelerationSamples(data);

        if (_loggedResource[WB_RES::OfflineMeasurement::ACTIVITY])
            recordActivity(data);

        if (_loggedResource[WB_RES::OfflineMeasurement::TAP])
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

void OfflineLogger::applyConfig(const WB_RES::OfflineConfig& config)
{
    ASSERT(!_logging && !_configured);
    bool valid_measurements = configureMeasurements(config);
    if (!valid_measurements)
    {
        asyncPut(
            WB_RES::LOCAL::OFFLINE_STATE(),
            AsyncRequestOptions::Empty,
            WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        return;
    }

    _options = {
        .useEcgCompression = !!(config.options & WB_RES::OfflineOptionsFlags::COMPRESSECGSAMPLES)
    };

    configureDataLogger(config);
}

bool OfflineLogger::configureMeasurements(const WB_RES::OfflineConfig& config)
{
    uint8_t count = 0;
    static wb::ResourceId resourceIds[WB_RES::OfflineMeasurement::COUNT] = {
        WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::ID,
        WB_RES::LOCAL::MEAS_HR::ID,
        wb::ID_INVALID_RESOURCE, // RR intervals use HR measurements
        WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID,
        WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::ID,
        WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::ID,
        WB_RES::LOCAL::MEAS_TEMP::ID,
        wb::ID_INVALID_RESOURCE, // Activity is based on acceleration, handle as special case
        wb::ID_INVALID_RESOURCE, // Tap detection is based on acceleration, handle as special case
    };

    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        _loggedResource[i] = false;
        _measurements[i] = {};
    }

    for (auto i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        if (config.sampleRates[i]) // Non-zero value indicates active measurement
        {
            if (count > MAX_MEASUREMENT_SUBSCRIPTIONS)
                return false;

            if (resourceIds[i] != wb::ID_INVALID_RESOURCE)
            {
                _measurements[count].resourceId = resourceIds[i];
                _measurements[count].sampleRate = config.sampleRates[i];
                count++;
            }
            else // Handle special measurements
            {
                if (i == WB_RES::OfflineMeasurement::RR)
                {
                    if (config.sampleRates[WB_RES::OfflineMeasurement::HR] == 0)
                    {
                        _measurements[count].resourceId = resourceIds[WB_RES::OfflineMeasurement::HR];
                        _measurements[count].sampleRate = 1;
                        count++;
                    }
                }
                else if (i == WB_RES::OfflineMeasurement::ACTIVITY)
                {
                    if (config.sampleRates[WB_RES::OfflineMeasurement::ACC] == 0)
                    {
                        _measurements[count].resourceId = resourceIds[WB_RES::OfflineMeasurement::ACC];
                        _measurements[count].sampleRate = DEFAULT_ACC_SAMPLE_RATE;
                        count++;
                    }
                }
                else if (i == WB_RES::OfflineMeasurement::TAP)
                {
                    if (config.sampleRates[WB_RES::OfflineMeasurement::ACC] == 0 &&
                        config.sampleRates[WB_RES::OfflineMeasurement::ACTIVITY] == 0)
                    {
                        _measurements[count].resourceId = resourceIds[WB_RES::OfflineMeasurement::ACC];
                        _measurements[count].sampleRate = DEFAULT_ACC_SAMPLE_RATE;
                        count++;
                    }
                }
            }
        }
    }

    return true;
}

uint8_t OfflineLogger::configureDataLogger(const WB_RES::OfflineConfig& config)
{
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
    };
    WB_RES::DataEntry entries[WB_RES::OfflineMeasurement::COUNT] = {};

    for (auto i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        _loggedResource[i] = (config.sampleRates[i] > 0);
        if (_loggedResource[i])
        {
            if (i == WB_RES::OfflineMeasurement::ECG && _options.useEcgCompression)
            {
                entries[count].path = "/Offline/Meas/ECG/Compressed";
            }
            else
            {
                entries[count].path = paths[i];
            }
            count++;
        }
    }

    WB_RES::DataLoggerConfig logConfig = {};
    logConfig.dataEntries.dataEntry = wb::MakeArray(entries, count);

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(),
        AsyncRequestOptions::Empty,
        logConfig);

    return count;
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

void OfflineLogger::compressECGSamples(const WB_RES::ECGData& data)
{
    // EXPERIMENTAL
    // Calculates deltas and encodes them using Elias Gamma coding with bijection
    // This has potential compression ratio of around 60%, but it might also
    // consume a lot more data if the signal is noisy.

    static DeltaCompression<int16_t, ECG_COMPRESSION_BLOCKSIZE> compressor;

    // Need to track timestamps separately since the blocks are not in sync
    // with incoming data
    static WbTime start = WbTimeGet();

    static int16_t buffer[16];
    size_t samples = data.samples.size();
    ASSERT(samples <= 16);
    for (size_t i = 0; i < samples; i++)
    {
        buffer[i] = (data.samples[i] >> 2); // Discard 2 LSBs
    }

    // Callback to write blocks as they get completed
    static auto onWrite = [&](uint8_t block[ECG_COMPRESSION_BLOCKSIZE]) {
        WB_RES::OfflineECGCompressedData ecg;
        ecg.timestamp = (WbTimeGet() - start) / 1000;
        ecg.bytes = wb::MakeArray(block, ECG_COMPRESSION_BLOCKSIZE);
        updateResource(WB_RES::LOCAL::OFFLINE_MEAS_ECG_COMPRESSED(), ResponseOptions::ForceAsync, ecg);
        };

    size_t compressed = compressor.pack_continuous(wb::MakeArray(buffer), onWrite);
    ASSERT(compressed == samples);
}

void OfflineLogger::recordHRAverages(const WB_RES::HRData& data)
{
    WB_RES::OfflineHRData hr;
    hr.average = static_cast<uint8_t>(roundf(data.average));
    updateResource(WB_RES::LOCAL::OFFLINE_MEAS_HR(), ResponseOptions::ForceAsync, hr);
}

void OfflineLogger::recordRRIntervals(const WB_RES::HRData& data)
{
    // RTOR samples: Bit-packed chunk of 12-bit values
    // 4 x 12 bits = 48 bits = 6 bytes
    constexpr uint8_t sampleBits = 12;
    constexpr uint8_t chunkSamples = 4;
    constexpr uint8_t bufferSize = sampleBits * chunkSamples / 8;

    static uint8_t buffer[bufferSize] = {};
    static uint8_t index = 0;

    for (size_t i = 0; i < data.rrData.size(); i++)
    {
        uint16_t sample = data.rrData[i];
        bit_pack::write<uint16_t, sampleBits, chunkSamples>(sample, buffer, index);
        index += 1;

        if (index == chunkSamples)
        {
            WB_RES::OfflineRRData rr;
            rr.intervalData = wb::MakeArray(buffer, bufferSize);

            updateResource(WB_RES::LOCAL::OFFLINE_MEAS_RR(), ResponseOptions::ForceAsync, rr);
            index = 0;
        }
    }
}

void OfflineLogger::recordAccelerationSamples(const WB_RES::AccData& data)
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

void OfflineLogger::recordGyroscopeSamples(const WB_RES::GyroData& data)
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

void OfflineLogger::recordMagnetometerSamples(const WB_RES::MagnData& data)
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

void OfflineLogger::recordActivity(const WB_RES::AccData& data)
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

void OfflineLogger::tapDetection(const WB_RES::AccData& data)
{
    constexpr float TAP_DETECTION_THRESHOLD = 12.0f;

    static uint8_t tap_count = 0;
    static uint32_t tap_timestamp = 0;
    static float tap_magnitude = 0.0f;

    static struct {
        wb::FloatVector3D vec = {};
        float len = INFINITY;
    } min;

    static struct {
        wb::FloatVector3D vec = {};
        float len = 0.0f;
    } max;

    for (size_t i = 0; i < data.arrayAcc.size(); i++)
    {
        float len = data.arrayAcc[i].length<float>();
        if (len < min.len)
        {
            min.len = len;
            min.vec = data.arrayAcc[i];
        }

        if (len > max.len)
        {
            max.len = len;
            max.vec = data.arrayAcc[i];
        }
    }

    float magnitude = max.vec.distance<float>(min.vec);
    if (magnitude > TAP_DETECTION_THRESHOLD)
    {
        tap_count += 1;
        tap_timestamp = data.timestamp;
        tap_magnitude += magnitude;
        min = {}; max = {};

        DebugLogger::info("%s: Tap detected, count %u", LAUNCHABLE_NAME, tap_count);
    }

    // No new taps within a couple seconds
    if (tap_count > 0 && data.timestamp - tap_timestamp > 2000)
    {
        if (tap_count > 1) // Ignore single taps
        {
            WB_RES::OfflineTapData tap;
            tap.timestamp = data.timestamp;
            tap.magnitude = float_to_fixed_point_Q10_6(tap_magnitude / tap_count);
            tap.count = tap_count;

            updateResource(
                WB_RES::LOCAL::OFFLINE_MEAS_TAP(),
                ResponseOptions::ForceAsync, tap);

            tap_timestamp = 0;
        }

        tap_count = 0;
        tap_magnitude = 0.0f;
        min = {}; max = {};
    }
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
    for (size_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        auto& entry = _measurements[i];
        if (entry.resourceId != wb::ID_INVALID_RESOURCE && !entry.subscribed)
        {
            DebugLogger::info("%s: Subscribing to measurement[%d]: %d",
                LAUNCHABLE_NAME, i, entry.resourceId.value);

            if (entry.sampleRate > 1)
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