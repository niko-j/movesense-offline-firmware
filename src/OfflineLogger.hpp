#pragma once

#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "app-resources/resources.h"
#include "meas_ecg/resources.h"
#include "meas_hr/resources.h"
#include "meas_acc/resources.h"
#include "meas_gyro/resources.h"
#include "meas_magn/resources.h"
#include "meas_temp/resources.h"

constexpr uint8_t MAX_MEASUREMENT_SUBSCRIPTIONS = 4;

class OfflineLogger FINAL : private wb::ResourceProvider, private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineLogger();
    ~OfflineLogger();

private: /* wb::LaunchableModule*/
    virtual bool initModule() OVERRIDE;
    virtual void deinitModule() OVERRIDE;
    virtual bool startModule() OVERRIDE;
    virtual void stopModule() OVERRIDE;

private: /* wb::ResourceProvider */
    virtual void onGetRequest(
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onPostRequest(
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

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& value,
        const wb::ParameterList& parameters) OVERRIDE;

private:
    void applyConfig(const WB_RES::OfflineConfig& config);
    bool configureMeasurements(const WB_RES::OfflineConfig& config);
    uint8_t configureDataLogger(const WB_RES::OfflineConfig& config);

    bool startLogging();
    void stopLogging();

    void recordECGSamples(const WB_RES::ECGData& data);
    void recordHeartRateSamples(const WB_RES::HRData& data);
    void recordAccelerationSamples(const WB_RES::AccData& data);
    void recordGyroscopeSamples(const WB_RES::GyroData& data);
    void recordMagnetometerSamples(const WB_RES::MagnData& data);
    void recordTemperatureSamples(const WB_RES::TemperatureValue& data);
    void recordActivity(const WB_RES::AccData& data);

    bool _configured;
    bool _logging;
    bool _loggedResource[WB_RES::OfflineMeasurement::COUNT];
    
    struct ResourceEntry
    {
        bool subscribed = false;
        uint16_t sampleRate = 0;
        wb::ResourceId resourceId = wb::ID_INVALID_RESOURCE;
    };
    ResourceEntry _measurements[MAX_MEASUREMENT_SUBSCRIPTIONS];
    ResourceEntry* findResourceEntry(wb::ResourceId id);
    bool isSubscribedToResources() const;
    void subscribeResources();
    void unsubscribeResources();
};
