#pragma once

#include <vector>
#include "sbem/sbem.hpp"
#include "sbem/sbem_types.hpp"

struct Samples
{
    std::vector<OfflineAccData> acc;
    std::vector<OfflineGyroData> gyro;
    std::vector<OfflineMagnData> magn;
    std::vector<OfflineHRData> hr;
    std::vector<OfflineECGData> ecg;
    std::vector<OfflineTempData> temp;

    Samples(const SbemDocument& sbem);

    bool exportAccSamples(const std::string& filepath);
    bool exportGyroSamples(const std::string& filepath);
    bool exportMagnSamples(const std::string& filepath);
    bool exportHRSamples(const std::string& filepath);
    bool exportECGSamples(const std::string& filepath);
    bool exportTempSamples(const std::string& filepath);
};
