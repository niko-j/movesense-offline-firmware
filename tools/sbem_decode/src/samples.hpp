#pragma once

#include <vector>
#include "offline_measurements.hpp"

struct Samples
{
    std::vector<OfflineAccData> acc;
    std::vector<OfflineGyroData> gyro;
    std::vector<OfflineMagnData> magn;
    std::vector<OfflineHRData> hr;
    std::vector<OfflineECGData> ecg;
    std::vector<OfflineTempData> temp;
    std::vector<OfflineActivityData> activity;
    std::vector<OfflineTapData> taps;

    Samples(const SbemDocument& sbem);
};
