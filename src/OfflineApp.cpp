#include "movesense.h"
#include "OfflineApp.hpp"

#include "app-resources/resources.h"
#include "modules-resources/resources.h"
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
#include "movesense_time/resources.h"

#include "common/core/dbgassert.h"
#include "DebugLogger.hpp"

const char* const OfflineApp::LAUNCHABLE_NAME = "OfflineApp";
constexpr uint8_t EEPROM_INIT_MAGIC = 0x42; // Change this for breaking changes

constexpr uint32_t TIMER_TICK_SLEEP = 1000;
constexpr uint32_t TIMER_TICK_LED = 250;
constexpr uint32_t TIMER_BLE_ADV_TIMEOUT = 30 * 1000;
constexpr uint32_t TIMER_GESTURE_LED_OVERRIDE_DURATION = 2000;
constexpr uint32_t TIMER_START_LOG_DELAY = 4000;

static const wb::LocalResourceId sProviderResources[] = {
    WB_RES::LOCAL::OFFLINE_CONFIG::LID,
    WB_RES::LOCAL::OFFLINE_STATE::LID,
    WB_RES::LOCAL::OFFLINE_DEBUG::LID
};

OfflineApp::OfflineApp()
    : ResourceProvider(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , ResourceClient(WBDEBUG_NAME(__FUNCTION__), WB_EXEC_CTX_APPLICATION)
    , LaunchableModule(LAUNCHABLE_NAME, WB_EXEC_CTX_APPLICATION)
    , m_config({})
    , m_debug({})
    , m_state({})
    , m_timers({})
{
}

OfflineApp::~OfflineApp()
{
}

bool OfflineApp::initModule()
{
    if (registerProviderResources(sProviderResources) != wb::HTTP_CODE_OK) {
        return false;
    }

    mModuleState = WB_RES::ModuleStateValues::INITIALIZED;
    return true;
}

void OfflineApp::deinitModule()
{
    unregisterProviderResources(sProviderResources);
    mModuleState = WB_RES::ModuleStateValues::UNINITIALIZED;
}

bool OfflineApp::startModule()
{
    mModuleState = WB_RES::ModuleStateValues::STARTED;

    // Check and store reset reason
    if (faultcom_GetLastFaultStr(false, (char*)m_debug.lastFault, sizeof(m_debug.lastFault)))
    {
        m_debug.resetTime = WbTimeGet();
    }

    // Automatically toggle UART off in release builds
#ifdef NDEBUG
    asyncPut(WB_RES::LOCAL::SYSTEM_SETTINGS_UARTON(), AsyncRequestOptions::Empty, false);
#endif

    // Setup logging
    WB_RES::DebugLogConfig logConfig = {
        .minimalLevel = WB_RES::DebugLevel::WARNING
    };
    asyncPut(WB_RES::LOCAL::SYSTEM_DEBUG_LOG_CONFIG(), AsyncRequestOptions::Empty, logConfig);

    // Read data from EEPROM
    asyncGet(
        WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(),
        AsyncRequestOptions::ForceAsync,
        0, g_OfflineDataEepromAddr, OFFLINE_DATA_EEPROM_SIZE);

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

void OfflineApp::stopModule()
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

void OfflineApp::onGetRequest(
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
        WB_RES::OfflineConfig data = getConfig();
        returnResult(request, wb::HTTP_CODE_OK, ResponseOptions::Empty, data);
        break;
    }
    case WB_RES::LOCAL::OFFLINE_DEBUG::LID:
    {
        WB_RES::OfflineDebugInfo data = {
            .resetTime = m_debug.resetTime,
            .lastFault = wb::MakeArray(m_debug.lastFault),
        };
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

void OfflineApp::onPutRequest(
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
        m_state.configRequest = &request;
        if (!applyConfig(conf))
        {
            m_state.configRequest = nullptr;
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

void OfflineApp::onGetResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onGetResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX::LID:
    {
        if (resultCode == whiteboard::HTTP_CODE_OK)
        {
            auto data = result.convertTo<const WB_RES::EepromData&>();

            if (data.bytes[0] == EEPROM_INIT_MAGIC)
            {
                ASSERT(data.bytes.size() == OFFLINE_DATA_EEPROM_SIZE);

                memcpy(&m_config, &data.bytes[1], sizeof(m_config));
                DebugLogger::info("%s: Offline mode configuration restored", LAUNCHABLE_NAME);

                if (m_debug.resetTime == 0)
                    memcpy(&m_debug, &data.bytes[1 + sizeof(m_config)], sizeof(m_debug));
                else
                    asyncSaveDataToEEPROM(); // New debug data to save
            }
            else
            {
                DebugLogger::info("%s: Offline mode is not configured, using defaults",
                    LAUNCHABLE_NAME);
            }

            if (!applyConfig(getConfig()))
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

void OfflineApp::onPostResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onPostResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_ADV::LID:
        if (resultCode == wb::HTTP_CODE_CREATED)
        {
            m_state.bleAdvertising = true;
        }
        break;
    default:
        break;
    }
}

void OfflineApp::onPutResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onPutResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::MEM_DATALOGGER_STATE::LID:
    {
        if (resultCode == wb::HTTP_CODE_OK)
        {
            // Should the logger be restarted immediately?
            if (m_state.restartLogger)
            {
                if (m_state.createNewLog)
                {
                    m_state.createNewLog = false;
                    asyncPost(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::Empty);
                }

                m_state.restartLogger = false;
                asyncPut(
                    WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::Empty,
                    WB_RES::DataLoggerState::DATALOGGER_LOGGING);
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
        if (resultCode == wb::HTTP_CODE_OK)
        {
            m_state.validConfig = true;

            if (m_state.configRequest)
            {
                asyncSaveDataToEEPROM();
                returnResult(*m_state.configRequest, wb::HTTP_CODE_OK);
                m_state.configRequest = nullptr;
            }

            if (m_state.id.getValue() == WB_RES::OfflineState::INIT)
                setState(WB_RES::OfflineState::RUNNING);
        }
        else
        {
            setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
            if (m_state.configRequest)
            {
                returnResult(*m_state.configRequest, wb::HTTP_CODE_BAD_REQUEST);
                m_state.configRequest = nullptr;
            }
        }
    }
    default:
        break;
    }
}

void OfflineApp::onDeleteResult(
    whiteboard::RequestId requestId,
    whiteboard::ResourceId resourceId,
    whiteboard::Result resultCode,
    const whiteboard::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onDeleteResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }

    switch (resourceId.localResourceId)
    {
    case WB_RES::LOCAL::COMM_BLE_ADV::LID:
        if (resultCode == wb::HTTP_CODE_OK)
        {
            m_state.bleAdvertising = false;
        }
        break;
    default:
        break;
    }
}

void OfflineApp::onSubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onSubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }
}

void OfflineApp::onUnsubscribeResult(
    wb::RequestId requestId,
    wb::ResourceId resourceId,
    wb::Result resultCode,
    const wb::Value& result)
{
    if (resultCode >= 400)
    {
        DebugLogger::error("%s: onUnsubscribeResult resource: %d, status: %d",
            LAUNCHABLE_NAME, resourceId.localResourceId, resultCode);
    }
}

void OfflineApp::onNotify(
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
        auto tap = value.convertTo<const WB_RES::TapGestureData&>();
        handleTapGesture(tap);
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

        if (m_config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES)
            overrideLed(TIMER_GESTURE_LED_OVERRIDE_DURATION);

        break;
    }
    default:
        DebugLogger::warning("%s: Unhandled notification from resource: %d", 
            LAUNCHABLE_NAME, resourceId.localResourceId);
        break;
    }
}

void OfflineApp::onTimer(whiteboard::TimerId timerId)
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

    if (timerId == m_timers.start_log_delay.id)
    {
        onStartLogging();
        return;
    }
}

void OfflineApp::asyncSaveDataToEEPROM()
{
    // Guarding that we don't end up in an endless loop.
    {
        static uint16_t failsave = 0;
        failsave++;
        ASSERT(failsave < 100);
    }

    DebugLogger::info("%s: Saving data in EEPROM", LAUNCHABLE_NAME);

    OfflineEepromData data;
    data.config = m_config;
    data.debug = m_debug;

    uint8_t buffer[OFFLINE_DATA_EEPROM_SIZE] = {};
    buffer[0] = EEPROM_INIT_MAGIC;
    memcpy(buffer + 1, &data, sizeof(data));
    WB_RES::EepromData eepromData = { .bytes = wb::MakeArray(buffer) };

    asyncPut(WB_RES::LOCAL::COMPONENT_EEPROM_EEPROMINDEX(), AsyncRequestOptions::ForceAsync,
        0, g_OfflineDataEepromAddr, eepromData);
}

WB_RES::OfflineConfig OfflineApp::getConfig() const
{
    return WB_RES::OfflineConfig{
        .wakeUpBehavior = static_cast<WB_RES::OfflineWakeup::Type>(m_config.wakeUp),
        .measurementParams = wb::MakeArray(m_config.params),
        .sleepDelay = m_config.sleepDelay,
        .options = m_config.options,
    };
}

bool OfflineApp::applyConfig(const WB_RES::OfflineConfig& config)
{
    m_state.validConfig = false; // Config is invalid until DataLogger accepts it
    m_logger.number_of_paths = 0;

    // Verify logging not running
    if (m_state.id.getValue() == WB_RES::OfflineState::RUNNING)
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
                config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT ||
                config.options & WB_RES::OfflineOptionsFlags::TRIPLETAPTOSTARTLOG
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

    bool init = (m_state.id.getValue() == WB_RES::OfflineState::INIT);

    // Check the need to reconfigure the sleep condition
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
            // Double tap to manually enter sleep
            asyncSubscribe(
                WB_RES::LOCAL::SYSTEM_STATES_STATEID(), AsyncRequestOptions::Empty,
                WB_RES::StateId::DOUBLETAP);
        }
    }
    else
    {
        // Changing sleep behavior between timer and double taps requires
        // resetting the device due to a likely software bug in system states API
        m_state.resetRequired |= ((m_config.sleepDelay > 0) != (config.sleepDelay > 0));
    }

    bool blePowerSave = !!(config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT);
    bool prevPowerSave = !!(m_config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT);

    if (blePowerSave != prevPowerSave || init)
    {
        if (blePowerSave)
            asyncSubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
        else if (!init)
            asyncUnsubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
    }

    m_config.wakeUp = config.wakeUpBehavior;
    m_config.sleepDelay = config.sleepDelay;
    m_config.options = config.options;
    memcpy(m_config.params, config.measurementParams.begin(), sizeof(m_config.params));

    configureLogger(config);

    DebugLogger::info("%s: Configuration changed!", LAUNCHABLE_NAME);
    return true;
}

void OfflineApp::configureLogger(const WB_RES::OfflineConfig& config)
{
    uint8_t count = 0;
    memset(m_logger.paths, 0, sizeof(m_logger.paths));

    bool ecgCompression = !!(config.options & WB_RES::OfflineOptionsFlags::COMPRESSECGSAMPLES);
    bool logTapGestures = !!(config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES);
    bool logShakeGestures = !!(config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES);
    bool logOrientation = !!(config.options & WB_RES::OfflineOptionsFlags::LOGORIENTATION);

    WB_RES::DataEntry entries[Logger::MAX_LOGGED_PATHS] = {};
    for (auto i = 0; i < WB_RES::OfflineMeasurement::COUNT; i++)
    {
        if (config.measurementParams[i])
        {
            switch (i)
            {
            case WB_RES::OfflineMeasurement::ECG:
                if (ecgCompression)
                    sprintf(m_logger.paths[count], "/Offline/Meas/ECG/Compressed/%u", config.measurementParams[i]);
                else
                    sprintf(m_logger.paths[count], "/Offline/Meas/ECG/%u", config.measurementParams[i]);
                break;
            case WB_RES::OfflineMeasurement::ACC:
                sprintf(m_logger.paths[count], "/Offline/Meas/Acc/%u", config.measurementParams[i]);
                break;
            case WB_RES::OfflineMeasurement::GYRO:
                sprintf(m_logger.paths[count], "/Offline/Meas/Gyro/%u", config.measurementParams[i]);
                break;
            case WB_RES::OfflineMeasurement::MAGN:
                sprintf(m_logger.paths[count], "/Offline/Meas/Magn/%u", config.measurementParams[i]);
                break;
            case WB_RES::OfflineMeasurement::HR:
                strcpy(m_logger.paths[count], "/Offline/Meas/HR");
                break;
            case WB_RES::OfflineMeasurement::RR:
                strcpy(m_logger.paths[count], "/Offline/Meas/RR");
                break;
            case WB_RES::OfflineMeasurement::TEMP:
                strcpy(m_logger.paths[count], "/Offline/Meas/Temp");
                break;
            case WB_RES::OfflineMeasurement::ACTIVITY:
                sprintf(m_logger.paths[count], "/Offline/Meas/Activity/%u", config.measurementParams[i]);
                break;
            }

            entries[count].path = m_logger.paths[count];
            count++;
        }
    }

    if (logTapGestures)
    {
        strcpy(m_logger.paths[count], "/Gesture/Tap");
        entries[count].path = m_logger.paths[count];
        count++;
    }

    if (logShakeGestures)
    {
        strcpy(m_logger.paths[count], "/Gesture/Shake");
        entries[count].path = m_logger.paths[count];
        count++;
    }

    if (logOrientation)
    {
        strcpy(m_logger.paths[count], "/Gesture/Orientation");
        entries[count].path = m_logger.paths[count];
        count++;
    }

    m_logger.number_of_paths = count;

    WB_RES::DataLoggerConfig logConfig = {};
    logConfig.dataEntries.dataEntry = wb::MakeArray(entries, count);
    asyncPut(WB_RES::LOCAL::MEM_DATALOGGER_CONFIG(), AsyncRequestOptions::Empty, logConfig);
}

void OfflineApp::startLogging()
{
    if (!m_state.validConfig || m_logger.number_of_paths == 0)
    {
        DebugLogger::info("%s: No configured measurements.", LAUNCHABLE_NAME);
        setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        return;
    }

    DebugLogger::info("%s: Starting Data Logger...", LAUNCHABLE_NAME);

    if (m_state.createNewLog)
    {
        m_state.createNewLog = false;
        asyncPost(WB_RES::LOCAL::MEM_LOGBOOK_ENTRIES(), AsyncRequestOptions::Empty);
    }

    // Subscribe to gestures to show LED confirmation on successful detection
    {
        if (m_config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES ||
            m_config.options & WB_RES::OfflineOptionsFlags::TRIPLETAPTOSTARTLOG)
        {
            asyncSubscribe(
                WB_RES::LOCAL::GESTURE_TAP(),
                AsyncRequestOptions::NotCriticalSubscription);
        }

        if (m_config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES &&
            !(m_config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT))
        {
            asyncSubscribe(
                WB_RES::LOCAL::GESTURE_SHAKE(),
                AsyncRequestOptions::NotCriticalSubscription);
        }
    }

    // Start logging in 2 seconds
    {
        overrideLed(TIMER_START_LOG_DELAY);
        m_timers.start_log_delay.id = ResourceClient::startTimer(TIMER_START_LOG_DELAY, false);
    }

    return;
}

void OfflineApp::stopLogging()
{
    ASSERT(m_state.id.getValue() == WB_RES::OfflineState::RUNNING);

    if (m_state.validConfig && m_logger.number_of_paths > 0)
    {
        DebugLogger::info("%s: Stopping Data Logger...", LAUNCHABLE_NAME);
        m_state.createNewLog = true; // Start a new entry
        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::Empty,
            WB_RES::DataLoggerState::DATALOGGER_READY);
    }

    if (m_config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES ||
        m_config.options & WB_RES::OfflineOptionsFlags::TRIPLETAPTOSTARTLOG)
    {
        asyncUnsubscribe(WB_RES::LOCAL::GESTURE_TAP());
    }

    if (m_config.options & WB_RES::OfflineOptionsFlags::LOGSHAKEGESTURES &&
        !(m_config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT))
    {
        asyncUnsubscribe(WB_RES::LOCAL::GESTURE_SHAKE());
    }
}

void OfflineApp::restartLogging()
{
    ASSERT(m_state.id.getValue() == WB_RES::OfflineState::RUNNING);

    DebugLogger::info("%s: Restarting Data Logger...", LAUNCHABLE_NAME);
    m_state.createNewLog = true;
    m_state.restartLogger = true;

    asyncPut(
        WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::Empty,
        WB_RES::DataLoggerState::DATALOGGER_READY);
}

void OfflineApp::onEnterSleep()
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

void OfflineApp::onWakeUp()
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

    if (m_state.validConfig)
        setState(WB_RES::OfflineState::RUNNING);
    else
        setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
}

void OfflineApp::onStartLogging()
{
    if (m_state.id.getValue() == WB_RES::OfflineState::RUNNING)
    {
        asyncPut(
            WB_RES::LOCAL::MEM_DATALOGGER_STATE(), AsyncRequestOptions::ForceAsync,
            WB_RES::DataLoggerState::DATALOGGER_LOGGING);
    }
}

void OfflineApp::setState(WB_RES::OfflineState state)
{
    if (state == m_state.id)
    {
        DebugLogger::warning("%s: setState: already in state %u",
            LAUNCHABLE_NAME, state.getValue());
        return;
    }

    m_state.stateEnterTimestamp = WbTimestampGet();

    // On exit

    bool isErrorState = (
        state == state.ERROR_SYSTEM_FAILURE ||
        state == state.ERROR_INVALID_CONFIG
        );

    if (!isErrorState)
    {
        switch (m_state.id.getValue())
        {
        case WB_RES::OfflineState::RUNNING:
            stopLogging();
            break;
        default:
            break;
        }
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

void OfflineApp::powerOff(bool reset)
{
    DebugLogger::info("%s: powerOff() reset: %s", LAUNCHABLE_NAME, reset ? "true" : "false");

    // Stop LED timer and turn the LED on
    {
        ResourceClient::stopTimer(m_timers.led.id);

        WB_RES::LedState ledState = {};
        ledState.isOn = true;
        asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
    }

    // Check if this is a reset
    if (reset)
    {
        DebugLogger::warning("%s: Device is being reset!", LAUNCHABLE_NAME);

        WB_RES::WakeUpState wakeup = { .state = 1, .level = 1 };
        asyncPut(WB_RES::LOCAL::COMPONENT_LSM6DS3_WAKEUP(), AsyncRequestOptions::Empty, wakeup);
        asyncPut(WB_RES::LOCAL::COMPONENT_MAX3000X_WAKEUP(), AsyncRequestOptions::Empty, 1);
        asyncPut(
            WB_RES::LOCAL::SYSTEM_MODE(), AsyncRequestOptions::ForceAsync,
            WB_RES::SystemModeValues::FULLPOWEROFF);
        return;
    }

    // Configure wake up triggers
    switch (m_config.wakeUp)
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
        WB_RES::WakeUpState wakeup = { .state = 2, .level = 0 }; // Level = delay between taps [0,7]
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


bool OfflineApp::onConnected()
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

void OfflineApp::sleepTimerTick()
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
        if (m_state.connections == 0)
            m_timers.sleep.elapsed += TIMER_TICK_SLEEP;

        if (m_timers.sleep.elapsed >= 30000)
            powerOff(false);

        break;
    }
    }
}

void OfflineApp::ledTimerTick()
{
    // LED indication intervals, starting with OFF time, then ON, then OFF...

    // Normal modes
    constexpr uint16_t LED_BLINK_SERIES_INIT[] = { 250, 250 };
    constexpr uint16_t LED_BLINK_SERIES_CONN[] = { 1000, 250 };
    constexpr uint16_t LED_BLINK_SERIES_RUN[] = { 4000, 250 };
    constexpr uint16_t LED_BLINK_SERIES_RUN_ADV[] = { 4000, 250, 250, 250 };

    // Error modes
    constexpr uint16_t LED_BLINK_SERIES_FULL_STORAGE[] = { 500, 250 };
    constexpr uint16_t LED_BLINK_SERIES_CONFIG_ERROR[] = { 500, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_SYSTEM_FAILURE[] = { 2000, 500, 500, 500, 500, 500 };
    constexpr uint16_t LED_BLINK_SERIES_LOW_BATTERY[] = { 2000, 2000 };

    if (m_state.ledOverride > 0)
    {
        m_state.ledOverride -= TIMER_TICK_LED;
        if (m_state.ledOverride <= 0)
        {
            m_timers.led.blinks = 0;

            WB_RES::LedState ledState = {};
            ledState.isOn = false;
            asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
        }
        else if (m_timers.led.blinks == 0)
        {
            m_timers.led.blinks = 1;

            WB_RES::LedState ledState = {};
            ledState.isOn = true;
            asyncPut(WB_RES::LOCAL::COMPONENT_LEDS_LEDINDEX(), AsyncRequestOptions::Empty, 0, ledState);
        }
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

void OfflineApp::overrideLed(uint32_t duration)
{
    m_state.ledOverride = duration;
    m_timers.led.elapsed = 0;
    m_timers.led.blinks = 0;
}

void OfflineApp::handleBlePeerChange(const WB_RES::PeerChange& peerChange)
{
    DebugLogger::verbose("%s: BLE PeerChange", LAUNCHABLE_NAME);
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
        {
            powerOff(true);
        }
        else if (m_state.id.getValue() == WB_RES::OfflineState::CONNECTED)
        {
            if (m_state.validConfig)
                setState(WB_RES::OfflineState::RUNNING);
            else
                setState(WB_RES::OfflineState::ERROR_INVALID_CONFIG);
        }
    }
    else if (m_state.connections == 1)
    {
        onConnected();
    }

    DebugLogger::info("%s: BLE connections: %d", LAUNCHABLE_NAME, m_state.connections);
}

void OfflineApp::handleSystemStateChange(const WB_RES::StateChange& stateChange)
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
            if (m_config.wakeUp == WB_RES::OfflineWakeup::CONNECTOR)
            {
                int diff = WbTimestampDifferenceMs(m_state.stateEnterTimestamp, stateChange.timestamp);
                if (diff > 2000)
                    onWakeUp();
            }
        }

        if (m_config.options & WB_RES::OfflineOptionsFlags::STUDSTOCONNECT)
        {
            if (!m_state.bleAdvertising &&
                m_state.id.getValue() == WB_RES::OfflineState::RUNNING)
            {
                setBleAdv(true);
                setBleAdvTimeout(TIMER_BLE_ADV_TIMEOUT);
            }
        }
        break;
    }
    case WB_RES::StateId::DOUBLETAP:
    {
        if (sleeping)
        {
            if (m_config.wakeUp == WB_RES::OfflineWakeup::DOUBLETAP)
                onWakeUp();
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

        if (sleeping && m_state.deviceMoving)
        {
            if (m_config.wakeUp == WB_RES::OfflineWakeup::MOVEMENT)
            {
                // Check that enough time has passed since state change
                int diff = WbTimestampDifferenceMs(m_state.stateEnterTimestamp, stateChange.timestamp);
                if (diff > 2000)
                    onWakeUp();
            }
        }
        break;
    }
    default:
        break;
    }
}

void OfflineApp::handleTapGesture(const WB_RES::TapGestureData& data)
{
    switch (m_state.id.getValue())
    {
    case WB_RES::OfflineState::RUNNING:
    {
        if (m_config.options & WB_RES::OfflineOptionsFlags::LOGTAPGESTURES)
            overrideLed(TIMER_GESTURE_LED_OVERRIDE_DURATION);

        if (m_config.options & WB_RES::OfflineOptionsFlags::TRIPLETAPTOSTARTLOG
            && data.count == 3)
        {
            overrideLed(TIMER_GESTURE_LED_OVERRIDE_DURATION);
            restartLogging();
        }

        break;
    }
    default: break;
    }
}

void OfflineApp::setBleAdv(bool enabled)
{
    if (enabled == m_state.bleAdvertising)
        return;

    if (enabled)
    {
        DebugLogger::info("%s: Turning on BLE advertising", LAUNCHABLE_NAME);
        asyncPost(WB_RES::LOCAL::COMM_BLE_ADV(), AsyncRequestOptions::Empty);
    }
    else
    {
        DebugLogger::info("%s: Turning off BLE advertising", LAUNCHABLE_NAME);
        asyncDelete(WB_RES::LOCAL::COMM_BLE_ADV(), AsyncRequestOptions::Empty);
    }
}

void OfflineApp::setBleAdvTimeout(uint32_t timeout)
{
    // Reset timer
    if (m_timers.ble_adv_off.id != wb::ID_INVALID_TIMER)
    {
        ResourceClient::stopTimer(m_timers.ble_adv_off.id);
        m_timers.ble_adv_off.id = wb::ID_INVALID_TIMER;
    }

    // Set new timer if timeout set
    if (timeout > 0)
    {
        if (m_config.options & WB_RES::OfflineOptionsFlags::SHAKETOCONNECT ||
            m_config.options & WB_RES::OfflineOptionsFlags::STUDSTOCONNECT)
        {
            DebugLogger::verbose("%s: BLE ADV timeout set to %u ms", LAUNCHABLE_NAME, timeout);
            m_timers.ble_adv_off.id = ResourceClient::startTimer(timeout, false);
        }
    }
    else
    {
        DebugLogger::verbose("%s: BLE ADV timeout disabled");
    }
}
