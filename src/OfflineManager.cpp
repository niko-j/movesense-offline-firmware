#include "movesense.h"
#include "OfflineManager.hpp"
#include "OfflineInternal.hpp"

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
#include "mem_logbook/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const OfflineManager::LAUNCHABLE_NAME = "OfflineMan";

constexpr uint8_t EEPROM_CONFIG_INDEX = 0;
constexpr uint32_t EEPROM_CONFIG_ADDR = 0;
constexpr uint8_t EEPROM_CONFIG_INIT_MAGIC = 0x40; // Change this for breaking changes

constexpr uint32_t TIMER_TICK_SLEEP = 1000;
constexpr uint32_t TIMER_TICK_LED = 250;
constexpr uint32_t TIMER_BLE_ADV_TIMEOUT = 60 * 1000;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_CONFIG::LID,
    WB_RES::LOCAL::OFFLINE_STATE::LID
};

OfflineManager::OfflineManager()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , _state(WB_RES::OfflineState::INIT)
    , _config({})
    , _connections(0)
    , _deviceMoving(true)
    , _shouldReset(false)
    , _sleepTimer(wb::ID_INVALID_TIMER)
    , _sleepTimerElapsed(0)
    , _ledTimer(wb::ID_INVALID_TIMER)
    , _ledTimerElapsed(0)
    , _ledBlinks(0)
    , _advOffTimer(wb::ID_INVALID_TIMER)
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
    asyncSubscribe(WB_RES::LOCAL::COMM_BLE_PEERS(), AsyncRequestOptions::Empty);

    // Subscribe to system states
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 1); // Battery low
    asyncSubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 2); // Connectors

    // Subscribe to logbook IsFull
    asyncSubscribe(WB_RES::LOCAL::MEM_LOGBOOK_ISFULL());

    // Setup timers
    _sleepTimer = ResourceClient::startTimer(TIMER_TICK_SLEEP, true);
    _ledTimer = ResourceClient::startTimer(TIMER_TICK_LED, true);

    mModuleState = WB_RES::ModuleStateValues::STARTED;
    return true;
}

void OfflineManager::stopModule()
{
    asyncUnsubscribe(WB_RES::LOCAL::COMM_BLE_PEERS());

    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 1); // Battery low
    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 2); // Connectors

    if (_config.sleepDelay)
        asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 0); // Movement
    else
        asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty, 3); // Double tap

    mModuleState = WB_RES::ModuleStateValues::STOPPED;
}

void OfflineManager::onGetRequest(
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
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
    {
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, _state);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        WB_RES::OfflineConfig data = internalToWb(_config);
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, data);
        break;
    }
    default:
        DebugLogger::warning("%s: Unimplemented GET for resource %d",
            LAUNCHABLE_NAME, request.getResourceId().localResourceId);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void OfflineManager::onPutRequest(
    const wb::Request& request,
    const wb::ParameterList& parameters)
{
    DebugLogger::verbose("%s: onPutRequest resource %d",
        LAUNCHABLE_NAME, request.getResourceId().localResourceId);

    if (mModuleState != WB_RES::ModuleStateValues::STARTED)
    {
        returnResult(request, wb::HTTP_CODE_SERVICE_UNAVAILABLE);
        return;
    }

    switch (request.getResourceId().localResourceId)
    {
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        const auto& conf = WB_RES::LOCAL::OFFLINE_CONFIG::PUT::ParameterListRef(parameters).getConfig();
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
    case WB_RES::LOCAL::OFFLINE_STATE::LID:
    {
        const auto& state = WB_RES::LOCAL::OFFLINE_STATE::PUT::ParameterListRef(parameters)
            .getState();

        DebugLogger::info("%s: State change request to %u", LAUNCHABLE_NAME, state);
        switch (state)
        {
        case WB_RES::OfflineState::INIT:
        case WB_RES::OfflineState::CONNECTED:
        case WB_RES::OfflineState::RUNNING:
        {
            DebugLogger::warning("%s: State change request refused", LAUNCHABLE_NAME);
            returnResult(request, wb::HTTP_CODE_FORBIDDEN);
            break;
        }
        default: // Basically, only error states are accepted
        {
            setState(state);
            returnResult(request, wb::HTTP_CODE_OK);
            break;
        }
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unimplemented PUT for resource %d",
            LAUNCHABLE_NAME, request.getResourceId().localResourceId);
        returnResult(request, wb::HTTP_CODE_NOT_IMPLEMENTED);
        break;
    }
}

void OfflineManager::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            OfflineConfig conf = {};
            auto data = result.convertTo<const WB_RES::EepromData&>();

            if (data.bytes[0] == EEPROM_CONFIG_INIT_MAGIC)
            {
                ASSERT(data.bytes.size() == 1 + sizeof(OfflineConfig));
                memcpy(&conf, &data.bytes[1], sizeof(OfflineConfig));

                DebugLogger::info("%s: Offline mode configuration restored",
                    LAUNCHABLE_NAME);
            }
            else
            {
                DebugLogger::info("%s: Offline mode is not configured, using defaults",
                    LAUNCHABLE_NAME);
            }

            if (applyConfig(internalToWb(conf)))
                startRecording();
        }
        else
        {
            DebugLogger::info("%s: Failed to read EEPROM: %d", LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled GET result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }

    ASSERT(resultCode < 400)
}

void OfflineManager::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            DebugLogger::info("%s: Offline config saved to EEPROM", LAUNCHABLE_NAME);
        }
        else
        {
            DebugLogger::error("%s: Failed to save offline config to EEPROM: %d",
                LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    case WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG::LID:
    {
        DebugLogger::info("%s: Debug log config PUT result: %d",
            LAUNCHABLE_NAME, resultCode);
        break;
    }
    case WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON::LID:
    {
        DebugLogger::info("%s: UartOn PUT result: %d",
            LAUNCHABLE_NAME, resultCode);
        break;
    }
    case WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP::LID:
    case WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to set wake up for %d, status %d",
                LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        }
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        break;
    }
    case WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to set LED state, status %d",
                LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    case WB_RES::LOCAL::SYSTEM_MODE::LID:
    {
        if (resultCode != wb::HTTP_CODE_ACCEPTED)
        {
            DebugLogger::error("%s: Failed to set system mode, status %d",
                LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled PUT result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineManager::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_PEERS::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            DebugLogger::info("%s: Subscribed to BLE peers", LAUNCHABLE_NAME);
        }
        else
        {
            DebugLogger::error("%s: Failed to subscribe BLE peers: %d", LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    case WB_RES::LOCAL::SYSTEM_STATES_STATEID::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to subscribe to state id, status: %d",
                LAUNCHABLE_NAME, resultCode);
        }
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        break;
    }
    case WB_RES::LOCAL::MEM_LOGBOOK_ISFULL::LID:
    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to subscribe logbook isfull, status: %d",
                LAUNCHABLE_NAME, resultCode);
        }
        ASSERT(resultCode == wb::HTTP_CODE_OK);
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled SUBSCRIBE result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineManager::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::SYSTEM_STATES_STATEID::LID:

    {
        if (resultCode != wb::HTTP_CODE_OK)
        {
            DebugLogger::error("%s: Failed to unsubscribe resource %d status: %d",
                LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        }
        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled UNSUBSCRIBE result - res: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
        break;
    }
}

void OfflineManager::onNotify(
    wb::ResourceId resourceId,
    const wb::Value& value,
    const wb::ParameterList& parameters)
{
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
    case WB_RES::LOCAL::MEM_LOGBOOK_ISFULL::LID:
    {
        auto isFull = value.convertTo<bool>();
        if (isFull)
            setState(WB_RES::OfflineState::ERROR_STORAGE_FULL);

        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
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

    if (timerId == _advOffTimer)
    {
        handleBleAdvTimeout();
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

bool OfflineManager::startRecording()
{
    if (_state.getValue() != WB_RES::OfflineState::CONNECTED &&
        _state.getValue() != WB_RES::OfflineState::INIT &&
        _connections == 0)
    {
        return false;
    }

    if (_advOffTimer == wb::ID_INVALID_TIMER && _config.wakeUpBehavior != OfflineConfig::WakeUpAlwaysOn)
        _advOffTimer = ResourceClient::startTimer(TIMER_BLE_ADV_TIMEOUT, false);

    setState(WB_RES::OfflineState::RUNNING);
    return true;
}

bool OfflineManager::onConnected()
{
    if (_state.getValue() == WB_RES::OfflineState::ERROR_BATTERY_LOW ||
        _state.getValue() == WB_RES::OfflineState::ERROR_SYSTEM_FAILURE ||
        _connections == 0)
    {
        return false;
    }

    if (_advOffTimer != wb::ID_INVALID_TIMER)
    {
        ResourceClient::stopTimer(_advOffTimer);
        _advOffTimer = wb::ID_INVALID_TIMER;
    }

    setState(WB_RES::OfflineState::CONNECTED);
    return true;
}

bool OfflineManager::applyConfig(const WB_RES::OfflineConfig& config)
{
    if (_state.getValue() != WB_RES::OfflineState::INIT &&
        _state.getValue() != WB_RES::OfflineState::CONNECTED &&
        _state.getValue() != WB_RES::OfflineState::ERROR_INVALID_CONFIG)
    {
        return false;
    }

    if (!validateConfig(config))
        return false;

    bool init = _state.getValue() == WB_RES::OfflineState::INIT;
    if (init)
    {
        if (config.sleepDelay > 0)
        {
            // Use movement detection and timer
            asyncSubscribe(
                WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
                WB_RES::StateId::MOVEMENT);
        }
        else
        {
            // double tap to manually enter sleep
            asyncSubscribe(
                WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
                WB_RES::StateId::DOUBLETAP);
        }
    }
    else
    {
        _shouldReset = true; // Always reset when config changed
    }

    _config = wbToInternal(config);
    return true;
}

bool OfflineManager::validateConfig(const WB_RES::OfflineConfig& config)
{
    // TODO: Add more validations
    bool sampleRatesDefined = config.sampleRates.size() == WB_RES::OfflineMeasurement::COUNT;
    return (sampleRatesDefined);
}

void OfflineManager::enterSleep()
{
    DebugLogger::info("%s: enterSleep()", LAUNCHABLE_NAME);

    // Configure wake up triggers
    switch (_config.wakeUpBehavior)
    {
    case WB_RES::OfflineWakeup::CONNECTOR:
    {
        asyncPut(WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP(), AsyncRequestOptions::Empty, 1);
        break;
    }
    case WB_RES::OfflineWakeup::MOVEMENT:
    {
        WB_RES::WakeUpState wakeup = { .state = 1, .level = 1 }; // Level = movement threshold [0,63]
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    case WB_RES::OfflineWakeup::SINGLETAP:
    {
        WB_RES::WakeUpState wakeup = { .state = 3, .level = 0 };
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    case WB_RES::OfflineWakeup::DOUBLETAP:
    {
        WB_RES::WakeUpState wakeup = { .state = 2, .level = 5 }; // Level = delay between taps [0,7]
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    default:
    case WB_RES::OfflineWakeup::ALWAYSON:
    {
        if (_shouldReset)
        {
            // Force reset and wakeup from connectors
            _shouldReset = false;
            asyncPut(WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP(), AsyncRequestOptions::Empty, 1);
            break;
        }
        else
        {
            DebugLogger::error("%s: Configured wake up behavior does not permit sleeping",
                LAUNCHABLE_NAME);
            _sleepTimerElapsed = 0;
            return;
        }
    }
    }

    // TODO: Wait a few moments before powering off??
    DebugLogger::info("%s: Goodbye!", LAUNCHABLE_NAME);
    asyncPut(WB_RES::LOCAL::SYSTEM_MODE(), AsyncRequestOptions::ForceAsync, WB_RES::SystemModeValues::FULLPOWEROFF);
}

void OfflineManager::setState(WB_RES::OfflineState state)
{
    if (state == _state)
    {
        DebugLogger::warning("%s: BUG: State change to active state", LAUNCHABLE_NAME);
        return;
    }

    _state = state;
    _ledTimerElapsed = 0;
    _sleepTimerElapsed = 0;

    DebugLogger::info("%s: STATE -> %u", LAUNCHABLE_NAME, _state.getValue());
    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::Empty, _state);
}

void OfflineManager::sleepTimerTick()
{
    switch (_state.getValue())
    {
    case WB_RES::OfflineState::CONNECTED:
    {
        break;
    }
    case WB_RES::OfflineState::RUNNING:
    {
        if (_config.sleepDelay > 0)
        {
            if (_deviceMoving)
            {
                _sleepTimerElapsed = 0;
            }
            else
            {
                _sleepTimerElapsed += TIMER_TICK_SLEEP;
            }

            if (_sleepTimerElapsed >= (_config.sleepDelay * 1000))
                enterSleep();
        }
        else
        {
            _sleepTimerElapsed = 0;
        }

        break;
    }
    case WB_RES::OfflineState::INIT:
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
    case WB_RES::OfflineState::ERROR_STORAGE_FULL:
    case WB_RES::OfflineState::ERROR_BATTERY_LOW:
    case WB_RES::OfflineState::ERROR_SYSTEM_FAILURE:
    {
        _sleepTimerElapsed += TIMER_TICK_SLEEP;

        if (_sleepTimerElapsed >= 30000)
            enterSleep();

        break;
    }
    }
}

void OfflineManager::ledTimerTick()
{
    // LED indication intervals, starting with OFF time, then ON, then OFF...

    // Normal modes
    constexpr uint16_t LED_BLINK_SERIES_INIT[] = { 250, 250 };
    constexpr uint16_t LED_BLINK_SERIES_CONN[] = { 10000, 1000 };
    constexpr uint16_t LED_BLINK_SERIES_RUN[] = { 5000, 500 };

    // Error modes
    constexpr uint16_t LED_BLINK_SERIES_FULL_STORAGE[] = { 2000, 500 };
    constexpr uint16_t LED_BLINK_SERIES_CONFIG_ERROR[] = { 2000, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_SYSTEM_FAILURE[] = { 2000, 500, 500, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_LOW_BATTERY[] = { 2000, 2000 };

    _ledTimerElapsed += TIMER_TICK_LED;
    bool ledOn = _ledBlinks % 2;
    uint16_t nextTimeout = 0;

    switch (_state)
    {
    case WB_RES::OfflineState::INIT:
        nextTimeout = LED_BLINK_SERIES_INIT[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::RUNNING:
        nextTimeout = LED_BLINK_SERIES_RUN[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::CONNECTED:
        nextTimeout = LED_BLINK_SERIES_CONN[_ledBlinks % 2]; break;
    case WB_RES::OfflineState::ERROR_SYSTEM_FAILURE:
        nextTimeout = LED_BLINK_SERIES_SYSTEM_FAILURE[_ledBlinks % 6]; break;
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
        nextTimeout = LED_BLINK_SERIES_CONFIG_ERROR[_ledBlinks % 4]; break;
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
    DebugLogger::info("%s: BLE PeerChange", LAUNCHABLE_NAME);
    if (peerChange.state == WB_RES::PeerStateValues::DISCONNECTED)
    {
        _connections--;
    }
    else if (peerChange.peer.handle.hasValue())
    {
        _connections++;
    }

    if (_connections == 0) {
        if (_shouldReset)
        {
            // Configuration changed, needs to reset
            enterSleep();
        }
        else
        {
            startRecording();
        }
    }
    else if (_connections == 1) {
        onConnected();
    }

    DebugLogger::info("%s: BLE connections: %d", LAUNCHABLE_NAME, _connections);
}

void OfflineManager::handleSystemStateChange(const WB_RES::StateChange& stateChange)
{
    DebugLogger::info("%s: System state change: id (%u) state (%u)",
        LAUNCHABLE_NAME, stateChange.stateId.getValue(), stateChange.newState);

    _sleepTimerElapsed = 0; // Any state change resets sleep timer

    switch (stateChange.stateId)
    {
    case WB_RES::StateId::DOUBLETAP:
        // Double tap turns off the device if sleep delay is not set
        if (_config.sleepDelay == 0 && stateChange.newState == 1)
            enterSleep();
        break;
    case WB_RES::StateId::MOVEMENT:
        // Track whether device is moving. 
        // If sleep delay is set, the device will enter sleep after a period of not moving.
        _deviceMoving = (stateChange.newState == 1);
        break;
    default:
        break;
    }
}

void OfflineManager::handleBleAdvTimeout()
{
    DebugLogger::info("%s: Turning off BLE advertising", LAUNCHABLE_NAME);
    _advOffTimer = wb::ID_INVALID_TIMER;
    asyncDelete(WB_RES::LOCAL::COMM_BLE_ADV(), AsyncRequestOptions::ForceAsync);
}
