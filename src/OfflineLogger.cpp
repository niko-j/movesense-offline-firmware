#include "movesense.h"

#include "OfflineLogger.hpp"
#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

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

const char* const OfflineLogger::LAUNCHABLE_NAME = "OfflineLog";

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_DATA::LID
};

OfflineLogger::OfflineLogger()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_DATA::EXECUTION_CONTEXT)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_DATA::EXECUTION_CONTEXT)
    , LaunchableModule(LAUNCHABLE_NAME, WB_RES::LOCAL::OFFLINE_DATA::EXECUTION_CONTEXT)
    , _isLogging(false)
{
    for (uint8_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        _measurements[i] = wb::ID_INVALID_RESOURCE;
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

    // Setup DataLogger to receive blocks from /Offline/Data resource
    WB_RES::DataEntry entry = { .path = "/Offline/Data" };
    WB_RES::DataLoggerConfig config = {};
    config.dataEntries.dataEntry = wb::MakeArray<WB_RES::DataEntry>(&entry, 1);

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(), 
        AsyncRequestOptions::Empty,
        config);
    
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
    DebugLogger::verbose("%s: onGetRequest()", LAUNCHABLE_NAME);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    default:
    {
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
    }
}

void OfflineLogger::onPostRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onPostRequest()", LAUNCHABLE_NAME);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_DATA::LID:
    {
        // TODO: Remove this API after done testing data logging
        const auto& data = WB_RES::LOCAL::OFFLINE_DATA::POST::ParameterListRef(parameters).getData();
        storeDataBlock(data);
        returnResult(request, wb::HTTP_CODE_OK);
        break;
    }
    default:
    {
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
    }
}

void OfflineLogger::onDeleteRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onDeleteRequest()", LAUNCHABLE_NAME);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_DATA::LID:
    {
        if(eraseData())
            returnResult(request, wb::HTTP_CODE_OK);
        else
            returnResult(request, wb::HTTP_CODE_BAD_REQUEST);
        break;
    }
    default:
    {
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
    }
}

void OfflineLogger::onSubscribe(
    const whiteboard::Request& request, 
    const whiteboard::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onSubscribe()", LAUNCHABLE_NAME);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    default:
    {
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
    }
}

void OfflineLogger::onUnsubscribe(
    const whiteboard::Request& request, 
    const whiteboard::ParameterList& parametes)
{
    DebugLogger::verbose("%s: onUnsubscribe()", LAUNCHABLE_NAME);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    default:
    {
        returnResult(request, wb::HTTP_CODE_OK);
        break;
    }
    }
}

void OfflineLogger::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onGetResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            auto config = result.convertTo<WB_RES::OfflineConfig>();
            startLogging(config);
        }
        else
        {
            DebugLogger::error("Failed to get offline config: %d", resultCode);
        }
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled notification from resource: %d", resourceId.localResourceId);
        break;
    }
    }
}

void OfflineLogger::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onPutResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineLogger::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onPostResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineLogger::onDeleteResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onDeleteResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineLogger::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onSubscribeResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineLogger::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onNotify %d", LAUNCHABLE_NAME, resourceId.localResourceId);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
    {
        auto state = value.convertTo<WB_RES::OfflineState>();

        if (state == WB_RES::OfflineState::ACTIVE && !_isLogging)
        {
            // Get configuration
            asyncGet(WB_RES::LOCAL::OFFLINE_CONFIG(), AsyncRequestOptions::ForceAsync);
        }
        else if (_isLogging)
        {
            stopLogging();
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
    {
        DebugLogger::warning("Unhandled notification from resource: %d", resourceId.localResourceId);
        break;
    }
    }
}

bool OfflineLogger::startLogging(const WB_RES::OfflineConfig& config)
{
    if (_isLogging)
        stopLogging();

    uint8_t i = 0;

    if (config.sampleRates[WB_RES::MeasurementSensors::ECG])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::ID;
        asyncSubscribe(
            _measurements[i], 
            AsyncRequestOptions::Empty, 
            config.sampleRates[WB_RES::MeasurementSensors::ECG]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::HR])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_HR::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::ACC])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::ID;
        asyncSubscribe(
            _measurements[i],
            AsyncRequestOptions::Empty, 
            config.sampleRates[WB_RES::MeasurementSensors::ACC]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::GYRO])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::ID;
        asyncSubscribe(
            _measurements[i], 
            AsyncRequestOptions::Empty, 
            config.sampleRates[WB_RES::MeasurementSensors::GYRO]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::MAGN])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::ID;
        asyncSubscribe(
            _measurements[i], 
            AsyncRequestOptions::Empty, 
            config.sampleRates[WB_RES::MeasurementSensors::MAGN]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::TEMP])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_TEMP::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty);
        i++;
    }

    _isLogging = (i > 0);

    if(_isLogging)
    {
        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(), 
            AsyncRequestOptions::Empty,
            WB_RES::DataLoggerState::DATALOGGER_LOGGING);
    }

    return _isLogging;
}

void OfflineLogger::stopLogging()
{
    if (!_isLogging)
        return;

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_STATE(),
        AsyncRequestOptions::Empty,
        WB_RES::DataLoggerState::DATALOGGER_READY);

    _isLogging = false;

    for (uint8_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        if (_measurements[i] != wb::ID_INVALID_RESOURCE)
            asyncUnsubscribe(_measurements[i]);
        _measurements[i] = wb::ID_INVALID_RESOURCE;
    }
}

void OfflineLogger::recordECGSamples(const WB_RES::ECGData& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block to DataLogger
}

void OfflineLogger::recordHeartRateSamples(const WB_RES::HRData& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block Data Logger
}

void OfflineLogger::recordAccelerationSamples(const WB_RES::AccData& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block Data Logger
}

void OfflineLogger::recordGyroscopeSamples(const WB_RES::GyroData& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block Data Logger
}

void OfflineLogger::recordMagnetometerSamples(const WB_RES::MagnData& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block Data Logger
}

void OfflineLogger::recordTemperatureSamples(const WB_RES::TemperatureValue& data)
{
    // TODO: Process samples and encode it into a block
    // TODO: Send block Data Logger
}

void OfflineLogger::storeDataBlock(const WB_RES::OfflineDataBlock& block)
{
    updateResource(WB_RES::LOCAL::OFFLINE_DATA(), ResponseOptions::Empty, block);
}

bool OfflineLogger::eraseData()
{
    if(_isLogging)
        return false;

    asyncDelete(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES());
    return true;
}
