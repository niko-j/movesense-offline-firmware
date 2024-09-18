#include "movesense.h"

#include "OfflineLogger.hpp"
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
    WB_RES::LOCAL::OFFLINE_DATA::LID,
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
    asyncSubscribe(WB_RES::LOCAL::OFFLINE_STATE(), AsyncRequestOptions::ForceAsync);
    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineLogger::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::OFFLINE_STATE(), AsyncRequestOptions::ForceAsync);
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineLogger::onGetRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    // TODO: Handle /Offline/Data streaming
}

void OfflineLogger::onDeleteRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    // TODO: Handle clearing LogBook
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
    DebugLogger::verbose("%s: onNotify %d", LAUNCHABLE_NAME, resourceId);

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
    case WB_RES::LOCAL::MEAS_HR::LID:
    case WB_RES::LOCAL::MEAS_ACC_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::LID:
    case WB_RES::LOCAL::MEAS_TEMP::LID:
    {
        // TODO: Collect samples in a buffer for compression.
        // TODO: Compress blocks and save to DataLogger
        DebugLogger::info("Received sample(s) from resource: %d", resourceId.localResourceId);
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled notification from resource: %d", resourceId.localResourceId);
        break;
    }
    }
}

void OfflineLogger::startLogging(const WB_RES::OfflineConfig& config)
{
    if (_isLogging)
        return;

    _isLogging = true;
    uint8_t i = 0;

    if (config.sampleRates[WB_RES::MeasurementSensors::ECG])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_ECG_REQUIREDSAMPLERATE::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty, config.sampleRates[WB_RES::MeasurementSensors::ECG]);
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
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty, config.sampleRates[WB_RES::MeasurementSensors::ACC]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::GYRO])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_GYRO_SAMPLERATE::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty, config.sampleRates[WB_RES::MeasurementSensors::GYRO]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::MAGN])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_MAGN_SAMPLERATE::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty, config.sampleRates[WB_RES::MeasurementSensors::MAGN]);
        i++;
    }

    if (config.sampleRates[WB_RES::MeasurementSensors::TEMP])
    {
        _measurements[i] = WB_RES::LOCAL::MEAS_TEMP::ID;
        asyncSubscribe(_measurements[i], AsyncRequestOptions::Empty);
        i++;
    }
}

void OfflineLogger::stopLogging()
{
    if (!_isLogging)
        return;

    _isLogging = false;

    for (uint8_t i = 0; i < MAX_MEASUREMENT_SUBSCRIPTIONS; i++)
    {
        if (_measurements[i] != wb::ID_INVALID_RESOURCE)
            asyncUnsubscribe(_measurements[i]);
        _measurements[i] = wb::ID_INVALID_RESOURCE;
    }
}