#pragma once
#include <whiteboard/LaunchableModule.h>
#include <whiteboard/ResourceClient.h>
#include <whiteboard/ResourceProvider.h>

#include "modules-resources/resources.h"
#include "meas_ecg/resources.h"
#include "meas_hr/resources.h"
#include "meas_acc/resources.h"
#include "meas_gyro/resources.h"
#include "meas_magn/resources.h"
#include "meas_temp/resources.h"
#include "internal/LowPassFilter.hpp"
#include "internal/compression/DeltaCompression.hpp"

class OfflineMeasurements FINAL : private wb::ResourceProvider, private wb::ResourceClient, public wb::LaunchableModule
{
public:
    static const char* const LAUNCHABLE_NAME;

    OfflineMeasurements();
    ~OfflineMeasurements();

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
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onSubscribe(
        const wb::Request& request,
        const wb::ParameterList& parameters) OVERRIDE;

    virtual void onUnsubscribe(
        const wb::Request& rRequest,
        const wb::ParameterList& parameters) OVERRIDE;

private: /* wb::ResourceClient */
    virtual void onSubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& result) OVERRIDE;

    virtual void onUnsubscribeResult(
        wb::RequestId requestId,
        wb::ResourceId resourceId,
        wb::Result resultCode,
        const wb::Value& rResultData) OVERRIDE;

    virtual void onNotify(
        wb::ResourceId resourceId,
        const wb::Value& value,
        const wb::ParameterList& parameters) OVERRIDE;

private:
    bool subscribeAcc(wb::LocalResourceId resourceId, int32_t param);
    bool subscribeGyro(wb::LocalResourceId resourceId, int32_t param);
    bool subscribeMagn(wb::LocalResourceId resourceId, int32_t param);
    bool subscribeHR(wb::LocalResourceId resourceId);
    bool subscribeECG(wb::LocalResourceId resourceId, int32_t param);
    bool subscribeTemp(wb::LocalResourceId resourceId);

    void dropAccSubscription(wb::LocalResourceId resourceId);
    void dropGyroSubscription(wb::LocalResourceId resourceId);
    void dropMagnSubscription(wb::LocalResourceId resourceId);
    void dropHRSubscription(wb::LocalResourceId resourceId);
    void dropECGSubscription(wb::LocalResourceId resourceId);
    void dropTempSubscription(wb::LocalResourceId resourceId);

    void recordECGSamples(const WB_RES::ECGData& data);
    void compressECGSamples(const WB_RES::ECGData& data);
    void recordHRAverages(const WB_RES::HRData& data);
    void recordRRIntervals(const WB_RES::HRData& data);
    void recordAccelerationSamples(const WB_RES::AccData& data);
    void recordGyroscopeSamples(const WB_RES::GyroData& data);
    void recordMagnetometerSamples(const WB_RES::MagnData& data);
    void recordTemperatureSamples(const WB_RES::TemperatureValue& data);
    void recordActivity(const WB_RES::AccData& data);

    uint16_t getAccSampleRate();

    struct State
    {
        uint8_t subscribers[WB_RES::OfflineMeasurement::COUNT] = {};
        uint16_t params[WB_RES::OfflineMeasurement::COUNT] = {};

        struct ECG
        {
            static constexpr uint8_t COMPRESSOR_BLOCK_SIZE = 32;
            int32_t sample_offset = 0;
            offline_meas::compression::DeltaCompression<int16_t, COMPRESSOR_BLOCK_SIZE> compressor;
            void reset();
        } ecg;

        struct HR
        {
            uint8_t average = 0;
            void reset();
        } hr;

        struct RtoR
        {
            uint32_t timestamp = 0;
            uint8_t index = 0;
            void reset();
        } r_to_r;

        struct Activity
        {
            uint32_t activity_start = 0;
            uint32_t accumulated_count = 0;
            float accumulated_average = 0;
            offline_meas::LowPassFilter lpf;
            void reset();
        } activity;

        struct Temperature
        {
            int8_t value = 0;
            void reset();
        } temperature;
    } m_state;

    struct Options
    {
        bool useEcgCompression;
    } m_options;
};
