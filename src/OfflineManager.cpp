#include "movesense.h"

#include "OfflineManager.hpp"
#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include "app-resources/resources.h"
#include "system_debug/resources.h"
#include "system_settings/resources.h"
#include "component_eeprom/resources.h"
#include "comm_ble/resources.h"
#include "ui_ind/resources.h"

const char* const OfflineManager::LAUNCHABLE_NAME = "OfflineMan";
constexpr uint32_t EEPROM_CONFIG_ADDR = 0;
constexpr uint8_t EEPROM_CONFIG_INIT_MAGIC = 0xA0;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_CONFIG::LID,
    WB_RES::LOCAL::OFFLINE_STATE::LID
};

OfflineManager::OfflineManager()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , LaunchableModule(LAUNCHABLE_NAME, WB_RES::LOCAL::OFFLINE_CONFIG::EXECUTION_CONTEXT)
    , _state(WB_RES::OfflineState::INIT)
    , _config({})
    , _connections(0)
{
}

OfflineManager::~OfflineManager()
{
}

bool OfflineManager::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK) {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineManager::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineManager::startModule()
{
    // Turn LED on until configured
    asyncPut(WB_RES::LOCAL::UI_IND_VISUAL(), AsyncRequestOptions::ForceAsync, WB_RES::VisualIndTypeValues::CONTINUOUS_VISUAL_INDICATION);

    // Setup debugging
    WB_RES::DebugLogConfig logConfig = {
        .minimalLevel = WB_RES::DebugLevel::VERBOSE
    };
    asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG(), AsyncRequestOptions::ForceAsync, logConfig);
    asyncPut(WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON(), AsyncRequestOptions::ForceAsync, true);

    //asyncReadConfigFromEEPROM();

    // Subscribe to BLE peers (Halt offline recording when connected)
    asyncSubscribe(WB_RES::LOCAL::COMM_BLE_PEERS(), AsyncRequestOptions::ForceAsync);

    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineManager::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::COMM_BLE_PEERS());
    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineManager::onGetRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::info("OfflineTracker::onGetRequest() called.");

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, _config);
        break;
    }
    default:
    {
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
    }
}

void OfflineManager::onPutRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::info("OfflineTracker::onPutRequest() called.");

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        const WB_RES::OfflineConfig& conf = WB_RES::LOCAL::OFFLINE_CONFIG::PUT::ParameterListRef(parameters).getConfig();

        if (!validateConfig(conf))
        {
            returnResult(request, wb::HTTP_CODE_BAD_REQUEST);
            return;
        }

        _config = conf;
        asyncSaveConfigToEEPROM();
        onInitialized();

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

void OfflineManager::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onGetResult %d, status: %d", LAUNCHABLE_NAME, resourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            auto data = result.convertTo<const WB_RES::EepromData&>();
            if (data.bytes[0] == EEPROM_CONFIG_INIT_MAGIC)
            {
                memcpy(&_config, &(data.bytes[1]), sizeof(WB_RES::OfflineConfig));
                onInitialized();
            }
            else
            {
                DebugLogger::info("Offline mode is not configured");
            }
        }
        else
        {
            DebugLogger::info("Failed to read EEPROM: %d", resultCode);
        }
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled GET result (%d) for resource: %d", resultCode, resourceId.localResourceId);
        break;
    }
    }
}

void OfflineManager::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::verbose("%s: onPutResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            DebugLogger::info("Offline config saved to EEPROM");
        }
        else
        {
            DebugLogger::error("Failed to save offline config to EEPROM: %d", resultCode);
        }
        break;
    }
    case WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG::LID:
    {
        DebugLogger::info("Debug log config PUT result: %d", resultCode);
        break;
    }
    case WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON::LID:
    {
        DebugLogger::info("UartOn PUT result: %d", resultCode);
        break;
    }
    case WB_RES::LOCAL::UI_IND_VISUAL::LID:
    {
        DebugLogger::info("Visual indicator PUT result: %d", resultCode);
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled PUT result (%d) for resource: %d", 
            resultCode, resourceId.localResourceId);
        break;
    }
    }
}

void OfflineManager::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::verbose("%s: onSubscribeResult %d, status: %d", 
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_PEERS::LID:
    {
        if(resultCode == wb::HTTP_CODE_OK)
        {
            DebugLogger::info("Subscribed to BLE peers");
        }
        else
        {
            DebugLogger::error("Failed to subscribe BLE peers: %d", resultCode);
        }
        break;
    }
    default:
    {
        DebugLogger::info("Subscibe result to resource %d: %d", resourceId.localResourceId, resultCode);
        break;
    }
    }
}

void OfflineManager::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onNotify %d", LAUNCHABLE_NAME, resourceId.localResourceId);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_PEERS::LID:
    {
        DebugLogger::info("BLE PeerChange");
        auto peerChange = value.convertTo<const WB_RES::PeerChange&>();

        if (peerChange.state == WB_RES::PeerStateValues::DISCONNECTED)
        {
            _connections--;
        }
        else if (peerChange.peer.handle.hasValue())
        {
            const auto handle = peerChange.peer.handle.getValue();

            WB_RES::ConnParams connParams = {
                .min_conn_interval = 12,
                .max_conn_interval = 36,
                .slave_latency = 2,
                .sup_timeout = 100
            };

            asyncPut(WB_RES::LOCAL::COMM_BLE_PEERS_CONNHANDLE_PARAMS(), AsyncRequestOptions::ForceAsync,
                handle, connParams);

            _connections++;
        }


        if (_connections == 0) {
            startRecording();
        }
        else if (_connections == 1) {
            stopRecording();
        }

        DebugLogger::info("BLE connections: %d", _connections);
        break;
    }
    default:
    {
        DebugLogger::warning("Unhandled notification from resource: %d", resourceId.localResourceId);
        break;
    }
    }
}

void OfflineManager::onTimer(whiteboard::TimerId timerId)
{
    // TODO: Implement periodic tasks if needed
}

void OfflineManager::onInitialized()
{
    DebugLogger::info("Offline mode initialization completed");

    _state = WB_RES::OfflineState::IDLE;
    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::ForceAsync, _state);
}

void OfflineManager::asyncReadConfigFromEEPROM()
{
    uint8_t len = sizeof(WB_RES::OfflineConfig) + 1;
    asyncGet(WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), AsyncRequestOptions::ForceAsync, 0, EEPROM_CONFIG_ADDR, len);
}

void OfflineManager::asyncSaveConfigToEEPROM()
{
    constexpr uint8_t len = sizeof(WB_RES::OfflineConfig);
    uint8_t data[len + 1] = { 0 };
    data[0] = EEPROM_CONFIG_INIT_MAGIC;
    memcpy(&data[1], &_config, len);

    WB_RES::EepromData eepromData = {
        .bytes = wb::MakeArray(data)
    };

    asyncPut(WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), AsyncRequestOptions::ForceAsync, 0, EEPROM_CONFIG_ADDR, eepromData);
}

bool OfflineManager::validateConfig(const WB_RES::OfflineConfig& config)
{
    return true; // TODO: Implement validation
}

void OfflineManager::startRecording()
{
    if (_state != WB_RES::OfflineState::IDLE)
        return;

    DebugLogger::info("Starting recording");

    _state = WB_RES::OfflineState::ACTIVE;
    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::ForceAsync, _state);
}

void OfflineManager::stopRecording()
{
    if (_state != WB_RES::OfflineState::ACTIVE)
        return;

    DebugLogger::info("Stopping recording");

    _state = WB_RES::OfflineState::IDLE;
    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::ForceAsync, _state);
}