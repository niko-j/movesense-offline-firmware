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
};
