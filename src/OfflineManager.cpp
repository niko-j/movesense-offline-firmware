#include "movesense.h"
#include "OfflineManager.hpp"
#include "utils/Conversions.hpp"

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
#include "mem_datalogger/resources.h"
#include "mem_logbook/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const OfflineManager::LAUNCHABLE_NAME = "OfflineMan";

constexpr uint8_t EEPROM_CONFIG_INDEX = 0;
constexpr uint32_t EEPROM_CONFIG_ADDR = 0;
constexpr uint8_t EEPROM_CONFIG_INIT_MAGIC = 0x40; // Change this for breaking changes

constexpr uint32_t TIMER_TICK_SLEEP = 1000;
constexpr uint32_t TIMER_TICK_LED = 250;
constexpr uint32_t TIMER_BLE_ADV_TIMEOUT = 30 * 1000;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_CONFIG::LID,
    WB_RES::LOCAL::OFFLINE_STATE::LID
};

OfflineManager::OfflineManager()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , m_config({})
    , m_state({})
    , m_timers({})
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
    mModuleState = WB_RES::ModuleStateValues::STARTED;

#if DEBUG
    // Setup debugging
    WB_RES::DebugLogConfig logConfig = {
        .minimalLevel = WB_RES::DebugLevel::VERBOSE
    };
    asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG(), AsyncRequestOptions::Empty, logConfig);
    asyncPut(WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON(), AsyncRequestOptions::Empty, true);
#endif

    asyncReadConfigFromEEPROM();

    // Subscribe to BLE peers (Halt offline recording when connected)
    asyncSubscribe(WB_RES::LOCAL::COMM_BLE_PEERS(), AsyncRequestOptions::Empty);

    // Subscribe to system states
    asyncSubscribe(
        WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::BATTERYSTATUS);

    asyncSubscribe(
        WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::CONNECTOR);

    // Subscribe to logbook IsFull
    asyncSubscribe(WB_RES::LOCAL::MEM_LOGBOOK_ISFULL());

    // Setup timers
    m_timers.sleep.id = ResourceClient::startTimer(TIMER_TICK_SLEEP, true);
    m_timers.led.id = ResourceClient::startTimer(TIMER_TICK_LED, true);

    return true;
}

void OfflineManager::stopModule()
{
    mModuleState = WB_RES::ModuleStateValues::STOPPED;

    asyncUnsubscribe(WB_RES::LOCAL::COMM_BLE_PEERS());

    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::BATTERYSTATUS);

    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::CONNECTOR);

    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::MOVEMENT);

    asyncUnsubscribe(WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
        WB_RES::StateId::DOUBLETAP);
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
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, m_state.id);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_CONFIG::LID:
    {
        WB_RES::OfflineConfig data = internalToWb(m_config);
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
    DebugLogger::info("%s: onGetResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

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
                setState(WB_RES::OfflineState::RUNNING);
            else
                setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        }
        else
        {
            DebugLogger::info("%s: Failed to read EEPROM: %d", LAUNCHABLE_NAME, resultCode);
        }
        break;
    }
    default:
        break;
    }

    ASSERT(resultCode < 400)
}

void OfflineManager::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPostResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_ADV::LID:
        m_state.bleAdvertising = true;
        break;
    default:
        break;
    }
}

void OfflineManager::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onPutResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_DATALOGGER_STATE::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            // Start new log after stopping data logger
            if (m_state.createNewLog)
            {
                m_state.createNewLog = false;
                asyncPost(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::Empty);
            }
        }
        else
        {
            switch (resultCode)
            {
            case wb::HTTP_CODE_INSUFFICIENT_STORAGE:
                setState(WB_RES::OfflineState::ERROR_STORAGE_FULL);
                break;
            default:
                setState(WB_RES::OfflineState::ERROR_SYSTEM_FAILURE);
                break;
            }
        }
        break;
    }
    case WB_RES::LOCAL::MEM_DATALOGGER_CONFIG::LID:
    {
        switch (resultCode)
        {
        case wb::HTTP_CODE_BAD_REQUEST:
        {
            setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
            break;
        }
        case wb::HTTP_CODE_OK:
        {
            break;
        }
        default:
        {
            setState(WB_RES::OfflineState::ERROR_SYSTEM_FAILURE);
            break;
        }
        }
        break;
    }
    default:
        break;
    }
}

void OfflineManager::onDeleteResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    DebugLogger::info("%s: onDeleteResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_ADV::LID:
        m_state.bleAdvertising = false;
        break;
    default:
        break;
    }
}

void OfflineManager::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::info("%s: onSubscribeResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
}

void OfflineManager::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    DebugLogger::info("%s: onUnsubscribeResult (res: %d), status: %d",
        LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
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
    case WB_RES::LOCAL::GESTURE_TAP::LID:
    {
        if (m_config.optionsFlags & OfflineConfig::OptionsLogTapGestures)
            m_state.ledOverride = 2000;
        break;
    }
    case WB_RES::LOCAL::GESTURE_SHAKE::LID:
    {
        auto gesture = value.convertTo<const WB_RES::ShakeGestureData&>();
        if (!m_state.bleAdvertising && gesture.duration > 2000)
        {
            setBleAdv(true);
            setBleAdvTimeout(TIMER_BLE_ADV_TIMEOUT);
        }

        if (m_config.optionsFlags & OfflineConfig::OptionsLogShakeGestures)
            m_state.ledOverride = 2000;

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
    if (timerId == m_timers.sleep.id)
    {
        sleepTimerTick();
        return;
    }

    if (timerId == m_timers.led.id)
    {
        ledTimerTick();
        return;
    }

    if (timerId == m_timers.ble_adv_off.id)
    {
        m_timers.ble_adv_off.id = wb::ID_INVALID_TIMER;
        setBleAdv(false);
        return;
    }
}

void OfflineManager::asyncReadConfigFromEEPROM()
{
    asyncGet(
        WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(),
        AsyncRequestOptions::ForceAsync,
        EEPROM_CONFIG_INDEX, EEPROM_CONFIG_ADDR, 1 + sizeof(m_config));
}

void OfflineManager::asyncSaveConfigToEEPROM()
{
    // Guarding that we don't end up in an endless loop.
    {
        static uint16_t failsave = 0;
        failsave++;
        ASSERT(failsave < 100);
    }

    uint8_t data[1 + sizeof(m_config)] = {};
    data[0] = EEPROM_CONFIG_INIT_MAGIC;
    memcpy(data + 1, &m_config, sizeof(m_config));

    WB_RES::EepromData eepromData = { .bytes = wb::MakeArray(data) };

    asyncPut(WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), AsyncRequestOptions::ForceAsync,
        EEPROM_CONFIG_INDEX, EEPROM_CONFIG_ADDR, eepromData);
}

bool OfflineManager::applyConfig(const WB_RES::OfflineConfig& config)
{
    if (m_state.id.getValue() != WB_RES::OfflineState::INIT &&
        m_state.id.getValue() != WB_RES::OfflineState::CONNECTED &&
        m_state.id.getValue() != WB_RES::OfflineState::ERROR_INVALID_CONFIG)
    {
        return false;
    }

    // Validations
    {
        // Subscribing to both DOUBLETAP and MOVEMENT system states seems to not work.
        // This is probably a bug in the core firmware.

        if (config.sleepDelay == 0) // Settings incompatible with "double tap to sleep"
        {
            if (
                // cannot subscribe system state MOVEMENT if already subscribed to DOUBLETAP
                config.wakeUpBehavior == WB_RES::OfflineWakeup::MOVEMENT ||

                // these will result in false positive DOUBLETAP events
                config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES ||
                config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES ||
                config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT
                )
            {
                return false;
            }
        }
        else // Settings incompatible with "sleep after period of inactivity"
        {
            if (
                // cannot subscribe system state DOUBLETAP if already subscribed to MOVEMENT
                config.wakeUpBehavior == WB_RES::OfflineWakeup::DOUBLETAP
                )
            {
                return false;
            }
        }
    }

    // Check the need to reconfigure the sleep condition
    if (m_state.id.getValue() == WB_RES::OfflineState::INIT)
    {
        configureSleep(config);
    }
    else
    {
        // Changing sleep behavior between timer and double taps requires
        // resetting the device due to a likely software bug in system states API
        m_state.resetRequired |= ((m_config.sleepDelay > 0) != (config.sleepDelay > 0));
    }

    bool blePowerSave = !!(config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT);
    bool prevPowerSave = !!(m_config.optionsFlags & OfflineConfig::OptionsShakeToConnect);

    if (blePowerSave != prevPowerSave)
    {
        if (blePowerSave)
            asyncSubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
        else
            asyncUnsubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
    }


    m_config = wbToInternal(config);
    m_state.measurements = configureLogger(config);
    return true;
}

void OfflineManager::configureSleep(const WB_RES::OfflineConfig& config)
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
        // Double tap to manually enter sleep
        asyncSubscribe(
            WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
            WB_RES::StateId::DOUBLETAP);
    }
}

uint8_t OfflineManager::configureLogger(const WB_RES::OfflineConfig& config)
{
    if (m_state.id.getValue() != WB_RES::OfflineState::INIT &&
        m_state.id.getValue() != WB_RES::OfflineState::CONNECTED)
    {
        DebugLogger::error(
            "%s: Cannot configure logger when not either in INIT or CONNECTED state",
            LAUNCHABLE_NAME);
        return 0;
    }

    uint8_t count = 0;
    memset(m_paths, 0, sizeof(m_paths));

    bool ecgCompression = !!(config.options & WB_RES::OfflineOptionsFlags::COMPRESSECGSAMPLES);
    bool logTapGestures = !!(config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES);
    bool logShakeGestures = !!(config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES);

    WB_RES::DataEntry entries[MAX_LOGGED_PATHS] = {};
    for (auto i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        if (config.sampleRates[i])
        {
            switch (i)
            {
            case WB_RES::OfflineMeasurement::ECG:
                if (ecgCompression)
                    sprintf(m_paths[count], "/Offline/Meas/ECG/Compressed/%u", config.sampleRates[i]);
                else
                    sprintf(m_paths[count], "/Offline/Meas/ECG/%u", config.sampleRates[i]);
                break;
            case WB_RES::OfflineMeasurement::ACC:
                sprintf(m_paths[count], "/Offline/Meas/Acc/%u", config.sampleRates[i]);
                break;
            case WB_RES::OfflineMeasurement::GYRO:
                sprintf(m_paths[count], "/Offline/Meas/Gyro/%u", config.sampleRates[i]);
                break;
            case WB_RES::OfflineMeasurement::MAGN:
                sprintf(m_paths[count], "/Offline/Meas/Magn/%u", config.sampleRates[i]);
                break;
            case WB_RES::OfflineMeasurement::HR:
                strcpy(m_paths[count], "/Offline/Meas/HR");
                break;
            case WB_RES::OfflineMeasurement::RR:
                strcpy(m_paths[count], "/Offline/Meas/RR");
                break;
            case WB_RES::OfflineMeasurement::TEMP:
                strcpy(m_paths[count], "/Offline/Meas/Temp");
                break;
            case WB_RES::OfflineMeasurement::ACTIVITY:
                strcpy(m_paths[count], "/Offline/Meas/Activity");
                break;
            }

            entries[count].path = m_paths[count];
            count++;
        }
    }

    if (logTapGestures)
    {
        strcpy(m_paths[count], "/Gesture/Tap");
        entries[count].path = m_paths[count];
        count++;
    }

    if (logShakeGestures)
    {
        strcpy(m_paths[count], "/Gesture/Shake");
        entries[count].path = m_paths[count];
        count++;
    }

    WB_RES::DataLoggerConfig logConfig = {};
    logConfig.dataEntries.dataEntry = wb::MakeArray(entries, count);
    asyncPut(WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(), AsyncRequestOptions::Empty, logConfig);
    return count;
}

void OfflineManager::startLogging()
{
    if (m_state.measurements == 0)
    {
        DebugLogger::info("%s: No configured measurements.", LAUNCHABLE_NAME);
        return;
    }

    DebugLogger::info("%s: Starting Data Logger...", LAUNCHABLE_NAME);

    // Subscribe to gestures to show LED confirmation on successful detection
    {
        if (m_config.optionsFlags & OfflineConfig::OptionsLogTapGestures)
        {
            asyncSubscribe(WB_RES::LOCAL::GESTURE_TAP());
        }

        if (m_config.optionsFlags & OfflineConfig::OptionsLogShakeGestures &&
            !(m_config.optionsFlags & OfflineConfig::OptionsShakeToConnect))
        {
            asyncSubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
        }
    }

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::ForceAsync,
        WB_RES::DataLoggerState::DATALOGGER_LOGGING);

    return;
}

void OfflineManager::stopLogging()
{
    ASSERT(m_state.id.getValue() == WB_RES::OfflineState::RUNNING);

    DebugLogger::info("%s: Stopping Data Logger...", LAUNCHABLE_NAME);
    m_state.createNewLog = true; // Start a new entry
    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::Empty,
        WB_RES::DataLoggerState::DATALOGGER_READY);

    if (m_config.optionsFlags & OfflineConfig::OptionsLogTapGestures)
    {
        asyncUnsubscribe(WB_RES::LOCAL::GESTURE_TAP());
    }

    if (m_config.optionsFlags & OfflineConfig::OptionsLogShakeGestures &&
        !(m_config.optionsFlags & OfflineConfig::OptionsShakeToConnect))
    {
        asyncUnsubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
    }
}

void OfflineManager::onEnterSleep()
{
    DebugLogger::info("%s: Entering sleep!", LAUNCHABLE_NAME);

    // Stop timers
    ResourceClient::stopTimer(m_timers.led.id);
    ResourceClient::stopTimer(m_timers.sleep.id);

    // Logging is automatically stopped when exiting RUNNING state

    // Stop BLE advertising
    if (m_state.bleAdvertising)
    {
        setBleAdv(false);
        setBleAdvTimeout(0);
    }

    // Turn off LED
    {
        WB_RES::LedState ledState = {};
        ledState.isOn = false;
        asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
    }
}

void OfflineManager::onWakeUp()
{
    if (m_state.id.getValue() != WB_RES::OfflineState::SLEEP)
        return;

    DebugLogger::info("%s: Waking up!", LAUNCHABLE_NAME);

    // Enable BLE ADV
    setBleAdv(true);

    // Start timers
    {
        m_timers.led.elapsed = 0;
        m_timers.led.id = ResourceClient::startTimer(TIMER_TICK_LED, true);
        ASSERT(m_timers.led.id != wb::ID_INVALID_TIMER);

        m_timers.sleep.elapsed = 0;
        m_timers.sleep.id = ResourceClient::startTimer(TIMER_TICK_SLEEP, true);
        ASSERT(m_timers.sleep.id != wb::ID_INVALID_TIMER);
    }
}

void OfflineManager::setState(WB_RES::OfflineState state)
{
    if (state == m_state.id)
    {
        DebugLogger::warning("%s: setState: already in state %u",
            LAUNCHABLE_NAME, state.getValue());
        return;
    }

    // On exit

    switch (m_state.id.getValue())
    {
    case WB_RES::OfflineState::SLEEP:
        onWakeUp();
        break;
    case WB_RES::OfflineState::RUNNING:
        stopLogging();
        break;
    default:
        break;
    }

    m_state.id = state;
    m_timers.led.elapsed = 0;
    m_timers.sleep.elapsed = 0;
    DebugLogger::info("%s: STATE -> %u", LAUNCHABLE_NAME, m_state.id.getValue());

    // On enter
    switch (state.getValue())
    {
    case WB_RES::OfflineState::SLEEP:
        onEnterSleep();
        break;
    case WB_RES::OfflineState::RUNNING:
        setBleAdvTimeout(TIMER_BLE_ADV_TIMEOUT);
        startLogging();
        break;
    default:
        setBleAdvTimeout(0);
        break;
    }

    updateResource(WB_RES::LOCAL::OFFLINE_STATE(), ResponseOptions::Empty, m_state.id);
}

void OfflineManager::powerOff()
{
    DebugLogger::info("%s: powerOff()", LAUNCHABLE_NAME);

    // Stop LED timer and turn the LED on
    {
        ResourceClient::stopTimer(m_timers.led.id);

        WB_RES::LedState ledState = {};
        ledState.isOn = true;
        asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
    }

    // Check if this is a reset
    if (m_state.resetRequired)
    {
        DebugLogger::warning(
            "%s: Device is being reset! Touch the connectors to turn on the device.",
            LAUNCHABLE_NAME);

        asyncPut(WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP(), AsyncRequestOptions::Empty, 1);

        asyncPut(
            WB_RES::LOCAL::SYSTEM_MODE(), AsyncRequestOptions::ForceAsync,
            WB_RES::SystemModeValues::FULLPOWEROFF);
        return;
    }

    // Configure wake up triggers
    switch (m_config.wakeUpBehavior)
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
    case WB_RES::OfflineWakeup::DOUBLETAP:
    {
        WB_RES::WakeUpState wakeup = { .state = 2, .level = 5 }; // Level = delay between taps [0,7]
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        break;
    }
    default:
    case WB_RES::OfflineWakeup::ALWAYSON:
    {
        DebugLogger::warning(
            "%s: Configured wake up behavior does not permit power off",
            LAUNCHABLE_NAME);
        m_timers.sleep.elapsed = 0;
        return;
    }
    }

    DebugLogger::info("%s: Goodbye!", LAUNCHABLE_NAME);
    asyncPut(
        WB_RES::LOCAL::SYSTEM_MODE(), AsyncRequestOptions::ForceAsync,
        WB_RES::SystemMode::FULLPOWEROFF);
}


bool OfflineManager::onConnected()
{
    if (m_state.id.getValue() == WB_RES::OfflineState::ERROR_BATTERY_LOW ||
        m_state.id.getValue() == WB_RES::OfflineState::ERROR_SYSTEM_FAILURE ||
        m_state.connections == 0)
    {
        return false;
    }

    setState(WB_RES::OfflineState::CONNECTED);
    return true;
}

void OfflineManager::sleepTimerTick()
{
    switch (m_state.id.getValue())
    {
    case WB_RES::OfflineState::CONNECTED:
    {
        break;
    }
    case WB_RES::OfflineState::RUNNING:
    {
        if (m_config.sleepDelay > 0)
        {
            if (m_state.deviceMoving || m_state.connectorActive)
            {
                m_timers.sleep.elapsed = 0;
            }
            else
            {
                m_timers.sleep.elapsed += TIMER_TICK_SLEEP;
            }

            if (m_timers.sleep.elapsed >= (m_config.sleepDelay * 1000))
                setState(WB_RES::OfflineState::SLEEP);
        }
        else
        {
            m_timers.sleep.elapsed = 0;
        }

        break;
    }
    case WB_RES::OfflineState::INIT:
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
    case WB_RES::OfflineState::ERROR_STORAGE_FULL:
    case WB_RES::OfflineState::ERROR_BATTERY_LOW:
    case WB_RES::OfflineState::ERROR_SYSTEM_FAILURE:
    {
        m_timers.sleep.elapsed += TIMER_TICK_SLEEP;

        if (m_timers.sleep.elapsed >= 30000)
            powerOff();

        break;
    }
    }
}

void OfflineManager::ledTimerTick()
{
    // LED indication intervals, starting with OFF time, then ON, then OFF...

    // Normal modes
    constexpr uint16_t LED_BLINK_SERIES_INIT[] = { 250, 250 };
    constexpr uint16_t LED_BLINK_SERIES_CONN[] = { 1000, 250 };
    constexpr uint16_t LED_BLINK_SERIES_RUN[] = { 4000, 250 };
    constexpr uint16_t LED_BLINK_SERIES_RUN_ADV[] = { 4000, 250, 250, 250 };

    // Error modes
    constexpr uint16_t LED_BLINK_SERIES_FULL_STORAGE[] = { 2000, 500 };
    constexpr uint16_t LED_BLINK_SERIES_CONFIG_ERROR[] = { 2000, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_SYSTEM_FAILURE[] = { 2000, 500, 500, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_LOW_BATTERY[] = { 2000, 2000 };

    if (m_state.ledOverride > 0)
    {
        m_timers.led.elapsed = 0;
        m_timers.led.blinks = 0;
        m_state.ledOverride -= TIMER_TICK_LED;

        WB_RES::LedState ledState = {};
        ledState.isOn = true;
        asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
        return;
    }

    m_timers.led.elapsed += TIMER_TICK_LED;
    uint8_t& blinks = m_timers.led.blinks;
    bool ledOn = m_timers.led.blinks % 2;
    uint16_t nextTimeout = 0;

    switch (m_state.id.getValue())
    {
    case WB_RES::OfflineState::INIT:
        nextTimeout = LED_BLINK_SERIES_INIT[blinks % 2]; break;
    case WB_RES::OfflineState::RUNNING:
        if (m_state.bleAdvertising)
            nextTimeout = LED_BLINK_SERIES_RUN_ADV[blinks % 4];
        else
            nextTimeout = LED_BLINK_SERIES_RUN[blinks % 2];
        break;
    case WB_RES::OfflineState::CONNECTED:
        nextTimeout = LED_BLINK_SERIES_CONN[blinks % 2]; break;
    case WB_RES::OfflineState::ERROR_SYSTEM_FAILURE:
        nextTimeout = LED_BLINK_SERIES_SYSTEM_FAILURE[blinks % 6]; break;
    case WB_RES::OfflineState::ERROR_INVALID_CONFIG:
        nextTimeout = LED_BLINK_SERIES_CONFIG_ERROR[blinks % 4]; break;
    case WB_RES::OfflineState::ERROR_STORAGE_FULL:
        nextTimeout = LED_BLINK_SERIES_FULL_STORAGE[blinks % 2]; break;
    case WB_RES::OfflineState::ERROR_BATTERY_LOW:
        nextTimeout = LED_BLINK_SERIES_LOW_BATTERY[blinks % 2]; break;
    }

    if (nextTimeout > 0 && m_timers.led.elapsed >= nextTimeout)
    {
        m_timers.led.elapsed = 0;
        m_timers.led.blinks++;

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
        m_state.connections--;
    }
    else if (peerChange.peer.handle.hasValue())
    {
        m_state.connections++;
    }

    if (m_state.connections == 0)
    {
        if (m_state.resetRequired)
            powerOff();
        else
            setState(WB_RES::OfflineState::RUNNING);
    }
    else if (m_state.connections == 1)
    {
        onConnected();
    }

    DebugLogger::info("%s: BLE connections: %d", LAUNCHABLE_NAME, m_state.connections);
}

void OfflineManager::handleSystemStateChange(const WB_RES::StateChange& stateChange)
{
    DebugLogger::info("%s: System state change: id (%u) state (%u)",
        LAUNCHABLE_NAME, stateChange.stateId.getValue(), stateChange.newState);

    bool sleeping = (m_state.id.getValue() == WB_RES::OfflineState::SLEEP);

    switch (stateChange.stateId)
    {
    case WB_RES::StateId::CONNECTOR:
    {
        m_state.connectorActive = (stateChange.newState == 1);
        if (sleeping)
        {
            if (m_config.wakeUpBehavior == OfflineConfig::WakeUpConnector)
                setState(WB_RES::OfflineState::RUNNING);
        }
        break;
    }
    case WB_RES::StateId::DOUBLETAP:
    {
        if (sleeping)
        {
            if (m_config.wakeUpBehavior == OfflineConfig::WakeUpDoubleTap)
                setState(WB_RES::OfflineState::RUNNING);
        }
        else
        {
            if (m_config.sleepDelay == 0 && stateChange.newState == 1)
                setState(WB_RES::OfflineState::SLEEP);
        }
        break;
    }
    case WB_RES::StateId::MOVEMENT:
    {
        // Track whether device is moving. 
        // If sleep delay is set, the device will enter sleep after a period of not moving.
        m_state.deviceMoving = (stateChange.newState == 1);

        if (sleeping)
        {
            if (m_config.wakeUpBehavior == OfflineConfig::WakeUpMovement)
                setState(WB_RES::OfflineState::RUNNING);
        }
        break;
    }
    default:
        break;
    }
}

void OfflineManager::setBleAdv(bool enabled)
{
    if (enabled == m_state.bleAdvertising)
        return;

    if (enabled)
    {


        DebugLogger::info("%s: Turning on BLE advertising", LAUNCHABLE_NAME);
        asyncPost(WB_RES::LOCAL::COMM_BLE_ADV(), AsyncRequestOptions::ForceAsync);
    }
    else
    {
        DebugLogger::info("%s: Turning off BLE advertising", LAUNCHABLE_NAME);
        asyncDelete(WB_RES::LOCAL::COMM_BLE_ADV(), AsyncRequestOptions::ForceAsync);
    }
}

void OfflineManager::setBleAdvTimeout(uint32_t timeout)
{
    if (timeout > 0)
    {
        bool enabled = !!(m_config.optionsFlags & OfflineConfig::OptionsShakeToConnect);
        if (m_timers.ble_adv_off.id == wb::ID_INVALID_TIMER && enabled)
        {
            DebugLogger::verbose("%s: BLE ADV timeout set to %u ms", LAUNCHABLE_NAME, timeout);
            m_timers.ble_adv_off.id = ResourceClient::startTimer(timeout, false);
        }
    }
    else
    {
        if (m_timers.ble_adv_off.id != wb::ID_INVALID_TIMER)
        {
            DebugLogger::verbose("%s: BLE ADV timeout disabled", LAUNCHABLE_NAME);
            ResourceClient::stopTimer(m_timers.ble_adv_off.id);
            m_timers.ble_adv_off.id = wb::ID_INVALID_TIMER;
        }
    }
}
