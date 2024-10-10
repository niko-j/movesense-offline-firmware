#include "movesense.h"

#include "OfflineManager.hpp"
#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

#include "app-resources/resources.h"
#include "system_debug/resources.h"
#include "system_settings/resources.h"
#include "system_mode/resources.h"
#include "system_states/resources.h"
#include "component_eeprom/resources.h"
#include "component_max3000x/resources.h"
#include "component_lsm6ds3/resources.h"
#include "component_led/resources.h"
#include "comm_ble/resources.h"

const char* const OfflineManager::LAUNCHABLE_NAME = "OfflineMan";

constexpr uint8_t EEPROM_CONFIG_INDEX = 0;
constexpr uint32_t EEPROM_CONFIG_ADDR = 0;
constexpr uint8_t EEPROM_CONFIG_INIT_MAGIC = 0xA0;

constexpr size_t TIMER_TICK_SLEEP = 1000;
constexpr size_t TIMER_TICK_LED = 250;

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
    , _sleepTimer(wb::ID_INVALID_TIMER)
    , _sleepTimerElapsed(0)
    , _ledTimer(wb::ID_INVALID_TIMER)
    , _ledTimerElapsed(0)
    , _ledBlinks(0)
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
    // Setup debugging
    WB_RES::DebugLogConfig logConfig = {
        .minimalLevel = WB_RES::DebugLevel::VERBOSE
    };
    asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG(), AsyncRequestOptions::Empty, logConfig);
    asyncPut(WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON(), AsyncRequestOptions::Empty, true);

    asyncReadConfigFromEEPROM();

    // Subscribe to BLE peers (Halt offline recording when connected)
    asyncSubscribe(WB_RES::LOCAL::COMM_BLE_PEERS(), AsyncRequestOptions::ForceAsync);

    // Subscribe to system states
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 0); // Movement
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 1); // Battery low
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 2); // Connectors
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 3); // Double tap
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 4); // Single tap

    // Setup timers
    _sleepTimer = ResourceClient::startTimer(TIMER_TICK_SLEEP, true);
    _ledTimer = ResourceClient::startTimer(TIMER_TICK_LED, true);

    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineManager::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::COMM_BLE_PEERS());
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 0); // Movement
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 1); // Battery low
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 2); // Connectors
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 3); // Double tap
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 4); // Single tap
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
        WB_RES::OfflineConfig data = {
            .wakeUpBehavior = _config.wakeUpBehavior,
            .sampleRates = wb::MakeArray(_config.sampleRates),
            .sleepDelay = _config.sleepDelay
        };
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, data);
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
        if (applyConfig(conf))
        {
            asyncSaveConfigToEEPROM();
            returnResult(request, wb::HTTP_CODE_OK);
        }
        else
        {
            returnResult(request, wb::HTTP_CODE_BAD_REQUEST);
        }

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
    DebugLogger::verbose("%s: onGetResult %d, status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            auto data = result.convertTo<const WB_RES::EepromData&>();
            if (data.bytes[0] == EEPROM_CONFIG_INIT_MAGIC)
            {
                if (data.bytes.size() != 1 + sizeof(Config))
                {
                    DebugLogger::error("Offline mode is not configured");
                    setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
                    return;
                }

                memcpy(&_config, &data.bytes[1], sizeof(Config));
                DebugLogger::info("Offline mode configuration restored");

                setState(WB_RES::OfflineState::ACTIVE);
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
        DebugLogger::warning("Unhandled GET result (%d) for resource: %d",
            resultCode, resourceId.localResourceId);
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
        if (resultCode == wb::HTTP_CODE_OK)
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
        auto peerChange = value.convertTo<const WB_RES::PeerChange&>();
        handleBlePeerChange(peerChange);
        break;
    }
    case WB_RES::LOCAL::SYSTEM_STATES_STATEID::LID:
    {
        auto stateChange = value.convertTo<const WB_RES::StateChange&>();
        handleSystemStateChange(stateChange);
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
    if (timerId == _sleepTimer)
    {
        sleepTimerTick();
        return;
    }

    if (timerId == _ledTimer)
    {
        ledTimerTick();
        return;
    }
}

void OfflineManager::asyncReadConfigFromEEPROM()
{
    asyncGet(
        WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), 
        AsyncRequestOptions::ForceAsync,
        EEPROM_CONFIG_INDEX, EEPROM_CONFIG_ADDR, 1 + sizeof(_config));
}

void OfflineManager::asyncSaveConfigToEEPROM()
{
    // Guarding that we don't end up in an endless loop.
    {
        static uint16_t failsave = 0;
        failsave++;
        ASSERT(failsave < 1000);
    }

    uint8_t data[1 + sizeof(_config)] = {};
    data[0] = EEPROM_CONFIG_INIT_MAGIC;
    memcpy(data + 1, &_config, sizeof(_config));

    WB_RES::EepromData eepromData = { .bytes = wb::MakeArray(data) };

    asyncPut(WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), AsyncRequestOptions::ForceAsync,
        EEPROM_CONFIG_INDEX, EEPROM_CONFIG_ADDR, eepromData);
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

bool OfflineManager::applyConfig(const WB_RES::OfflineConfig& config)
{
    if (!validateConfig(config))
        return false;

    _config = {
        .sampleRates = {},
        .wakeUpBehavior = config.wakeUpBehavior,
        .sleepDelay = config.sleepDelay,
    };
    memcpy(&_config.sampleRates, &config.sampleRates[0], sizeof(_config.sampleRates));

    return true;
}

bool OfflineManager::validateConfig(const WB_RES::OfflineConfig& config)
{
    // TODO: Add more validations
    bool sampleRatesDefined = config.sampleRates.size() == WB_RES::MeasurementSensors::COUNT;
    return (sampleRatesDefined);
}

void OfflineManager::enterSleep()
{
    // Configure wake up triggers
    switch (_config.wakeUpBehavior)
    {
    case WB_RES::WakeUpBehavior::HR:
    {
        asyncPut(WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP(), AsyncRequestOptions::Empty, 1);
        break;
    }
    case WB_RES::WakeUpBehavior::MOVEMENT:
    {
        WB_RES::WakeUpState wakeup = { .state = 1, .level = 16 }; // Level = movement threshold [0,63]
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    case WB_RES::WakeUpBehavior::DOUBLETAPON:
    case WB_RES::WakeUpBehavior::DOUBLETAPONOFF:
    {
        WB_RES::WakeUpState wakeup = { .state = 2, .level = 5 }; // Level = delay between taps [0,7]
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    case WB_RES::WakeUpBehavior::SINGLETAPON:
    case WB_RES::WakeUpBehavior::SINGLETAPONOFF:
    {
        WB_RES::WakeUpState wakeup = { .state = 3, .level = 0 };
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    default:
    case WB_RES::WakeUpBehavior::ALWAYSON:
    {
        DebugLogger::error("Configured wake up behavior does not permit sleeping");
        _sleepTimerElapsed = 0;
        return;
    }
    }

    DebugLogger::info("Full power off");
    asyncPut(WB_RES::LOCAL::SYSTEM_MODE(), AsyncRequestOptions::ForceAsync, WB_RES::SystemModeValues::FULLPOWEROFF);
}

void OfflineManager::setState(WB_RES::OfflineState state)
{
    _state = state;
    _ledTimerElapsed = 0;
    _sleepTimerElapsed = 0;

    DebugLogger::info("STATE -> %u", _state.getValue());

    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::Empty, _state);
}

void OfflineManager::sleepTimerTick()
{
    _sleepTimerElapsed += TIMER_TICK_SLEEP;

    switch (_state)
    {
    case WB_RES::OfflineState::ACTIVE:
    {
        if (_config.sleepDelay > 0 && _sleepTimerElapsed >= (_config.sleepDelay * 1000))
            enterSleep();
        break;
    }
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
    case WB_RES::OfflineState::ERROR_STORAGE_FULL:
    case WB_RES::OfflineState::ERROR_BATTERY_LOW:
    {
        if (_sleepTimerElapsed >= 30000)
            enterSleep();
        break;
    }
    default:
    {
        _sleepTimerElapsed = 0;
        break;
    }
    }
}

void OfflineManager::ledTimerTick()
{
    // LED indication intervals, starting with OFF time, then ON, then OFF...
    constexpr uint16_t LED_BLINK_SERIES_INIT[] = { 250, 250 }; // Rapid blinking
    constexpr uint16_t LED_BLINK_SERIES_POWER_ON[] = { 5000, 250 }; // Single blink every 5 seconds
    constexpr uint16_t LED_BLINK_SERIES_FULL_STORAGE[] = { 2000, 250 }; // Single blink every 2 seconds
    constexpr uint16_t LED_BLINK_SERIES_ERROR[] = { 2000, 250, 500, 250 }; // Two rapid blinks every 2 seconds
    constexpr uint16_t LED_BLINK_SERIES_LOW_BATTERY[] = { 1000, 1000 }; // Long blink every other second

    _ledTimerElapsed += TIMER_TICK_LED;
    bool ledOn = _ledBlinks % 2;
    uint16_t nextTimeout = 0;

    switch (_state)
    {
    case WB_RES::OfflineState::INIT:
        nextTimeout = LED_BLINK_SERIES_INIT[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::IDLE:
    case WB_RES::OfflineState::ACTIVE:
        nextTimeout = LED_BLINK_SERIES_POWER_ON[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
        nextTimeout = LED_BLINK_SERIES_ERROR[_ledBlinks % 4]; break;
    case WB_RES::OfflineState::ERROR_STORAGE_FULL:
        nextTimeout = LED_BLINK_SERIES_FULL_STORAGE[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::ERROR_BATTERY_LOW:
        nextTimeout = LED_BLINK_SERIES_LOW_BATTERY[_ledBlinks % 2]; break;
    }

    if (nextTimeout > 0 && _ledTimerElapsed >= nextTimeout)
    {
        _ledTimerElapsed = 0;
        _ledBlinks++;

        WB_RES::LedState ledState = {};
        ledState.isOn = !ledOn;

        asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
    }
}

void OfflineManager::handleBlePeerChange(const WB_RES::PeerChange& peerChange)
{
    DebugLogger::info("BLE PeerChange");
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

        asyncPut(
            WB_RES::LOCAL::COMM_BLE_PEERS_CONNHANDLE_PARAMS(),
            AsyncRequestOptions::ForceAsync,
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
}

void OfflineManager::handleSystemStateChange(const WB_RES::StateChange& stateChange)
{
    DebugLogger::info("System state change: id (%u) state (%u)", 
        stateChange.stateId.getValue(), stateChange.newState);

    _sleepTimerElapsed = 0; // Any state change resets sleep timer
    
    if(_state == WB_RES::OfflineState::ACTIVE)
    {
        if (_config.wakeUpBehavior == WB_RES::WakeUpBehavior::DOUBLETAPONOFF &&
            stateChange.stateId == WB_RES::StateId::DOUBLETAP &&
            stateChange.newState == 1)
        {
            enterSleep();
            return;
        }

        if (_config.wakeUpBehavior == WB_RES::WakeUpBehavior::SINGLETAPONOFF &&
            stateChange.stateId == WB_RES::StateId::TAP &&
            stateChange.newState == 1)
        {
            enterSleep();
            return;
        }
    }
}
