#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "app-resources/resources.h"
#include "modules-resources/resources.h"
#include "comm_ble/resources.h"
#include "system_states/resources.h"

extern const size_t g_OfflineConfigEepromAddr;
extern const size_t g_OfflineConfigEepromLen;

struct OfflineConfigData
{
    uint8_t wakeUp = WB_RES::OfflineWakeup::CONNECTOR;
    uint16_t params[WB_RES::OfflineMeasurement::COUNT] = {};
    uint16_t sleepDelay = 60;
    uint8_t options = WB_RES::OfflineOptionsFlags::SHAKETOCONNECT;
};

#define OFFLINE_CONFIG_EEPROM_SIZE (1 + sizeof(OfflineConfigData))
#define OFFLINE_CONFIG_EEPROM_AREA(addr, len) \
    static_assert(len >= OFFLINE_CONFIG_EEPROM_SIZE && "Insufficient offline config EEPROM data area size."); \
    const size_t g_OfflineConfigEepromAddr = addr; \
    const size_t g_OfflineConfigEepromLen = len;

class OfflineApp FINAL : private wb::ResourceProvider, private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineApp();
    ~OfflineApp();

private: /* wb::LaunchableModule*/
    virtual bool initModule() OVERRIDE;
    virtual void deinitModule() OVERRIDE;
    virtual bool startModule() OVERRIDE;
    virtual void stopModule() OVERRIDE;

private: /* wb::ResourceProvider */
    virtual void onGetRequest(
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onPutRequest(
        const whiteboard::Request& request,
        const whiteboard::ParameterList& parameters) OVERRIDE;

private: /* wb::ResourceClient */
    virtual void onGetResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onPostResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onPutResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onDeleteResult(
        whiteboard::RequestId requestId,
        whiteboard::ResourceId resourceId,
        whiteboard::Result resultCode,
        const whiteboard::Value& result) OVERRIDE;

    virtual void onSubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onUnsubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& value,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onTimer(whiteboard::TimerId timerId) OVERRIDE;

private:
    OfflineConfigData m_config;

    struct State
    {
        WB_RES::OfflineState id = WB_RES::OfflineState::INIT;
        uint8_t connections = 0;
        uint8_t measurements = 0;
        bool deviceMoving = true;
        bool connectorActive = false;
        bool bleAdvertising = true;
        bool createNewLog = false;
        bool resetRequired = false;
        bool restartLogger = false;
        int ledOverride = 0;
    } m_state;

    struct Timers
    {
        struct Sleep
        {
            wb::TimerId id = wb::ID_INVALID_TIMER;
            uint32_t elapsed = 0;
        } sleep;

        struct LED
        {
            wb::TimerId id = wb::ID_INVALID_TIMER;
            uint32_t elapsed = 0;
            uint8_t blinks = 0;
        } led;

        struct BleAdvOff
        {
            wb::TimerId id = wb::ID_INVALID_TIMER;
        } ble_adv_off;
    } m_timers;

    struct Logger
    {
        static constexpr size_t MAX_LOGGED_PATHS = (
            WB_RES::OfflineMeasurement::COUNT + WB_RES::Gesture::COUNT
            );
        static constexpr size_t MAX_PATH_LEN = 42;
        char paths[MAX_LOGGED_PATHS][MAX_PATH_LEN];
    } m_logger;

    void asyncReadConfigFromEEPROM();
    void asyncSaveConfigToEEPROM();

    WB_RES::OfflineConfig getConfig() const;
    void setConfig(const WB_RES::OfflineConfig& config);
    bool applyConfig(const WB_RES::OfflineConfig& config);

    void startLogging();
    void stopLogging();
    void restartLogging();
    uint8_t configureLogger(const WB_RES::OfflineConfig& config);

    void setState(WB_RES::OfflineState state);
    void powerOff();
    bool onConnected();
    void onEnterSleep();
    void onWakeUp();

    void sleepTimerTick();
    void ledTimerTick();

    void handleBlePeerChange(const WB_RES::PeerChange& peerChange);
    void handleSystemStateChange(const WB_RES::StateChange& stateChange);
    void handleTapGesture(const WB_RES::TapGestureData& data);
    void setBleAdv(bool enabled);
    void setBleAdvTimeout(uint32_t timeout);
};
