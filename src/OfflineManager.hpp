#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "app-resources/resources.h"
#include "comm_ble/resources.h"
#include "system_states/resources.h"
#include "OfflineTypes.hpp"

class OfflineManager FINAL : private wb::ResourceProvider, private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineManager();
    ~OfflineManager();

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

    virtual void onPutResult(
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
    void asyncReadConfigFromEEPROM();
    void asyncSaveConfigToEEPROM();

    bool startRecording();
    bool onConnected();

    bool applyConfig(const WB_RES::OfflineConfig& config);
    bool validateConfig(const WB_RES::OfflineConfig& config);

    void enterSleep();
    void setState(WB_RES::OfflineState state);

    void sleepTimerTick();
    void ledTimerTick();

    void handleBlePeerChange(const WB_RES::PeerChange& peerChange);
    void handleSystemStateChange(const WB_RES::StateChange& stateChange);

    void systemReset();

    OfflineConfig _config;

    WB_RES::OfflineState _state;
    uint8_t _connections;
    bool _deviceMoving;
    bool _shouldReset;

    wb::TimerId _sleepTimer;
    uint32_t _sleepTimerElapsed;

    wb::TimerId _ledTimer;
    uint32_t _ledTimerElapsed;
    uint8_t _ledBlinks;
};
